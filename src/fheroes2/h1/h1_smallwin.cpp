// Heroes I small windows (homm1-remaster project), note v123.
#include "h1_smallwin.h"

#include <array>
#include <string>
#include <vector>

#include "cursor.h"
#include "game.h"
#include "game_delays.h"
#include "game_interface.h"
#include "h1_gfx.h"
#include "h1_map_render.h"
#include "h1_newgame.h"
#include "h1_ui.h"
#include "heroes.h"
#include "interface_gamearea.h"
#include "localevent.h"
#include "maps_fileinfo.h"
#include "maps_tiles.h"
#include "screen.h"
#include "settings.h"
#include "world.h"

namespace H1
{
    bool showScenarioInfo()
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        UI::Layout layout = UI::loadLayout( "sceninfo.bin" );
        if ( layout.width == 0 ) {
            return false;
        }
        const Settings & conf = Settings::Get();
        const Maps::FileInfo & map = conf.getCurrentMapInfo();
        const NewGameChoice & choice = lastNewGameChoice();
        auto set = [&layout]( const int id, const std::string & text ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->text = text;
            }
        };
        // Tables 0x493da8 (game difficulty), 0x493878 (computer level), 0x493df8 (human level), 0x493d08 (size), 0x493d18 (map difficulty).
        static const char * const gameDifficulty[6] = { "Easy", "Normal", "Hard", "Expert", "Lord Ironfist", "Lord Slayer" };
        static const char * const computerLevel[5] = { "None", "Dumb", "Average", "Smart", "Genius" };
        static const char * const humanLevel[5] = { "Human-", "Human-Easy", "Human-Normal", "Human-Hard", "Human-Expert" };
        static const char * const sizeName[3] = { "Small", "Medium", "Large" };
        static const char * const mapDifficulty[4] = { "Easy", "Normal", "Tough", "Impossible" };

        set( 0x68, map.name ); // +0x15
        // [Hot seat: the seat of the player to move (DAT_004ac968 > 0) shows that seat's own level; only seat 0 is followed here.]
        set( 0x69, gameDifficulty[std::clamp( choice.difficulty, 0, 5 )] );
        std::string opponents;
        for ( int seat = 1; seat < 4; ++seat ) {
            const int level = std::clamp( choice.levels[seat], 0, 4 );
            opponents += std::string( seat < choice.humans ? humanLevel[level] : computerLevel[level] ) + "\n";
        }
        set( 0x6a, opponents );
        set( 0x6b, choice.kingOfTheHill != 0 ? "Yes" : "No" );
        set( 0x6c, std::to_string( choice.rating ) + "%" );
        int difficulty = 0;
        int size = 0;
        std::string description;
        if ( readOriginalMapHeader( map.filename, difficulty, size, &description ) ) {
            set( 100, sizeName[std::clamp( size, 0, 2 )] );
            set( 0x65, mapDifficulty[std::clamp( difficulty, 0, 3 )] );
            set( 0x66, description );
        }
        else {
            set( 100, "" );
            set( 0x65, "" );
            set( 0x66, map.description );
        }
        // Item 0x67: newgame.icn frame colour x 2 + 0xb of the player to move.
        const int colour = Color::GetIndex( conf.CurrentColor() );
        if ( UI::Item * it = layout.find( 0x67 ) ) {
            if ( colour >= 0 && colour < 4 ) {
                it->frame = colour * 2 + 0xb;
            }
        }
        UI::runLayoutDialog( layout, 0x9f, 0xe, false );
        return true;
    }

    int32_t dimensionDoorDestination( const bool onBoat )
    {
        UI::Layout layout = UI::loadLayout( "dimdoor.bin" );
        Interface::AdventureMap & adventureMap = Interface::AdventureMap::Get();
        Interface::GameArea & area = adventureMap.getGameArea();
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        const UI::Item * cancel = layout.find( 0x8000 );
        const std::vector<std::array<uint8_t, 10>> tiles = gameTileBytes();

        // FUN_0046138c: a tile in the view suits when its object byte 8 and byte 7 have no 0x80 bit; the cursor shows it (4 / 0).
        // [Heroes standing on a tile are not in the tile bytes here: the engine's hero check stands in.]
        auto suits = [&tiles]( const int32_t index ) {
            if ( index < 0 || static_cast<size_t>( index ) >= tiles.size() ) {
                return false;
            }
            const std::array<uint8_t, 10> & t = tiles[static_cast<size_t>( index )];
            return ( t[8] & 0x80 ) == 0 && ( t[7] & 0x80 ) == 0 && world.getTile( index ).getHero() == nullptr;
        };
        int pressed = -1;
        auto draw = [&]() {
            adventureMap.redraw( Interface::REDRAW_GAMEAREA );
            UI::drawLayout( display, layout, ox, oy, pressed );
            display.render();
        };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        Cursor & cursor = Cursor::Get();
        LocalEvent & le = LocalEvent::Get();
        draw();
        int32_t result = -1;
        while ( le.HandleEvents( Game::isDelayNeeded( { Game::DelayType::MAPS_DELAY } ) ) ) {
            const fheroes2::Point m = le.getMouseCursorPos();
            const bool overCancel = cancel != nullptr && m.x >= ox + cancel->x && m.y >= oy + cancel->y && m.x < ox + cancel->x + cancel->w
                                    && m.y < oy + cancel->y + cancel->h;
            const int newPressed = ( overCancel && le.isMouseLeftButtonPressed() ) ? 0x8000 : -1;
            bool redraw = false;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                redraw = true;
            }
            if ( ( overCancel && le.MouseClickLeft() ) || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                break;
            }
            int32_t tile = -1;
            if ( area.GetROI() & m ) {
                tile = area.GetValidTileIdFromPoint( m );
            }
            const bool valid = suits( tile );
            cursor.SetThemes( valid ? Cursor::CURSOR_HERO_MOVE : Cursor::POINTER );
            if ( valid && le.MouseClickLeft() ) {
                result = tile;
                break;
            }
            if ( Game::validateAnimationDelay( Game::DelayType::MAPS_DELAY ) ) {
                Game::updateAdventureMapAnimationIndex();
                redraw = true;
            }
            if ( redraw ) {
                draw();
            }
        }
        adventureMap.redraw( Interface::REDRAW_ALL );
        display.render();
        if ( result < 0 ) {
            return -1;
        }
        // FUN_00462adf: on a boat the target must be water (ground tile < 0x14), on land it must be land.
        const bool water = tiles[static_cast<size_t>( result )][0] < 0x14;
        if ( water != onBoat ) {
            UI::messageBox( "Dimension Door failed!!!", UI::MESSAGE_OKAY, 0x61, 0x91 );
            return -2;
        }
        return result;
    }
}
