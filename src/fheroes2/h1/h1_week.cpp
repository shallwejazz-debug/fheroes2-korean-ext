// Heroes I week and month rules (homm1-remaster project, note v168).
#include "h1_week.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

#include "army.h"
#include "castle.h"
#include "color.h"
#include "h1_castle.h"
#include "h1_newgame.h"
#include "h1_overview.h"
#include "h1_rules.h"
#include "h1_ext_objects.h"
#include "h1_sound.h"
#include "h1_ui.h"
#include "h1_visit.h"
#include "kingdom.h"
#include "monster.h"
#include "rand.h"
#include "world.h"

namespace
{
    // DAT_004a7bb0 (0 name week, 1 creature week, -1 nothing proclaimed yet), DAT_004a7bf0 (week name 0..14 or creature 0..0x17),
    // DAT_004a9e90 (0 name month, 1 creature month, 2 plague), DAT_004a7ba8 (month name 0..9 or creature).
    int weekKind = -1;
    int weekValue = 0;
    int monthKind = 0;
    int monthValue = 0;

    // 0x4936e0, 0x4936b8, 0x493068 (lower case creature names), 0x490ce0 (creature months).
    const char * const weekNames[15] = { "Squirrel", "Rabbit", "Gopher", "Badger", "Rat", "Eagle", "Weasel", "Raven",
                                         "Mongoose", "Dog", "Aardvark", "Lizard", "Tortoise", "Hedgehog", "Condor" };
    const char * const monthNames[10] = { "Grasshopper", "Ant", "Dragonfly", "Spider", "Butterfly", "Bumblebee", "Locust", "Earthworm", "Hornet", "Beetle" };
    const int monthCreatures[12] = { 0, 6, 13, 14, 9, 15, 7, 8, 18, 19, 16, 20 };

    int roll( const int low, const int high )
    {
        return static_cast<int>( Rand::Get( static_cast<uint32_t>( low ), static_cast<uint32_t>( high ) ) );
    }

    // DAT_00491e7f[creature * 0x1f] (creature +7): the creature's weekly growth (FUN_0041a1b9 local_20 before the bonuses).
    int baseGrowth( const Castle & castle, const int number, const int bits )
    {
        const int creature = H1::dwellingCreatureOf( castle, number );
        if ( creature < 0 ) {
            return 0;
        }
        int growth = H1::creatureRow( creature ).growth;
        if ( bits & 0x10 ) {
            growth += 2; // the well
        }
        return growth;
    }

    uint32_t dwellingType( const int number )
    {
        return static_cast<uint32_t>( DWELLING_MONSTER1 ) << ( number - 7 );
    }

    // FUN_00413b0e: a neutral castle gets one troop of its race each week: 1..15 -> 1-5 level 1 x 8..15, 6-10 level 2 x 5..7,
    // 11-13 level 3 x 3..5, 14-15 level 4 x 1..3 (FUN_0041c6f2: joins the same creature, else the first empty slot, else nothing).
    void neutralCastleTroop( Castle & castle )
    {
        const int r = roll( 1, 15 );
        int level = 0;
        int count = 0;
        if ( r < 6 ) {
            level = 0;
            count = roll( 8, 15 );
        }
        else if ( r < 11 ) {
            level = 1;
            count = roll( 5, 7 );
        }
        else if ( r < 14 ) {
            level = 2;
            count = roll( 3, 5 );
        }
        else {
            level = 3;
            count = roll( 1, 3 );
        }
        const int creature = H1::dwellingCreatureOf( castle, 7 + level );
        if ( creature < 0 ) {
            return;
        }
        castle.GetArmy().JoinTroop( Monster( H1::creatureRow( creature ).monsterId ), static_cast<uint32_t>( count ), false );
    }
}

namespace H1::Week
{
    void reset()
    {
        weekKind = -1;
        weekValue = 0;
        monthKind = 0;
        monthValue = 0;
    }

    std::string savedState()
    {
        std::ostringstream out;
        out << "week " << monthKind << ' ' << monthValue << ' ' << weekKind << ' ' << weekValue;
        return out.str();
    }

    void loadSavedState( const std::string & line )
    {
        reset();
        std::istringstream in( line );
        std::string tag;
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        if ( in >> tag >> a >> b >> c >> d && tag == "week" ) {
            monthKind = a;
            monthValue = b;
            weekKind = c;
            weekValue = d;
        }
    }

    void newWeek( const bool newMonth )
    {
        // FUN_0041a1b9: a week name 0..14; unless the week-of-month counter is 4 (this week starts a month), 1 in 4 a creature week.
        weekKind = 0;
        weekValue = roll( 0, 0xe );
        if ( !newMonth && roll( 1, 4 ) == 1 ) {
            weekKind = 1;
            weekValue = roll( 0, 0x17 );
            if ( H1::isExtension() ) {
                // H1-INTENDED(D16): the 8 neutral creatures (28..35) can be the week's creature too, each as likely as a castle creature.
                weekValue = roll( 0, 0x17 + H1::extensionCreatureCount );
                if ( weekValue > 0x17 ) {
                    weekValue = H1::firstExtensionCreature + weekValue - 0x18;
                }
            }
        }

        // H1-INTENDED(D16): extension dwellings and markets on the map.
        H1::extensionObjectsNewWeek( weekKind == 1 ? weekValue : -1 );

        for ( Castle * castle : world.h1AllCastles() ) {
            const int bits = castleBuildingBits( *castle );
            const PlayerColor owner = castle->GetColor();
            int level = 0;
            if ( owner != PlayerColor::NONE ) {
                const Kingdom & kingdom = world.GetKingdom( owner );
                if ( kingdom.isControlAI() ) {
                    level = computerLevel( kingdom );
                }
            }
            for ( int number = 7; number < 13; ++number ) {
                if ( ( bits & ( 1 << number ) ) == 0 ) {
                    continue;
                }
                int growth = baseGrowth( *castle, number, bits );
                // A computer player's castle: level 3 x 1.24, level 4 x 1.36 (0x48d038 / 0x48d040, __ftol).
                if ( level == 3 ) {
                    growth = static_cast<int>( growth * 1.24 );
                }
                else if ( level == 4 ) {
                    growth = static_cast<int>( growth * 1.36 );
                }
                if ( weekKind == 1 && dwellingCreatureOf( *castle, number ) == weekValue ) {
                    growth += 5;
                }
                const uint32_t type = dwellingType( number );
                castle->h1SetDwellingCount( type, castle->getMonstersInDwelling( type ) + static_cast<uint32_t>( growth ) );
            }
        }

        // (FUN_0041a1b9 then refreshes the tavern heroes and the map objects: the objects are World::NewWeek -> newWeekObjectValues.)

        for ( Castle * castle : world.h1AllCastles() ) {
            if ( castle->GetColor() == PlayerColor::NONE ) {
                neutralCastleTroop( *castle );
            }
        }

        if ( !newMonth ) {
            return;
        }

        // FUN_0041aa04: 1..10: 1-5 a month name 0..9, 6-9 a creature month (0x490ce0), 10 plague.
        const int r = roll( 1, 10 );
        if ( r < 6 ) {
            monthKind = 0;
            monthValue = roll( 0, 9 );
        }
        else if ( r < 10 ) {
            monthKind = 1;
            monthValue = monthCreatures[roll( 0, 0xb )];
            if ( H1::isExtension() ) {
                // H1-INTENDED(D16): creature months also of the 8 neutral creatures (wandering monsters of them appear on the map by the
                // original month rule), each as likely as one of the 12 creatures of the original month table.
                const int k = roll( 0, 0xb + H1::extensionCreatureCount );
                monthValue = ( k < 12 ) ? monthCreatures[k] : H1::firstExtensionCreature + k - 12;
            }
        }
        else {
            monthKind = 2;
        }
        for ( Castle * castle : world.h1AllCastles() ) {
            const int bits = castleBuildingBits( *castle );
            for ( int number = 7; number < 13; ++number ) {
                if ( ( bits & ( 1 << number ) ) == 0 ) {
                    continue;
                }
                const uint32_t type = dwellingType( number );
                int count = static_cast<int>( castle->getMonstersInDwelling( type ) );
                if ( monthKind == 1 && dwellingCreatureOf( *castle, number ) == monthValue ) {
                    count *= 2;
                }
                if ( monthKind == 2 ) {
                    // The growth without the computer and creature-week bonuses comes off first, then half (short >> 1).
                    count -= baseGrowth( *castle, number, bits );
                    if ( count < 0 ) {
                        count = 0;
                    }
                    count >>= 1;
                }
                castle->h1SetDwellingCount( type, static_cast<uint32_t>( count ) );
            }
        }
        if ( monthKind == 1 ) {
            spawnMonthMonsters( monthValue );
        }
    }

    void showWeekMessage()
    {
        if ( weekKind == -1 ) {
            return;
        }
        char text[256];
        int track = 0x32;
        if ( world.BeginMonth() ) {
            track = 0x33;
            if ( monthKind == 0 ) {
                std::snprintf( text, sizeof( text ), "Astrologers proclaim month of the %s.\n\nAll dwellings increase population.",
                               monthNames[std::clamp( monthValue, 0, 9 )] );
            }
            else if ( monthKind == 1 ) {
                const char * name = H1::creatureNameOne( monthValue ); // H1-INTENDED(D16): 28..35 too
                std::string upper( name );
                upper[0] = static_cast<char>( upper[0] - 0x20 );
                std::snprintf( text, sizeof( text ), "Astrologers proclaim month of the %s.\n\n%s population doubles!\n\nAll dwellings increase population.",
                               name, upper.c_str() );
            }
            else {
                std::snprintf( text, sizeof( text ), "Astrologers proclaim month of the PLAGUE!\n\nAll populations are halved." );
            }
        }
        else if ( weekKind == 0 ) {
            std::snprintf( text, sizeof( text ), "Astrologers proclaim week of the %s.\n\nAll dwellings increase population.", weekNames[std::clamp( weekValue, 0, 14 )] );
        }
        else {
            const char * name = H1::creatureNameOne( weekValue ); // H1-INTENDED(D16): 28..35 too
            std::string upper( name );
            upper[0] = static_cast<char>( upper[0] - 0x20 );
            std::snprintf( text, sizeof( text ), "Astrologers proclaim week of the %s.\n\n%s growth +5.\n\nAll dwellings increase population.", name, upper.c_str() );
        }
        const int mapMusic = currentMusic();
        playMusic( track );
        UI::messageBox( text, UI::MESSAGE_OKAY, 0x61, -1 );
        playMusic( mapMusic );
    }
}
