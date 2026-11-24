// Heroes I creature window (homm1-remaster project). See h1_armywin.h and docs/analysis note v60.
#include "h1_armywin.h"
#include "h1_visit.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "castle.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "race.h"
#include "screen.h"

namespace
{
    // Creature names: H1::creatureNameOne (table 0x493068; H1-INTENDED(D16) 28..35). FUN_00417c59 makes the first letter upper case.

    // Table 0x4938b0 (index = creature byte +0x0d), 0x4935bc (morale -3..3), 0x49359c (luck -3..3).
    const char * const speedNames[5] = { "", "Slow", "Medium", "Fast", "Blazing" };
    const char * const moraleNames[7] = { "Treason", "Awful", "Poor", "Normal", "Good", "Great", "Blood!" };
    const char * const luckNames[7] = { "Cursed", "Awful", "Bad", "Normal", "Good", "Great", "Irish" };

    // armywin.bin item ids (DOS file; the Windows code uses 0x7800 / 0x7803 for the two buttons).
    enum : int
    {
        ID_COUNT_BACK = 1,
        ID_COUNT = 2,
        ID_NAME = 3,
        ID_STATS = 4,
        ID_CREATURE = 5, // added by FUN_00417c59 (FUN_0047bb00 with id 5)
        ID_EXIT = 0x8000,
        ID_DISMISS = 0x8003
    };

    bool hasH1Artifact( const Heroes & hero, const int h1Artifact )
    {
        // FUN_004472d0: is the artifact in one of the 14 slots (hero +0xa0)?
        const int engineArtifact = H1::engineArtifactFromH1( h1Artifact );
        return engineArtifact >= 0 && hero.GetBagArtifacts().isPresentArtifact( Artifact( engineArtifact ) );
    }

    int limit3( const int v )
    {
        return v < -3 ? -3 : ( v > 3 ? 3 : v );
    }

    std::string format( const char * fmt, const char * s, const int v )
    {
        char buffer[128];
        std::snprintf( buffer, sizeof( buffer ), fmt, s, v );
        return buffer;
    }
}

namespace H1
{
    int creatureFromMonster( const int monsterId )
    {
        // H1-INTENDED(D16): 36 creatures in extension games (h1_rules.h).
        for ( int i = 0; i < creatureCount(); ++i ) {
            if ( creatureRow( i ).monsterId == monsterId ) {
                return i;
            }
        }
        return -1;
    }

    int armyMorale( const Army & army )
    {
        int groups[5] = { 0, 0, 0, 0, 0 };
        int changes = 0;
        int last = -1;
        for ( size_t k = 0; k < 5; ++k ) {
            const Troop * troop = army.GetTroop( k );
            const int creature = ( troop != nullptr && troop->isValid() ) ? creatureFromMonster( troop->GetID() ) : -1;
            if ( creature < 0 ) {
                continue;
            }
            // H1-INTENDED(D16): extension creatures (28..) count with the neutral group 24..27 (group 4); before v241 they were counted past
            // the end of `groups` (creature / 6 = 5..6).
            ++groups[std::min( creature / 6, 4 )];
            // The original compares with the previous filled slot only, so "A B A" counts 3 changes (as written).
            if ( creature != last ) {
                ++changes;
                last = creature;
            }
        }
        if ( changes < 2 ) {
            return 0;
        }
        int present = 0;
        for ( const int g : groups ) {
            if ( g != 0 ) {
                ++present;
            }
        }
        switch ( present ) {
        case 1:
            return 1;
        case 3:
            return -1;
        case 4:
            return -2;
        case 5:
            return -3;
        default:
            return 0;
        }
    }

    int heroMorale( const Heroes * hero, const Army & army, const Castle * castle )
    {
        int morale = armyMorale( army );
        if ( hero != nullptr ) {
            // Hero +0x1c (class) == 0: Knight.
            if ( hero->GetRace() == Race::KNGT ) {
                ++morale;
            }
            // Hero +0x35: the map object bonuses until the next battle (h1_visit bonus bits, note v198).
            morale += bonusMorale( *hero );
            for ( int art = 8; art <= 11; ++art ) {
                if ( hasH1Artifact( *hero, art ) ) {
                    ++morale;
                }
            }
            if ( hasH1Artifact( *hero, 12 ) ) {
                morale -= 2;
            }
        }
        if ( castle != nullptr && castle->isBuild( BUILD_TAVERN ) ) {
            ++morale;
        }
        return limit3( morale );
    }

    int heroLuck( const Heroes * hero )
    {
        if ( hero == nullptr ) {
            return 0;
        }
        int luck = 0;
        for ( int art = 0x1f; art <= 0x22; ++art ) {
            if ( hasH1Artifact( *hero, art ) ) {
                ++luck;
            }
        }
        // Hero +0x36 (h1_visit bonus bits, note v198).
        luck += bonusLuck( *hero );
        return limit3( luck );
    }

    bool showCreatureWindow( const int monsterId, const int count, const Heroes * hero, Army * army, const bool hideDismiss, const bool popup,
                             const Castle * castle, const int windowY, const BattleUnitView * battle )
    {
        const int32_t windowX = ( battle != nullptr ) ? battle->x : 0x77;
        const int32_t windowTop = ( battle != nullptr ) ? battle->y : windowY;
        const int facing = ( battle != nullptr ) ? battle->facing : 0;
        const int creature = creatureFromMonster( monsterId );
        if ( creature < 0 ) {
            return false;
        }
        const CreatureRow & row = creatureRow( creature );

        UI::Layout layout = UI::loadLayout( "armywin.bin" );

        // Creature picture: "<name>.wlk" for flag 2, else "<name>.wip" (swordsman's file is "swrdsman"); frame 0 at (x, 0xa4).
        // x: 0x74 (param_7 = 0, the only value the hero screen passes), 0x49 for flag 1 creatures. Mirrored only when param_7 = 1.
        const std::string fileBase = creatureFile( creature );
        UI::Item picture;
        picture.type = UI::ITEM_ICON;
        // FUN_00417c59: flag 1 creatures 0x49 (param_7 = 0) / 0x95 (param_7 = 1) / 0x1e (other); others 0x6a (param_7 = 1) / 0x74.
        if ( row.flags & 1 ) {
            picture.x = ( facing == 0 ) ? 0x49 : ( facing == 1 ? 0x95 : 0x1e );
        }
        else {
            picture.x = ( facing == 1 ) ? 0x6a : 0x74;
        }
        picture.mirror = ( facing == 1 );
        picture.y = 0xa4;
        picture.w = 0x56;
        picture.h = 0x95;
        // H1-INTENDED(D16): extension creatures always ".wip" (made by tools/extension/make_ext_creatures.py: walk frames, or the standing
        // frame for the bone dragon, whose flight frames do not fit; note v241).
        picture.name = fileBase + ( ( ( row.flags & 2 ) && creature < firstExtensionCreature ) ? ".wlk" : ".wip" );
        picture.frame = 0;
        picture.id = ID_CREATURE;
        picture.flags = 0x10;
        // FUN_00477880 with -1 puts the new item one layer above the top one: drawn last.
        layout.items.push_back( picture );

        std::string name = creatureNameOne( creature );
        name[0] = static_cast<char>( name[0] - 0x20 );
        if ( UI::Item * it = layout.find( ID_NAME ) ) {
            it->text = name;
        }

        // Stats text (format strings 0x490bb4 .. 0x490c08, labels table 0x4938c8).
        std::string text = format( "%s%d", "Attack Skill: ", row.attack );
        // Hero attack (+0x30) added in brackets when not zero.
        const int attackBonus = ( hero != nullptr ) ? hero->GetAttack() : 0;
        if ( attackBonus != 0 ) {
            text += format( "%s (%d)", "", row.attack + attackBonus );
        }
        text += format( "\n%s%d", "Defense Skill: ", row.defense );
        // Hero defense (+0x31); +3 more for a battle unit in state 9 (not in this window's callers yet).
        const int defenseBonus = ( ( hero != nullptr ) ? hero->GetDefense() : 0 ) + ( ( battle != nullptr && battle->spell9 ) ? 3 : 0 );
        if ( defenseBonus != 0 ) {
            text += format( "%s (%d)", "", row.defense + defenseBonus );
        }
        if ( ( row.flags & 4 ) != 0 ) {
            // Shots: the battle unit's (+0x1e) when there is one, else the table's; "Shots left: " only in battle (DAT_004a7f64 + 0x2e == 1).
            const int shots = ( battle != nullptr ) ? battle->shotsLeft : row.shots;
            if ( shots > 0 ) {
                text += format( "\n%s%d", ( battle != nullptr ) ? "Shots left: " : "Shots: ", shots );
            }
        }
        text += format( "\n%s%d", "Damage: ", row.damageMin );
        if ( row.damageMin != row.damageMax ) {
            text += format( "%s-%d", "", row.damageMax );
        }
        text += format( "\n%s%d", "Hit Points: ", row.hp );
        text += std::string( "\nSpeed: " ) + ( row.speedLevel >= 0 && row.speedLevel < 5 ? speedNames[row.speedLevel] : "" );
        const int morale = ( army != nullptr ) ? heroMorale( hero, *army, castle ) : 0;
        text += std::string( "\nMorale: " ) + moraleNames[morale + 3];
        text += std::string( "\nLuck: " ) + luckNames[heroLuck( hero ) + 3];
        if ( UI::Item * it = layout.find( ID_STATS ) ) {
            it->text = text;
        }

        auto hide = [&layout]( const int id ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->hidden = true;
                it->inactive = true;
            }
        };
        if ( hideDismiss ) {
            hide( ID_DISMISS );
        }
        if ( popup ) {
            hide( ID_EXIT );
        }
        if ( count < 1 ) {
            hide( ID_COUNT_BACK );
            hide( ID_COUNT );
        }
        else if ( UI::Item * it = layout.find( ID_COUNT ) ) {
            it->text = std::to_string( count );
        }

        if ( popup ) {
            UI::runLayoutDialog( layout, windowX, windowTop, true );
            return false;
        }

        // FUN_0041853a: every 0x5a ticks (GetTickCount, milliseconds) the picture shows the next of frames 0..5.
        using Clock = std::chrono::steady_clock;
        Clock::time_point next = Clock::now() + std::chrono::milliseconds( 0x5a );
        int counter = 0;
        auto tick = [&]() {
            if ( Clock::now() <= next ) {
                return false;
            }
            ++counter;
            if ( UI::Item * it = layout.find( ID_CREATURE ) ) {
                it->frame = counter % 6;
            }
            next = Clock::now() + std::chrono::milliseconds( 0x5a );
            return true;
        };

        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        bool dismissed = false;
        while ( true ) {
            const int result = UI::runLayoutDialogTicking( layout, windowX, windowTop, tick );
            if ( result != ID_DISMISS ) {
                break;
            }
            // 0x7803: "Are you sure ..." Yes/No at (0xb1, 0x36), over the still open window; No keeps the window open.
            UI::drawLayout( display, layout, ( display.width() - 640 ) / 2 + windowX, ( display.height() - 480 ) / 2 + windowTop, -1 );
            if ( UI::messageBox( "Are you sure you want to dismiss this army?", UI::MESSAGE_YES_NO, 0xb1, 0x36 ) == 0x8005 ) {
                dismissed = true;
                break;
            }
        }
        // The window is drawn again after the question, so put back the screen from before the window.
        fheroes2::Copy( saved, display );
        display.render();
        if ( !dismissed ) {
            return false;
        }

        // After the window: every slot holding this creature is emptied (FUN_00417c59, loop over the 5 slots).
        if ( army != nullptr ) {
            for ( size_t k = 0; k < 5; ++k ) {
                Troop * troop = army->GetTroop( k );
                if ( troop != nullptr && troop->isValid() && troop->GetID() == monsterId ) {
                    troop->Reset();
                }
            }
        }
        return true;
    }
}
