// Heroes I main menu (homm1-remaster project). See h1_menu.h and docs/analysis note v51.
#include "h1_menu.h"

#include <cstdint>
#include <string>
#include <vector>

#include "cursor.h"
#include "game.h"
#include "game_exit.h"
#include "game_hotkeys.h"
#include "game_mode.h"
#include "h1_gfx.h"
#include "image.h"
#include "localevent.h"
#include "pal.h"
#include "screen.h"

namespace
{
    // HoMM1 .bmp (agg note): u16 0x21, u16 width, u16 height, then raw 8-bit palette indices.
    fheroes2::Image readBmp( const std::string & name )
    {
        const std::vector<uint8_t> & b = H1::getH1AggFile( name );
        fheroes2::Image image;
        if ( b.size() < 6 ) {
            return image;
        }
        const int32_t w = b[2] | ( b[3] << 8 );
        const int32_t h = b[4] | ( b[5] << 8 );
        if ( b.size() < 6 + static_cast<size_t>( w ) * h ) {
            return image;
        }
        image._disableTransformLayer();
        image.resize( w, h );
        std::copy( b.begin() + 6, b.begin() + 6 + static_cast<ptrdiff_t>( w ) * h, image.image() );
        return image;
    }

    // stpmain.bin, read with tools/format/h1bin.py (note v51): dialog 210 x 426 at screen (400, 35).
    struct MenuButton
    {
        int32_t x, y, w, h;
        uint32_t released, pressed;
        fheroes2::GameMode mode;
        Game::HotKeyEvent hotkey;
    };
}

namespace H1
{
    fheroes2::GameMode runMainMenu()
    {
        const int32_t dialogX = 400;
        const int32_t dialogY = 35;
        const std::vector<MenuButton> buttons = {
            { 33, 45, 127, 63, 2, 3, fheroes2::GameMode::NEW_GAME, Game::HotKeyEvent::MAIN_MENU_NEW_GAME },
            { 33, 111, 127, 63, 0, 1, fheroes2::GameMode::LOAD_GAME, Game::HotKeyEvent::MAIN_MENU_LOAD_GAME },
            { 33, 177, 127, 63, 4, 5, fheroes2::GameMode::HIGHSCORES_STANDARD, Game::HotKeyEvent::MAIN_MENU_HIGHSCORES },
            { 33, 243, 127, 63, 6, 7, fheroes2::GameMode::CREDITS, Game::HotKeyEvent::MAIN_MENU_CREDITS },
            { 33, 309, 127, 63, 8, 9, fheroes2::GameMode::QUIT_GAME, Game::HotKeyEvent::GLOBAL_APP_QUIT },
        };

        // Compose the 640 x 480 screen.
        fheroes2::Image screen = readBmp( "heroes.bmp" );
        if ( screen.empty() ) {
            return fheroes2::GameMode::QUIT_GAME;
        }
        // Items of type 0x40 (flags 0x40): 16-pixel strips right of and below the dialog background -> drawn here as a shadow
        // with the darkening colour table [how HoMM1 draws them: unconfirmed].
        fheroes2::ApplyPalette( screen, dialogX + 16, dialogY + 410, screen, dialogX + 16, dialogY + 410, 194, 16, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        fheroes2::ApplyPalette( screen, dialogX + 194, dialogY + 16, screen, dialogX + 194, dialogY + 16, 16, 394, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        const fheroes2::Image background = readBmp( "redback.bmp" );
        fheroes2::Copy( background, 0, 0, screen, dialogX, dialogY, 194, 410 );

        const std::vector<fheroes2::Sprite> & icn = getIcn( "btnmain.icn" );

        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t offsetX = ( display.width() - 640 ) / 2;
        const int32_t offsetY = ( display.height() - 480 ) / 2;

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();

        auto areaOf = [&]( const MenuButton & b ) { return fheroes2::Rect( offsetX + dialogX + b.x, offsetY + dialogY + b.y, b.w, b.h ); };
        auto draw = [&]( const std::vector<bool> & pressedState ) {
            fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
            fheroes2::Copy( screen, 0, 0, display, offsetX, offsetY, 640, 480 );
            for ( size_t i = 0; i < buttons.size(); ++i ) {
                const uint32_t frame = pressedState[i] ? buttons[i].pressed : buttons[i].released;
                if ( frame < icn.size() ) {
                    const fheroes2::Sprite & s = icn[frame];
                    fheroes2::Blit( s, display, offsetX + dialogX + buttons[i].x + s.x(), offsetY + dialogY + buttons[i].y + s.y() );
                }
            }
            display.render();
        };

        std::vector<bool> state( buttons.size(), false );
        draw( state );

        while ( le.HandleEvents() ) {
            std::vector<bool> newState( buttons.size(), false );
            for ( size_t i = 0; i < buttons.size(); ++i ) {
                newState[i] = le.isMouseLeftButtonPressedAndHeldInArea( areaOf( buttons[i] ) );
            }
            if ( newState != state ) {
                state = newState;
                draw( state );
            }
            for ( const MenuButton & b : buttons ) {
                if ( le.MouseClickLeft( areaOf( b ) ) || Game::HotKeyPressEvent( b.hotkey ) ) {
                    if ( b.mode == fheroes2::GameMode::QUIT_GAME && Game::processExitEvent() != fheroes2::GameMode::QUIT_GAME ) {
                        draw( state );
                        break;
                    }
                    return b.mode;
                }
            }
        }
        return fheroes2::GameMode::QUIT_GAME;
    }
}
