// Heroes I "New Game" flow (homm1-remaster project). See h1_newgame.h and docs/analysis note v53.
// Function names are from the decompiled Windows HEROESW.EXE (baseline for rules); the .bin layouts and images are from the DOS HEROES.AGG.
#include "h1_newgame.h"
#include "h1_rmg.h"
#include <chrono>
#include "h1_ext_objects.h"
#include "h1_visit.h"
#include "h1_week.h"
#include "h1_ai.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "cursor.h"
#include "dir.h"
#include "dialog.h"
#include "game.h"
#include "game_mode.h"
#include "h1_gfx.h"
#include "h1_mode.h"
#include "h1_sound.h"
#include "h1_ui.h"
#include "h1_endgame.h"
#include "h1_rules.h"
#include "h1_map_render.h"
#include "game_video.h"
#include "game_io.h"
#include "artifact.h"
#include "heroes.h"
#include "castle.h"
#include "resource.h"
#include "kingdom.h"
#include "image.h"
#include "localevent.h"
#include "maps_fileinfo.h"
#include "players.h"
#include "rand.h"
#include "screen.h"
#include "settings.h"
#include "system.h"
#include "ui_dialog.h"
#include "world.h"

namespace
{
    // Item ids of newgame.bin (tools/format/h1bin.py) as used by FUN_0041384b / FUN_004132ca.
    // The DOS .bin gives OK / Cancel the ids 0x8002 / 0x8001; the Windows code compares 0x7802 / 0x7801 (Windows .bin), same buttons.
    enum : int
    {
        ID_OPPONENT_1 = 2, // .. 4: opponent portraits
        ID_OPPONENT_TEXT_1 = 5, // .. 7
        ID_COLOUR = 8,
        ID_SCENARIO_BUTTON = 0xc,
        ID_DIFFICULTY_FRAME_1 = 0xd, // .. 0x10: selection frame over the 4 difficulty pictures
        ID_SCENARIO_TEXT = 0x11,
        ID_SCENARIO_BOX = 0x12,
        ID_KING_OF_THE_HILL = 0x13,
        ID_RATING_TEXT = 0x14,
        ID_CANCEL = 0x8001,
        ID_OKAY = 0x8002
    };

    // String tables of HEROESW.EXE.
    const char * const humanLevelText[5] = { "Human\n", "Human\nEasy", "Human\nNormal", "Human\nHard", "Human\nExpert" }; // 0x493de0
    const char * const computerLevelText[5] = { "None", "Dumb", "Average", "Smart", "Genius" }; // 0x493878
    const char * const helpText[9] = {
        // 0x493b58, chosen by FUN_004132ca for a right click
        "Accept these settings and start a new game.",
        "Return to the main menu.",
        "Challenge all computer players as 'King of the Hill'.  Computer players will be offended by your boastfulness, and lay off each other in an "
        "attempt to beat you to a pulp.",
        "Select which scenario to play.",
        "Change the starting difficulty at which you will play.  Higher difficulty levels start you off with fewer resources.",
        "Change the difficulty of this opponent.  Smarter computer players are more aggressive and think longer for each turn.",
        "Change your banner color.",
        "The difficulty rating reflects a combination of various settings for your game.  This number will be applied to your final score.",
        "Change the starting difficulty of another human player.  Higher difficulty levels start you off with fewer resources." };
    const char * const helpTextGameType[4] = {
        // 0x493c50, chosen by FUN_0040d910 for a right click on stpnewgm.bin: ids 1, 2, 3, Cancel
        "A single player game playing out a single map.", "A single player game playing through a series of maps.",
        "A multi-player game, with several human players competing against each other on a single map.", "Cancel back to the main menu." };

    const char * const helpTextMultiPlayer[5] = {
        // 0x493c28, FUN_0040d71d: stpmp.bin ids 1..4, Cancel
        "Play a Hot Seat game, where 2 to 4 players play around the same computer, switching into the 'Hot Seat' when it is their turn.",
        "Play a network game, where 2 players use their own computers connected through a LAN (Local Area Network).",
        "Play a modem game, where 2 players use ther own computers connected over the phone lines using modems.",
        "Play a direct connect game, where 2 players use ther own computers directly connected through their serial port by a null modem.",
        "Cancel back to the main menu." };
    const char * const helpTextHotSeat[4] = {
        // 0x493bf8, FUN_0040d4e2: stphotst.bin ids 1..3, Cancel
        "Play with 2 human players, and optionally, up to 2 additional computer players.", "Play with 3 human players, and optionally 1 computer player.",
        "Play with 4 human players.", "Cancel back to the main menu." };

    // Remembered between visits of the screen while the program runs (DAT_004909e0 and the DAT_004a72xx copies in FUN_00413dd1).
    bool remembered = false;
    H1::NewGameChoice choice;
    std::string currentMapFile; // DAT_004a82c0 (engine: path of the converted .fh2m)
    std::string currentMapName; // DAT_004a7bd0
    int currentMapSize = 0; // DAT_00493f38
    int currentMapDifficulty = 0; // DAT_00493f3c

    // DOS scan codes used by the .bin files so far -> engine keys.
    fheroes2::Key keyFromScanCode( const int code )
    {
        switch ( code ) {
        case 1:
            return fheroes2::Key::KEY_ESCAPE;
        case 28:
            return fheroes2::Key::KEY_ENTER;
        case 31:
            return fheroes2::Key::KEY_S;
        case 46:
            return fheroes2::Key::KEY_C;
        case 50:
            return fheroes2::Key::KEY_M;
        default:
            return fheroes2::Key::NONE;
        }
    }

    std::string baseName( const std::string & path )
    {
        const size_t slash = path.find_last_of( "/\\" );
        return slash == std::string::npos ? path : path.substr( slash + 1 );
    }

    // FUN_0041eeab: the scenario list for n human players uses the file pattern "????1???.MAP", "?????2??.MAP", "??????3?.MAP", "???????4.MAP".
    bool matchesPlayerPattern( const std::string & file, const int humans )
    {
        const std::string name = baseName( file );
        const size_t index = static_cast<size_t>( 3 + humans );
        return name.size() > index && name[index] == static_cast<char>( '0' + humans );
    }

    // FUN_0041f2df.
    int computeRating( H1::NewGameChoice & c, const int humans )
    {
        static const int difficultyRating[4] = { 0, 10, 20, 30 };
        int rating = ( c.difficulty >= 0 && c.difficulty < 4 ) ? difficultyRating[c.difficulty] : 0;
        for ( int i = 1; i < 4; ++i ) {
            if ( i < humans ) {
                rating += ( c.levels[i] * 5 - 5 ) * 2;
            }
            else {
                static const int computerRating[5] = { -10, 5, 10, 15, 20 };
                if ( c.levels[i] >= 0 && c.levels[i] < 5 ) {
                    rating += computerRating[c.levels[i]];
                }
            }
        }
        c.playerCount = 0;
        for ( int i = 0; i < 4; ++i ) {
            if ( c.levels[i] > 0 ) {
                ++c.playerCount;
            }
        }
        if ( c.kingOfTheHill != 0 ) {
            if ( c.playerCount - humans == 2 ) {
                rating += 5;
            }
            else if ( c.playerCount - humans == 3 ) {
                rating += 10;
            }
        }
        if ( currentMapSize == 1 ) {
            rating += 10;
        }
        else if ( currentMapSize == 2 ) {
            rating += 20;
        }
        static const int mapDifficultyRating[4] = { 20, 30, 40, 50 };
        if ( currentMapDifficulty >= 0 && currentMapDifficulty < 4 ) {
            rating += mapDifficultyRating[currentMapDifficulty];
        }
        return rating;
    }

    // FUN_0041384b: update texts, frames and visibility from the settings.
    void refresh( H1::UI::Layout & layout, const int humans )
    {
        if ( H1::UI::Item * it = layout.find( ID_SCENARIO_TEXT ) ) {
            std::string name = currentMapName;
            const size_t dot = name.find( '.' );
            if ( dot != std::string::npos ) {
                name.resize( dot );
            }
            it->text = name;
        }
        for ( int i = 0; i < 4; ++i ) {
            if ( H1::UI::Item * it = layout.find( ID_DIFFICULTY_FRAME_1 + i ) ) {
                it->hidden = ( i != choice.difficulty );
            }
        }
        for ( int i = 1; i < 4; ++i ) {
            if ( H1::UI::Item * it = layout.find( ID_OPPONENT_1 + i - 1 ) ) {
                it->frame = ( i < humans ) ? 0x1a : choice.levels[i] + 5;
            }
            if ( H1::UI::Item * it = layout.find( ID_OPPONENT_TEXT_1 + i - 1 ) ) {
                const int level = std::clamp( choice.levels[i], 0, 4 );
                it->text = ( i < humans ) ? humanLevelText[level] : computerLevelText[level];
            }
        }
        choice.rating = computeRating( choice, humans );
        if ( H1::UI::Item * it = layout.find( ID_RATING_TEXT ) ) {
            char buffer[64];
            std::snprintf( buffer, sizeof( buffer ), "%s %d%%", "Difficulty Rating:", choice.rating );
            it->text = buffer;
        }
        if ( choice.colour != -1 ) {
            if ( H1::UI::Item * it = layout.find( ID_COLOUR ) ) {
                it->frame = choice.colour * 2 + 0xb;
            }
        }
        if ( H1::UI::Item * it = layout.find( ID_KING_OF_THE_HILL ) ) {
            it->frame = choice.kingOfTheHill + 0x1b;
        }
    }

    // Topmost item with an id handled by the dialog that contains the point. Hidden items still take clicks: the difficulty pictures have no id,
    // only the hidden selection frames 0xd..0x10 above them do.
    int itemAt( const H1::UI::Layout & layout, const int32_t px, const int32_t py, const std::vector<int> & ids )
    {
        for ( auto it = layout.items.rbegin(); it != layout.items.rend(); ++it ) {
            if ( std::find( ids.begin(), ids.end(), it->id ) == ids.end() ) {
                continue;
            }
            if ( px >= it->x && py >= it->y && px < it->x + it->w && py < it->y + it->h ) {
                return it->id;
            }
        }
        return -1;
    }

    struct Screen
    {
        fheroes2::Image background; // heroes.bmp, 640 x 480
        int32_t offsetX = 0;
        int32_t offsetY = 0;
    };

    Screen makeScreen()
    {
        Screen s;
        s.background = H1::UI::readBmp( "heroes.bmp" );
        const fheroes2::Display & display = fheroes2::Display::instance();
        s.offsetX = ( display.width() - 640 ) / 2;
        s.offsetY = ( display.height() - 480 ) / 2;
        return s;
    }

    void drawDialog( const Screen & s, const H1::UI::Layout & layout, const int32_t x, const int32_t y, const int pressedId )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        fheroes2::Image screen = s.background;
        H1::UI::drawLayout( screen, layout, x, y, pressedId );
        fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
        fheroes2::Copy( screen, 0, 0, display, s.offsetX, s.offsetY, screen.width(), screen.height() );
        display.render();
    }

    void showHelp( const char * text )
    {
        if ( text != nullptr ) {
            // FUN_00422fc2 style 4: shown while the mouse button is held.
            H1::UI::messageBox( text, H1::UI::MESSAGE_POPUP );
        }
    }

    // Generic loop for a dialog of buttons only (stpnewgm.bin). Returns the clicked button id.
    int runButtonDialog( const Screen & s, const H1::UI::Layout & layout, const int32_t x, const int32_t y, const std::vector<const char *> & help,
                         const std::vector<int> & helpIds )
    {
        LocalEvent & le = LocalEvent::Get();
        std::vector<int> ids;
        for ( const H1::UI::Item & it : layout.items ) {
            if ( it.type == H1::UI::ITEM_BUTTON ) {
                ids.push_back( it.id );
            }
        }
        int pressed = -1;
        drawDialog( s, layout, x, y, pressed );
        while ( le.HandleEvents() ) {
            const fheroes2::Point mouse = le.getMouseCursorPos();
            const int32_t mx = mouse.x - s.offsetX - x;
            const int32_t my = mouse.y - s.offsetY - y;
            const int over = itemAt( layout, mx, my, ids );
            const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                drawDialog( s, layout, x, y, pressed );
            }
            if ( over != -1 && le.MouseClickLeft() ) {
                return over;
            }
            if ( over != -1 && le.isMouseRightButtonPressed() ) {
                for ( size_t i = 0; i < helpIds.size(); ++i ) {
                    if ( helpIds[i] == over && i < help.size() ) {
                        showHelp( help[i] );
                        drawDialog( s, layout, x, y, -1 );
                    }
                }
            }
            for ( const H1::UI::Item & it : layout.items ) {
                if ( it.type == H1::UI::ITEM_BUTTON && keyFromScanCode( it.hotkey ) != fheroes2::Key::NONE && le.isKeyPressed( keyFromScanCode( it.hotkey ) ) ) {
                    return it.id;
                }
            }
        }
        return ID_CANCEL;
    }

    // ---- File requester (request.bin) in scenario mode, with reqextra.bin under it: FUN_0041eeab, FUN_00467e1c, FUN_0046878c,
    //      FUN_004699ea (list), FUN_00468c47 (clicks), FUN_0046976a (slider), FUN_0046a097 (map details). ----
    const char * const mapSizeText[3] = { "Small", "Medium", "Large" }; // 0x493d08
    const char * const mapDifficultyText[4] = { "Easy", "Normal", "Tough", "Impossible" }; // 0x493d18

    struct RequesterEntry
    {
        const Maps::FileInfo * info = nullptr;
        std::string file; // base name, the list is sorted by it (FUN_00467e1c, __strcmpi)
        int difficulty = 0; // header byte 2
        int size = 0; // header byte 3
        std::string description; // header 0x9a
    };

    std::string upper( std::string s )
    {
        for ( char & c : s ) {
            if ( c >= 'a' && c <= 'z' ) {
                c = static_cast<char>( c - 'a' + 'A' );
            }
        }
        return s;
    }

    std::string currentMapDescription; // DAT_004a7348

    // World +0x9f (note v163): "NEWGAME" for a new game, the file name after loading, the saved name after saving (not AUTOSAVE / PLYREXIT).
    std::string gameFileName = "NEWGAME";

    std::string upperBase( const std::string & path )
    {
        std::string name = upper( baseName( path ) );
        const size_t dot = name.find( '.' );
        if ( dot != std::string::npos ) {
            name.resize( dot );
        }
        return name;
    }

    // ---- The same requester for saved games (note v160): FUN_00467e1c lists ".\GAMES\*.GM*" (campaign: "*.CGM") sorted by name without
    //      case, FUN_0046878c mode 0 = load / 1 = save, FUN_004699ea list, FUN_00468c47 clicks, FUN_00469f58 result name. No reqextra.bin. ----
    struct SaveEntry
    {
        std::string name; // without extension (list text)
        std::string path; // the engine save file
    };

    std::string runSaveRequester( const bool save, const std::vector<SaveEntry> & entries, const std::string & extension, const std::string & folder,
                                  const std::string & currentName )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t offsetX = ( display.width() - 640 ) / 2;
        const int32_t offsetY = ( display.height() - 480 ) / 2;
        fheroes2::Image background( 640, 480 );
        fheroes2::Copy( display, offsetX, offsetY, background, 0, 0, 640, 480 );

        const int32_t reqX = save ? 0xa0 : 0x136;
        const int32_t reqY = save ? 0x28 : 0xe;
        H1::UI::Layout req = H1::UI::loadLayout( "request.bin" );
        const int32_t sliderX = 0x11b;
        int32_t sliderY = 0x38;
        const std::vector<fheroes2::Sprite> & scrollIcn = H1::getIcn( "scroll.icn" );

        const int n = static_cast<int>( entries.size() );
        int top = 0;
        int sel = -1;
        std::string typed = currentName; // +0x4f: the name in the edit field (save mode)
        // Save mode: the row with the current game's name is selected (FUN_0046878c, __strcmpi with the player count digit).
        if ( save ) {
            for ( int i = 0; i < n; ++i ) {
                if ( upper( entries[i].name ) == upper( typed ) ) {
                    sel = i;
                }
            }
        }
        if ( H1::UI::Item * it = req.find( 16 ) ) {
            it->text = save ? "File to Save:" : "File to Load:";
        }
        auto setOkay = [&req]( const bool on ) {
            if ( H1::UI::Item * it = req.find( ID_OKAY ) ) {
                it->greyed = !on;
                it->inactive = !on;
            }
        };
        // FUN_00468bbd( local_14 ): save mode always 1; load mode 1 only with a selection.
        setOkay( save || sel != -1 );
        bool editing = false;
        H1::UI::TextEdit edit; // the field while typing (note v172)

        auto updateList = [&]() {
            for ( int r = 0; r < 10; ++r ) {
                H1::UI::Item * it = req.find( r + 4 );
                if ( it == nullptr ) {
                    continue;
                }
                const int index = top + r;
                if ( index < n ) {
                    it->hidden = false;
                    std::string name = entries[index].name;
                    while ( !name.empty() && H1::UI::textWidth( name, "bigfont.fnt" ) > 0xcf ) {
                        name.pop_back();
                    }
                    it->text = name;
                }
                else {
                    it->hidden = true;
                }
                it->colour = ( index == sel ) ? 0xe8 : 1;
            }
            // Item 0xf: the edit field, shown with the typed name in save mode, hidden in load mode.
            if ( H1::UI::Item * it = req.find( 15 ) ) {
                it->hidden = !save;
                it->text = editing ? edit.shown() : typed;
            }
            sliderY = ( n < 11 ) ? 0x86 : static_cast<int32_t>( top * ( 156.0 / ( n - 10 ) ) + 56.0 );
        };

        auto draw = [&]( const int pressedId ) {
            fheroes2::Image screen = background;
            H1::UI::drawLayout( screen, req, reqX, reqY, pressedId );
            if ( scrollIcn.size() > 4 ) {
                const fheroes2::Sprite & sp = scrollIcn[4];
                fheroes2::Blit( sp, screen, reqX + sliderX + sp.x(), reqY + sliderY + sp.y() );
            }
            fheroes2::Copy( screen, 0, 0, display, offsetX, offsetY, 640, 480 );
            display.render();
        };

        // FUN_00468c47 case 0xf: letters, digits, '_', ' ' and the characters of 0x4a2720 are kept; the first other one ends the name;
        // trailing spaces go. A name that is left and starts above ' ' unselects the list.
        auto finishEdit = [&]() {
            editing = false;
            static const std::string extra = "$%'-_@~`!(){}^#&+,;=[].";
            std::string name;
            for ( const char c : typed ) {
                const bool ok = ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == ' '
                                || extra.find( c ) != std::string::npos;
                if ( !ok ) {
                    break;
                }
                name.push_back( c );
            }
            while ( !name.empty() && name.back() == ' ' ) {
                name.pop_back();
            }
            if ( !name.empty() && name[0] > ' ' ) {
                sel = -1;
                setOkay( true );
            }
            typed = name;
        };

        // The field's max length 9 (request.bin) lets TextEdit keep 8 characters, the same as memset( 9 ) of the copy in FUN_00468c47.

        // FUN_00469f58: no selection = the typed name with the save extension; else the row's file.
        auto result = [&]() -> std::string {
            if ( sel == -1 ) {
                return System::concatPath( folder, typed + extension );
            }
            return entries[sel].path;
        };

        updateList();
        LocalEvent & le = LocalEvent::Get();
        const std::vector<int> ids = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, ID_OKAY, ID_CANCEL };
        int pressed = -1;
        draw( pressed );

        while ( le.HandleEvents() ) {
            if ( editing ) {
                // The edit loop (0x47e1be..0x47e600) takes only key messages; mouse clicks do nothing until Enter or Esc (note v177).
                bool done = false;
                if ( le.isAnyKeyPressed() ) {
                    const H1::UI::Item * field = req.find( 15 );
                    if ( field != nullptr ) {
                        done = edit.key( static_cast<int>( le.getPressedKeyValue() ), LocalEvent::getCurrentKeyModifiers(), *field ) != 0;
                    }
                    typed = edit.text;
                }
                if ( done ) {
                    typed = edit.text;
                    finishEdit();
                }
                updateList();
                draw( -1 );
                continue;
            }
            const fheroes2::Point mouse = le.getMouseCursorPos();
            const int32_t mx = mouse.x - offsetX - reqX;
            const int32_t my = mouse.y - offsetY - reqY;
            const bool onSlider = mx >= sliderX && mx < sliderX + 8 && my >= sliderY && my < sliderY + 0x11;
            int over = itemAt( req, mx, my, ids );
            if ( H1::UI::Item * it = req.find( over ) ) {
                if ( ( it->type == H1::UI::ITEM_BUTTON && it->inactive ) || it->hidden ) {
                    over = -1;
                }
            }
            if ( onSlider && le.isMouseLeftButtonPressed() ) {
                const int32_t grab = my - sliderY;
                while ( le.HandleEvents() && le.isMouseLeftButtonPressed() ) {
                    const int32_t y = le.getMouseCursorPos().y - offsetY - reqY;
                    const int32_t newY = std::clamp( y - grab, 0x38, 0xd4 );
                    if ( n >= 11 ) {
                        const int newTop = std::clamp( static_cast<int>( ( newY - 56 ) / ( 156.0 / ( n - 9 ) ) ), 0, n - 10 );
                        if ( newTop != top ) {
                            top = newTop;
                            updateList();
                        }
                    }
                    sliderY = newY;
                    draw( -1 );
                }
                updateList();
                draw( -1 );
                continue;
            }
            const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                draw( pressed );
            }
            int clicked = ( over != -1 && le.MouseClickLeft() ) ? over : -1;
            bool keyMoved = false;
            if ( le.isKeyPressed( fheroes2::Key::KEY_ENTER ) ) {
                clicked = ID_OKAY;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                clicked = ID_CANCEL;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_UP ) && sel > 0 ) {
                --sel;
                if ( sel < top ) {
                    --top;
                }
                keyMoved = true;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_DOWN ) && sel < n - 1 ) {
                ++sel;
                if ( top + 10 <= sel ) {
                    ++top;
                }
                keyMoved = true;
            }

            if ( clicked == 1 && top > 0 ) {
                --top;
            }
            else if ( clicked == 2 && top + 10 < n ) {
                ++top;
                if ( n <= top + 9 ) {
                    top = n - 10;
                }
            }
            else if ( clicked == 3 ) {
                const int count = std::max( n - 9, 1 );
                const int y = ( mouse.y - offsetY ) - ( reqY + 0x38 ) - 9;
                top = ( y * 100 ) / ( 0x3d54 / count );
                if ( n <= top + 9 ) {
                    top = n - 10;
                }
                top = std::max( top, 0 );
            }
            else if ( clicked >= 4 && clicked <= 13 ) {
                const int index = clicked - 4 + top;
                if ( index == sel ) {
                    return result(); // a second click on the selected row is OK
                }
                sel = index;
                if ( sel < n ) {
                    setOkay( true );
                }
                else {
                    sel = -1;
                    setOkay( save );
                }
            }
            else if ( clicked == 15 && save ) {
                // FUN_00477af0 type 7: typing into the field (a type 0x100 field starts empty, note v172).
                edit.text = typed;
                edit.start();
                typed = edit.text;
                editing = true;
            }
            else if ( clicked == ID_CANCEL ) {
                return {};
            }
            else if ( clicked == ID_OKAY ) {
                if ( sel == -1 && typed.empty() ) {
                    H1::UI::messageBox( "Please make a selection from the list, or press cancel.", H1::UI::MESSAGE_OKAY );
                }
                else {
                    return result();
                }
            }
            if ( clicked != -1 || keyMoved ) {
                updateList();
                draw( pressed );
            }
        }
        return {};
    }

    // Returns the chosen entry index, or -1 for Cancel. background = the screen under the requester (heroes.bmp + newgame dialog).
    int runScenarioRequester( const Screen & s, const fheroes2::Image & background, const std::vector<RequesterEntry> & entries )
    {
        const int32_t reqX = 0x136;
        const int32_t reqY = 0xe;
        const int32_t extraX = 0x136;
        const int32_t extraY = 0x14c;
        H1::UI::Layout req = H1::UI::loadLayout( "request.bin" );
        H1::UI::Layout extra = H1::UI::loadLayout( "reqextra.bin" );
        // Slider: FUN_0047bb00( 0x11b, 0x38, 8, 0x11, "scroll.icn", frame 4, ..., id 0xe ) added to the requester (FUN_0046878c).
        const int32_t sliderX = 0x11b;
        int32_t sliderY = 0x38;
        const std::vector<fheroes2::Sprite> & scrollIcn = H1::getIcn( "scroll.icn" );

        const int n = static_cast<int>( entries.size() );
        int top = 0; // +0x1b0
        int sel = -1; // +0x1b2
        // Mode 0 with a ".MAP" pattern: select the entry whose first 8 letters match the current map file (FUN_0046878c).
        const std::string current = upper( baseName( currentMapFile ) ).substr( 0, 8 );
        for ( int i = 0; i < n; ++i ) {
            if ( upper( entries[i].file ).compare( 0, 8, current ) == 0 ) {
                sel = i;
            }
        }
        if ( H1::UI::Item * it = req.find( 16 ) ) {
            it->text = "File to Load:";
        }

        auto setOkay = [&req]( const bool selected ) {
            // FUN_00468bbd: with a selection OK gets state 2 and loses 8, without one it gets 8 (greyed) and loses 2.
            if ( H1::UI::Item * it = req.find( ID_OKAY ) ) {
                it->greyed = !selected;
                it->inactive = !selected;
            }
        };
        setOkay( sel != -1 );

        // FUN_0046a097 (called through FUN_004696cd after every message): map details under the list. It also changes the current map name,
        // description, size and difficulty right away; Cancel (FUN_0041eeab) puts back only the file, name and description.
        auto updateDetails = [&]() {
            if ( sel < 0 ) {
                return;
            }
            const RequesterEntry & e = entries[sel];
            currentMapSize = e.size;
            currentMapDifficulty = e.difficulty;
            currentMapName = e.info->name;
            currentMapDescription = e.description;
            if ( H1::UI::Item * it = extra.find( 100 ) ) {
                it->text = ( e.size >= 0 && e.size < 3 ) ? mapSizeText[e.size] : "";
            }
            if ( H1::UI::Item * it = extra.find( 101 ) ) {
                it->text = ( e.difficulty >= 0 && e.difficulty < 4 ) ? mapDifficultyText[e.difficulty] : "";
            }
            if ( H1::UI::Item * it = extra.find( 102 ) ) {
                it->text = e.description;
            }
        };

        // FUN_004699ea: 10 rows (ids 4..13), the selected row in colour 0xe8, others colour 1. The edit field (id 15) stays hidden in load mode.
        auto updateList = [&]() {
            for ( int r = 0; r < 10; ++r ) {
                H1::UI::Item * it = req.find( r + 4 );
                if ( it == nullptr ) {
                    continue;
                }
                const int index = top + r;
                if ( index < n ) {
                    it->hidden = false;
                    // Windows cuts the name until it fits in 0xcf pixels (FUN_0047b000 loop); the DOS version does not cut.
                    std::string name = entries[index].info->name;
                    while ( !name.empty() && H1::UI::textWidth( name, "bigfont.fnt" ) > 0xcf ) {
                        name.pop_back();
                    }
                    it->text = name;
                }
                else {
                    it->hidden = true;
                }
                it->colour = ( index == sel ) ? 0xe8 : 1;
            }
            if ( H1::UI::Item * it = req.find( 15 ) ) {
                it->hidden = true;
            }
            sliderY = ( n < 11 ) ? 0x86 : static_cast<int32_t>( top * ( 156.0 / ( n - 10 ) ) + 56.0 );
        };

        auto draw = [&]( const int pressedId ) {
            fheroes2::Display & display = fheroes2::Display::instance();
            fheroes2::Image screen = background;
            H1::UI::drawLayout( screen, extra, extraX, extraY, -1 );
            H1::UI::drawLayout( screen, req, reqX, reqY, pressedId );
            if ( scrollIcn.size() > 4 ) {
                const fheroes2::Sprite & sp = scrollIcn[4];
                fheroes2::Blit( sp, screen, reqX + sliderX + sp.x(), reqY + sliderY + sp.y() );
            }
            fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
            fheroes2::Copy( screen, 0, 0, display, s.offsetX, s.offsetY, screen.width(), screen.height() );
            display.render();
        };

        updateDetails();
        updateList();
        LocalEvent & le = LocalEvent::Get();
        const std::vector<int> ids = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, ID_OKAY, ID_CANCEL };
        int pressed = -1;
        draw( pressed );

        while ( le.HandleEvents() ) {
            const fheroes2::Point mouse = le.getMouseCursorPos();
            const int32_t mx = mouse.x - s.offsetX - reqX;
            const int32_t my = mouse.y - s.offsetY - reqY;
            const bool onSlider = mx >= sliderX && mx < sliderX + 8 && my >= sliderY && my < sliderY + 0x11;
            int over = itemAt( req, mx, my, ids );
            if ( H1::UI::Item * it = req.find( over ) ) {
                if ( it->type == H1::UI::ITEM_BUTTON && it->inactive ) {
                    over = -1; // [assumed: a button without state bit 2 takes no clicks]
                }
            }

            // Slider drag (id 0xe, FUN_0046976a).
            if ( onSlider && le.isMouseLeftButtonPressed() ) {
                const int32_t grab = my - sliderY;
                while ( le.HandleEvents() && le.isMouseLeftButtonPressed() ) {
                    const int32_t y = le.getMouseCursorPos().y - s.offsetY - reqY;
                    const int32_t newY = std::clamp( y - grab, 0x38, 0xd4 );
                    if ( n >= 11 ) {
                        int newTop = static_cast<int>( ( newY - 56 ) / ( 156.0 / ( n - 9 ) ) );
                        newTop = std::clamp( newTop, 0, n - 10 );
                        if ( newTop != top ) {
                            top = newTop;
                            updateList();
                        }
                    }
                    sliderY = newY;
                    draw( -1 );
                }
                updateList();
                draw( -1 );
                continue;
            }

            const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                draw( pressed );
            }

            int clicked = ( over != -1 && le.MouseClickLeft() ) ? over : -1;
            if ( le.isKeyPressed( fheroes2::Key::KEY_ENTER ) ) {
                clicked = ID_OKAY;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                clicked = ID_CANCEL;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_UP ) ) {
                // Scan code 0x48.
                if ( sel > 0 ) {
                    --sel;
                    if ( sel < top ) {
                        --top;
                    }
                    updateList();
                }
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_DOWN ) ) {
                // Scan code 0x50.
                if ( sel < n - 1 ) {
                    ++sel;
                    if ( top + 10 <= sel ) {
                        ++top;
                    }
                    updateList();
                }
            }

            if ( clicked == 1 ) {
                if ( top > 0 ) {
                    --top;
                    updateList();
                }
            }
            else if ( clicked == 2 ) {
                if ( top + 10 < n ) {
                    ++top;
                    if ( n <= top + 9 ) {
                        top = n - 10;
                    }
                    updateList();
                }
            }
            else if ( clicked == 3 ) {
                // Click on the slider track: jump (integer maths of FUN_00468c47).
                const int count = std::max( n - 9, 1 );
                const int y = ( mouse.y - s.offsetY ) - ( reqY + 0x38 ) - 9;
                top = ( y * 100 ) / ( 0x3d54 / count );
                if ( n <= top + 9 ) {
                    top = n - 10;
                }
                if ( top < 0 ) {
                    top = 0;
                }
                updateList();
            }
            else if ( clicked >= 4 && clicked <= 13 ) {
                const int index = clicked - 4 + top;
                if ( index == sel ) {
                    return sel; // a second click on the selected row is OK
                }
                sel = index;
                if ( sel < n ) {
                    setOkay( true );
                }
                else {
                    sel = -1;
                    setOkay( false );
                }
                updateList();
            }
            else if ( clicked == ID_CANCEL ) {
                return -1;
            }
            else if ( clicked == ID_OKAY ) {
                if ( sel == -1 ) {
                    H1::UI::messageBox( "Please make a selection from the list, or press cancel.", H1::UI::MESSAGE_OKAY );
                }
                else {
                    return sel;
                }
            }
            if ( clicked != -1 || le.isKeyPressed( fheroes2::Key::KEY_UP ) || le.isKeyPressed( fheroes2::Key::KEY_DOWN ) ) {
                updateDetails();
                draw( pressed );
            }
        }
        return -1;
    }

    bool selectDefaultMap( const MapsFileInfoList & maps, const int humans )
    {
        // FUN_00413dd1: if the current map is a campaign map ("camp") or does not fit the human count, use the default scenario.
        bool needDefault = currentMapFile.empty() || !matchesPlayerPattern( currentMapFile, humans );
        if ( !needDefault ) {
            needDefault = std::none_of( maps.begin(), maps.end(), []( const Maps::FileInfo & fi ) { return fi.filename == currentMapFile; } );
        }
        if ( !needDefault ) {
            return true;
        }
        const std::string wanted = ( humans == 1 ) ? "AES31000" : "CNM51234";
        for ( const Maps::FileInfo & fi : maps ) {
            if ( baseName( fi.filename ).compare( 0, wanted.size(), wanted ) == 0 ) {
                currentMapFile = fi.filename;
                currentMapName = fi.name;
                // Original values set together with the default map (FUN_00413dd1).
                currentMapSize = ( humans == 1 ) ? 0 : 1;
                currentMapDifficulty = ( humans == 1 ) ? 0 : 1;
                return true;
            }
        }
        return false;
    }

    // Returns true for OK.
    bool runSettingsScreen( const Screen & s, const int humans )
    {
        // H1-INTENDED(D16) extension mode: a fresh random map (maps/RNDM1234.fh2m, "Random <n>") at the top of the scenario list; in
        // original games that file is left out (owner 2026-11-18 12:06, note v245).
        const bool extensionGame = choice.extension != 0;
        if ( extensionGame ) {
            H1::prepareRandomMap( static_cast<uint32_t>( std::chrono::system_clock::now().time_since_epoch().count() & 0x7fffffff ) );
        }
        MapsFileInfoList allMaps = Maps::getAllMapFileInfos( static_cast<uint8_t>( humans ) );
        MapsFileInfoList maps;
        for ( const Maps::FileInfo & fi : allMaps ) {
            if ( !extensionGame && baseName( fi.filename ).compare( 0, 8, H1::randomMapBase ) == 0 ) {
                continue;
            }
            if ( matchesPlayerPattern( fi.filename, humans ) ) {
                maps.push_back( fi );
            }
        }
        if ( maps.empty() || !selectDefaultMap( maps, humans ) ) {
            H1::UI::messageBox( "No converted Heroes I maps were found.", H1::UI::MESSAGE_OKAY ); // project message
            return false;
        }
        // Scenario list entries, sorted by file name like FUN_00467e1c.
        std::vector<RequesterEntry> entries;
        for ( const Maps::FileInfo & fi : maps ) {
            RequesterEntry e;
            e.info = &fi;
            e.file = baseName( fi.filename );
            H1::readOriginalMapHeader( fi.filename, e.difficulty, e.size, &e.description );
            entries.push_back( e );
        }
        std::sort( entries.begin(), entries.end(), []( const RequesterEntry & a, const RequesterEntry & b ) {
            // H1-INTENDED(D16): the random map first
            const bool ra = a.file.compare( 0, 8, H1::randomMapBase ) == 0;
            const bool rb = b.file.compare( 0, 8, H1::randomMapBase ) == 0;
            if ( ra != rb ) {
                return ra;
            }
            return upper( a.file ) < upper( b.file );
        } );

        // FUN_00413dd1 reads origdata.bin first (FUN_004129f2), then puts back the remembered choices. origdata.bin values (note v52):
        // levels 2 for every record, player 0 colour 0, King of the Hill 0. The game difficulty (+0x1fe) is not in the file; the original
        // DOS game run in DOSBox shows Normal (1) on the first visit (note v179).
        H1::NewGameChoice c;
        c.difficulty = 1;
        c.extension = choice.extension; // H1-INTENDED(D16): set by the game type button just before
        if ( remembered ) {
            const int extension = c.extension;
            c = choice;
            c.extension = extension;
            for ( int i = 1; i < humans; ++i ) {
                if ( c.levels[i] == 0 ) {
                    c.levels[i] = c.levels[0];
                }
            }
        }
        choice = c;

        H1::UI::Layout layout = H1::UI::loadLayout( "newgame.bin" );
        const int32_t x = 0x136;
        const int32_t y = 0xe;
        const std::vector<int> ids = { 2, 3, 4, ID_COLOUR, ID_SCENARIO_BUTTON, 0xd, 0xe, 0xf, 0x10, ID_SCENARIO_TEXT, ID_SCENARIO_BOX, ID_KING_OF_THE_HILL,
                                       ID_RATING_TEXT, ID_CANCEL, ID_OKAY };
        refresh( layout, humans );

        LocalEvent & le = LocalEvent::Get();
        int pressed = -1;
        drawDialog( s, layout, x, y, pressed );
        while ( le.HandleEvents() ) {
            const fheroes2::Point mouse = le.getMouseCursorPos();
            const int over = itemAt( layout, mouse.x - s.offsetX - x, mouse.y - s.offsetY - y, ids );
            const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                drawDialog( s, layout, x, y, pressed );
            }

            int clicked = ( over != -1 && le.MouseClickLeft() ) ? over : -1;
            if ( le.isKeyPressed( fheroes2::Key::KEY_ENTER ) ) {
                clicked = ID_OKAY; // hotkey 28 of the OK button
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                clicked = ID_CANCEL; // hotkey 1 of the Cancel button
            }

            if ( over != -1 && le.isMouseRightButtonPressed() ) {
                // FUN_004132ca, right click: help text index.
                int index = -1;
                if ( over == ID_COLOUR ) {
                    index = 6;
                }
                else if ( over >= 2 && over <= 4 ) {
                    index = ( over - 1 < humans ) ? 8 : 5;
                }
                else if ( over == ID_CANCEL ) {
                    index = 1;
                }
                else if ( over == ID_OKAY ) {
                    index = 0;
                }
                else if ( over == ID_SCENARIO_BUTTON || over == ID_SCENARIO_TEXT || over == ID_SCENARIO_BOX ) {
                    index = 3;
                }
                else if ( over >= 0xd && over <= 0x10 ) {
                    index = 4;
                }
                else if ( over == ID_KING_OF_THE_HILL ) {
                    index = 2;
                }
                else if ( over == ID_RATING_TEXT ) {
                    index = 7;
                }
                if ( index >= 0 ) {
                    showHelp( helpText[index] );
                    drawDialog( s, layout, x, y, pressed );
                }
                continue;
            }

            if ( clicked == -1 ) {
                continue;
            }
            // FUN_004132ca, left click.
            if ( clicked >= 2 && clicked <= 4 ) {
                const int seat = clicked - 1;
                choice.levels[seat] = ( choice.levels[seat] + 1 ) % 5;
                if ( seat < humans && choice.levels[seat] == 0 ) {
                    choice.levels[seat] = 1;
                }
            }
            else if ( clicked == ID_COLOUR ) {
                choice.colour = ( choice.colour + 1 ) & 3;
            }
            else if ( clicked == ID_SCENARIO_BUTTON || clicked == ID_SCENARIO_TEXT || clicked == ID_SCENARIO_BOX ) {
                // FUN_0041eeab: request.bin + reqextra.bin over the new game screen.
                const std::string oldFile = currentMapFile;
                const std::string oldName = currentMapName;
                const std::string oldDescription = currentMapDescription;
                fheroes2::Image background = s.background;
                H1::UI::drawLayout( background, layout, x, y, -1 );
                const int chosen = runScenarioRequester( s, background, entries );
                if ( chosen >= 0 ) {
                    currentMapFile = entries[chosen].info->filename;
                }
                else {
                    currentMapFile = oldFile;
                    currentMapName = oldName;
                    currentMapDescription = oldDescription;
                }
            }
            else if ( clicked >= 0xd && clicked <= 0x10 ) {
                choice.difficulty = clicked - 0xd;
            }
            else if ( clicked == ID_KING_OF_THE_HILL ) {
                choice.kingOfTheHill = 1 - choice.kingOfTheHill;
            }
            else if ( clicked == ID_CANCEL ) {
                return false;
            }
            else if ( clicked == ID_OKAY ) {
                int players = 0;
                for ( int i = 0; i < 4; ++i ) {
                    if ( choice.levels[i] > 0 ) {
                        ++players;
                    }
                }
                choice.playerCount = players;
                choice.humans = humans;
                if ( players < 2 ) {
                    // FUN_00422fc2( text, 1, 0xb1, 0x3c ).
                    H1::UI::messageBox( "A game requires at least one opponent.", H1::UI::MESSAGE_OKAY, 0xb1, 0x3c );
                    drawDialog( s, layout, x, y, -1 );
                    continue;
                }
                // Move the remaining opponents to the front (records 1..3).
                if ( choice.levels[1] == 0 ) {
                    if ( choice.levels[2] == 0 ) {
                        choice.levels[1] = choice.levels[3];
                        choice.levels[3] = 0;
                    }
                    else {
                        choice.levels[1] = choice.levels[2];
                        choice.levels[2] = 0;
                    }
                }
                if ( choice.levels[2] == 0 && choice.levels[3] != 0 ) {
                    choice.levels[2] = choice.levels[3];
                    choice.levels[3] = 0;
                }
                remembered = true;
                return true;
            }
            refresh( layout, humans );
            drawDialog( s, layout, x, y, pressed );
        }
        return false;
    }

    PlayerColor engineColour( const int h1Colour )
    {
        // HoMM1 colour order: flag files b-, g-, r-, y- and newgame.icn frames 11/13/15/17 (blue, green, red, yellow).
        switch ( h1Colour ) {
        case 0:
            return PlayerColor::BLUE;
        case 1:
            return PlayerColor::GREEN;
        case 2:
            return PlayerColor::RED;
        case 3:
            return PlayerColor::YELLOW;
        default:
            return PlayerColor::NONE;
        }
    }

    // Campaign (note v126): world +3 lord 1..4, +7 map 0..8, +0xb how many maps so far, +0xf start day.
    struct CampaignState
    {
        bool active = false;
        int lord = 0;
        int scenario = 0;
        int count = 0;
        int startDay = 1;
    };
    CampaignState campaignState;

    // Computer colours that never played (seats set to "None"): no "vanquished" message for them.
    std::vector<PlayerColor> droppedAtStart;

    // Table 0x493d58: campaign map victory texts (congrats.bin text 100).
    const char * const campaignVictory[9] = {
        "Gateway has fallen!  The other lords have abandoned their castles and fled.  They have alerted their homelands and now gather their forces.  Speed is of the essence.",
        "The Archipelago of the Ancients has been subdued and added to your domain.  On the horizon lies a vast, unexplored - and hostile - continent.",
        "The healing power of the Eye of Goros spreads throughout the land.  The population unites behind you and the other lords retreat.  The war for domination begins.",
        "With your victory, the other lords have made their final retreat.  They must each in turn be fought one-on-one in their homelands, and their personal castles must be captured.",
        "The knights are broken in battle!  You have conquered their homeland.",
        "The barbarian castle has been overthrown and their army scattered!",
        "You have burst through the forest maze and destroyed Lamanda's castle.",
        "You have followed the gargoyles to the castle of Lord Alamar and have shattered the might of the warlocks.",
        "The dragons join your cause and the competing warlords capitulate.  You now rule a vast and united land as the one true King!" };

    // Table 0x493d30: campaign stories (FUN_0041421f text 1).
    const char * const campaignStory[9] = {
        "You have established a foothold in the new land.  This small island is fiercely contested by three other factions, all vying to capture the strategic town - Gateway.  The town is located in the center of the island, and so dominates its surroundings that the other factions will surrender to the lord that captures it.  Beware the dragon guardian of Gateway!",
        "Your way to the mainland is blocked by the Archipelago of the Ancients, a series of four large islands, each held by a different lord.  The opposition must all be subdued, and they are better led this time.  Boats are a necessity - use them wisely!",
        "Chaos.  A maelstrom of combat plagues the land.  The people suffer, but will rally behind the wielder of the Eye of Goros, an artifact that can heal the wounded land.  It was buried and lost eons ago.  The first lord to uncover the Eye will unite the people and conquer the land - but it lies in a vast territory with only pieces of a puzzle to guide the way.",
        "With the founding of a homeland, the other lords now take you seriously.  All seek to dominate the central continent, and each is suspicious of the others.  The territory is huge, the opposition distant.  Resources are scarce and should be fiercely defended.  You must be the last lord left to claim victory.",
        "The land of the knights, led by Lord Ironfist, is divided by the twisting Floodwater River.  Ironfist is counting on the river to protect him.  The only town suitable to boat-building lies upon the river far to the east.  To defeat Ironfist, you must capture his home castle in the far northwest.",
        "Far to the north, beyond the Trackless Desert, lies the Frozen Wastes.  It is the homeland of Lord Slayer and his barbarian followers.  Once the mountain pass has been breached by either side, barbarian raiders will stream south.  The desert may harbor unknown allies who can aid you.  Slayer's castle lies just northeast of the pass.",
        "Warned of your approach, the sorceress Queen Lamanda worked on a dreadful magic and sank the approach to the only port.  You must find the teleport gate to assault the southwestern land and capture the port.  The only landfall is far to the northeast.  From there you must struggle through the forest maze to locate her castle in the extreme northwest.",
        "The warlocks' castle lies shrouded in the smokey volcanic rift.  To reach Lord Alamar's home castle in the extreme southeast, you must wander through the Minotaur Maze.  The warlocks are overconfident and not expecting an attack, so sure are they that none can navigate the maze.  Gargoyles have been set to dissuade invaders from the true path.",
        "Final victory lies within your grasp - but the defeated warlords have pooled their last resources and have banded together against you.  If you can bend the dragons to your will and force them to side with you, all the other warlords will submit and the land will be yours to rule.  Capture the Dragon Citadel on the central island and victory is yours!" };

    // Table 0x492769 (+ s x 0x55): +0x13..+0x15 opponent levels; byte before the entry: King of the Hill.
    const int campaignOpponents[9][3] = { { 1, 1, 1 }, { 2, 2, 2 }, { 2, 2, 2 }, { 2, 2, 2 }, { 3, 0, 0 }, { 3, 0, 0 }, { 3, 0, 0 }, { 3, 0, 0 }, { 3, 3, 3 } };
    // Table 0x492a66: colour of each lord (1 Ironfist, 2 Slayer, 3 Lamanda, 4 Alamar).
    const int lordColour[5] = { 0, 2, 1, 3, 0 };

    // H1-INTENDED(D16) extension mode: starting bonus (owner 2026-11-17 23:29, as Heroes III): gold, resources or an artifact for the
    // first hero. People choose in a box of three pictures; computer players get one at random.
    void applyStartBonus()
    {
        // Minor artifacts: not the ultimate ones (0..3), the Fizbin of Misfortune (12) or the magic book (37).
        std::vector<int> minor;
        for ( int a = 4; a < 37; ++a ) {
            if ( a != 12 ) {
                minor.push_back( a );
            }
        }
        const PlayerColorsSet humanColours = Settings::Get().GetPlayers().GetColors( CONTROL_HUMAN, true );
        for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW } ) {
            Kingdom & kingdom = world.GetKingdom( c );
            if ( !kingdom.isPlay() ) {
                continue;
            }
            const int artifact = minor[static_cast<size_t>( Rand::Get( 0, static_cast<uint32_t>( minor.size() - 1 ) ) )];
            int pick = static_cast<int>( Rand::Get( 0, 2 ) );
            if ( humanColours & c ) {
                H1::UI::Layout layout = H1::UI::loadLayout( "evntwin3.bin" );
                if ( H1::UI::Item * it = layout.find( 1 ) ) {
                    it->text = "Choose a starting bonus.";
                }
                for ( H1::UI::Item & it : layout.items ) {
                    if ( it.type == H1::UI::ITEM_BUTTON ) {
                        it.hidden = true;
                        it.inactive = true;
                    }
                }
                const char * const captions[3] = { "2000 Gold", "Resources", H1::artifactName( artifact ) };
                const int32_t boxHeight = 3 * 0x2d + 0x81;
                for ( int k = 0; k < 3; ++k ) {
                    const int32_t centre = 0x37 + k * 0x58;
                    H1::UI::Item icon;
                    icon.type = H1::UI::ITEM_ICON;
                    icon.name = ( k == 2 ) ? "artifact.icn" : "resource.icn";
                    icon.frame = ( k == 0 ) ? 6 : ( k == 1 ? 2 : artifact );
                    icon.w = ( k == 2 ) ? 0x40 : 0x26;
                    icon.h = ( k == 2 ) ? 0x40 : 0x20;
                    icon.x = centre - icon.w / 2;
                    icon.y = boxHeight - 0x40 - 0x48 + ( 0x40 - icon.h ) / 2;
                    icon.flags = 0x10;
                    icon.id = 0x9001 + k;
                    layout.items.push_back( icon );
                    layout.clickableIcons.push_back( icon.id );
                    H1::UI::Item label;
                    label.type = H1::UI::ITEM_TEXT;
                    label.name = "smalfont.fnt";
                    label.colour = 1;
                    label.align = 1;
                    label.x = centre - 0x2f;
                    label.y = boxHeight - 0x40 - 0x48 + 0x44;
                    label.w = 0x5e;
                    label.h = 0x18;
                    label.text = captions[k];
                    layout.items.push_back( label );
                }
                fheroes2::Display & display = fheroes2::Display::instance();
                const int answer = H1::UI::runLayoutDialog( layout, ( display.width() - 640 ) / 2 + 0xb1, ( display.height() - 480 ) / 2 + 0x1c, false );
                if ( answer >= 0x9001 && answer <= 0x9003 ) {
                    pick = answer - 0x9001;
                }
            }
            if ( pick == 0 ) {
                kingdom.AddFundsResource( Funds( Resource::GOLD, 2000 ) );
            }
            else if ( pick == 1 ) {
                Funds f;
                f.wood = 10;
                f.ore = 10;
                f.mercury = 3;
                f.sulfur = 3;
                f.crystal = 3;
                f.gems = 3;
                kingdom.AddFundsResource( f );
            }
            else if ( !kingdom.GetHeroes().empty() ) {
                const int id = H1::engineArtifactFromH1( artifact );
                if ( id >= 0 ) {
                    kingdom.GetHeroes().front()->GetBagArtifacts().PushArtifact( Artifact( id ) );
                }
            }
        }
    }

    // H1-INTENDED(D16) extension mode: start race window (owner 2026-11-17, mock results/extension/race_window_mock_v3.png). The four town
    // castle pictures (townbkgN.bmp with the castle, frame 0 of farmcast/frstcast/plnscast/mtncast.icn) in the event window of evntwin5.bin;
    // RANDOM keeps the original random race. Returns the HoMM1 race 0..3 or -1 (random).
    int chooseStartRace( const std::string & who )
    {
        H1::UI::Layout layout = H1::UI::loadLayout( "evntwin5.bin" );
        if ( layout.items.empty() ) {
            return -1;
        }
        if ( H1::UI::Item * it = layout.find( 1 ) ) {
            it->text = who.empty() ? std::string( "Choose the race you will start with." ) : who + "\n" + "Choose the race you will start with.";
            it->h = 0x24;
        }
        for ( H1::UI::Item & it : layout.items ) {
            if ( it.type == H1::UI::ITEM_BUTTON ) {
                it.hidden = true;
                it.inactive = true;
                if ( it.id == 0x8002 && H1::getIcn( "system.icn" ).size() > 14 ) {
                    // The OKAY button place: RANDOM (system.icn 13/14, tools/extension/make_ext_buttons.py).
                    it.hidden = false;
                    it.inactive = false;
                    it.frame = 13;
                    it.pressedFrame = 14;
                    it.id = 0x910f; // (0x9101..0x9105 are the race pictures)
                }
            }
        }
        // Grid order of the mock: knight, barbarian / sorceress, warlock; H1-INTENDED(D16) necromancer (5th race) centred below.
        const int order[5] = { 0, 2, 1, 3, 4 };
        const char * const castleIcn[5] = { "farmcast.icn", "frstcast.icn", "plnscast.icn", "mtncast.icn", "necrcast.icn" };
        const char * const raceName[5] = { "Knight", "Sorceress", "Barbarian", "Warlock", "Necromancer" };
        const int raceCount = H1::getIcn( "necrcast.icn" ).empty() ? 4 : 5;
        // Five races: three over two, smaller pictures in the same space (H1-INTENDED(D16)).
        const int32_t picW = ( raceCount == 5 ) ? 0x4e : 0x74;
        const int32_t picH = ( raceCount == 5 ) ? 0x2a : 0x3e;
        for ( int k = 0; k < raceCount; ++k ) {
            const int race = order[k];
            int32_t px = 0x17 + ( k % 2 ) * ( picW + 8 );
            int32_t py = 0x58 + ( k / 2 ) * ( picH + 0x24 );
            if ( raceCount == 5 ) {
                px = ( k < 3 ) ? 0x17 + k * ( picW + 6 ) : 0x17 + ( picW + 6 ) / 2 + ( k - 3 ) * ( picW + 6 );
                py = 0x5a + ( k / 3 ) * ( picH + 0x1a );
            }
            fheroes2::Image town = H1::UI::readBmp( "townbkg" + std::to_string( race ) + ".bmp" );
            const std::vector<fheroes2::Sprite> & icn = H1::getIcn( castleIcn[race] );
            int32_t cx = 0;
            int32_t cy = 0;
            int32_t cw = town.width();
            int32_t ch = town.height();
            if ( !icn.empty() && !town.empty() ) {
                const fheroes2::Sprite & castle = icn[0];
                fheroes2::Blit( castle, town, castle.x(), castle.y() );
                // A box around the castle with the picture's shape, inside the town picture.
                cw = std::max( castle.width(), castle.height() * picW / picH ) + 0x10;
                ch = cw * picH / picW;
                cx = std::clamp( castle.x() + castle.width() / 2 - cw / 2, 0, std::max( 0, town.width() - cw ) );
                cy = std::clamp( castle.y() + castle.height() - ch + 6, 0, std::max( 0, town.height() - ch ) );
                cw = std::min( cw, town.width() - cx );
                ch = std::min( ch, town.height() - cy );
            }
            auto picture = std::make_shared<fheroes2::Image>( picW, picH );
            picture->_disableTransformLayer();
            fheroes2::Fill( *picture, 0, 0, picW, picH, 0x7b );
            if ( !town.empty() && cw > 0 && ch > 0 ) {
                fheroes2::Image crop( cw, ch );
                crop._disableTransformLayer();
                fheroes2::Copy( town, cx, cy, crop, 0, 0, cw, ch );
                fheroes2::Image small( picW - 4, picH - 4 );
                small._disableTransformLayer();
                fheroes2::Resize( crop, small );
                fheroes2::Copy( small, 0, 0, *picture, 2, 2, picW - 4, picH - 4 );
            }
            fheroes2::Fill( *picture, 1, 1, picW - 2, 1, 0x80 );
            fheroes2::Fill( *picture, 1, picH - 2, picW - 2, 1, 0x80 );
            fheroes2::Fill( *picture, 1, 1, 1, picH - 2, 0x80 );
            fheroes2::Fill( *picture, picW - 2, 1, 1, picH - 2, 0x80 );
            H1::UI::Item image;
            image.type = H1::UI::ITEM_IMAGE;
            image.flags = 0x1000;
            image.x = px;
            image.y = py;
            image.w = picW;
            image.h = picH;
            image.picture = picture;
            image.id = 0x9101 + race;
            layout.items.push_back( image );
            layout.clickableIcons.push_back( image.id );
            H1::UI::Item label;
            label.type = H1::UI::ITEM_TEXT;
            label.name = "smalfont.fnt";
            label.colour = 1;
            label.align = 1;
            label.x = px;
            label.y = py + picH + 2;
            label.w = picW;
            label.h = 0x10;
            label.text = raceName[race];
            layout.items.push_back( label );
        }
        fheroes2::Display & display = fheroes2::Display::instance();
        const int answer = H1::UI::runLayoutDialog( layout, ( display.width() - 640 ) / 2 + 0xb1, ( display.height() - 480 ) / 2 + 0x1c, false );
        return ( answer >= 0x9101 && answer < 0x9101 + raceCount ) ? answer - 0x9101 : -1;
    }

    fheroes2::GameMode startGame()
    {
        Settings & conf = Settings::Get();
        MapsFileInfoList maps = Maps::getAllMapFileInfos( static_cast<uint8_t>( std::max( 1, choice.humans ) ) );
        const auto it = std::find_if( maps.begin(), maps.end(), []( const Maps::FileInfo & fi ) { return fi.filename == currentMapFile; } );
        if ( it == maps.end() ) {
            return fheroes2::GameMode::MAIN_MENU;
        }
        // H1-INTENDED(D8): Hot Seat only; network, modem and direct connection are not ported (docs/INTENDED_DIFFERENCES.md).
        // Hot Seat (DAT_004a7bbc = 2..4 human seats, note v175) is the engine's Hot Seat game type.
        conf.SetGameType( choice.humans > 1 ? Game::TYPE_HOTSEAT : Game::TYPE_STANDARD );
        conf.setCurrentMapInfo( *it );

        // Human colour: same swap as the engine's own player list (gui/player_info.cpp) when the map allows that colour.
        const PlayerColor wanted = engineColour( choice.colour );
        const PlayerColorsSet humans = conf.GetPlayers().GetColors( CONTROL_HUMAN, true );
        if ( wanted != PlayerColor::NONE && ( it->AllowCompHumanColors() & wanted ) && !( humans & wanted ) ) {
            for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW, PlayerColor::ORANGE, PlayerColor::PURPLE } ) {
                if ( humans & c ) {
                    Players::SetPlayerControl( c, CONTROL_AI | CONTROL_HUMAN );
                    break;
                }
            }
            Players::SetPlayerControl( wanted, CONTROL_HUMAN );
        }

        // Game difficulty 0..3 -> engine EASY..EXPERT (what each level changes is ported with the rules later).
        Game::saveDifficulty( choice.difficulty );

        conf.GetPlayers().SetStartGame();

        // Standard game (note v138): FUN_0041fb75 colours, then FUN_004147d7 start castles.
        // Seat 0 = the human with the chosen colour; seats 1 .. playerCount - 1 = computers, each a random colour 0..3 not used yet.
        // Maps without castle setup owners: seat by seat a random castle of slots 0..3 without an owner (FUN_00411e7c, up to 10,000 tries;
        // else the first free one, FUN_00411e1d); slots no seat drew become neutral towns. Maps with owners: owner n = seat n
        // (world_loadmap.cpp). Campaign (note v169): the same FUN_0041fb75 - seat 0 has the lord's colour (0x492a66), and the table entries
        // for seats 1..3 (+0x18 / +0x1a / +0x1c of each scenario) are 4, 4 and 30 in every scenario, so those seats get random colours; the
        // castles keep the map's owners (FUN_0041d443, owner n = seat n).
        std::vector<int> seatColours;
        {
            int human = -1;
            for ( int c = 0; c < 4; ++c ) {
                if ( conf.GetPlayers().GetColors( CONTROL_HUMAN, true ) & Color::IndexToColor( c ) ) {
                    human = c;
                    break;
                }
            }
            if ( human >= 0 ) {
                bool used[4] = { false, false, false, false };
                seatColours.push_back( human );
                used[human] = true;
                for ( int seat = 1; seat < std::min( choice.playerCount, 4 ); ++seat ) {
                    int c = 0;
                    do {
                        c = static_cast<int>( Rand::Get( 0, 3 ) );
                    } while ( used[c] );
                    used[c] = true;
                    seatColours.push_back( c );
                }
                int slotColour[4] = { -1, -1, -1, -1 };
                for ( const int c : seatColours ) {
                    int slot = -1;
                    for ( int tries = 0; tries <= 9999; ++tries ) {
                        const int r = static_cast<int>( Rand::Get( 0, 3 ) );
                        if ( slotColour[r] == -1 ) {
                            slot = r;
                            break;
                        }
                    }
                    if ( slot < 0 ) {
                        for ( int r = 0; r < 4 && slot < 0; ++r ) {
                            if ( slotColour[r] == -1 ) {
                                slot = r;
                            }
                        }
                    }
                    if ( slot >= 0 ) {
                        slotColour[slot] = c;
                    }
                }
                H1::setStartCastlePlan( seatColours, slotColour );
                for ( int seat = 0; seat < 4; ++seat ) {
                    choice.seatColour[seat] = ( seat < static_cast<int>( seatColours.size() ) ) ? seatColours[seat] : -1;
                }
                if ( choice.humans > 1 && !campaignState.active ) {
                    // Hot Seat: seats 0 .. humans - 1 are people (DAT_004a9b60), the other seats computers; every colour a seat
                    // did not draw is a computer colour that leaves below.
                    for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW, PlayerColor::ORANGE, PlayerColor::PURPLE } ) {
                        Players::SetPlayerControl( c, CONTROL_AI );
                    }
                    for ( size_t seat = 0; seat < seatColours.size(); ++seat ) {
                        if ( static_cast<int>( seat ) < choice.humans ) {
                            Players::SetPlayerControl( Color::IndexToColor( seatColours[seat] ), CONTROL_HUMAN );
                        }
                    }
                    conf.GetPlayers().SetStartGame();
                }
            }
        }

        H1::clearForcedStartRaces();
        if ( choice.extension != 0 && !campaignState.active ) {
            // H1-INTENDED(D16): each person picks a start race (Hot Seat: in seat order, with the seat colour named).
            const int people = std::max( 1, choice.humans );
            const char * const colourName[4] = { "Blue", "Green", "Red", "Yellow" };
            for ( int seat = 0; seat < people && seat < static_cast<int>( seatColours.size() ); ++seat ) {
                const int c = seatColours[seat];
                const std::string who = ( people > 1 && c >= 0 && c < 4 ) ? std::string( colourName[c] ) : std::string();
                H1::setForcedStartRace( c, chooseStartRace( who ) );
            }
        }
        const bool loaded = world.loadResurrectionMap( currentMapFile );
        H1::clearForcedStartRaces();
        if ( loaded ) {
            H1::Ai::resetState();
            H1::Week::reset();
            gameFileName = "NEWGAME";
            // Game +0x1ff = number of seats with a level (the human and the opponents not set to "None"); the seats after it do not play
            // (FUN_004147d7 sets up only seats below +0x1ff). Standard game: the seat colours above (note v138). Campaign: the converter gives
            // seat n the engine colour n, so the seats after the first (playerCount - 1) computer seats leave (note v137); their towns becoming
            // neutral is the engine loss handling [unconfirmed].
            H1::clearStartCastlePlan();
            droppedAtStart.clear();
            int keep = std::max( 0, choice.playerCount - 1 );
            for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW, PlayerColor::ORANGE, PlayerColor::PURPLE } ) {
                if ( !( conf.GetPlayers().GetColors( CONTROL_AI, true ) & c ) ) {
                    continue;
                }
                if ( !seatColours.empty() ) {
                    // Standard game: the computer colours that are seats play, the others leave.
                    if ( std::find( seatColours.begin(), seatColours.end(), Color::GetIndex( c ) ) == seatColours.end() ) {
                        world.GetKingdom( c ).LossPostActions();
                        droppedAtStart.push_back( c );
                    }
                    continue;
                }
                if ( keep > 0 ) {
                    --keep;
                    continue;
                }
                world.GetKingdom( c ).LossPostActions();
                droppedAtStart.push_back( c );
            }
            if ( choice.extension != 0 ) {
                H1::placeExtensionObjects(); // H1-INTENDED(D16): neutral creatures and HoMM2 objects on the map (note v240)
                applyStartBonus(); // H1-INTENDED(D16)
            }
            return fheroes2::GameMode::START_GAME;
        }
        fheroes2::showStandardTextMessage( "Warning", "The map is corrupted or doesn't exist.", Dialog::OK );
        return fheroes2::GameMode::MAIN_MENU;
    }

    // FUN_0041454b: set up campaign map s (CAMP%d.CMP, converted as CAMPAIGN%d.fh2m) and start it.
    fheroes2::GameMode startCampaignScenario( const int scenario, const bool showWindow = true )
    {
        campaignState.active = true;
        campaignState.scenario = scenario;
        currentMapFile.clear();
        const std::string wanted = "CAMPAIGN" + std::to_string( scenario + 1 ) + ".fh2m";
        for ( const Maps::FileInfo & fi : Maps::getAllMapFileInfos( 1 ) ) {
            if ( upper( baseName( fi.filename ) ) == upper( wanted ) ) {
                currentMapFile = fi.filename;
            }
        }
        if ( currentMapFile.empty() ) {
            H1::UI::messageBox( "The converted campaign maps (CAMPAIGN1..9.fh2m) were not found.", H1::UI::MESSAGE_OKAY ); // project message
            campaignState.active = false;
            return fheroes2::GameMode::MAIN_MENU;
        }
        // Player 0: level 4, the lord's colour; opponents from the table; King of the Hill only on map 8.
        choice.colour = lordColour[std::clamp( campaignState.lord, 1, 4 )];
        choice.levels[0] = 4;
        choice.humans = 1;
        choice.playerCount = 1;
        for ( int seat = 1; seat < 4; ++seat ) {
            choice.levels[seat] = campaignOpponents[scenario][seat - 1];
            if ( choice.levels[seat] != 0 ) {
                ++choice.playerCount;
            }
        }
        choice.kingOfTheHill = ( scenario == 8 ) ? 1 : 0;
        int mapDifficulty = 0;
        int mapSize = 0;
        H1::readOriginalMapHeader( currentMapFile, mapDifficulty, mapSize );
        currentMapDifficulty = mapDifficulty;
        choice.rating = computeRating( choice, 1 );
        const fheroes2::GameMode mode = startGame();
        if ( mode != fheroes2::GameMode::START_GAME ) {
            campaignState.active = false;
            return mode;
        }
        // FUN_00427056: the goal town of maps 0 and 4..7 is named from the campaign table (0x492769 + 2: "Gateway", "Castle Ironfist" ...).
        {
            static const int goal[9][2] = { { 0x24, 0x23 }, { -1, -1 }, { -1, -1 }, { -1, -1 }, { 0x0d, 0x0a }, { 0x3e, 0x14 }, { 0x08, 0x08 }, { 0x42, 0x45 }, { -1, -1 } };
            static const char * const goalNames[9] = { "Gateway", "", "", "", "Castle Ironfist", "Castle Slayer", "Castle Lamanda", "Castle Alamar", "" };
            if ( goal[scenario][0] >= 0 ) {
                Castle * castle = world.getCastleEntrance( { goal[scenario][0], goal[scenario][1] } );
                if ( castle == nullptr ) {
                    castle = world.getCastle( { goal[scenario][0], goal[scenario][1] } );
                }
                if ( castle != nullptr ) {
                    castle->setH1Name( goalNames[scenario] );
                }
            }
        }
        // The date goes on from the previous map (world +0xf).
        world.setCampaignStartDay( static_cast<uint32_t>( std::max( 1, campaignState.startDay ) ) );
        // Starting resources (table +0x1c + seat x 14): wood 30, mercury 10, ore 30, sulfur 10, crystal 10, gems 10, gold 10000 for every seat.
        for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW } ) {
            Kingdom & kingdom = world.GetKingdom( c );
            kingdom.OddFundsResource( kingdom.GetFunds() );
            kingdom.AddFundsResource( Funds( 30, 30, 10, 10, 10, 10, 10000 ) );
        }
        // FUN_0041421f( s, 0 ) before the map starts.
        if ( showWindow ) {
            H1::showCampaignWindow( false );
        }
        return mode;
    }
}

namespace H1
{
    bool isCampaignGame()
    {
        return campaignState.active;
    }

    void assignOriginalCastleNames( const std::vector<Castle *> & castles )
    {
        // Table 0x493380.
        static const char * const names[36] = { "Blackridge", "Pinehurst",   "Woodhaven",   "Hillstone",  "Whiteshield", "Bloodreign",
                                                "Dragontooth", "Greywind",   "Blackwind",   "Portsmith",  "Middle Gate", "Tundara",
                                                "Vulcania",   "Sansobar",    "Atlantium",   "Baywatch",   "Wildabar",    "Fountainhead",
                                                "Vertigo",    "Winterkill",  "Nightshadow", "Sandcaster", "Lakeside",    "Olympus",
                                                "Necropolis", "Burlock",     "Xabran",      "Dragadune",  "Alamar",      "Kalindra",
                                                "Blackfang",  "Basenji",     "Algary",      "Sorpigal",   "Dusk",        "Erliquin" };
        // FUN_00411e7c: random index 0..35 until a free one (up to 10,000 tries; the table has as many names as castle slots).
        std::array<bool, 36> used{};
        for ( Castle * castle : castles ) {
            if ( castle == nullptr ) {
                continue;
            }
            int index = -1;
            for ( int tries = 0; tries < 10000; ++tries ) {
                const int r = static_cast<int>( Rand::Get( 0, 35 ) );
                if ( !used[r] ) {
                    index = r;
                    break;
                }
            }
            if ( index < 0 ) {
                continue;
            }
            used[index] = true;
            castle->setH1Name( names[index] );
        }
    }

    bool isExtension()
    {
        return H1::isH1Mode() && choice.extension != 0 && !campaignState.active;
    }

    int campaignScenario()
    {
        return campaignState.active ? campaignState.scenario : -1;
    }

    bool showCampaignWindow( const bool inGame )
    {
        // FUN_0041421f: campaign.bin at (0x69, 0x60): text 1 = story (table 0x493d30), 2 = map name (0x493d80), 3 = campaign.icn frame
        // count + 4; at the start only OKAY (0x8002, centre); in the game OKAY (0x8000, left) and RESTART SCENARIO (0x385).
        static const char * const names[9] = { "Gateway", "The Archipelago", "The Wounded Land", "Free-for-All", "Castle Ironfist", "Castle Slayer", "Castle Lamanda", "Castle Alamar", "King-of-the-Hill" };
        H1::UI::Layout layout = H1::UI::loadLayout( "campaign.bin" );
        if ( layout.width == 0 || !campaignState.active ) {
            return false;
        }
        const int s = std::clamp( campaignState.scenario, 0, 8 );
        if ( H1::UI::Item * it = layout.find( 1 ) ) {
            it->text = campaignStory[s];
        }
        if ( H1::UI::Item * it = layout.find( 2 ) ) {
            it->text = names[s];
        }
        if ( H1::UI::Item * it = layout.find( 3 ) ) {
            it->frame = campaignState.count + 4;
        }
        const std::vector<int> hiddenIds = inGame ? std::vector<int>{ 0x8002 } : std::vector<int>{ 0x8000, 0x385 };
        for ( const int id : hiddenIds ) {
            if ( H1::UI::Item * it = layout.find( id ) ) {
                it->hidden = true;
            }
        }
        if ( !inGame ) {
            H1::playMusic( 0x30 );
        }
        const int result = H1::UI::runLayoutDialog( layout, 0x69, 0x60, false );
        // RESTART SCENARIO: "Are you sure you want to restart this scenario?" Yes -> FUN_0041454b of the same map.
        return result == 0x385 && H1::UI::messageBox( "Are you sure you want to restart this scenario?", H1::UI::MESSAGE_YES_NO ) == 0x8005;
    }

    fheroes2::GameMode restartCampaignScenario()
    {
        return startCampaignScenario( campaignState.scenario, false );
    }

    std::string pendingLoad;


    void setPendingLoad( const std::string & path )
    {
        pendingLoad = path;
    }

    std::string takePendingLoad()
    {
        std::string path;
        path.swap( pendingLoad );
        return path;
    }

    std::string selectSaveFile( const bool save )
    {
        // The engine's save files stand in for .\GAMES\*.GM* (campaign: *.CGM) [adapted: the engine's own save format and folder].
        const std::string folder = Game::GetSaveDir();
        const bool campaign = Settings::Get().isCampaignGameType();
        const std::string extension = campaign ? Game::GetSaveFileExtension( Game::TYPE_CAMPAIGN ) : Game::GetSaveFileExtension( Game::TYPE_STANDARD );
        std::vector<SaveEntry> entries;
        ListFiles files;
        files.ReadDir( folder, extension );
        for ( const std::string & path : files ) {
            std::string name = baseName( path );
            if ( name.size() <= extension.size() || name.compare( name.size() - extension.size(), extension.size(), extension ) != 0 ) {
                continue;
            }
            name.resize( name.size() - extension.size() );
            entries.push_back( { name, path } );
        }
        // FUN_00467e1c: a new name goes before the first one __strcmpi puts after it.
        std::stable_sort( entries.begin(), entries.end(), []( const SaveEntry & a, const SaveEntry & b ) { return upper( a.name ) < upper( b.name ); } );
        // Save mode: the edit field starts with the game's file name (world +0x9f) without its extension.
        std::string current;
        if ( save ) {
            // World +0x9f up to the first '.'.
            current = gameFileName;
            const size_t dot = current.find( '.' );
            if ( dot != std::string::npos ) {
                current.resize( dot );
            }
        }
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        return runSaveRequester( save, entries, extension, folder, current );
    }

    void saveSideState( const std::string & path )
    {
        {
            const std::string name = upperBase( path );
            if ( name.compare( 0, 8, "AUTOSAVE" ) != 0 && name.compare( 0, 8, "PLYREXIT" ) != 0 ) {
                gameFileName = name;
            }
        }
        std::ofstream out( path, std::ios::trunc );
        out << "h1side 1\n" << choice.difficulty << ' ' << choice.levels[0] << ' ' << choice.levels[1] << ' ' << choice.levels[2] << ' ' << choice.levels[3]
            << ' ' << choice.colour << ' ' << choice.kingOfTheHill << ' ' << choice.rating << ' ' << choice.playerCount << ' ' << choice.humans << '\n'
            << ( campaignState.active ? 1 : 0 ) << ' ' << campaignState.lord << ' ' << campaignState.scenario << ' ' << campaignState.count << ' '
            << campaignState.startDay << '\n'
            << "seats " << choice.seatColour[0] << ' ' << choice.seatColour[1] << ' ' << choice.seatColour[2] << ' ' << choice.seatColour[3] << '\n'
            << H1::Ai::savedState() << '\n' << H1::Week::savedState() << '\n';
        // Tile bytes the game changed (new month monsters, note v168).
        const auto & patches = H1::tilePatches();
        out << "tiles " << patches.size();
        for ( const auto & [index, bytes] : patches ) {
            out << ' ' << index;
            for ( const uint8_t b : bytes ) {
                out << ' ' << static_cast<int>( b );
            }
        }
        out << '\n';
        // The terrain picture choices of FUN_0041d268 (note v195): one digit 0..3 per tile.
        const std::vector<uint8_t> & variants = H1::terrainVariants();
        out << "terrain " << ( variants.empty() ? std::string( "-" ) : std::string() );
        for ( const uint8_t v : variants ) {
            out << static_cast<char>( '0' + ( v & 3 ) );
        }
        out << '\n';
        // Hero +0xae bonus bits (note v198).
        out << H1::bonusSavedState() << '\n';
        out << "ext " << choice.extension << '\n';
        // H1-INTENDED(D16): the random map's tile bytes go with the save ("<save>.h1t"), since the next extension game makes the map again.
        H1::saveTileBytesCopy( path + "t" );
    }

    void loadSideState( const std::string & path )
    {
        gameFileName = upperBase( path );
        H1::resetTilePatches();
        H1::useTileBytesCopy( path + "t" ); // H1-INTENDED(D16): a random-map save's own tile bytes (h1_gfx.h)
        H1::Week::reset();
        H1::resetBonusBits();
        NewGameChoice c;
        CampaignState k;
        std::ifstream in( path );
        std::string tag;
        int version = 0;
        int active = 0;
        if ( in >> tag >> version && tag == "h1side"
             && in >> c.difficulty >> c.levels[0] >> c.levels[1] >> c.levels[2] >> c.levels[3] >> c.colour >> c.kingOfTheHill >> c.rating >> c.playerCount >> c.humans
             && in >> active >> k.lord >> k.scenario >> k.count >> k.startDay ) {
            k.active = active != 0;
            std::string seatsTag;
            if ( !( in >> seatsTag >> c.seatColour[0] >> c.seatColour[1] >> c.seatColour[2] >> c.seatColour[3] ) || seatsTag != "seats" ) {
                for ( int & v : c.seatColour ) {
                    v = -1;
                }
            }
            // The computer players' saved values (note v153); an older save without them starts them fresh.
            std::string aiLine;
            std::getline( in >> std::ws, aiLine );
            H1::Ai::loadSavedState( aiLine );
            // The week and month values and the changed tile bytes (note v168); older saves have none.
            std::string weekLine;
            if ( std::getline( in >> std::ws, weekLine ) ) {
                H1::Week::loadSavedState( weekLine );
                std::string tilesTag;
                size_t n = 0;
                if ( in >> tilesTag >> n && tilesTag == "tiles" ) {
                    for ( size_t i = 0; i < n; ++i ) {
                        int32_t index = 0;
                        std::array<uint8_t, 10> bytes{};
                        if ( !( in >> index ) ) {
                            break;
                        }
                        for ( uint8_t & b : bytes ) {
                            int v = 0;
                            in >> v;
                            b = static_cast<uint8_t>( v );
                        }
                        H1::patchTileBytes( index, bytes );
                    }
                }
                // Older saves have no terrain line: their terrain stays as the map has it.
                std::string terrainTag;
                std::string digits;
                if ( in >> terrainTag >> digits && terrainTag == "terrain" && digits.size() == 72 * 72 ) {
                    std::vector<uint8_t> variants( digits.size() );
                    for ( size_t i = 0; i < digits.size(); ++i ) {
                        variants[i] = static_cast<uint8_t>( ( digits[i] - '0' ) & 3 );
                    }
                    H1::setTerrainVariants( std::move( variants ) );
                }
                std::string bonusLine;
                if ( std::getline( in >> std::ws, bonusLine ) ) {
                    H1::loadBonusSavedState( bonusLine );
                }
                std::string extTag;
                int ext = 0;
                if ( in >> extTag >> ext && extTag == "ext" ) {
                    c.extension = ext;
                }
            }
        }
        else {
            c = NewGameChoice();
            k = CampaignState();
            H1::Ai::resetState();
        }
        choice = c;
        campaignState = k;
    }

    int seatLevel( const PlayerColor color )
    {
        if ( campaignState.active ) {
            return std::max( { choice.levels[1], choice.levels[2], choice.levels[3] } );
        }
        for ( int seat = 0; seat < 4; ++seat ) {
            if ( choice.seatColour[seat] >= 0 && Color::IndexToColor( choice.seatColour[seat] ) == color ) {
                return choice.levels[seat];
            }
        }
        return -1;
    }

    bool isCampaignStopTile( const int x, const int y )
    {
        if ( !campaignState.active ) {
            return false;
        }
        static const int stop[9][2] = { { 0x24, 0x23 }, { -1, -1 }, { -1, -1 }, { -1, -1 }, { 0x0d, 0x0a }, { 0x3e, 0x14 }, { 0x08, 0x08 }, { 0x42, 0x45 }, { -1, -1 } };
        const int s = std::clamp( campaignState.scenario, 0, 8 );
        return stop[s][0] == x && stop[s][1] == y;
    }

    int campaignObjective( const PlayerColor human )
    {
        // FUN_00424700 (note v125 section 3): 1 = the human reached the goal, 2 = an enemy did first, 0 = neither (or a map without goal).
        if ( !campaignState.active ) {
            return 0;
        }
        const int s = campaignState.scenario;
        auto ownerAt = []( const int x, const int y ) {
            const Castle * castle = world.getCastleEntrance( { x, y } );
            if ( castle == nullptr ) {
                castle = world.getCastle( { x, y } );
            }
            return castle != nullptr ? castle->GetColor() : PlayerColor::NONE;
        };
        auto judge = [human]( const PlayerColor owner, const bool enemyLoses ) {
            if ( owner == human ) {
                return 1;
            }
            return ( enemyLoses && owner != PlayerColor::NONE ) ? 2 : 0;
        };
        switch ( s ) {
        case 0:
            return judge( ownerAt( 0x24, 0x23 ), true );
        case 4:
            return judge( ownerAt( 0x0d, 0x0a ), false );
        case 5:
            return judge( ownerAt( 0x3e, 0x14 ), false );
        case 6:
            return judge( ownerAt( 0x08, 0x08 ), false );
        case 7:
            return judge( ownerAt( 0x42, 0x45 ), false );
        case 2: {
            // A hero with one of the ultimate artifacts 0..3 (FUN_004472d0).
            PlayerColor holder = PlayerColor::NONE;
            for ( const PlayerColor c : { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW } ) {
                for ( const Heroes * hero : world.GetKingdom( c ).GetHeroes() ) {
                    for ( int a = 0; a < 4; ++a ) {
                        const int id = H1::engineArtifactFromH1( a );
                        if ( hero != nullptr && id >= 0 && hero->GetBagArtifacts().isPresentArtifact( Artifact( id ) ) ) {
                            holder = c;
                        }
                    }
                }
            }
            return judge( holder, true );
        }
        case 8: {
            // The dragon city (mine list entry 0, +0x1443d).
            const std::vector<std::array<uint8_t, 10>> tiles = H1::gameTileBytes();
            for ( size_t i = 0; i < tiles.size(); ++i ) {
                if ( ( tiles[i][8] & 0x7f ) == 0x16 && ( tiles[i][8] & 0x80 ) != 0 ) {
                    return judge( world.ColorCapturedObject( static_cast<int32_t>( i ) ), true );
                }
            }
            return 0;
        }
        default:
            return 0;
        }
    }

    bool campaignHasGoal()
    {
        return campaignState.active && campaignState.scenario != 1 && campaignState.scenario != 3;
    }

    fheroes2::GameMode campaignScenarioWon()
    {
        int s = std::clamp( campaignState.scenario, 0, 8 );
        const int days = static_cast<int>( world.CountDay() );
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        if ( s == 8 ) {
            // DAT_004a7338 = 2: videos 4 (win01) and 6 (win02), then the campaign high score (days, name = the lord, table 0x493db4).
            Video::ShowVideo( { { "h1/ANIM/WIN01C.SMK", Video::VideoControl::PLAY_CUTSCENE_WAIT } } );
            // FUN_0043d4d8 (video 6): from frame 0x17 on, every frame gets DAT_004a7c18 drawn with bigfont.fnt by FUN_0047ab10 in the box
            // (0x1d, 0x152, 0x145, 0x73), colour 1, centred (note v170). The text was made by FUN_004216ec before
            // the videos: "My heroes, ..." with %d = DAT_004c0b9c (the day number).
            {
                char text[512];
                std::snprintf( text, sizeof( text ),
                               "My heroes, our foes have been scattered, their castles broken and laid bare.  The great campaign is now complete, and I "
                               "stand before you as the undisputed High King!\n\nOur victory was achieved in %d days!",
                               days );
                fheroes2::Image box( 0x145, 0x73 );
                box.reset();
                // Colour 1 goes through the Windows table 0x492b18 (FUN_0047a5f0) -> 11 of the video's palette. The DOS version passes 1 as
                // it is (FUN_0003e908), but it plays its own DANIM videos; the port plays the Windows ANIM files, so the Windows value fits them
                // (note v170: 11 is white text on these files, 1 is dark red).
                int textColour = 11;
                H1::UI::drawText( box, text, "bigfont.fnt", 0, 0, 0x145, 0x73, textColour, 1 );
                // Frame time from the .smk header (frame rate field: negative = 1/100000 s units, positive = milliseconds).
                int32_t frameMs = 100;
                std::string path;
                if ( Video::getVideoFilePath( "h1/ANIM/WIN02.SMK", path ) ) {
                    std::ifstream f( path, std::ios::binary );
                    uint8_t h[20] = {};
                    if ( f.read( reinterpret_cast<char *>( h ), 20 ) ) {
                        const int32_t rate = static_cast<int32_t>( h[16] | ( h[17] << 8 ) | ( h[18] << 16 ) | ( static_cast<uint32_t>( h[19] ) << 24 ) );
                        frameMs = ( rate < 0 ) ? -rate / 100 : ( rate > 0 ? rate : 100 );
                    }
                }
                const std::vector<Video::Subtitle> overlay{ Video::Subtitle( std::move( box ), static_cast<uint32_t>( frameMs ) * 0x16, 3600000, { 0x1d, 0x152 } ) };
                Video::ShowVideo( { { "h1/ANIM/WIN02.SMK", Video::VideoControl::PLAY_CUTSCENE_WAIT } }, overlay );
            }
            static const char * const lordNames[5] = { "Expert", "Lord Ironfist", "Lord Slayer", "Queen Lamanda", "Lord Alamar" };
            H1::addCampaignHighScore( days, lordNames[std::clamp( campaignState.lord, 0, 4 )] );
            campaignState.active = false;
            return fheroes2::GameMode::MAIN_MENU;
        }
        // FUN_004266c1 for a campaign: congrats.bmp + congrats.bin with the map's text, music 0x36, any key.
        H1::playMusic( 0x36 );
        fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
        const fheroes2::Image bmp = H1::UI::readBmp( "congrats.bmp" );
        fheroes2::Copy( bmp, 0, 0, display, ox, oy, bmp.width(), bmp.height() );
        H1::UI::Layout layout = H1::UI::loadLayout( "congrats.bin" );
        if ( H1::UI::Item * it = layout.find( 100 ) ) {
            it->text = campaignVictory[s];
        }
        H1::UI::drawLayout( display, layout, ox, oy, -1 );
        display.render();
        {
            LocalEvent & le = LocalEvent::Get();
            while ( le.HandleEvents() ) {
                if ( le.isAnyKeyPressed() || le.MouseClickLeft() || le.MouseClickRight() ) {
                    break;
                }
            }
        }
        // Next map: start day = days + 1, map + 1 (the lord's own castle map 4..7 is skipped), count + 1; save "SCENWN%02d".
        campaignState.startDay = days + 1;
        int next = s + 1;
        ++campaignState.count;
        if ( next - 4 == campaignState.lord - 1 ) {
            ++next;
        }
        const fheroes2::GameMode mode = startCampaignScenario( next, false );
        if ( mode != fheroes2::GameMode::START_GAME ) {
            return mode;
        }
        char name[16];
        std::snprintf( name, sizeof( name ), "SCENWN%02d", campaignState.count );
        const std::string path = System::concatPath( Game::GetSaveDir(), std::string( name ) + Game::GetSaveFileExtension() );
        Game::Save( path );
        const std::string question = std::string( "Your campaign has been saved as " ) + name + ".  Would you like to start the next scenario?";
        if ( H1::UI::messageBox( question, H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
            campaignState.active = false;
            return fheroes2::GameMode::MAIN_MENU;
        }
        H1::showCampaignWindow( false );
        return mode;
    }

    fheroes2::GameMode campaignScenarioLost()
    {
        // Video 7, then "Would you like to replay this scenario?" (FUN_0041454b of the same map).
        Video::ShowVideo( { { "h1/ANIM/LOSE1.SMK", Video::VideoControl::PLAY_CUTSCENE_WAIT } } );
        if ( H1::UI::messageBox( "Would you like to replay this scenario?", H1::UI::MESSAGE_YES_NO ) == 0x8005 ) {
            return startCampaignScenario( campaignState.scenario );
        }
        campaignState.active = false;
        return fheroes2::GameMode::MAIN_MENU;
    }

    bool isDroppedAtStart( const PlayerColor color )
    {
        return std::find( droppedAtStart.begin(), droppedAtStart.end(), color ) != droppedAtStart.end();
    }

    const NewGameChoice & lastNewGameChoice()
    {
        return choice;
    }

    bool readOriginalMapHeader( const std::string & fh2mFile, int & mapDifficulty, int & mapSize, std::string * description )
    {
        // .h1t layout: 5184 x 10 tile bytes, 36 x 3 castle list, 5184 placed mask, then 2 bytes: original header bytes 2 and 3.
        const std::streamoff offset = 5184 * 10 + 36 * 3 + 5184;
        std::ifstream f( fh2mFile + ".h1t", std::ios::binary );
        if ( !f ) {
            return false;
        }
        f.seekg( offset );
        char b[2];
        if ( !f.read( b, 2 ) ) {
            return false;
        }
        mapDifficulty = static_cast<uint8_t>( b[0] );
        mapSize = static_cast<uint8_t>( b[1] );
        if ( description != nullptr ) {
            std::getline( f, *description, '\0' );
        }
        return true;
    }

    fheroes2::GameMode runNewGame()
    {
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        const Screen s = makeScreen();
        if ( s.background.empty() ) {
            return fheroes2::GameMode::MAIN_MENU;
        }

        // FUN_0040cad1: stpnewgm.bin at (400, 0x23). Ids: 1 Standard, 2 Campaign, 3 Multi-Player, 0x8001 Cancel.
        H1::UI::Layout typeLayout = H1::UI::loadLayout( "stpnewgm.bin" );
        // H1-INTENDED(D16): an "Extended Game" button in the empty place above Cancel (btnnewgm.icn frames 8 / 9 made by
        // tools/extension/make_ext_buttons.py, file h1/ext/BTNNEWGM.ICN); without that file the dialog stays as the original.
        const bool extensionButton = H1::getIcn( "btnnewgm.icn" ).size() >= 10;
        if ( extensionButton ) {
            H1::UI::Item button;
            button.type = H1::UI::ITEM_BUTTON;
            button.x = 33;
            button.y = 243;
            button.w = 127;
            button.h = 63;
            button.name = "btnnewgm.icn";
            button.frame = 8;
            button.pressedFrame = 9;
            button.hotkey = 0x12; // E
            button.id = 4;
            typeLayout.items.push_back( button );
        }
        while ( true ) {
            const int id = runButtonDialog( s, typeLayout, 400, 0x23,
                                            { helpTextGameType[0], helpTextGameType[1], helpTextGameType[2], helpTextGameType[3],
                                              "A single player game with the extension rules: more creatures, buildings, items and choices.  Not part of the original game." },
                                            { 1, 2, 3, ID_CANCEL, 4 } );
            choice.extension = ( id == 4 ) ? 1 : 0;
            if ( id == ID_CANCEL ) {
                return fheroes2::GameMode::MAIN_MENU;
            }
            if ( id == 2 ) {
                // FUN_0040bde0: stpcmpgn.bin at (400, 0x23): 1..4 the lords, 0x8001 Cancel (note v126).
                const H1::UI::Layout lordLayout = H1::UI::loadLayout( "stpcmpgn.bin" );
                const int lord = runButtonDialog( s, lordLayout, 400, 0x23, { "", "", "", "", "" }, { 1, 2, 3, 4, ID_CANCEL } );
                if ( lord < 1 || lord > 4 ) {
                    // FUN_0040bde0 returns 0 -> FUN_0040cad1 returns 0 -> its callers (FUN_00413dd1 / line ~5325) go back to the main menu
                    // (note v176).
                    return fheroes2::GameMode::MAIN_MENU;
                }
                campaignState = CampaignState();
                campaignState.lord = lord;
                campaignState.count = 0;
                campaignState.startDay = 1;
                return startCampaignScenario( 0 );
            }
            int humans = 1;
            if ( id == 3 ) {
                // FUN_0040c8b9: stpmp.bin at (400, 0x23): 1 Hot Seat, 2 Network, 3 Modem, 4 Direct Connect, Cancel (help 0x493c28).
                const H1::UI::Layout mpLayout = H1::UI::loadLayout( "stpmp.bin" );
                const int mp = runButtonDialog( s, mpLayout, 400, 0x23, { helpTextMultiPlayer[0], helpTextMultiPlayer[1], helpTextMultiPlayer[2], helpTextMultiPlayer[3], helpTextMultiPlayer[4] },
                                                { 1, 2, 3, 4, ID_CANCEL } );
                if ( mp < 1 || mp > 4 ) {
                    return fheroes2::GameMode::MAIN_MENU; // Cancel: FUN_0040c8b9 returns 0x7800 -> main menu (note v176)
                }
                if ( mp != 1 ) {
                    // Owner decision (2026-11-16): only Hot Seat is ported. Project message (not original text) in the HoMM1 box.
                    H1::UI::messageBox( "Network, modem and direct connect games are not part of this port.  Hot Seat is.", H1::UI::MESSAGE_OKAY );
                    continue;
                }
                // FUN_0040c2f6: stphotst.bin at (400, 0x23): 1..3 = 2..4 human players (DAT_004a7bbc), Cancel (help 0x493bf8).
                const H1::UI::Layout hotLayout = H1::UI::loadLayout( "stphotst.bin" );
                const int hot = runButtonDialog( s, hotLayout, 400, 0x23, { helpTextHotSeat[0], helpTextHotSeat[1], helpTextHotSeat[2], helpTextHotSeat[3] },
                                                 { 1, 2, 3, ID_CANCEL } );
                if ( hot < 1 || hot > 3 ) {
                    return fheroes2::GameMode::MAIN_MENU; // Cancel: FUN_0040c2f6 returns 0x7800 -> main menu (note v176)
                }
                humans = hot + 1;
            }
            else if ( id != 1 && id != 4 ) {
                continue;
            }
            // Standard game: one human player (DAT_004a7bbc = 1); Hot Seat: 2..4.
            campaignState.active = false;
            if ( !runSettingsScreen( s, humans ) ) {
                return fheroes2::GameMode::MAIN_MENU;
            }
            return startGame();
        }
    }
}

