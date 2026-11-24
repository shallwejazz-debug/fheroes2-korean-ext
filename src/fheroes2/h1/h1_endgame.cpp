// Heroes I end of game screens (homm1-remaster project), note v124.
#include "h1_endgame.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "cursor.h"
#include "game_video.h"
#include "h1_battle.h"
#include "h1_gfx.h"
#include "h1_newgame.h"
#include "h1_sound.h"
#include "h1_ui.h"
#include "localevent.h"
#include "screen.h"
#include "settings.h"
#include "world.h"

namespace
{
    namespace UI = H1::UI;

    // One high score record (0x57 bytes): name (+0, 0x11), map or campaign name (+0x11, 0xf), score or days (+0x20, int, -1 = empty).
    struct Record
    {
        std::string name;
        std::string map;
        int32_t score = -1;
    };

    std::string fileName( const bool campaign )
    {
        // "%sSTANDARD.HS" / "%sCAMPAIGN.HS" with the DATA folder; here the Heroes I data folder next to HEROES.AGG.
        return campaign ? "h1/CAMPAIGN.HS" : "h1/STANDARD.HS";
    }

    std::array<Record, 10> readRecords( const bool campaign )
    {
        std::array<Record, 10> records;
        std::ifstream in( fileName( campaign ), std::ios::binary );
        if ( !in ) {
            return records;
        }
        std::vector<char> data( 10 * 0x57, 0 );
        in.read( data.data(), static_cast<std::streamsize>( data.size() ) );
        for ( size_t i = 0; i < 10; ++i ) {
            const char * r = data.data() + i * 0x57;
            records[i].name.assign( r, strnlen( r, 0x11 ) );
            records[i].map.assign( r + 0x11, strnlen( r + 0x11, 0xf ) );
            std::memcpy( &records[i].score, r + 0x20, 4 );
        }
        return records;
    }

    void writeRecords( const bool campaign, const std::array<Record, 10> & records )
    {
        std::vector<char> data( 10 * 0x57, 0 );
        for ( size_t i = 0; i < 10; ++i ) {
            char * r = data.data() + i * 0x57;
            std::memcpy( r, records[i].name.data(), std::min<size_t>( records[i].name.size(), 0x10 ) );
            std::memcpy( r + 0x11, records[i].map.data(), std::min<size_t>( records[i].map.size(), 0xe ) );
            std::memcpy( r + 0x20, &records[i].score, 4 );
        }
        std::ofstream out( fileName( campaign ), std::ios::binary | std::ios::trunc );
        out.write( data.data(), static_cast<std::streamsize>( data.size() ) );
    }

    // FUN_0042587f: the slot of a new score (-1 = not good enough); fills the name with the entry window and writes the file.
    int addRecord( const int score, const bool standard, const std::string & map )
    {
        std::array<Record, 10> records = readRecords( !standard );
        int slot = -1;
        for ( int i = 0; i < 10; ++i ) {
            if ( ( standard && records[i].score <= score ) || ( !standard && score <= records[i].score ) || records[i].score == -1 ) {
                slot = i;
                break;
            }
        }
        if ( slot < 0 ) {
            return -1;
        }
        for ( int i = 8; i >= slot; --i ) {
            records[i + 1] = records[i];
        }
        records[slot].name = H1::textEntry( "Please enter your name for the high score list.", 0x10 );
        records[slot].map = map;
        records[slot].score = score;
        writeRecords( !standard, records );
        return slot;
    }

    // FUN_00426a7f: wait for a key or a mouse button.
    void waitAnyInput()
    {
        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            if ( le.isAnyKeyPressed() || le.MouseClickLeft() || le.MouseClickRight() ) {
                break;
            }
        }
    }

    void showHighScoresImpl( bool campaign, const int highlight, const bool highlightCampaign )
    {
        UI::Layout layout = UI::loadLayout( "hiscore.bin" );
        if ( layout.width == 0 ) {
            return;
        }
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        std::array<int, 10> anim{};
        std::array<int, 10> creature{};
        std::array<bool, 10> filled{};

        // FUN_004663ae: fill the list for the standard (campaign = false) or the campaign file.
        auto fill = [&]() {
            const std::array<Record, 10> records = readRecords( campaign );
            if ( UI::Item * it = layout.find( 0x67 ) ) {
                it->frame = campaign ? 0xc : 7;
            }
            if ( UI::Item * it = layout.find( 0x68 ) ) {
                it->frame = campaign ? 8 : 9;
            }
            if ( UI::Item * it = layout.find( 100 ) ) {
                it->hidden = !campaign;
            }
            if ( UI::Item * it = layout.find( 0x93 ) ) {
                it->hidden = campaign;
            }
            for ( int i = 0; i < 10; ++i ) {
                const Record & r = records[i];
                filled[i] = r.score != -1;
                creature[i] = filled[i] ? H1::rankingCreature( r.score, !campaign ) : 0;
                if ( filled[i] ) {
                    anim[i] = ( anim[i] + 1 ) % 0x12;
                }
                if ( UI::Item * it = layout.find( 0xc9 + i ) ) {
                    it->hidden = !filled[i];
                    it->frame = creature[i] * 7 + anim[i] / 3;
                }
                std::string title = filled[i] ? H1::creatureNameOne( creature[i] ) : "";
                if ( !title.empty() ) {
                    title[0] = static_cast<char>( title[0] - 0x20 );
                }
                const std::string texts[4] = { filled[i] ? r.name : "", filled[i] ? r.map : "", filled[i] ? std::to_string( r.score ) : "", title };
                // The new entry in colour 0xbf on its own list.
                const bool marked = ( i == highlight && campaign == highlightCampaign );
                for ( int k = 0; k < 4; ++k ) {
                    if ( UI::Item * it = layout.find( 0x6a + i * 4 + k ) ) {
                        it->text = texts[k];
                        it->colour = marked ? 0xbf : 1;
                    }
                }
            }
        };
        fill();

        int pressed = -1;
        auto draw = [&]() {
            fheroes2::Image frame( display.width(), display.height() );
            fheroes2::Copy( display, frame );
            const fheroes2::Image bmp = UI::readBmp( "hiscore.bmp" );
            fheroes2::Copy( bmp, 0, 0, frame, ox, oy, bmp.width(), bmp.height() );
            UI::drawLayout( frame, layout, ox, oy, pressed );
            fheroes2::Copy( frame, display );
            display.render();
        };
        auto buttonAt = [&]( const fheroes2::Point & p ) {
            for ( const UI::Item & it : layout.items ) {
                if ( it.type == UI::ITEM_BUTTON && !it.hidden && p.x >= ox + it.x && p.y >= oy + it.y && p.x < ox + it.x + it.w && p.y < oy + it.y + it.h ) {
                    return it.id;
                }
            }
            return -1;
        };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        draw();
        using Clock = std::chrono::steady_clock;
        Clock::time_point next = Clock::now() + std::chrono::milliseconds( 0x78 );
        while ( le.HandleEvents() ) {
            const int over = buttonAt( le.getMouseCursorPos() );
            const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
            bool redraw = false;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                redraw = true;
            }
            if ( le.isKeyPressed( fheroes2::Key::KEY_TAB ) ) {
                break; // Exit hotkey 15 (Tab)
            }
            if ( over != -1 && le.MouseClickLeft() ) {
                if ( over == 0x8000 ) {
                    break;
                }
                if ( over == 100 || over == 0x93 ) {
                    campaign = !campaign;
                    fill();
                    redraw = true;
                }
            }
            // FUN_00466145: every 0x78 ms the creature pictures take their next frame (frame = creature x 7 + step / 3, 18 steps).
            if ( Clock::now() >= next ) {
                next = Clock::now() + std::chrono::milliseconds( 0x78 );
                for ( int i = 0; i < 10; ++i ) {
                    anim[i] = ( anim[i] + 1 ) % 0x12;
                    if ( UI::Item * it = layout.find( 0xc9 + i ) ) {
                        it->frame = creature[i] * 7 + anim[i] / 3;
                    }
                }
                redraw = true;
            }
            if ( redraw ) {
                draw();
            }
        }
    }
}

namespace H1
{
    int baseScore( const int days )
    {
        int score = 200;
        if ( days <= 0x3c ) {
            score -= days;
        }
        else {
            score -= 0x3c;
            if ( days <= 0x78 ) {
                score = static_cast<int>( score - ( days - 0x3c ) * 0.5 );
            }
            else {
                score = static_cast<int>( score - 30.0 );
                if ( days <= 0x168 ) {
                    score = static_cast<int>( score - ( days - 0x78 ) * 0.25 );
                }
                else {
                    score = static_cast<int>( score - 60.0 );
                    score = static_cast<int>( score - ( days - 0x168 ) * 0.125 );
                }
            }
        }
        return std::max( score, 0x14 );
    }

    int rankingCreature( const int score, const bool standard )
    {
        // Tables 0x492570 (standard: threshold, creature) and 0x4925e0 (campaign days), searched from entry 27 down.
        static const int standardThreshold[28] = { 0, 7, 14, 21, 28, 35, 42, 49, 56, 63, 70, 77, 84, 91, 98, 105, 112, 119, 126, 133, 140, 147, 154, 161, 168, 175, 182, 189 };
        static const int standardCreature[28] = { 0, 6, 12, 18, 24, 7, 1, 19, 13, 2, 8, 25, 14, 20, 3, 9, 15, 21, 4, 26, 16, 10, 22, 5, 27, 11, 17, 23 };
        static const int campaignThreshold[28] = { 3600, 3400, 3200, 3000, 2600, 2400, 2200, 2000, 1800, 1600, 1500, 1400, 1300, 1200,
                                                   1100, 1000, 900,  800,  750,  700,  650,  600,  550,  500,  450,  400,  350,  300 };
        static const int campaignCreature[28] = { 0, 6, 12, 18, 24, 7, 1, 19, 13, 2, 8, 25, 14, 20, 3, 9, 15, 21, 4, 26, 16, 10, 22, 5, 27, 11, 17, 23 };
        for ( int i = 27; i >= 0; --i ) {
            if ( standard ? ( standardThreshold[i] <= score ) : ( score <= campaignThreshold[i] ) ) {
                return standard ? standardCreature[i] : campaignCreature[i];
            }
        }
        return standardCreature[0];
    }

    std::string textEntry( const std::string & prompt, const int maxLength )
    {
        UI::Layout layout = UI::loadLayout( "dataentr.bin" );
        H1::UI::TextEdit edit; // FUN_00426d00 starts typing in field 10 at once; a type 0x100 field starts empty (note v172)
        std::string & text = edit.text;
        if ( layout.width == 0 ) {
            return text;
        }
        edit.start();
        if ( UI::Item * it = layout.find( 1 ) ) {
            it->text = prompt;
        }
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        const int32_t ox = ( display.width() - 640 ) / 2 + 0xb1;
        const int32_t oy = ( display.height() - 480 ) / 2 + 0x14;
        const UI::Item * ok = layout.find( 0x8002 );
        int pressed = -1;
        auto draw = [&]() {
            if ( UI::Item * it = layout.find( 10 ) ) {
                it->text = edit.shown();
            }
            fheroes2::Image frame = saved;
            UI::drawLayout( frame, layout, ox, oy, pressed );
            fheroes2::Copy( frame, display );
            display.render();
        };
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        draw();
        while ( le.HandleEvents() ) {
            const fheroes2::Point m = le.getMouseCursorPos();
            const bool overOk = ok != nullptr && m.x >= ox + ok->x && m.y >= oy + ok->y && m.x < ox + ok->x + ok->w && m.y < oy + ok->y + ok->h;
            const int newPressed = ( overOk && le.isMouseLeftButtonPressed() ) ? 0x8002 : -1;
            bool redraw = false;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                redraw = true;
            }
            if ( overOk && le.MouseClickLeft() ) {
                break;
            }
            if ( le.isAnyKeyPressed() ) {
                const UI::Item * field = layout.find( 10 );
                if ( field != nullptr && edit.key( static_cast<int>( le.getPressedKeyValue() ), LocalEvent::getCurrentKeyModifiers(), *field ) != 0 ) {
                    break; // Enter (OKAY hotkey 28) or Esc (text from before = empty)
                }
                redraw = true;
            }
            if ( redraw ) {
                draw();
            }
        }
        fheroes2::Copy( saved, display );
        display.render();
        // strncpy with DAT_004a9d20 - 1.
        if ( maxLength > 0 && static_cast<int>( text.size() ) > maxLength - 1 ) {
            text.resize( static_cast<size_t>( maxLength - 1 ) );
        }
        return text;
    }

    void showHighScores( const bool campaign )
    {
        H1::playMusic( 0x30 );
        showHighScoresImpl( campaign, -1, false );
    }

    bool runStandardVictory()
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        // FUN_004266c1: base score from the days (DAT_004c0b9c), final = rating (world +0) x base / 100, ranking creature, music 0x36.
        const int days = static_cast<int>( world.CountDay() );
        const int base = baseScore( days );
        const int rating = lastNewGameChoice().rating;
        const int final = rating * base / 100;
        H1::playMusic( 0x36 );

        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
        const fheroes2::Image bmp = UI::readBmp( "congrats.bmp" );
        fheroes2::Copy( bmp, 0, 0, display, ox, oy, bmp.width(), bmp.height() );
        UI::Layout layout = UI::loadLayout( "congspre.bin" );
        static const char * const labels[5] = { "Days Spent:", "Base Score:", "Difficulty Rating:", "Final Score:", "Ranking:" };
        auto set = [&layout]( const int id, const std::string & text ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->text = text;
            }
        };
        set( 100, "A Glorious Victory!" );
        for ( int i = 0; i < 5; ++i ) {
            set( 0x65 + i, labels[i] );
        }
        set( 0x6a, std::to_string( days ) );
        set( 0x6b, std::to_string( base ) );
        set( 0x6c, std::to_string( rating ) + "%" );
        set( 0x6d, std::to_string( final ) );
        set( 0x6e, H1::creatureNameOne( rankingCreature( final, true ) ) );
        UI::drawLayout( display, layout, ox, oy, -1 );
        display.render();
        {
            const CursorRestorer cursorRestorer( true, Cursor::POINTER );
            waitAnyInput();
        }
        // FUN_0042587f( final, 1, ..., map name +0x15 ), then the list with music 0x30.
        const int slot = addRecord( final, true, Settings::Get().getCurrentMapInfo().name );
        H1::playMusic( 0x30 );
        fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
        showHighScoresImpl( false, slot, false );
        return true;
    }

    void addCampaignHighScore( const int days, const std::string & lord )
    {
        H1::playMusic( 0x36 );
        const int slot = addRecord( days, false, lord );
        H1::playMusic( 0x30 );
        fheroes2::Display & display = fheroes2::Display::instance();
        fheroes2::Fill( display, 0, 0, display.width(), display.height(), 0 );
        showHighScoresImpl( true, slot, true );
    }

    bool runLoss()
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        AudioManager::ResetAudio();
        H1::playMusic( -1 );
        // FUN_0043dde6(7): lose1.smk (local_40[0] / local_3c[0] = 7 for both video settings).
        Video::ShowVideo( { { "h1/ANIM/LOSE1.SMK", Video::VideoControl::PLAY_CUTSCENE_WAIT } } );
        return true;
    }
}
