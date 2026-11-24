// Heroes I hero screen (homm1-remaster project). See h1_heroscreen.h and docs/analysis note v58.
#include "h1_heroes.h"
#include "h1_heroscreen.h"
#include "h1_visit.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "color.h"
#include "cursor.h"
#include "dialog.h"
#include "h1_armywin.h"
#include "mp2.h"
#include "h1_gfx.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "image.h"
#include "localevent.h"
#include "race.h"
#include "screen.h"
#include "spell_book.h"

namespace
{
    // Item ids of herowind.bin (DOS). Exit / Dismiss are 0x8000 / 0x8003 in the DOS file (0x7800 / 0x7803 in the Windows code).
    enum : int
    {
        ID_TITLE = 2,
        ID_ARTIFACT_BACK = 6, // .. 19
        ID_ARTIFACT = 20, // .. 33
        ID_PORTRAIT = 65,
        ID_STAT_TEXT = 76, // .. 79
        ID_STAT_HIGHLIGHT = 81, // .. 84 (strip.icn 1, hidden, clickable)
        ID_CREST = 86,
        ID_ARMY_BACK = 87, // .. 91
        ID_ARMY_MONSTER = 92, // .. 96
        ID_ARMY_COUNT = 97, // .. 101
        ID_ARMY_SELECT = 102, // .. 106
        ID_MORALE = 200, // .. 202
        ID_LUCK = 203, // .. 205
        ID_EXPERIENCE_ICON = 206,
        ID_EXPERIENCE = 207,
        ID_STATUS = 302,
        ID_EXIT = 0x8000,
        ID_DISMISS = 0x8003
    };

    // HEROESW.EXE tables.
    const char * const className[5] = { "Knight", "Barbarian", "Sorceress", "Warlock", "Necromancer" }; // 0x493058; H1-INTENDED(D16) 5th
    const char * const statName[4] = { "Attack Skill", "Defense Skill", "Spell Power", "Knowledge" }; // 0x493028
    const char * const statText[4] = { // 0x493040
        "Your attack skill is a bonus added to each creature's attack skill.", "Your defense skill is a bonus added to each creature's defense skill.",
        "Your spell power determines the length or power of a spell.", "Your knowledge is the number of each spell you are able to memorize." };
    // Creature names: H1::creatureNameOne (table 0x493068; H1-INTENDED(D16) 28..35).
    const char * const artifactText[38] = { // 0x492ef8
        "Ultimate Book\n(+12 Knowledge)\n\nThe Ultimate Book of Knowledge increases your knowledge by 12.",
        "Ultimate Sword\n(+12 Attack)\n\nThe Ultimate Sword of Dominion increases your attack skill by 12.",
        "Ultimate Cloak\n(+12 Defense)\n\nThe Ultimate Cloak of Protection increases your defense skill by 12.",
        "Ultimate Wand\n(+12 Spell Power)\n\nThe Ultimate Wand of Magic increases your spell power by 12.",
        "Arcane Necklace\n(+4 Spell Power)\n\nThe Arcane Necklace of Magic increases your spell power by 4.",
        "Caster's Bracelet\n(+2 Spell Power)\n\nThe Caster's Bracelet of Magic increases your spell power by 2.",
        "Mage's Ring\n(+2 Spell Power)\n\nThe Mage's Ring of Power increases your spell power by 2.",
        "Witches Broach\n(+3 Spell Power)\n\nThe Witch's Broach of Magic increases your spell power by 3.",
        "Medal\n\nThe Medal of Valor increases your morale.",
        "Medal\n\nThe Medal of Courage increases your morale.",
        "Medal\n\nThe Medal of Honor increases your morale.",
        "Medal\n\nThe Medal of Distinction increases your morale.",
        "Fizbin\n\nThe Fizbin of Misfortune greatly decreases your morale.",
        "Thunder Mace\n(+1 Attack)\n\nThe Thunder Mace of Dominion increases your attack skill by 1.",
        "Armored Gauntlets\n(+1 Defense)\n\nThe Armored Gauntlets of Protection increase your defense skill by 1.",
        "Defender Helm\n(+1 Defense)\n\nThe Defender Helm of Protection increases your defense skill by 1.",
        "Giant Flail\n(+1 Attack)\n\nThe Giant Flail of Dominion increases your attack skill by 1.",
        "Ballista\n\nThe Ballista of Quickness lets your catapult fire twice per combat round.",
        "Stealth Shield\n(+2 Defense)\n\nThe Stealth Shield of Protection increases your defense skill by 2.",
        "Dragon Sword\n(+3 Attack)\n\nThe Dragon Sword of Dominion increases your attack skill by 3.",
        "Power Axe\n(+2 Attack)\n\nThe Power Axe of Dominion increases your attack skill by 2.",
        "Divine Breastplate\n(+3 Defense)\n\nThe Divine Breastplate of Protection increases your defense skill by 3.",
        "Minor Scroll\n(+2 Knowledge)\n\nThe Minor Scroll of Knowledge increases your knowledge by 2.",
        "Major Scroll\n(+3 Knowledge)\n\nThe Major Scroll of Knowledge increases your knowledge by 3.",
        "Superior Scroll\n(+4 Knowledge)\n\nThe Superior Scroll of Knowledge increases your knowledge by 4.",
        "Foremost Scroll\n(+5 Knowledge)\n\nThe Foremost Scroll of Knowledge increases your knowledge by 5.",
        "Endless Sack\n\nThe Endless Sack of Gold provides you with 1000 gold per day.",
        "Endless Bag\n\nThe Endless Bag of Gold provides you with 750 gold per day.",
        "Endless Purse\n\nThe Endless Purse of Gold provides you with 500 gold per day.",
        "Nomad Boots\n\nThe Nomad Boots of Mobility increase your movement on land.",
        "Traveler's Boots\n\nThe Traveler's Boots of Mobility increase your movement on land.",
        "Rabbit's Foot\n\nThe Lucky Rabbit's Foot increases your luck in combat.",
        "Horseshoe\n\nThe Golden Horseshoe increases your luck in combat.",
        "Coin\n\nThe Gambler's Lucky Coin increases your luck in combat.",
        "Clover\n\nThe Four-Leaf Clover increases your luck in combat.",
        "Compass\n\nThe True Compass of Mobility increases your movement on land and sea.",
        "Astrolabe\n\nThe Sailors' Astrolabe of Mobility increases your movement on sea.",
        "Magic Book\n\nThe Magic Book enables you to cast spells." };

    constexpr int magicBookArtifact = 37; // '%'

    int heroClass( const Heroes & hero )
    {
        switch ( hero.GetRace() ) {
        case Race::KNGT:
            return 0;
        case Race::BARB:
            return 1;
        case Race::SORC:
            return 2;
        case Race::WRLK:
            return 3;
        case Race::NECR:
            return 4; // H1-INTENDED(D16)
        default:
            return 0;
        }
    }

    int h1Creature( const Troop & troop )
    {
        if ( !troop.isValid() ) {
            return -1;
        }
        return H1::creatureFromMonster( troop.GetID() ); // H1-INTENDED(D16): 36 creatures in extension games
    }

    int h1Artifact( const Artifact & art )
    {
        if ( !art.isValid() ) {
            return -1;
        }
        if ( art.GetID() == Artifact::MAGIC_BOOK ) {
            return magicBookArtifact;
        }
        return H1::h1ArtifactFromEngine( art.GetID() );
    }

    std::string fmt( const char * format, const int value )
    {
        char buffer[64];
        std::snprintf( buffer, sizeof( buffer ), format, value );
        return buffer;
    }

    struct Screen
    {
        Heroes & hero;
        H1::UI::Layout layout;
        int selectedSlot = -1; // DAT_004a7b90
        int hoverId = -1; // DAT_004a7bc8 + 0x56

        // FUN_00448296: army slots.
        void updateArmy()
        {
            Army & army = hero.GetArmy();
            for ( int k = 0; k < 5; ++k ) {
                const Troop * troop = army.GetTroop( static_cast<size_t>( k ) );
                const int creature = ( troop != nullptr ) ? h1Creature( *troop ) : -1;
                H1::UI::Item * back = layout.find( ID_ARMY_BACK + k );
                H1::UI::Item * monster = layout.find( ID_ARMY_MONSTER + k );
                H1::UI::Item * count = layout.find( ID_ARMY_COUNT + k );
                H1::UI::Item * select = layout.find( ID_ARMY_SELECT + k );
                if ( creature < 0 ) {
                    if ( back ) {
                        back->frame = 2;
                    }
                    if ( monster ) {
                        monster->hidden = true;
                    }
                    if ( count ) {
                        count->hidden = true;
                    }
                }
                else {
                    if ( back ) {
                        back->frame = std::min( creature / 6, 4 ) + 3; // H1-INTENDED(D16): extension creatures on the neutral background
                    }
                    if ( monster ) {
                        monster->frame = creature;
                        monster->hidden = false;
                    }
                    if ( count ) {
                        count->text = fmt( "%d", static_cast<int>( troop->GetCount() ) );
                        count->hidden = false;
                    }
                }
                // FUN_004481eb: the selection strip is shown on the selected slot only.
                if ( select ) {
                    select->hidden = ( k != selectedSlot );
                }
            }
        }

        // FUN_00447aad: everything else.
        void updateAll( const bool hideDismiss )
        {
            const int cls = heroClass( hero );
            if ( H1::UI::Item * it = layout.find( ID_TITLE ) ) {
                it->text = hero.GetName() + " the " + className[cls];
            }
            for ( int k = 0; k < 5; ++k ) {
                if ( H1::UI::Item * it = layout.find( ID_STAT_HIGHLIGHT + k ) ) {
                    it->hidden = true;
                }
                if ( H1::UI::Item * it = layout.find( ID_ARMY_SELECT + k ) ) {
                    it->hidden = true;
                }
            }
            if ( hideDismiss ) {
                if ( H1::UI::Item * it = layout.find( ID_DISMISS ) ) {
                    it->hidden = true;
                    it->inactive = true;
                }
            }
            if ( H1::UI::Item * it = layout.find( ID_PORTRAIT ) ) {
                char name[16];
                std::snprintf( name, sizeof( name ), "port%04d.icn", std::max( 0, H1::heroNumberOfId( hero.getPortraitId() ) ) );
                it->name = name;
            }
            const int stats[4] = { hero.GetAttack(), hero.GetDefense(), hero.GetPower(), hero.GetKnowledge() };
            for ( int k = 0; k < 4; ++k ) {
                if ( H1::UI::Item * it = layout.find( ID_STAT_TEXT + k ) ) {
                    it->text = fmt( "%d", stats[k] );
                }
            }
            // Luck (FUN_0041ca0a, ids 203..205): frame 0xc bad, 0x10 neutral, 0xb good; morale (FUN_00467754, ids 200..202): 0xe / 0x11 / 0xd.
            // Count of pictures = |value|, at least 1.
            auto setRow = [this]( const int firstId, const int value, const int bad, const int neutral, const int good ) {
                const int count = std::max( std::abs( value ), 1 );
                for ( int k = 0; k < 3; ++k ) {
                    if ( H1::UI::Item * it = layout.find( firstId + k ) ) {
                        it->frame = ( value < 0 ) ? bad : ( value == 0 ? neutral : good );
                        it->hidden = ( k + 1 > count );
                    }
                }
            };
            setRow( ID_LUCK, H1::heroLuck( &hero ), 0xc, 0x10, 0xb );
            setRow( ID_MORALE, H1::heroMorale( &hero, hero.GetArmy() ), 0xe, 0x11, 0xd );
            if ( H1::UI::Item * it = layout.find( ID_EXPERIENCE ) ) {
                it->text = fmt( "\n%d", static_cast<int>( hero.GetExperience() ) );
            }
            if ( H1::UI::Item * it = layout.find( ID_CREST ) ) {
                const int colour = std::max( 0, Color::GetIndex( hero.GetColor() ) );
                char name[16];
                std::snprintf( name, sizeof( name ), "crst%04d.icn", H1::pictureClass( cls ) + colour * 4 ); // H1-INTENDED(D16): necromancer = warlock crest for now
                it->name = name;
            }
            updateArmy();
            const BagArtifacts & bag = hero.GetBagArtifacts();
            for ( int k = 0; k < 14; ++k ) {
                const int art = ( static_cast<size_t>( k ) < bag.size() ) ? h1Artifact( bag[k] ) : -1;
                H1::UI::Item * icon = layout.find( ID_ARTIFACT + k );
                H1::UI::Item * back = layout.find( ID_ARTIFACT_BACK + k );
                if ( art < 0 ) {
                    if ( icon ) {
                        icon->hidden = true;
                    }
                    if ( back ) {
                        back->hidden = true;
                    }
                }
                else {
                    if ( icon ) {
                        icon->frame = art;
                        icon->hidden = false;
                    }
                    // Artifacts 0..3 (the ultimate ones) keep the background picture, the others hide it.
                    if ( back ) {
                        back->hidden = ( art >= 4 );
                    }
                }
            }
            setStatus( "Hero Screen" );
        }

        void setStatus( const std::string & text )
        {
            if ( H1::UI::Item * it = layout.find( ID_STATUS ) ) {
                it->text = text;
            }
        }

        int artifactAt( const int slot ) const
        {
            const BagArtifacts & bag = hero.GetBagArtifacts();
            return ( slot >= 0 && static_cast<size_t>( slot ) < bag.size() ) ? h1Artifact( bag[slot] ) : -1;
        }

        int creatureAt( const int slot )
        {
            const Troop * troop = hero.GetArmy().GetTroop( static_cast<size_t>( slot ) );
            return ( troop != nullptr ) ? h1Creature( *troop ) : -1;
        }

        // FUN_0044916e: status bar text for the item under the mouse.
        void statusFor( const int id )
        {
            if ( id >= ID_STAT_HIGHLIGHT && id < ID_STAT_HIGHLIGHT + 4 ) {
                setStatus( std::string( "View " ) + statName[id - ID_STAT_HIGHLIGHT] + " Info" );
            }
            else if ( id >= ID_ARTIFACT && id < ID_ARTIFACT + 14 ) {
                const int art = artifactAt( id - ID_ARTIFACT );
                if ( art < 0 ) {
                    setStatus( "Empty" );
                }
                else if ( art == magicBookArtifact ) {
                    setStatus( "View Spells" );
                }
                else {
                    setStatus( std::string( "View " ) + H1::artifactName( art ) + " Info" );
                }
            }
            else if ( id >= ID_ARMY_SELECT && id < ID_ARMY_SELECT + 5 ) {
                const int slot = id - ID_ARMY_SELECT;
                const int creature = creatureAt( slot );
                if ( selectedSlot == -1 || slot == selectedSlot ) {
                    setStatus( creature < 0 ? std::string( "Empty" ) : std::string( "Select " ) + H1::creatureNameOne( creature ) );
                }
                else {
                    const int moving = creatureAt( selectedSlot );
                    const std::string movingName = ( moving >= 0 ) ? H1::creatureNameOne( moving ) : "";
                    setStatus( creature < 0 ? "Move " + movingName : "Exchange " + movingName + " with " + H1::creatureNameOne( creature ) );
                }
            }
            else if ( id == 0x55 ) {
                setStatus( "Additional hero characteristics" );
            }
            else if ( id == ID_CREST ) {
                setStatus( "Kingdom Overview" );
            }
            else if ( id >= ID_LUCK && id < ID_LUCK + 3 ) {
                const int v = H1::heroLuck( &hero );
                setStatus( v > 0 ? "View Good Luck Info" : ( v == 0 ? "View Neutral Luck Info" : "View Bad Luck Info" ) );
            }
            else if ( id >= ID_MORALE && id < ID_MORALE + 3 ) {
                const int v = H1::heroMorale( &hero, hero.GetArmy() );
                setStatus( v > 0 ? "View Good Morale Info" : ( v == 0 ? "View Neutral Morale Info" : "View Bad Morale Info" ) );
            }
            else if ( id == ID_EXIT ) {
                setStatus( "Exit Hero Screen" );
            }
            else if ( id == ID_EXPERIENCE_ICON || id == ID_EXPERIENCE ) {
                setStatus( "View Experience Info" );
            }
            else if ( id == ID_DISMISS ) {
                setStatus( "Dismiss " + hero.GetName() + " the " + className[heroClass( hero )] );
            }
            else {
                setStatus( "Hero Screen" );
            }
        }
    };

    // FUN_00448450, left click: vstat.bin at (0xb1, 0x19) with the stat name and text.
    void showStatDialog( const int stat )
    {
        H1::UI::Layout layout = H1::UI::loadLayout( "vstat.bin" );
        if ( H1::UI::Item * it = layout.find( 1 ) ) {
            it->text = statName[stat];
        }
        if ( H1::UI::Item * it = layout.find( 2 ) ) {
            it->text = statText[stat];
        }
        H1::UI::runLayoutDialog( layout, 0xb1, 0x19, false );
    }
    // FUN_00425164: morale window text (tables 0x493cb0..0x493d00). Visit flags hero +0xae / +0xaf: the engine's visited objects stand in
    // (Statue -> the engine Temple, as the map converter places it) [temporary]. "Battle cowardice %d" (hero +0x37) is not kept by the engine.
    std::string moraleText( const Heroes & hero )
    {
        const int morale = H1::heroMorale( &hero, hero.GetArmy() );
        std::string text = morale > 0 ? "Good Morale\n\nGood morale may give your armies extra attacks in combat."
                                      : ( morale == 0 ? "Neutral Morale\n\nNeutral morale means your armies will never be blessed with extra attacks or freeze in combat."
                                                      : "Bad Morale\n\nBad morale may cause your armies to freeze in combat." );
        text += "\n\n\nCurrent Morale Modifiers:";
        const size_t start = text.size();
        if ( hero.GetRace() == Race::KNGT ) {
            text += "\nKnight bonus +1";
        }
        const int army = H1::armyMorale( hero.GetArmy() );
        if ( army > 0 ) {
            // The group named is the one of the last filled slot (table 0x493628).
            static const char * const groups[5] = { "human", "plains", "forest", "mountain", "neutral" };
            int group = 0;
            for ( size_t k = 0; k < 5; ++k ) {
                const Troop * t = hero.GetArmy().GetTroop( k );
                const int c = ( t != nullptr && t->isValid() ) ? H1::creatureFromMonster( t->GetID() ) : -1;
                if ( c >= 0 ) {
                    group = std::min( c / 6, 4 ); // H1-INTENDED(D16): extension creatures (28..) are in the neutral group
                }
            }
            text += std::string( "\nAll " ) + groups[group] + " troops +1";
        }
        if ( army == -1 ) {
            text += "\nTroops of 3 alignments -1";
        }
        if ( army == -2 ) {
            text += "\nTroops of 4 alignments -2";
        }
        if ( army == -3 ) {
            text += "\nTroops of 5 alignments -3";
        }
        static const std::pair<int, const char *> artifacts[] = { { 8, "\nMedal of Valor +1" },
                                                                  { 9, "\nMedal of Courage +1" },
                                                                  { 10, "\nMedal of Honor +1" },
                                                                  { 11, "\nMedal of Distinction +1" },
                                                                  { 12, "\nFizbin of Misfortune -2" } };
        for ( const auto & [art, line] : artifacts ) {
            const int id = H1::engineArtifactFromH1( art );
            if ( id >= 0 && hero.GetBagArtifacts().isPresentArtifact( Artifact( id ) ) ) {
                text += line;
            }
        }
        // Hero +0xae bonus bits (h1_visit, note v198).
        static const std::pair<int, const char *> visits[] = { { 2, "\nBuoy visited +1" },
                                                               { 8, "\nOasis visited +1" },
                                                               { 0x100, "\nStatue visited +2" },
                                                               { 0x20, "\nGraveyard robber -1" },
                                                               { 0x40, "\nShipwreck robber -1" } };
        for ( const auto & [bit, line] : visits ) {
            if ( H1::hasBonusBit( hero, bit ) ) {
                text += line;
            }
        }
        if ( text.size() == start ) {
            text += "\nnone";
        }
        return text;
    }

    // FUN_00425590: luck window text (tables 0x493838..0x493860).
    std::string luckText( const Heroes & hero )
    {
        const int luck = H1::heroLuck( &hero );
        std::string text = luck > 0 ? "Good Luck\n\nGood luck sometimes lets your armies get lucky attacks (double strength) in combat."
                                    : ( luck == 0 ? "Neutral Luck\n\nNeutral luck means your armies will never get lucky or unlucky attacks on the enemy."
                                                  : "Bad Luck\n\nBad luck sometimes falls on your armies in combat, causing their attacks to only do half damage." );
        text += "\n\n\nCurrent Luck Modifiers:";
        const size_t start = text.size();
        static const std::pair<int, const char *> artifacts[] = { { 0x1f, "\nLucky Rabbit's Foot +1" },
                                                                  { 0x20, "\nGolden Horseshoe +1" },
                                                                  { 0x21, "\nGambler's Lucky Coin +1" },
                                                                  { 0x22, "\nFour-Leaf Clover +1" } };
        for ( const auto & [art, line] : artifacts ) {
            const int id = H1::engineArtifactFromH1( art );
            if ( id >= 0 && hero.GetBagArtifacts().isPresentArtifact( Artifact( id ) ) ) {
                text += line;
            }
        }
        if ( H1::hasBonusBit( hero, 0x10 ) ) {
            text += "\nFaerie ring visited +1";
        }
        if ( H1::hasBonusBit( hero, 4 ) ) {
            text += "\nFountain visited +1";
        }
        if ( text.size() == start ) {
            text += "\nnone";
        }
        return text;
    }

}

namespace H1
{
    const char * artifactInfoText( const int h1Artifact )
    {
        if ( h1Artifact >= firstExtensionArtifact ) {
            return extensionArtifactDescription( h1Artifact ); // H1-INTENDED(D16)
        }
        return ( h1Artifact >= 0 && h1Artifact < 38 ) ? artifactText[h1Artifact] : "";
    }

    int runHeroScreen( Heroes & hero, const bool readonly, const bool disableDismiss )
    {
        Screen s{ hero, UI::loadLayout( "herowind.bin" ) };
        // FUN_00447aad hides Dismiss for a read-only view and in the cases the engine passes as disableDismiss.
        s.updateAll( readonly || disableDismiss );

        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        // FUN_00447a5a: heroscrn.bmp is the background of the whole 640 x 480 screen.
        const fheroes2::Image background = UI::readBmp( "heroscrn.bmp" );

        int pressed = -1;
        auto draw = [&]() {
            fheroes2::Image frame( 640, 480 );
            frame._disableTransformLayer();
            fheroes2::Fill( frame, 0, 0, 640, 480, 0 );
            fheroes2::Copy( background, 0, 0, frame, 0, 0, std::min( 640, background.width() ), std::min( 480, background.height() ) );
            UI::drawLayout( frame, s.layout, 0, 0, pressed );
            fheroes2::Copy( frame, 0, 0, display, ox, oy, 640, 480 );
            display.render();
        };

        const std::vector<int> clickable = [] {
            std::vector<int> ids;
            for ( int k = 0; k < 14; ++k ) {
                ids.push_back( ID_ARTIFACT_BACK + k );
                ids.push_back( ID_ARTIFACT + k );
            }
            for ( int k = 0; k < 4; ++k ) {
                ids.push_back( ID_STAT_HIGHLIGHT + k );
            }
            for ( int k = 0; k < 5; ++k ) {
                ids.push_back( ID_ARMY_SELECT + k );
            }
            for ( int k = 0; k < 3; ++k ) {
                ids.push_back( ID_MORALE + k );
                ids.push_back( ID_LUCK + k );
            }
            ids.insert( ids.end(), { ID_CREST, ID_EXPERIENCE_ICON, ID_EXPERIENCE, ID_EXIT, ID_DISMISS } );
            return ids;
        }();
        auto itemAt = [&]( const fheroes2::Point & p ) {
            for ( auto it = s.layout.items.rbegin(); it != s.layout.items.rend(); ++it ) {
                if ( std::find( clickable.begin(), clickable.end(), it->id ) == clickable.end() || it->inactive ) {
                    continue;
                }
                if ( p.x >= ox + it->x && p.y >= oy + it->y && p.x < ox + it->x + it->w && p.y < oy + it->y + it->h ) {
                    // Artifact background ids 6..19 are under the artifact pictures: report the artifact slot.
                    if ( it->id >= ID_ARTIFACT_BACK && it->id < ID_ARTIFACT_BACK + 14 ) {
                        return it->id - ID_ARTIFACT_BACK + ID_ARTIFACT;
                    }
                    return it->id;
                }
            }
            return -1;
        };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        draw();
        int result = Dialog::CANCEL;

        while ( le.HandleEvents() ) {
            const fheroes2::Point mouse = le.getMouseCursorPos();
            const int over = itemAt( mouse );
            bool redraw = false;
            if ( over != s.hoverId ) {
                s.hoverId = over;
                s.statusFor( over );
                redraw = true;
            }
            const int newPressed = ( le.isMouseLeftButtonPressed() && ( over == ID_EXIT || over == ID_DISMISS ) ) ? over : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                redraw = true;
            }
            if ( redraw ) {
                draw();
            }

            const bool left = ( over != -1 && le.MouseClickLeft() );
            const bool right = ( over != -1 && le.isMouseRightButtonPressed() );
            if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                break; // Exit hotkey 1
            }
            if ( !left && !right ) {
                continue;
            }

            if ( over >= ID_ARTIFACT && over < ID_ARTIFACT + 14 ) {
                const int art = s.artifactAt( over - ID_ARTIFACT );
                if ( art >= 0 ) {
                    if ( art == magicBookArtifact && left ) {
                        // Spell book (FUN_004170ba) - the engine book stands in [temporary].
                        hero.OpenSpellBook( SpellBook::Filter::ALL, false, false, {} );
                    }
                    else {
                        // FUN_004485ff: style 1 on a left click, 4 (popup) on a right click; x -1, y 0x1c.
                        UI::messageBox( artifactInfoText( art ), left ? UI::MESSAGE_OKAY : UI::MESSAGE_POPUP, -1, 0x1c );
                    }
                }
            }
            else if ( over >= ID_STAT_HIGHLIGHT && over < ID_STAT_HIGHLIGHT + 4 ) {
                const int stat = over - ID_STAT_HIGHLIGHT;
                if ( left ) {
                    showStatDialog( stat );
                }
                else {
                    UI::messageBox( std::string( statName[stat] ) + "\n\n" + statText[stat], UI::MESSAGE_POPUP, 0xb1, 0x19 );
                }
            }
            else if ( over >= ID_ARMY_SELECT && over < ID_ARMY_SELECT + 5 ) {
                const int slot = over - ID_ARMY_SELECT;
                Army & army = hero.GetArmy();
                Troop * troop = army.GetTroop( static_cast<size_t>( slot ) );
                if ( right ) {
                    if ( troop != nullptr && troop->isValid() ) {
                        // FUN_00417c59 as a popup: no Exit, no Dismiss (local_44 = 1 for a right click).
                        H1::showCreatureWindow( troop->GetID(), static_cast<int>( troop->GetCount() ), &hero, &army, true, true );
                    }
                }
                else if ( s.selectedSlot == -1 ) {
                    if ( troop != nullptr && troop->isValid() ) {
                        s.selectedSlot = slot;
                    }
                }
                else if ( slot == s.selectedSlot ) {
                    if ( troop != nullptr && troop->isValid() ) {
                        // FUN_0044969c: Dismiss hidden when the army has one filled slot (FUN_00467aa0 == 1) or in the special case
                        // DAT_004a9b54 + 0xf7 == 1 [meaning unconfirmed; the engine's read-only view stands in, temporary].
                        // Dismissing empties every slot with that creature (inside H1::showCreatureWindow).
                        H1::showCreatureWindow( troop->GetID(), static_cast<int>( troop->GetCount() ), &hero, &army,
                                                readonly || army.GetOccupiedSlotCount() == 1, false );
                    }
                    s.selectedSlot = -1;
                }
                else if ( !readonly ) {
                    // Swap (or move into an empty slot): exchange the two slots.
                    Troop * other = army.GetTroop( static_cast<size_t>( s.selectedSlot ) );
                    if ( troop != nullptr && other != nullptr ) {
                        Army::SwapTroops( *troop, *other );
                    }
                    s.selectedSlot = -1;
                }
                s.updateArmy();
                s.statusFor( over );
            }
            else if ( over >= ID_MORALE && over < ID_MORALE + 3 ) {
                // FUN_00425164: left = OK box (1), right = popup (4).
                UI::messageBox( moraleText( hero ), left ? UI::MESSAGE_OKAY : UI::MESSAGE_POPUP );
            }
            else if ( over >= ID_LUCK && over < ID_LUCK + 3 ) {
                // FUN_00425590
                UI::messageBox( luckText( hero ), left ? UI::MESSAGE_OKAY : UI::MESSAGE_POPUP );
            }
            else if ( over == ID_CREST ) {
                // The kingdom overview is reached from the castle screen and the adventure buttons (note v74).
            }
            else if ( over == ID_EXPERIENCE_ICON || over == ID_EXPERIENCE ) {
                // "Level %d\n\nCurrent experience %d\nNext level %d" (FUN_00448bc4 / FUN_00448af4 give the level and the next level;
                // the engine experience table stands in [temporary]).
                char buffer[128];
                std::snprintf( buffer, sizeof( buffer ), "Level %d\n\nCurrent experience %d\nNext level %d", hero.GetLevel(), static_cast<int>( hero.GetExperience() ),
                               static_cast<int>( Heroes::GetExperienceFromLevel( hero.GetLevel() ) ) );
                UI::messageBox( buffer, left ? UI::MESSAGE_OKAY : UI::MESSAGE_POPUP );
            }
            else if ( over == ID_EXIT && left ) {
                break;
            }
            else if ( over == ID_DISMISS && left ) {
                // FUN_00448649: "Are you sure you want to dismiss this Hero?" Yes/No at (0xb1, 0x1c).
                if ( UI::messageBox( "Are you sure you want to dismiss this Hero?", UI::MESSAGE_YES_NO, 0xb1, 0x1c ) == 0x8005 ) {
                    result = Dialog::DISMISS;
                    break;
                }
            }
            draw();
        }

        fheroes2::Copy( saved, display );
        display.render();
        return result;
    }
}
