/***************************************************************************
 *   H2-CHEATS: the original Heroes II cheat codes, see h2_cheats.h
 ***************************************************************************/

#include "h2_cheats.h"

#include "army.h"
#include "color.h"
#include "players.h"
#include "game_interface.h"
#include "game_over.h"
#include "heroes.h"
#include "maps.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "monster.h"
#include "settings.h"
#include "world.h"

namespace
{
    uint32_t typedCode = 0;
    uint32_t forcedResult = 0;

    int digitOf( const fheroes2::Key key )
    {
        switch ( key ) {
        case fheroes2::Key::KEY_0:
            return 0;
        case fheroes2::Key::KEY_1:
            return 1;
        case fheroes2::Key::KEY_2:
            return 2;
        case fheroes2::Key::KEY_3:
            return 3;
        case fheroes2::Key::KEY_4:
            return 4;
        case fheroes2::Key::KEY_5:
            return 5;
        case fheroes2::Key::KEY_6:
            return 6;
        case fheroes2::Key::KEY_7:
            return 7;
        case fheroes2::Key::KEY_8:
            return 8;
        case fheroes2::Key::KEY_9:
            return 9;
        default:
            return -1;
        }
    }
}

namespace H2Cheats
{
    bool key( const fheroes2::Key key )
    {
        const int digit = digitOf( key );
        if ( digit < 0 ) {
            return false;
        }
        // HEROES2W.EXE 0x491524: code = (code * 10 + digit) mod 10,000,000
        typedCode = ( typedCode * 10 + static_cast<uint32_t>( digit ) ) % 10000000;

        if ( typedCode % 100000 == 32167 ) {
            Heroes * hero = Interface::GetFocusHeroes();
            if ( hero != nullptr ) {
                // 0x4915a8: 5 Black Dragons join the selected hero (nothing happens without a selected hero)
                hero->GetArmy().JoinTroop( Monster( Monster::BLACK_DRAGON ), 5, false );
                Interface::AdventureMap::Get().redraw( Interface::REDRAW_ICONS | Interface::REDRAW_STATUS );
                return true;
            }
        }
        if ( typedCode % 1000 == 911 ) {
            forcedResult = GameOver::WINS_ALL;
            return true;
        }
        if ( typedCode % 10000 == 1313 ) {
            forcedResult = GameOver::LOSS_ALL;
            return true;
        }
        if ( typedCode == 8675309 ) {
            // 0x491674: the whole map for all six player colours
            for ( int32_t i = 0; i < world.w() * world.h(); ++i ) {
                world.getTile( i ).ClearFog( static_cast<PlayerColorsSet>( 0x3F ) ); // the six player colours
            }
            Maps::updateFogDirectionsInArea( { 0, 0 }, { world.w() - 1, world.h() - 1 },
                                             Players::GetPlayerFriends( Settings::Get().CurrentColor() ) );
            Interface::AdventureMap::Get().redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );
            return true;
        }
        return false;
    }

    uint32_t takeForcedResult()
    {
        const uint32_t result = forcedResult;
        forcedResult = 0;
        return result;
    }
}
