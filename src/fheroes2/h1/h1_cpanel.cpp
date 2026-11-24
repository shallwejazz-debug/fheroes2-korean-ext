// Heroes I game options window (homm1-remaster project). See h1_cpanel.h.
#include "h1_cpanel.h"

#include <algorithm>
#include <string>

#include "audio.h"
#include "h1_ui.h"
#include "screen.h"
#include "settings.h"

namespace
{
    // H1-INTENDED(D6): the DOS options window (Auto Save, no Windows Sound Quality) (docs/INTENDED_DIFFERENCES.md).
    bool autoSave = true;
    bool showPath = true;
    int lastAiSpeed = 0;

    // Text table 0x4935d0 (DOS strings at 0x105xxx): value 0 Off, 1 On (full), 2..10 "On\nVolume 9" .. "On\nVolume 1".
    const char * const volumeText[11] = { "Off", "On", "On\nVolume 9", "On\nVolume 8", "On\nVolume 7", "On\nVolume 6", "On\nVolume 5", "On\nVolume 4",
                                          "On\nVolume 3", "On\nVolume 2", "On\nVolume 1" };
    // Table 0x493600.
    const char * const speedText[5] = { "Walk", "Trot", "Canter", "Gallop", "Jump" };

    // Added item (owner decision 2026-11-15: combat speed as a basic option): text between OKAY and INFO, click to change.
    constexpr int ID_COMBAT_SPEED = 0x40;

    // DOS help table (strings after "Wrathmont"/"Vesper"), by the item -> index switch of FUN_004609b0.
    const char * help( const int id )
    {
        switch ( id ) {
        case 1:
            return "Start a single or multi-player game.";
        case 2:
            return "Load a previously saved game.";
        case 3:
            return "Save the current game.";
        case 4:
            return "Quit Heroes of Might and Magic and return to the DOS prompt.";
        case 5:
            return "Toggle ambient music on/off";
        case 6:
            return "Toggle foreground sounds on/off";
        case 7:
            return "Change the speed at which Heroes move on the main screen.";
        case 11:
            return "Toggle 'Autosave' on/off.  'Autosave' saves your game automatically at the end of each turn to a special file, called 'AUTOSAVE'.";
        case 12:
            return "Toggle 'Show Path' on/off.  If 'Show Path' is on, your first click on a map location will show the path to get there, your second "
                   "will start you moving. If this option is off, one click starts you moving immediately.";
        case 13:
            return "Toggle 'Show Enemy Moves' on/off.  If on, all enemies moving within your visible area will be shown.  If off, no computer movement "
                   "will be shown.  Note that this option is automatically set to off during network and modem play.";
        case 0x11:
            return "View information on the scenario you are currently playing.";
        case ID_COMBAT_SPEED:
            // [added item and text, not in the original]
            return "Change the speed of combat animations: 1x is the original speed.";
        case 0x8000:
            return "Exit this menu without doing anything.";
        default:
            return nullptr;
        }
    }

    // Original volume value v (0 off, 1 full, k = volume 11 - k) <-> engine volume 0..10 [the engine scale stands in].
    int toH1Volume( const int engine )
    {
        return engine <= 0 ? 0 : 11 - std::min( engine, 10 );
    }

    int toEngineVolume( const int h1 )
    {
        return h1 <= 0 ? 0 : 11 - h1;
    }

    // Speed 0..4 (Walk .. Jump) <-> engine hero speed 1..10 (10 = no walking): 2, 4, 6, 8, 10 [temporary: original step timing not compared].
    int toH1Speed( const int engine )
    {
        return std::clamp( ( engine - 1 ) / 2, 0, 4 );
    }

    int toEngineSpeed( const int h1 )
    {
        return ( h1 + 1 ) * 2;
    }

    void refresh( H1::UI::Layout & layout )
    {
        // FUN_00460557 with the DOS items.
        const Settings & conf = Settings::Get();
        const int music = toH1Volume( conf.MusicVolume() );
        const int effects = toH1Volume( conf.SoundVolume() );
        const int speed = toH1Speed( conf.HeroesMoveSpeed() );
        const int hideEnemy = ( conf.AIMoveSpeed() == 0 ) ? 1 : 0;
        auto frame = [&layout]( const int id, const int f ) {
            if ( H1::UI::Item * it = layout.find( id ) ) {
                it->frame = f;
            }
        };
        auto text = [&layout]( const int id, const char * s ) {
            if ( H1::UI::Item * it = layout.find( id ) ) {
                it->text = s;
            }
        };
        frame( 5, music == 0 ? 10 : 11 );
        frame( 6, effects == 0 ? 12 : 13 );
        frame( 7, 0xe + speed );
        // DOS cpanel.icn frames 19 / 20 = Auto Save off / on (disk with / without the sign) [frame order read from the pictures].
        frame( 11, 0x13 + ( autoSave ? 1 : 0 ) );
        frame( 12, 0x15 + ( showPath ? 1 : 0 ) );
        frame( 13, 0x18 - hideEnemy );
        text( 8, volumeText[music] );
        text( 9, volumeText[effects] );
        text( 10, speedText[speed] );
        text( 14, volumeText[autoSave ? 1 : 0] );
        text( 15, volumeText[showPath ? 1 : 0] );
        text( 16, volumeText[1 - hideEnemy] );
        if ( H1::UI::Item * it = layout.find( ID_COMBAT_SPEED ) ) {
            it->text = "Combat\nSpeed " + std::to_string( H1::combatSpeedMultiplier() ) + "x";
        }
    }
}

namespace H1
{
    bool isAutoSaveOn()
    {
        return autoSave;
    }

    bool isShowPathOn()
    {
        return showPath;
    }

    // H1-INTENDED(D2): combat speed option in the DOS options window (docs/INTENDED_DIFFERENCES.md). Do not change without the owner.
    int combatSpeedMultiplier()
    {
        switch ( Settings::Get().BattleSpeed() ) {
        case 2:
            return 2;
        case 3:
            return 4;
        default:
            return 1;
        }
    }

    GameOptionsResult runGameOptions()
    {
        UI::Layout layout = UI::loadLayout( "cpanel.bin" );
        UI::Item speedItem;
        speedItem.type = UI::ITEM_TEXT;
        speedItem.x = 0x78;
        speedItem.y = 0x197;
        speedItem.w = 0x51;
        speedItem.h = 0x1b;
        speedItem.name = "smalfont.fnt";
        speedItem.colour = 1;
        speedItem.align = 1;
        speedItem.id = ID_COMBAT_SPEED;
        layout.items.push_back( speedItem );
        layout.clickableIcons = { 5, 6, 7, 11, 12, 13, ID_COMBAT_SPEED };
        Settings & conf = Settings::Get();
        bool changed = false;

        while ( true ) {
            refresh( layout );
            const int id = UI::runLayoutDialog( layout, 0xa0, 10, help, 0xb1 );
            switch ( id ) {
            case 5:
                conf.SetMusicVolume( toEngineVolume( ( toH1Volume( conf.MusicVolume() ) + 1 ) % 11 ) );
                Music::setVolume( 100 * conf.MusicVolume() / 10 );
                changed = true;
                continue;
            case 6:
                conf.SetSoundVolume( toEngineVolume( ( toH1Volume( conf.SoundVolume() ) + 1 ) % 11 ) );
                Mixer::setVolume( 100 * conf.SoundVolume() / 10 );
                changed = true;
                continue;
            case 7:
                conf.SetHeroesMoveSpeed( toEngineSpeed( ( toH1Speed( conf.HeroesMoveSpeed() ) + 1 ) % 5 ) );
                changed = true;
                continue;
            case ID_COMBAT_SPEED: {
                const int next = ( combatSpeedMultiplier() == 1 ) ? 2 : ( combatSpeedMultiplier() == 2 ? 3 : 1 );
                conf.SetBattleSpeed( next );
                changed = true;
                continue;
            }
            case 11:
                autoSave = !autoSave;
                continue;
            case 12:
                showPath = !showPath;
                continue;
            case 13:
                // View Enemy Movement: the engine's computer speed 0 hides computer moves; turning it on again gives back the last speed.
                if ( conf.AIMoveSpeed() == 0 ) {
                    conf.SetAIMoveSpeed( lastAiSpeed > 0 ? lastAiSpeed : 5 );
                }
                else {
                    lastAiSpeed = conf.AIMoveSpeed();
                    conf.SetAIMoveSpeed( 0 );
                }
                changed = true;
                continue;
            default:
                break;
            }
            if ( changed ) {
                conf.Save( Settings::configFileName );
            }
            const char * question = nullptr;
            switch ( id ) {
            case 1:
                question = "Are you sure you want to restart?  (Your current game will be lost)";
                break;
            case 2:
                question = "Are you sure you want to load a new game?  (Your current game will be lost)";
                break;
            case 4:
                question = "Are you sure you want to quit?  (Your current game will be lost)";
                break;
            case 3:
                return GameOptionsResult::SAVE_GAME;
            case 0x11:
                return GameOptionsResult::SCENARIO_INFO;
            default:
                return GameOptionsResult::NONE;
            }
            // Yes / No at (0xb1, 0x50) over the still open window; No keeps the window open.
            fheroes2::Display & display = fheroes2::Display::instance();
            UI::drawLayout( display, layout, ( display.width() - 640 ) / 2 + 0xa0, ( display.height() - 480 ) / 2 + 10, -1 );
            if ( UI::messageBox( question, UI::MESSAGE_YES_NO, 0xb1, 0x50 ) == 0x8005 ) {
                return id == 1 ? GameOptionsResult::NEW_GAME : ( id == 2 ? GameOptionsResult::LOAD_GAME : GameOptionsResult::QUIT );
            }
        }
    }
}
