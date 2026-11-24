/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "interface_buttons.h"

#include "h1_cpanel.h"
#include "h1_newgame.h"

#include <algorithm>
#include <cassert>

#include "dialog.h"
#include "game_interface.h"
#include "h1_gfx.h"
#include "h1_ui.h"
#include "heroes.h"
#include "icn.h"
#include "interface_base.h"
#include "kingdom.h"
#include "localevent.h"
#include "mp2.h"
#include "route.h"
#include "screen.h"
#include "settings.h"
#include "translations.h"
#include "ui_dialog.h"
#include "world.h"

namespace
{
    // homm1-remaster: Heroes I adventure buttons, adv_wind.bin (note v56). Six 48 x 36 buttons from advbtns.icn at (480, 320); the
    // offsets are from the panel corner. Help texts: table 0x493578 (adventure manager handler, message 0xe with ids 1..6).
    struct H1AdventureButton
    {
        int id;
        int32_t dx;
        int32_t dy;
        int released;
        int pressed;
        const char * help;
    };

    const H1AdventureButton h1Buttons[6] = { { 1, 0, 0, 0, 1, "Next Hero\n\nSelect the next Hero." },
                                             { 2, 48, 0, 2, 3, "Continue Movement\n\nContinue the Hero's movement along his current path." },
                                             { 3, 96, 0, 4, 5, "Kingdom Summary\n\nView a summary of your kingdom." },
                                             { 4, 0, 36, 6, 7, "End Turn\n\nEnd your turn and let the computer take its turn." },
                                             { 5, 48, 36, 8, 9, "Adventure Options\n\nBring up the adventure options menu." },
                                             { 6, 96, 36, 10, 11, "Game Options\n\nBring up the game options menu." } };

    int h1PressedButton = -1;

    fheroes2::Rect h1ButtonRect( const fheroes2::Rect & area, const H1AdventureButton & b )
    {
        return { area.x + b.dx, area.y + b.dy, 48, 36 };
    }
}

void Interface::ButtonsPanel::SavePosition()
{
    Settings & conf = Settings::Get();

    conf.SetPosButtons( GetRect().getPosition() );
    conf.Save( Settings::configFileName );
}

void Interface::ButtonsPanel::setRedraw() const
{
    _interface.setRedraw( REDRAW_BUTTONS );
}

void Interface::ButtonsPanel::SetPos( int32_t x, int32_t y )
{
    BorderWindow::SetPosition( x, y );

    const int icnbtn = Settings::Get().isEvilInterfaceEnabled() ? ICN::ADVEBTNS : ICN::ADVBTNS;

    _buttonNextHero.setICNInfo( icnbtn, 0, 1 );
    _buttonHeroMovement.setICNInfo( icnbtn, 2, 3 );
    _buttonKingdom.setICNInfo( icnbtn, 4, 5 );
    _buttonSpell.setICNInfo( icnbtn, 6, 7 );
    _buttonEndTurn.setICNInfo( icnbtn, 8, 9 );
    _buttonAdventure.setICNInfo( icnbtn, 10, 11 );
    _buttonFile.setICNInfo( icnbtn, 12, 13 );
    _buttonSystem.setICNInfo( icnbtn, 14, 15 );

    _setButtonStatus();

    x = GetArea().x;
    y = GetArea().y;

    // Top row
    _buttonNextHero.setPosition( x, y );
    _nextHeroRect = _buttonNextHero.area();

    _buttonHeroMovement.setPosition( _nextHeroRect.x + _nextHeroRect.width, y );
    _heroMovementRect = _buttonHeroMovement.area();

    _buttonKingdom.setPosition( _heroMovementRect.x + _heroMovementRect.width, y );
    _kingdomRect = _buttonKingdom.area();

    _buttonSpell.setPosition( _kingdomRect.x + _kingdomRect.width, y );
    _spellRect = _buttonSpell.area();

    // Bottom row
    y = _nextHeroRect.y + _nextHeroRect.height;

    _buttonEndTurn.setPosition( x, y );
    _endTurnRect = _buttonEndTurn.area();

    _buttonAdventure.setPosition( _endTurnRect.x + _endTurnRect.width, y );
    _adventureRect = _buttonAdventure.area();

    _buttonFile.setPosition( _adventureRect.x + _adventureRect.width, y );
    _fileRect = _buttonFile.area();

    _buttonSystem.setPosition( _fileRect.x + _fileRect.width, y );
    _systemRect = _buttonSystem.area();
}

void Interface::ButtonsPanel::_redraw()
{
    const Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        if ( !conf.ShowButtons() ) {
            return;
        }

        BorderWindow::Redraw();
    }

    _setButtonStatus();

    if ( H1::isGraphicsMode() ) {
        const std::vector<fheroes2::Sprite> & icn = H1::getIcn( "advbtns.icn" );
        fheroes2::Display & display = fheroes2::Display::instance();
        for ( const H1AdventureButton & b : h1Buttons ) {
            const int frame = ( h1PressedButton == b.id ) ? b.pressed : b.released;
            if ( frame < static_cast<int>( icn.size() ) ) {
                const fheroes2::Rect r = h1ButtonRect( GetArea(), b );
                fheroes2::Blit( icn[frame], display, r.x + icn[frame].x(), r.y + icn[frame].y() );
            }
        }
        return;
    }

    _buttonNextHero.draw();
    _buttonHeroMovement.draw();
    _buttonKingdom.draw();
    _buttonSpell.draw();
    _buttonEndTurn.draw();
    _buttonAdventure.draw();
    _buttonFile.draw();
    _buttonSystem.draw();
}

fheroes2::GameMode Interface::ButtonsPanel::queueEventProcessing()
{
    captureMouse();

    LocalEvent & le = LocalEvent::Get();

    if ( H1::isGraphicsMode() ) {
        // Heroes I: six buttons. The actions are the engine's for now (the engine dialogs behind them are still HoMM2) [temporary].
        // Original actions (FUN_00456759): 1 next hero, 2 continue movement, 3 kingdom summary, 4 end turn (asks "One or more Heroes may
        // still move..." with Yes/No first), 5 adventure options, 6 game options.
        _setButtonStatus();
        int over = -1;
        for ( const H1AdventureButton & b : h1Buttons ) {
            if ( le.isMouseCursorPosInArea( h1ButtonRect( GetArea(), b ) ) ) {
                over = b.id;
            }
        }
        const int pressedNow = ( over != -1 && le.isMouseLeftButtonPressedInArea( h1ButtonRect( GetArea(), h1Buttons[over - 1] ) ) ) ? over : -1;
        if ( pressedNow != h1PressedButton ) {
            h1PressedButton = pressedNow;
            _interface.redraw( REDRAW_BUTTONS );
            fheroes2::Display::instance().render( GetArea() );
        }
        fheroes2::GameMode result = fheroes2::GameMode::CANCEL;
        if ( over == -1 ) {
            return result;
        }
        const fheroes2::Rect rect = h1ButtonRect( GetArea(), h1Buttons[over - 1] );
        if ( le.MouseClickLeft( rect ) ) {
            switch ( over ) {
            case 1:
                if ( _buttonNextHero.isEnabled() ) {
                    _interface.EventNextHero();
                }
                break;
            case 2:
                if ( _buttonHeroMovement.isEnabled() ) {
                    result = _interface.EventHeroMovement();
                }
                break;
            case 3:
                _interface.EventKingdomInfo();
                break;
            case 4:
                result = _interface.EventEndTurn();
                break;
            case 5:
                result = _interface.EventAdventureDialog();
                break;
            case 6:
                // HoMM1 game options (DOS cpanel.bin, note v83). Saving and the scenario information still use engine windows [temporary].
                switch ( H1::runGameOptions() ) {
                case H1::GameOptionsResult::NEW_GAME:
                    result = fheroes2::GameMode::NEW_GAME;
                    break;
                case H1::GameOptionsResult::LOAD_GAME: {
                    // FUN_0040cf57: the requester over the map; Cancel stays in the game (note v160).
                    const std::string file = H1::selectSaveFile( false );
                    if ( !file.empty() ) {
                        H1::setPendingLoad( file );
                        result = fheroes2::GameMode::LOAD_GAME;
                    }
                    break;
                }
                case H1::GameOptionsResult::QUIT:
                    result = fheroes2::GameMode::QUIT_GAME;
                    break;
                case H1::GameOptionsResult::SAVE_GAME:
                    result = _interface.EventSaveGame();
                    break;
                case H1::GameOptionsResult::SCENARIO_INFO:
                    result = _interface.EventScenarioInformation();
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
        else if ( le.isMouseRightButtonPressedInArea( rect ) ) {
            // FUN_00422fc2( text, 4, -1, -1 ) on the adventure screen: x 0x61.
            H1::UI::messageBox( h1Buttons[over - 1].help, H1::UI::MESSAGE_POPUP, 0x61, -1 );
        }
        return result;
    }

    // In the "no interface" mode, the buttons panel may be overlapped by other UI elements, so we can't render the
    // pressed or released buttons exclusively in a usual way. The overlapping UI elements should also be rendered.
    const auto drawOnPressOrRelease = [this, &le]( fheroes2::Button & button, const fheroes2::Rect & buttonRect ) {
        bool shouldRedraw = false;

        if ( le.isMouseLeftButtonPressedInArea( buttonRect ) ) {
            if ( !button.isPressed() && button.press() ) {
                shouldRedraw = true;
            }
        }
        else {
            if ( !button.isReleased() && button.release() ) {
                shouldRedraw = true;
            }
        }

        if ( shouldRedraw ) {
            _interface.redraw( REDRAW_BUTTONS );

            // Render now only the changed part of the screen.
            fheroes2::Display::instance().render( buttonRect );
        }
    };

    drawOnPressOrRelease( _buttonNextHero, _nextHeroRect );
    drawOnPressOrRelease( _buttonHeroMovement, _heroMovementRect );
    drawOnPressOrRelease( _buttonKingdom, _kingdomRect );
    drawOnPressOrRelease( _buttonSpell, _spellRect );
    drawOnPressOrRelease( _buttonEndTurn, _endTurnRect );
    drawOnPressOrRelease( _buttonAdventure, _adventureRect );
    drawOnPressOrRelease( _buttonFile, _fileRect );
    drawOnPressOrRelease( _buttonSystem, _systemRect );

    fheroes2::GameMode res = fheroes2::GameMode::CANCEL;

    // Move the window border.
    if ( Settings::Get().ShowButtons() && BorderWindow::QueueEventProcessing() ) {
        setRedraw();
    }
    else if ( _buttonNextHero.isEnabled() && le.MouseClickLeft( _nextHeroRect ) ) {
        _interface.EventNextHero();
    }
    else if ( _buttonHeroMovement.isEnabled() && le.MouseClickLeft( _heroMovementRect ) ) {
        res = _interface.EventHeroMovement();
    }
    // The 'MouseLongPressLeft()' for '_heroMovementRect' should be called only if current hero
    // has valid path not to trigger the '_mouseButtonLongPressDelay'.
    // The inlined 'isMouseLeftButtonPressedInArea()' method is called not to call 'isValidForMovement()'
    // method every time this button is enabled.
    else if ( _buttonHeroMovement.isEnabled() && le.isMouseLeftButtonPressedInArea( _heroMovementRect ) &&
              []() {
                  const Heroes * currentHero = GetFocusHeroes();
                  assert( currentHero != nullptr );

                  return currentHero->GetPath().isValidForMovement();
              }()
              && le.MouseLongPressLeft( _heroMovementRect ) ) {
        _interface.EventResetHeroPath();
    }
    else if ( le.MouseClickLeft( _kingdomRect ) ) {
        _interface.EventKingdomInfo();
    }
    else if ( _buttonSpell.isEnabled() && le.MouseClickLeft( _spellRect ) ) {
        _interface.EventCastSpell();
    }
    else if ( le.MouseClickLeft( _endTurnRect ) ) {
        res = _interface.EventEndTurn();
    }
    else if ( le.MouseClickLeft( _adventureRect ) ) {
        res = _interface.EventAdventureDialog();
    }
    else if ( le.MouseClickLeft( _fileRect ) ) {
        res = _interface.EventFileDialog();
    }
    else if ( le.MouseClickLeft( _systemRect ) ) {
        _interface.EventSystemDialog();
    }
    else if ( le.isMouseRightButtonPressedInArea( _nextHeroRect ) ) {
        fheroes2::showStandardTextMessage( _( "Next Hero" ), _( "Select the next Hero." ), Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _heroMovementRect ) ) {
        fheroes2::showStandardTextMessage(
            _( "Hero Movement" ),
            _( "Start the Hero's movement along the current path or re-visit the object occupied by the Hero. Press and hold this button to reset the Hero's path." ),
            Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _kingdomRect ) ) {
        fheroes2::showStandardTextMessage( _( "Kingdom Summary" ), _( "View a summary of your Kingdom." ), Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _spellRect ) ) {
        fheroes2::showStandardTextMessage( _( "Cast Spell" ), _( "Cast an adventure spell." ), Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _endTurnRect ) ) {
        fheroes2::showStandardTextMessage( _( "End Turn" ), _( "End your turn and let the computer take its turn." ), Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _adventureRect ) ) {
        fheroes2::showStandardTextMessage( _( "Adventure Options" ), _( "Bring up the adventure options menu." ), Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _fileRect ) ) {
        fheroes2::showStandardTextMessage( _( "File Options" ), _( "Bring up the file options menu, allowing you to load, save, start a new game or quit." ),
                                           Dialog::ZERO );
    }
    else if ( le.isMouseRightButtonPressedInArea( _systemRect ) ) {
        fheroes2::showStandardTextMessage( _( "System Options" ), _( "Bring up the system options menu, allowing you to customize your game." ), Dialog::ZERO );
    }

    return res;
}

void Interface::ButtonsPanel::_setButtonStatus()
{
    Heroes * currentHero = GetFocusHeroes();

    if ( currentHero && currentHero->GetPath().isValidForMovement() && currentHero->MayStillMove( false, true ) ) {
        // Move button: to move the hero by the specified path.
        _buttonHeroMovement.setICNIndexes( 2, 3 );
        _buttonHeroMovement.enable();
    }
    else if ( currentHero && MP2::isRevisitAllowedForObject( currentHero->getObjectTypeUnderHero(), currentHero->isShipMaster() ) ) {
        // Action button: to visit the action object hero is standing on.
        _buttonHeroMovement.setICNIndexes( 16, 17 );
        _buttonHeroMovement.enable();
    }
    else if ( currentHero && currentHero->GetPath().isValidForMovement() ) {
        // Inactive button: to show the hero's path can be reset.
        _buttonHeroMovement.setICNIndexes( 18, 19 );
        _buttonHeroMovement.enable();
    }
    else {
        // Inactive button (disabled): to show that the hero as no path specified.
        _buttonHeroMovement.setICNIndexes( 18, 19 );
        _buttonHeroMovement.disable();
    }

    if ( currentHero && currentHero->HaveSpellBook() && currentHero->MayCastAdventureSpells() ) {
        _buttonSpell.enable();
    }
    else {
        _buttonSpell.disable();
    }

    const Kingdom & kingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    const VecHeroes & heroes = kingdom.GetHeroes();

    const bool isMovableHeroPresent = std::any_of( heroes.begin(), heroes.end(), []( const Heroes * hero ) {
        assert( hero != nullptr );

        return hero->MayStillMove( false, false );
    } );

    if ( isMovableHeroPresent ) {
        _buttonNextHero.enable();
    }
    else {
        _buttonNextHero.disable();
    }
}
