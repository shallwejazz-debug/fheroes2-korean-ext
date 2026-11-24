// H1-INTENDED(D17): cheat codes on the adventure map (owner 2026-11-18 16:42 "치트키나 트레이너 … 전체 맵 밝히기(2편의 8675309 같은),
// 자원 늘리기, 특정 유닛 넣기"). The original HoMM1 has no cheat codes; these are typed with the number keys (not the keypad) on the adventure
// map of a human player, like the HoMM2 codes:
//   8675309   the whole map is revealed (HoMM2's code)
//   32167     5 dragons join the selected hero (HoMM2's code gives black dragons; HoMM1 has one dragon)
//   1111      gold +10000, every other resource +20
//   999 + two digits   10 creatures of that HoMM1 creature number (00 peasant .. 27 ghost; 28 .. 39 the extension creatures, extension
//                      games only) join the selected hero
// The number keys do nothing else on the HoMM1 adventure map, so in HoMM1 mode they all go to the cheat input.
#include "h1_cheats.h"

#include <string>

#include "army.h"
#include "game_interface.h"
#include "h1_mode.h"
#include "h1_newgame.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "kingdom.h"
#include "maps.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "monster.h"
#include "players.h"
#include "resource.h"
#include "settings.h"
#include "world.h"

namespace
{
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

    bool endsWith( const std::string & text, const std::string & end )
    {
        return text.size() >= end.size() && text.compare( text.size() - end.size(), end.size(), end ) == 0;
    }

    Heroes * selectedHero()
    {
        return Interface::GetFocusHeroes();
    }

    void addCreatures( const int creature, const uint32_t count )
    {
        Heroes * hero = selectedHero();
        if ( hero == nullptr ) {
            H1::UI::messageBox( "Cheat: select a hero first.", H1::UI::MESSAGE_OKAY );
            return;
        }
        if ( !hero->GetArmy().JoinTroop( Monster( H1::creatureRow( creature ).monsterId ), count, false ) ) {
            H1::UI::messageBox( "Cheat: there is no room in the army.", H1::UI::MESSAGE_OKAY );
            return;
        }
        Interface::AdventureMap::Get().redraw( Interface::REDRAW_ICONS | Interface::REDRAW_STATUS );
        H1::UI::messageBox( "Cheat: " + std::to_string( count ) + " " + H1::creatureNameMany( creature ) + " join the army.", H1::UI::MESSAGE_OKAY );
    }
}

namespace H1
{
    bool cheatKey( const fheroes2::Key key )
    {
        const int digit = digitOf( key );
        if ( digit < 0 ) {
            return false;
        }
        static std::string typed;
        typed += static_cast<char>( '0' + digit );
        if ( typed.size() > 8 ) {
            typed.erase( 0, typed.size() - 8 );
        }

        const PlayerColor color = Settings::Get().CurrentColor();
        Kingdom & kingdom = world.GetKingdom( color );

        if ( endsWith( typed, "8675309" ) ) {
            typed.clear();
            const PlayerColorsSet colors = static_cast<PlayerColorsSet>( color );
            for ( int32_t i = 0; i < world.w() * world.h(); ++i ) {
                world.getTile( i ).ClearFog( colors );
            }
            // the fog edge pictures are kept per tile and only recomputed by this call (as Maps::ClearFog does)
            Maps::updateFogDirectionsInArea( { 0, 0 }, { world.w() - 1, world.h() - 1 }, Players::GetPlayerFriends( color ) );
            Interface::AdventureMap::Get().redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );
            H1::UI::messageBox( "Cheat: the whole map is revealed.", H1::UI::MESSAGE_OKAY );
        }
        else if ( endsWith( typed, "32167" ) ) {
            typed.clear();
            addCreatures( 23, 5 );
        }
        else if ( endsWith( typed, "1111" ) ) {
            typed.clear();
            kingdom.AddFundsResource( Funds( 20, 20, 20, 20, 20, 20, 10000 ) );
            Interface::AdventureMap::Get().redraw( Interface::REDRAW_STATUS );
            H1::UI::messageBox( "Cheat: resources added.", H1::UI::MESSAGE_OKAY );
        }
        else if ( typed.size() >= 5 && typed.compare( typed.size() - 5, 3, "999" ) == 0 ) {
            const int creature = ( typed[typed.size() - 2] - '0' ) * 10 + ( typed[typed.size() - 1] - '0' );
            typed.clear();
            const int last = H1::isExtension() ? 39 : 27;
            if ( creature > last ) {
                H1::UI::messageBox( "Cheat: there is no creature with that number.", H1::UI::MESSAGE_OKAY );
            }
            else {
                addCreatures( creature, 10 );
            }
        }
        return true;
    }
}
