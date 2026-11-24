/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2012 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "artifact.h"
#include "artifact_ultimate.h"
#include "audio.h"
#include "audio_manager.h"
#include "castle.h"
#include "dialog.h"
#include "dialog_system_options.h"
#include "direction.h"
#include "game.h"
#include "game_delays.h"
#include "game_interface.h" // IWYU pragma: associated
#include "game_io.h"
#include "game_mode.h"
#include "game_over.h"
#include "heroes.h"
#include "h1_newgame.h"
#include "h1_smallwin.h"
#include "h1_sound.h" // homm1-remaster
#include "image.h"
#include "interface_base.h"
#include "h1_gfx.h"
#include "h1_ui.h"
#include "interface_buttons.h"
#include "interface_gamearea.h"
#include "interface_icons.h"
#include "interface_radar.h"
#include "interface_status.h"
#include "kingdom.h"
#include "logging.h"
#include "m82.h"
#include "maps.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "mus.h"
#include "players.h"
#include "puzzle.h"
#include "route.h"
#include "screen.h"
#include "settings.h"
#include "spell.h"
#include "spell_book.h"
#include "system.h"
#include "tools.h"
#include "translations.h"
#include "ui_dialog.h"
#include "ui_tool.h"
#include "view_world.h"
#include "h1_cpanel.h"
#include "h1_ai.h"
#include "h1_mode.h"
#include "world.h"

void Interface::AdventureMap::ShowPathOrStartMoveHero( Heroes * hero, const int32_t destinationIdx )
{
    if ( hero == nullptr || !Maps::isValidAbsIndex( destinationIdx ) ) {
        return;
    }

    assert( !hero->Modes( Heroes::ENABLEMOVE ) );

    const Route::Path & path = hero->GetPath();

    // Calculate and show the hero's path
    if ( path.GetDestinationIndex() != destinationIdx ) {
        hero->calculatePath( destinationIdx );

        // homm1-remaster: HoMM1 "Show Path" off - one click starts the hero moving at once (note v83).
        if ( H1::isH1Mode() && !H1::isShowPathOn() && path.isValidForMovement() && hero->MayStillMove( false, true ) ) {
            _startHeroMove( *hero );
            return;
        }

        DEBUG_LOG( DBG_GAME, DBG_TRACE, hero->GetName() << ", distance: " << world.getDistance( *hero, destinationIdx ) << ", route: " << path.String() )

        _gameArea.SetRedraw();
        _buttonsPanel.setRedraw();
    }
    // Start the hero's movement
    else if ( path.isValidForMovement() && hero->MayStillMove( false, true ) ) {
        _startHeroMove( *hero );
    }
}

void Interface::AdventureMap::MoveHeroFromArrowKeys( Heroes & hero, const int direction )
{
    const int32_t heroIndex = hero.GetIndex();

    if ( !Maps::isValidDirection( heroIndex, direction ) ) {
        return;
    }

    const int32_t dstIndex = Maps::GetDirectionIndex( heroIndex, direction );
    const Maps::Tile & tile = world.getTile( dstIndex );

    if ( !tile.isPassableFrom( Direction::CENTER, hero.isShipMaster(), false, hero.GetColor() ) ) {
        return;
    }

    ShowPathOrStartMoveHero( &hero, dstIndex );
}

void Interface::AdventureMap::_startHeroMove( Heroes & hero )
{
    SetFocus( &hero, true );
    RedrawFocus();

    hero.SetMove( true );

    // We pass this delay to start hero moving immediately and set all the variables needed to handle game events correctly
    // and to stop handling mouse click events until hero stops. Otherwise there could be a rare case
    // when double click is faster than this delay and the second click will also be handled which should not happen.
    Game::passAnimationDelay( Game::DelayType::CURRENT_HERO_DELAY );
}

void Interface::AdventureMap::EventSwitchFocusedHero( const int32_t tileIndex )
{
    Heroes * selectedHero = world.getTile( tileIndex ).getHero();
    if ( selectedHero == nullptr || selectedHero == GetFocusHeroes() || selectedHero->GetColor() != Settings::Get().GetPlayers().getCurrentColor() ) {
        return;
    }
    SetFocus( selectedHero, false );
    RedrawFocus();
}

void Interface::AdventureMap::EventNextHero()
{
    const Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    const VecHeroes & myHeroes = myKingdom.GetHeroes();

    if ( myHeroes.empty() ) {
        return;
    }

    if ( GetFocusHeroes() ) {
        VecHeroes::const_iterator it = std::find( myHeroes.begin(), myHeroes.end(), GetFocusHeroes() );
        VecHeroes::const_iterator currentHero = it;

        do {
            ++it;

            if ( it == myHeroes.end() ) {
                it = myHeroes.begin();
            }

            if ( ( *it )->MayStillMove( true, false ) ) {
                SetFocus( *it, false );
                break;
            }
        } while ( it != currentHero );
    }
    else {
        for ( Heroes * hero : myHeroes ) {
            if ( hero->MayStillMove( true, false ) ) {
                SetFocus( hero, false );
                break;
            }
        }
    }

    RedrawFocus();
}

fheroes2::GameMode Interface::AdventureMap::EventHeroMovement()
{
    if ( Heroes * hero = GetFocusHeroes(); hero ) {
        if ( hero->GetPath().isValidForMovement() && hero->MayStillMove( false, true ) ) {
            _startHeroMove( *hero );
        }
        else if ( MP2::isRevisitAllowedForObject( hero->getObjectTypeUnderHero(), hero->isShipMaster() ) ) {
            return EventDefaultAction();
        }
        else if ( hero->GetPath().isValidForMovement() ) {
            fheroes2::showStandardTextMessage( _( "Hero Movement Points" ),
                                               _( "The hero's army cannot move anymore today. The movement points will be refilled tomorrow after you end your turn." ),
                                               Dialog::OK );
        }
    }

    return fheroes2::GameMode::CANCEL;
}

void Interface::AdventureMap::EventResetHeroPath()
{
    Heroes * hero = GetFocusHeroes();
    if ( hero == nullptr ) {
        return;
    }

    hero->GetPath().Reset();

    setRedraw( REDRAW_GAMEAREA | REDRAW_BUTTONS );
}

void Interface::AdventureMap::EventKingdomInfo() const
{
    Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    myKingdom.openOverviewDialog();

    _iconsPanel.setRedraw();
}

void Interface::AdventureMap::EventCastSpell()
{
    Heroes * hero = GetFocusHeroes();
    if ( hero == nullptr ) {
        return;
    }

    // Center on the hero before opening the spell book
    _gameArea.SetCenter( hero->GetCenter() );
    redraw( REDRAW_GAMEAREA | REDRAW_RADAR_CURSOR );

    const Spell spell = hero->OpenSpellBook( SpellBook::Filter::ADVN, true, false, {} );
    if ( !spell.isValid() ) {
        return;
    }

    hero->ActionSpellCast( spell );

    // The spell will consume the hero's spell points (and perhaps also movement points) and can move the
    // hero to another location, so we may have to update the terrain music theme and environment sounds
    ResetFocus( GameFocus::HEROES, true );
    RedrawFocus();
}

fheroes2::GameMode Interface::AdventureMap::EventEndTurn() const
{
#ifndef NDEBUG
    const Heroes * focusedHero = GetFocusHeroes();
#endif
    assert( focusedHero == nullptr || !focusedHero->Modes( Heroes::ENABLEMOVE ) );

    const Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );

    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: FUN_00456759 button 4 - the question in the HoMM1 box (style 2, Yes = 0x8005), adventure screen x 0x61 (note v59).
        // [temporary] "heroes may still move" is the engine's test (the original FUN_0041150c is not compared yet).
        if ( !myKingdom.HeroesMayStillMove()
             || H1::UI::messageBox( "One or more Heroes may still move, are you sure you want to end your turn?", H1::UI::MESSAGE_YES_NO, 0x61, -1 ) == 0x8005 ) {
            return fheroes2::GameMode::END_TURN;
        }
        return fheroes2::GameMode::CANCEL;
    }

    if ( !myKingdom.HeroesMayStillMove()
         || Dialog::YES
                == fheroes2::showStandardTextMessage( _( "End Turn" ), _( "One or more heroes may still move, are you sure you want to end your turn?" ),
                                                      Dialog::YES | Dialog::NO ) ) {
        return fheroes2::GameMode::END_TURN;
    }

    return fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::AdventureMap::EventAdventureDialog()
{
    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: Heroes I adventure options (FUN_00460fa6, apanel.bin at (0xa0, 0x28), note v59). Buttons: 1 view world,
        // 2 puzzle, 3 cast an adventure spell, 4 dig, 0x8000 cancel. Without a selected hero 3 and 4 are greyed and take no clicks.
        // Right click help: table 0x493548 (FUN_004611b9), popup at x 0xb1.
        H1::UI::Layout layout = H1::UI::loadLayout( "apanel.bin" );
        if ( GameFocus::HEROES != GetFocusType() ) {
            for ( const int id : { 3, 4 } ) {
                if ( H1::UI::Item * it = layout.find( id ) ) {
                    it->greyed = true;
                    it->inactive = true;
                }
            }
        }
        const int id = H1::UI::runLayoutDialog(
            layout, 0xa0, 0x28,
            []( const int button ) -> const char * {
                switch ( button ) {
                case 1:
                    return "View the entire world.";
                case 2:
                    return "View the obelisk puzzle.";
                case 3:
                    return "Cast an adventure spell.";
                case 4:
                    return "Dig for the Ultimate Artifact.";
                case 0x8000:
                    return "Exit this menu without doing anything.";
                default:
                    return nullptr;
                }
            },
            0xb1 );
        switch ( id ) {
        case 1:
            EventViewWorld();
            break;
        case 2:
            EventPuzzleMaps();
            break;
        case 3:
            EventCastSpell();
            break;
        case 4:
            return EventDigArtifact();
        default:
            break;
        }
        return fheroes2::GameMode::CANCEL;
    }


    switch ( Dialog::AdventureOptions( GameFocus::HEROES == GetFocusType() ) ) {
    case Dialog::WORLD:
        EventViewWorld();
        break;

    case Dialog::PUZZLE:
        EventPuzzleMaps();
        break;

    case Dialog::INFO:
        return EventScenarioInformation();

    case Dialog::DIG:
        return EventDigArtifact();

    default:
        break;
    }

    return fheroes2::GameMode::CANCEL;
}

void Interface::AdventureMap::EventViewWorld()
{
    ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::OnlyVisible, *this );
}

fheroes2::GameMode Interface::AdventureMap::EventFileDialog() const
{
    return Dialog::FileOptions();
}

void Interface::AdventureMap::EventSystemDialog() const
{
    fheroes2::showSystemOptionsDialog();
}

void Interface::AdventureMap::EventNextTown()
{
    Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );
    VecCastles & myCastles = myKingdom.GetCastles();

    if ( !myCastles.empty() ) {
        if ( GetFocusCastle() ) {
            VecCastles::const_iterator it = std::find( myCastles.begin(), myCastles.end(), GetFocusCastle() );
            ++it;
            if ( it == myCastles.end() )
                it = myCastles.begin();
            SetFocus( *it );
        }
        else
            ResetFocus( GameFocus::CASTLE, false );

        RedrawFocus();
    }
}

fheroes2::GameMode Interface::AdventureMap::EventNewGame() const
{
    return Dialog::YES == fheroes2::showStandardTextMessage( "", _( "Are you sure you want to restart? (Your current game will be lost.)" ), Dialog::YES | Dialog::NO )
               ? fheroes2::GameMode::NEW_GAME
               : fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::AdventureMap::EventSaveGame() const
{
    if ( H1::isH1Mode() ) {
        // homm1-remaster: FUN_00460889 - the requester in save mode at (0xa0, 0x28); no overwrite question; "Game saved successfully."
        // at (0xb1, -1) only when it worked (note v160).
        const std::string filename = H1::selectSaveFile( true );
        if ( !filename.empty() && Game::Save( filename ) ) {
            H1::UI::messageBox( "Game saved successfully.", H1::UI::MESSAGE_OKAY, 0xb1, -1 );
        }
        return fheroes2::GameMode::CANCEL;
    }
    while ( true ) {
        const std::string filename = Dialog::SelectFileSave();
        if ( filename.empty() ) {
            return fheroes2::GameMode::CANCEL;
        }

        if ( System::IsFile( filename )
             && Dialog::NO == fheroes2::showStandardTextMessage( "", _( "Are you sure you want to overwrite the save with this name?" ), Dialog::YES | Dialog::NO ) ) {
            continue;
        }

        if ( Game::Save( filename ) ) {
            fheroes2::showStandardTextMessage( "", _( "Game saved successfully." ), Dialog::OK );
        }
        else {
            fheroes2::showStandardTextMessage( "", _( "There was an issue during saving." ), Dialog::OK );
        }
        return fheroes2::GameMode::CANCEL;
    }
}

fheroes2::GameMode Interface::AdventureMap::EventLoadGame() const
{
    if ( H1::isH1Mode() ) {
        // homm1-remaster: the HoMM1 question, then the requester over the map (note v160).
        if ( H1::UI::messageBox( "Are you sure you want to load a new game?  (Your current game will be lost)", H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
            return fheroes2::GameMode::CANCEL;
        }
        const std::string file = H1::selectSaveFile( false );
        if ( file.empty() ) {
            return fheroes2::GameMode::CANCEL;
        }
        H1::setPendingLoad( file );
        return fheroes2::GameMode::LOAD_GAME;
    }
    return Dialog::YES
                   == fheroes2::showStandardTextMessage( "", _( "Are you sure you want to load a new game? (Your current game will be lost.)" ),
                                                         Dialog::YES | Dialog::NO )
               ? fheroes2::GameMode::LOAD_GAME
               : fheroes2::GameMode::CANCEL;
}

void Interface::AdventureMap::EventPuzzleMaps() const
{
    world.GetKingdom( Settings::Get().CurrentColor() ).PuzzleMaps().ShowMapsDialog();
}

fheroes2::GameMode Interface::AdventureMap::EventScenarioInformation()
{
    // homm1-remaster: Heroes I campaign window (campaign.bin, note v127) or scenario information (sceninfo.bin, note v123).
    if ( H1::isCampaignGame() && H1::isGraphicsMode() ) {
        return H1::showCampaignWindow( true ) ? H1::restartCampaignScenario() : fheroes2::GameMode::CANCEL;
    }
    if ( !Settings::Get().isCampaignGameType() && H1::showScenarioInfo() ) {
        return fheroes2::GameMode::CANCEL;
    }

    if ( Settings::Get().isCampaignGameType() ) {
        fheroes2::Display & display = fheroes2::Display::instance();
        fheroes2::ImageRestorer saver( display, 0, 0, display.width(), display.height() );

        // We are opening campaign scenario info. It is a full screen image change. So do fade-out and set the fade-in.
        fheroes2::fadeOutDisplay();
        Game::setDisplayFadeIn();

        AudioManager::ResetAudio();

        const fheroes2::GameMode returnMode = Game::SelectCampaignScenario( fheroes2::GameMode::CANCEL, true );
        if ( returnMode == fheroes2::GameMode::CANCEL ) {
            // We are going back to the Adventure map with fade-in.
            Game::setDisplayFadeIn();

            saver.restore();

            Game::restoreSoundsForCurrentFocus();
        }
        else {
            saver.reset();
        }

        return returnMode;
    }

    Dialog::GameInfo();
    return fheroes2::GameMode::CANCEL;
}

void Interface::AdventureMap::EventSwitchHeroSleeping()
{
    Heroes * hero = GetFocusHeroes();

    if ( hero ) {
        hero->Modes( Heroes::SLEEPER ) ? hero->ResetModes( Heroes::SLEEPER ) : hero->SetModes( Heroes::SLEEPER );

        setRedraw( REDRAW_HEROES );
        _buttonsPanel.setRedraw();
    }
}

fheroes2::GameMode Interface::AdventureMap::EventDigArtifact()
{
    Heroes * hero = GetFocusHeroes();
    if ( hero == nullptr ) {
        // TODO: verify how this method can be called if a hero is not selected.
        return fheroes2::GameMode::CANCEL;
    }

    if ( H1::isH1Mode() ) {
        // homm1-remaster: FUN_00456943 (note v154).
        H1::Ai::dig( *hero );
        redraw( REDRAW_GAMEAREA | REDRAW_HEROES | REDRAW_BUTTONS );
        fheroes2::Display::instance().render();
        return GameOver::Result::Get().checkGameOver();
    }

    if ( hero->isShipMaster() ) {
        fheroes2::showStandardTextMessage( "", _( "Try looking on land!!!" ), Dialog::OK );
        return fheroes2::GameMode::CANCEL;
    }

    if ( hero->GetBagArtifacts().isFull() ) {
        fheroes2::showStandardTextMessage(
            "", _( "Searching for the Ultimate Artifact is fruitless. Your hero could not carry it even if he found it - all his artifact slots are full." ),
            Dialog::OK );
        return fheroes2::GameMode::CANCEL;
    }

    if ( hero->GetMaxMovePoints() > hero->GetMovePoints() ) {
        fheroes2::showStandardTextMessage( "", _( "Digging for artifacts requires a whole day, try again tomorrow." ), Dialog::OK );
        return fheroes2::GameMode::CANCEL;
    }

    // Original Editor allows to put an Ultimate Artifact on an invalid tile. So checking tile index solves this issue.
    const UltimateArtifact & ultimateArtifact = world.GetUltimateArtifact();
    if ( world.getTile( hero->GetIndex() ).isSuitableForUltimateArtifact() || ( ultimateArtifact.getPosition() == hero->GetIndex() && !ultimateArtifact.isFound() ) ) {
        if ( H1::isGraphicsMode() ) {
            H1::playSound( "digsound.82m" ); // homm1-remaster: FUN_00456943 digging (human player only, as here)
        }
        else {
            AudioManager::PlaySound( M82::DIGSOUND );
        }

        hero->ResetMovePoints();
        hero->GetPath().Reset();

        if ( world.DiggingForUltimateArtifact( hero->GetCenter() ) ) {
            const AudioManager::MusicRestorer musicRestorer;

            if ( Settings::Get().MusicMIDI() ) {
                AudioManager::PlaySound( M82::TREASURE );
            }
            else {
                AudioManager::PlayMusic( MUS::ULTIMATE_ARTIFACT, Music::PlaybackMode::PLAY_ONCE );
            }

            const Artifact & ultimate = ultimateArtifact.GetArtifact();

            if ( !hero->PickupArtifact( ultimate ) ) {
                assert( 0 );
            }

            std::string msg( _( "After spending many hours digging here, you have uncovered the %{artifact}." ) );
            StringReplace( msg, "%{artifact}", ultimate.GetName() );

            const fheroes2::ArtifactDialogElement artifactUI( ultimate.GetID() );
            fheroes2::showStandardTextMessage( _( "Congratulations!" ), std::move( msg ), Dialog::OK, { &artifactUI } );
        }
        else {
            fheroes2::showStandardTextMessage( "", _( "Nothing here. Where could it be?" ), Dialog::OK );
        }

        redraw( REDRAW_HEROES | REDRAW_BUTTONS );
        fheroes2::Display::instance().render();

        // check if the game is over due to conditions related to the ultimate artifact
        return GameOver::Result::Get().checkGameOver();
    }

    fheroes2::showStandardTextMessage( "", _( "Try searching on clear ground." ), Dialog::OK );

    return fheroes2::GameMode::CANCEL;
}

fheroes2::GameMode Interface::AdventureMap::EventDefaultAction()
{
    if ( Heroes * hero = GetFocusHeroes(); hero ) {
        if ( MP2::isRevisitAllowedForObject( hero->getObjectTypeUnderHero(), hero->isShipMaster() ) ) {
            hero->Action( hero->GetIndex() );

            // The action object can alter the status of the hero (e.g. Stables or Well) or
            // move it to another location (e.g. Stone Liths or Whirlpool)
            ResetFocus( GameFocus::HEROES, true );
            RedrawFocus();

            // If the hero has performed an action, we have to check the completion condition
            // of the scenario
            if ( hero->isAction() ) {
                hero->ResetAction();

                return GameOver::Result::Get().checkGameOver();
            }
        }
    }
    else if ( Castle * castle = GetFocusCastle(); castle ) {
        Game::OpenCastleDialog( *castle );
    }

    return fheroes2::GameMode::CANCEL;
}

void Interface::AdventureMap::EventOpenFocus() const
{
    if ( Heroes * hero = GetFocusHeroes(); hero ) {
        Game::OpenHeroesDialog( *hero, true, true );
    }
    else if ( Castle * castle = GetFocusCastle(); castle ) {
        Game::OpenCastleDialog( *castle );
    }
}

void Interface::AdventureMap::EventSwitchShowRadar() const
{
    Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        if ( conf.ShowRadar() ) {
            conf.SetShowRadar( false );
            _gameArea.SetRedraw();
        }
        else {
            conf.SetShowRadar( true );
            _radar.SetRedraw( REDRAW_RADAR );
        }
    }
}

void Interface::AdventureMap::EventSwitchShowButtons() const
{
    Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        if ( conf.ShowButtons() ) {
            conf.SetShowButtons( false );
            _gameArea.SetRedraw();
        }
        else {
            conf.SetShowButtons( true );
            _buttonsPanel.setRedraw();
        }
    }
}

void Interface::AdventureMap::EventSwitchShowStatus() const
{
    Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        if ( conf.ShowStatus() ) {
            conf.SetShowStatus( false );
            _gameArea.SetRedraw();
        }
        else {
            conf.SetShowStatus( true );
            _statusPanel.setRedraw();
        }
    }
}

void Interface::AdventureMap::EventSwitchShowIcons() const
{
    Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        if ( conf.ShowIcons() ) {
            conf.SetShowIcons( false );
            _gameArea.SetRedraw();
        }
        else {
            conf.SetShowIcons( true );
            _iconsPanel.setRedraw();
        }
    }
}

void Interface::AdventureMap::EventSwitchShowControlPanel() const
{
    Settings & conf = Settings::Get();

    if ( conf.isHideInterfaceEnabled() ) {
        conf.SetShowControlPanel( !conf.ShowControlPanel() );
        _gameArea.SetRedraw();
    }
}

void Interface::AdventureMap::EventKeyArrowPress( const int dir )
{
    if ( Heroes * hero = GetFocusHeroes(); hero ) {
        MoveHeroFromArrowKeys( *hero, dir );

        return;
    }

    if ( _gameArea.isDragScroll() ) {
        return;
    }

    if ( Settings::Get().ScrollSpeed() == SCROLL_SPEED_NONE ) {
        // Scrolling is disabled.
        return;
    }

    switch ( dir ) {
    case Direction::TOP_LEFT:
        _gameArea.SetScroll( SCROLL_TOP );
        _gameArea.SetScroll( SCROLL_LEFT );
        break;
    case Direction::TOP:
        _gameArea.SetScroll( SCROLL_TOP );
        break;
    case Direction::TOP_RIGHT:
        _gameArea.SetScroll( SCROLL_TOP );
        _gameArea.SetScroll( SCROLL_RIGHT );
        break;
    case Direction::RIGHT:
        _gameArea.SetScroll( SCROLL_RIGHT );
        break;
    case Direction::BOTTOM_RIGHT:
        _gameArea.SetScroll( SCROLL_BOTTOM );
        _gameArea.SetScroll( SCROLL_RIGHT );
        break;
    case Direction::BOTTOM:
        _gameArea.SetScroll( SCROLL_BOTTOM );
        break;
    case Direction::BOTTOM_LEFT:
        _gameArea.SetScroll( SCROLL_BOTTOM );
        _gameArea.SetScroll( SCROLL_LEFT );
        break;
    case Direction::LEFT:
        _gameArea.SetScroll( SCROLL_LEFT );
        break;
    default:
        break;
    }
}
