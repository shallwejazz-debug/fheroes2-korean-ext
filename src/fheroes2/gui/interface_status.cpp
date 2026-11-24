/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
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

#include "interface_status.h"

#include "h1_gfx.h"
#include "h1_ui.h"
#include "race.h"
#include "h1_armywin.h"
#include "h1_rules.h"

#include <cassert>
#include <chrono>
#include <string>
#include <utility>

#include "army.h"
#include "castle.h"
#include "color.h"
#include "dialog.h"
#include "game.h"
#include "game_assets.h"
#include "game_delays.h"
#include "game_interface.h"
#include "heroes.h"
#include "icn.h"
#include "image.h"
#include "interface_base.h"
#include "kingdom.h"
#include "localevent.h"
#include "math_base.h"
#include "players.h"
#include "screen.h"
#include "settings.h"
#include "tools.h"
#include "translations.h"
#include "ui_constants.h"
#include "ui_dialog.h"
#include "ui_text.h"
#include "world.h"

void Interface::StatusPanel::SavePosition()
{
    Settings & conf = Settings::Get();

    conf.SetPosStatus( GetRect().getPosition() );
    conf.Save( Settings::configFileName );
}

void Interface::StatusPanel::setRedraw() const
{
    _interface.setRedraw( REDRAW_STATUS );
}

void Interface::StatusPanel::SetPos( int32_t x, int32_t y )
{
    const int32_t width = 144;
    int32_t height = 72;

    if ( !Settings::Get().isHideInterfaceEnabled() ) {
        height = fheroes2::Display::instance().height() - y - fheroes2::borderWidthPx;
    }

    BorderWindow::SetPosition( x, y, width, height );
}

void Interface::StatusPanel::SetState( const StatusType status )
{
    // SetResource() should be used to set this status
    assert( status != StatusType::STATUS_RESOURCE );

    if ( _state != StatusType::STATUS_RESOURCE ) {
        _state = status;
    }
}

void Interface::StatusPanel::_redraw() const
{
    const Settings & conf = Settings::Get();
    if ( conf.isHideInterfaceEnabled() && !conf.ShowStatus() ) {
        // The window is hidden.
        return;
    }

    const fheroes2::Rect & pos = GetArea();

    if ( conf.isHideInterfaceEnabled() ) {
        fheroes2::Fill( fheroes2::Display::instance(), pos.x, pos.y, pos.width, pos.height, fheroes2::GetColorId( 0x51, 0x31, 0x18 ) );
        BorderWindow::Redraw();
    }
    else {
        _drawBackground();
    }

    // Do not draw anything if the game hasn't really started yet
    if ( world.CountDay() == 0 ) {
        return;
    }

    // draw info: Day and Funds and Army
    const fheroes2::Sprite & ston = Assets::getImage( conf.isEvilInterfaceEnabled() ? ICN::STONBAKE : ICN::STONBACK, 0 );
    const int32_t stonHeight = ston.height();

    if ( _state == StatusType::STATUS_AITURN ) {
        _drawAITurns();
    }
    else if ( StatusType::STATUS_UNKNOWN != _state && pos.height >= ( stonHeight * 3 + 15 ) ) {
        _drawDayInfo();

        if ( ( Players::HumanColors() & conf.CurrentColor() ) || conf.IsGameType( Game::TYPE_AUTO_PLAYTEST ) ) {
            _drawKingdomInfo( stonHeight + 5 );

            if ( _state != StatusType::STATUS_RESOURCE ) {
                _drawArmyInfo( 2 * stonHeight + 10 );
            }
            else {
                _drawResourceInfo( 2 * stonHeight + 10 );
            }
        }
    }
    else if ( StatusType::STATUS_UNKNOWN != _state && pos.height >= ( stonHeight * 2 + 15 ) ) {
        _drawDayInfo();

        switch ( _state ) {
        case StatusType::STATUS_FUNDS:
            _drawKingdomInfo( stonHeight + 5 );
            break;
        case StatusType::STATUS_DAY:
        case StatusType::STATUS_ARMY:
            _drawArmyInfo( stonHeight + 5 );
            break;
        case StatusType::STATUS_RESOURCE:
            _drawResourceInfo( stonHeight + 5 );
            break;
        case StatusType::STATUS_UNKNOWN:
        case StatusType::STATUS_AITURN:
            assert( 0 ); // we shouldn't even reach this code
            break;
        default:
            break;
        }
    }
    else {
        const int32_t offsetY = pos.height > stonHeight ? ( pos.height - stonHeight ) / 2 : 0;

        switch ( _state ) {
        case StatusType::STATUS_DAY:
            _drawDayInfo();
            break;
        case StatusType::STATUS_FUNDS:
            _drawKingdomInfo( offsetY );
            break;
        case StatusType::STATUS_ARMY:
            _drawArmyInfo( offsetY );
            break;
        case StatusType::STATUS_RESOURCE:
            _drawResourceInfo( offsetY );
            break;
        case StatusType::STATUS_UNKNOWN:
        case StatusType::STATUS_AITURN:
            assert( 0 ); // we shouldn't even reach this code
            break;
        default:
            break;
        }
    }
}

namespace
{
    // homm1-remaster: FUN_0045a5c3 (note v179). The window shows the hero's army when a hero has the focus, else the kingdom's funds.
    // The date (mode 1) and the funds (mode 2) are shown for 3000 ms only: at the start of the turn (FUN_0041ea28) and after a click.
    Interface::StatusType h1DefaultState()
    {
        return ( Interface::GetFocusHeroes() != nullptr ) ? Interface::StatusType::STATUS_ARMY : Interface::StatusType::STATUS_FUNDS;
    }

    Interface::StatusType h1LastState = Interface::StatusType::STATUS_UNKNOWN;
    std::chrono::steady_clock::time_point h1StateSince;
}

void Interface::StatusPanel::NextState()
{
    if ( H1::isGraphicsMode() && _state != StatusType::STATUS_AITURN ) {
        // The click on the window (ids 2000..0x898 in FUN_0045f5a6): temporary funds -> date; the usual view -> date when it shows the
        // funds (DAT_004a17cc == 2), else funds; the date -> the usual view.
        const StatusType usual = h1DefaultState();
        if ( _state == StatusType::STATUS_FUNDS && usual != StatusType::STATUS_FUNDS ) {
            _state = StatusType::STATUS_DAY;
        }
        else if ( _state == usual || _state == StatusType::STATUS_RESOURCE ) {
            _state = ( usual == StatusType::STATUS_FUNDS ) ? StatusType::STATUS_DAY : StatusType::STATUS_FUNDS;
        }
        else {
            _state = usual;
        }
        h1LastState = _state;
        h1StateSince = std::chrono::steady_clock::now();
        return;
    }

    const int32_t areaHeight = GetArea().height;
    const fheroes2::Sprite & ston = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::STONBAKE : ICN::STONBACK, 0 );
    const int32_t stonHeight = ston.height();

    const bool skipDayStatus = areaHeight >= ( stonHeight * 2 + 15 ) && areaHeight < ( stonHeight * 3 + 15 );

    if ( StatusType::STATUS_DAY == _state ) {
        _state = StatusType::STATUS_FUNDS;
    }
    else if ( StatusType::STATUS_FUNDS == _state ) {
        _state = ( GameFocus::UNSEL == GetFocusType() ? StatusType::STATUS_DAY : StatusType::STATUS_ARMY );
    }
    else if ( StatusType::STATUS_ARMY == _state ) {
        if ( skipDayStatus ) {
            _state = StatusType::STATUS_FUNDS;
        }
        else {
            _state = StatusType::STATUS_DAY;
        }
    }
    else if ( StatusType::STATUS_RESOURCE == _state ) {
        _state = StatusType::STATUS_ARMY;
    }

    if ( _state == StatusType::STATUS_ARMY ) {
        const Castle * castle = GetFocusCastle();

        // Skip an empty army in the castle
        if ( castle && !castle->GetArmy().isValid() ) {
            NextState();
        }
    }
}

void Interface::StatusPanel::_drawKingdomInfo( const int32_t offsetY ) const
{
    fheroes2::Point pos = GetArea().getPosition();
    const Kingdom & myKingdom = world.GetKingdom( Settings::Get().CurrentColor() );

    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: Heroes I kingdom view (FUN_0045b62d, note v59): ressmall.icn at (+1, +1); 9 numbers in smalfont, 0x20 wide,
        // centred, colour 1: wood, mercury, ore, sulfur, crystal, gems on the lower row, then gold, castles (castle built) and towns.
        fheroes2::Display & display = fheroes2::Display::instance();
        const std::vector<fheroes2::Sprite> & ressmall = H1::getIcn( "ressmall.icn" );
        if ( !ressmall.empty() ) {
            fheroes2::Blit( ressmall[0], display, pos.x + 1 + ressmall[0].x(), pos.y + offsetY + 1 + ressmall[0].y() );
        }
        const Funds & funds = myKingdom.GetFunds();
        const int32_t values[9] = { funds.Get( Resource::WOOD ),   funds.Get( Resource::MERCURY ), funds.Get( Resource::ORE ),
                                     funds.Get( Resource::SULFUR ), funds.Get( Resource::CRYSTAL ), funds.Get( Resource::GEMS ),
                                     funds.Get( Resource::GOLD ),   static_cast<int32_t>( myKingdom.GetCountCastle() ),
                                     static_cast<int32_t>( myKingdom.GetCountTown() ) };
        const int32_t xs[9] = { 15, 38, 61, 85, 109, 132, 123, 27, 80 };
        const int32_t ys[9] = { 59, 59, 59, 59, 59, 59, 28, 28, 28 };
        for ( int i = 0; i < 9; ++i ) {
            H1::UI::drawText( display, std::to_string( values[i] ), "smalfont.fnt", pos.x + xs[i] - 16, pos.y + offsetY + ys[i], 0x20, 0xc, 1, 1 );
        }
        return;
    }

    pos.y += offsetY + 3;

    // sprite all resource
    fheroes2::Display & display = fheroes2::Display::instance();
    fheroes2::Blit( Assets::getImage( ICN::RESSMALL, 0 ), display, pos.x + 6, pos.y );

    pos.y += 27;

    // count castle
    fheroes2::Text text( std::to_string( myKingdom.GetCountCastle() ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 26 - text.width() / 2, pos.y, display );
    // count town
    text.set( std::to_string( myKingdom.GetCountTown() ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 78 - text.width() / 2, pos.y, display );
    // count gold
    text.set( std::to_string( myKingdom.GetFunds().Get( Resource::GOLD ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 122 - text.width() / 2, pos.y, display );

    pos.y += 30;

    // count wood
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::WOOD ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 15 - text.width() / 2, pos.y, display );
    // count mercury
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::MERCURY ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 37 - text.width() / 2, pos.y, display );
    // count ore
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::ORE ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 60 - text.width() / 2, pos.y, display );
    // count sulfur
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::SULFUR ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 84 - text.width() / 2, pos.y, display );
    // count crystal
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::CRYSTAL ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 108 - text.width() / 2, pos.y, display );
    // count gems
    text.set( fheroes2::abbreviateNumber( myKingdom.GetFunds().Get( Resource::GEMS ) ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + 130 - text.width() / 2, pos.y, display );
}

void Interface::StatusPanel::_drawDayInfo( const int32_t offsetY ) const
{
    const fheroes2::Rect & pos = GetArea();

    const uint32_t dayOfWeek = world.GetDay();
    const uint32_t weekOfMonth = world.GetWeek();
    const uint32_t month = world.GetMonth();
    const int icnType = Settings::Get().isEvilInterfaceEnabled() ? ICN::SUNMOONE : ICN::SUNMOON;

    uint32_t icnId = dayOfWeek > 1 ? 0 : ( ( weekOfMonth - 1 ) % 4 ) + 1;
    // Special case
    if ( dayOfWeek == 1 && weekOfMonth == 1 && month == 1 ) {
        icnId = 0;
    }

    fheroes2::Display & display = fheroes2::Display::instance();

    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: Heroes I day view (FUN_0045ae53, note v56): sunmoon.icn at the panel corner (frame = week on day 1 of a week,
        // except the first day of the game), "Month: %d  Week: %d" in smalfont at (-1, 0x1d) 0x91 wide, "Day: %d" in bigfont at
        // (-1, 0x2e) 0x91 x 0x19, both centred, colour 1. The stonback.icn background is drawn by _drawBackground.
        const std::vector<fheroes2::Sprite> & sunmoon = H1::getIcn( "sunmoon.icn" );
        if ( icnId < sunmoon.size() ) {
            fheroes2::Blit( sunmoon[icnId], display, pos.x + sunmoon[icnId].x(), pos.y + offsetY + sunmoon[icnId].y() );
        }
        H1::UI::drawText( display, "Month: " + std::to_string( month ) + "  Week: " + std::to_string( weekOfMonth ), "smalfont.fnt", pos.x - 1,
                          pos.y + offsetY + 0x1d, 0x91, 0xc, 1, 1 );
        H1::UI::drawText( display, "Day: " + std::to_string( dayOfWeek ), "bigfont.fnt", pos.x - 1, pos.y + offsetY + 0x2e, 0x91, 0x19, 1, 1 );
        return;
    }

    fheroes2::Blit( Assets::getImage( icnType, icnId ), display, pos.x, pos.y + 1 + offsetY );

    std::string message = _( "Month: %{month} Week: %{week}" );
    StringReplace( message, "%{month}", month );
    StringReplace( message, "%{week}", weekOfMonth );
    fheroes2::Text text( message, fheroes2::FontType::smallWhite() );
    text.draw( pos.x + ( pos.width - text.width() ) / 2, pos.y + 32 + offsetY, display );

    message = _( "Day: %{day}" );
    StringReplace( message, "%{day}", dayOfWeek );
    text.set( message, fheroes2::FontType::normalWhite() );
    text.draw( pos.x + ( pos.width - text.width() ) / 2, pos.y + 48 + offsetY, display );
}

void Interface::StatusPanel::SetResource( const int resource, const uint32_t count )
{
    _lastResource = resource;
    _lastResourceCount = count;
    _state = StatusType::STATUS_RESOURCE;

    _showLastResourceDelay.reset();
}

void Interface::StatusPanel::_drawResourceInfo( const int32_t offsetY ) const
{
    const fheroes2::Rect & pos = GetArea();

    fheroes2::Display & display = fheroes2::Display::instance();

    std::string message = _( "You find a small\nquantity of %{resource}." );
    StringReplace( message, "%{resource}", Resource::String( _lastResource ) );
    fheroes2::Text text{ std::move( message ), fheroes2::FontType::smallWhite() };
    text.draw( pos.x, pos.y + 6 + offsetY, pos.width, display );

    const fheroes2::Sprite & spr = Assets::getImage( ICN::RESOURCE, Resource::getIconIcnIndex( _lastResource ) );
    fheroes2::Blit( spr, display, pos.x + ( pos.width - spr.width() ) / 2, pos.y + 6 + offsetY + text.height( pos.width ) );

    text.set( std::to_string( _lastResourceCount ), fheroes2::FontType::smallWhite() );
    text.draw( pos.x + ( pos.width - text.width() ) / 2, pos.y + offsetY + text.height() * 2 + spr.height() + 10, display );
}

void Interface::StatusPanel::_drawArmyInfo( const int32_t offsetY ) const
{
    const Army * armyTroops = nullptr;

    if ( const Heroes * focusedHero = GetFocusHeroes(); focusedHero != nullptr ) {
        armyTroops = &focusedHero->GetArmy();
    }
    else if ( const Castle * focusedCastle = GetFocusCastle(); focusedCastle != nullptr ) {
        armyTroops = &focusedCastle->GetArmy();
    }

    const Heroes * h1Hero = GetFocusHeroes();
    if ( H1::isGraphicsMode() && h1Hero != nullptr ) {
        // homm1-remaster: Heroes I hero army view (FUN_0045b9fb, note v59): smcrest.icn (class + colour x 4) at (+15, +3), the first 8
        // letters of the name in smalfont at (-5, +26) 0x42 wide, then each stack: mons32.icn at the place below, its number in smalfont.
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Rect & pos = GetArea();
        const int32_t baseY = pos.y + offsetY;
        int cls = 0;
        switch ( h1Hero->GetRace() ) {
        case Race::BARB:
            cls = 1;
            break;
        case Race::SORC:
            cls = 2;
            break;
        case Race::WRLK:
            cls = 3;
            break;
        default:
            break;
        }
        const std::vector<fheroes2::Sprite> & crest = H1::getIcn( "smcrest.icn" );
        const size_t crestFrame = static_cast<size_t>( cls + std::max( 0, Color::GetIndex( h1Hero->GetColor() ) ) * 4 );
        if ( crestFrame < crest.size() ) {
            fheroes2::Blit( crest[crestFrame], display, pos.x + 15 + crest[crestFrame].x(), baseY + 3 + crest[crestFrame].y() );
        }
        H1::UI::drawText( display, h1Hero->GetName().substr( 0, 8 ), "smalfont.fnt", pos.x - 5, baseY + 26, 0x42, 0xc, 1, 1 );

        // Creature widths, table 0x492550.
        static const int creatureWidth[28] = { 20, 20, 20, 25, 25, 24, 21, 21, 25, 27, 22, 20, 23, 23, 21, 22, 25, 23, 27, 22, 29, 28, 32, 27, 21, 26, 21, 29 };
        const Army & army = h1Hero->GetArmy();
        int stacks = 0;
        for ( size_t i = 0; i < army.Size(); ++i ) {
            if ( army.GetTroop( i ) != nullptr && army.GetTroop( i )->isValid() ) {
                ++stacks;
            }
        }
        const std::vector<fheroes2::Sprite> & mons32 = H1::getIcn( "mons32.icn" );
        int n = 0;
        for ( size_t i = 0; i < army.Size(); ++i ) {
            const Troop * troop = army.GetTroop( i );
            if ( troop == nullptr || !troop->isValid() ) {
                continue;
            }
            const int creature = H1::creatureFromMonster( troop->GetID() );
            const int32_t rowY = ( n < 3 ) ? 0x26 : 3;
            int32_t x = 0;
            switch ( n ) {
            case 0:
                x = ( stacks < 3 ) ? 0x4d : 0x65;
                break;
            case 1:
                x = ( stacks == 2 ) ? 0x1c : 0x34;
                break;
            case 2:
                x = 3;
                break;
            case 3:
                x = ( stacks == 4 ) ? 0x4d : 0x65;
                break;
            default:
                x = 0x34;
                break;
            }
            // H1-INTENDED(D16): extension creatures 28..35 from h1/ext/mons32x.icn, their width from the picture.
            const std::vector<fheroes2::Sprite> & icons = ( creature >= 28 ) ? H1::getIcn( H1::smallCreatureIcn( creature ) ) : mons32;
            const int frame = ( creature >= 0 ) ? H1::smallCreatureFrame( creature ) : -1;
            if ( frame >= 0 && static_cast<size_t>( frame ) < icons.size() ) {
                const fheroes2::Sprite & sp = icons[frame];
                fheroes2::Blit( sp, display, pos.x + x + sp.x(), baseY + rowY + sp.y() );
                const std::string count = std::to_string( troop->GetCount() );
                const int32_t width = ( creature < 28 ) ? creatureWidth[creature] : sp.x() + sp.width();
                const int32_t textX = ( width < 0x1c && count.size() < 3 ) ? x + 0x1e : width + x + 2;
                H1::UI::drawText( display, count, "smalfont.fnt", pos.x + textX, baseY + 22 + rowY, static_cast<int32_t>( count.size() ) * 5, 0xc, 1, 1 );
            }
            ++n;
        }
        return;
    }

    if ( armyTroops ) {
        const fheroes2::Rect & pos = GetArea();
        Army::drawMultipleMonsterLines( *armyTroops, pos.x + 4, pos.y + 1 + offsetY, 138, true, true );
    }
}

void Interface::StatusPanel::_drawAITurns() const
{
    // restore background
    _drawBackground();

    fheroes2::Display & display = fheroes2::Display::instance();

    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: Heroes I computer turn view (FUN_0045a894, note v59): hourglas.icn 0 at (+13, +11), brcrest.icn = colour of the
        // player at (+15, +13), at (+79, +13) the sand: frames 27..30 changing every 300 ms, and the progress frame (step + 1) over it.
        const std::vector<fheroes2::Sprite> & glass = H1::getIcn( "hourglas.icn" );
        const std::vector<fheroes2::Sprite> & crest = H1::getIcn( "brcrest.icn" );
        const fheroes2::Rect & roi = GetArea();
        auto blit = [&display]( const std::vector<fheroes2::Sprite> & icn, const size_t frame, const int32_t x, const int32_t y ) {
            if ( frame < icn.size() ) {
                fheroes2::Blit( icn[frame], display, x + icn[frame].x(), y + icn[frame].y() );
            }
        };
        blit( glass, 0, roi.x + 13, roi.y + 11 );
        blit( crest, static_cast<size_t>( std::max( 0, Color::GetIndex( Settings::Get().CurrentColor() ) ) ), roi.x + 15, roi.y + 13 );
        // [temporary] the 300 ms step is taken from the engine's adventure map animation counter.
        blit( glass, 27 + ( Game::getAdventureMapAnimationIndex() % 4 ), roi.x + 79, roi.y + 13 );
        blit( glass, 1 + ( _aiTurnProgress % 10 ), roi.x + 79, roi.y + 13 );
        return;
    }

    const fheroes2::Sprite & glass = Assets::getImage( ICN::HOURGLAS, 0 );
    const fheroes2::Rect & statusRoi = GetArea();

    int32_t posX = statusRoi.x + ( statusRoi.width - glass.width() ) / 2;
    int32_t posY = statusRoi.y + ( statusRoi.height - glass.height() ) / 2;

    fheroes2::Blit( glass, display, posX, posY );

    uint32_t colorIndex = 0;

    switch ( Settings::Get().CurrentColor() ) {
    case PlayerColor::BLUE:
        break;
    case PlayerColor::GREEN:
        colorIndex = 1;
        break;
    case PlayerColor::RED:
        colorIndex = 2;
        break;
    case PlayerColor::YELLOW:
        colorIndex = 3;
        break;
    case PlayerColor::ORANGE:
        colorIndex = 4;
        break;
    case PlayerColor::PURPLE:
        colorIndex = 5;
        break;
    default:
        // Have you added a new player color? Check the logic above!
        assert( 0 );
        return;
    }

    const fheroes2::Sprite & crest = Assets::getImage( ICN::BRCREST, colorIndex );

    posX += 2;
    posY += 2;

    fheroes2::Blit( crest, display, posX, posY );

    posX += glass.width() - 3;

    uint32_t animationIndex = Game::getAdventureMapAnimationIndex() - _grainsAnimationIndexOffset;

    if ( animationIndex < 10 ) {
        // Start of the sand grains animation.
        animationIndex = 11 + ( animationIndex % 10 );
    }
    else {
        // Sand grains animation loop.
        animationIndex = 21 + ( animationIndex % 10 );
    }

    const fheroes2::Sprite & sandGains = Assets::getImage( ICN::HOURGLAS, animationIndex );
    fheroes2::Blit( sandGains, display, posX - sandGains.width() - sandGains.x(), posY + sandGains.y() );

    const fheroes2::Sprite & sand = Assets::getImage( ICN::HOURGLAS, 1 + ( _aiTurnProgress % 10 ) );
    fheroes2::Blit( sand, display, posX - sand.width() - sand.x(), posY + sand.y() );
}

void Interface::StatusPanel::_drawBackground() const
{
    fheroes2::Display & display = fheroes2::Display::instance();
    const fheroes2::Sprite & icnston = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::STONBAKE : ICN::STONBACK, 0 );
    const fheroes2::Rect & pos = GetArea();

    if ( !Settings::Get().isHideInterfaceEnabled() && display.height() - fheroes2::borderWidthPx - icnston.height() > pos.y ) {
        // top
        const int32_t startY = 11;
        const int32_t copyHeight = 46;
        fheroes2::Rect srcrt( 0, 0, icnston.width(), startY );
        fheroes2::Point dstpt( pos.x, pos.y );
        fheroes2::Copy( icnston, srcrt.x, srcrt.y, display, dstpt.x, dstpt.y, srcrt.width, srcrt.height );

        // middle
        srcrt.y = startY;
        srcrt.height = copyHeight;
        const int32_t count = ( pos.height - ( icnston.height() - copyHeight ) ) / copyHeight;
        for ( int32_t i = 0; i < count; ++i ) {
            dstpt = fheroes2::Point( pos.x, pos.y + copyHeight * i + startY );
            fheroes2::Copy( icnston, srcrt.x, srcrt.y, display, dstpt.x, dstpt.y, srcrt.width, srcrt.height );
        }

        // bottom
        srcrt.height = icnston.height() - startY;
        dstpt = fheroes2::Point( pos.x, pos.y + pos.height - srcrt.height );
        fheroes2::Copy( icnston, srcrt.x, srcrt.y, display, dstpt.x, dstpt.y, srcrt.width, srcrt.height );
    }
    else {
        fheroes2::Copy( icnston, 0, 0, display, pos.x, pos.y, icnston.width(), icnston.height() );
    }
}

void Interface::StatusPanel::QueueEventProcessing()
{
    captureMouse();

    // Move the window border
    if ( Settings::Get().ShowStatus() && BorderWindow::QueueEventProcessing() ) {
        setRedraw();
        return;
    }

    LocalEvent & le = LocalEvent::Get();
    const fheroes2::Rect & pos = GetArea();

    if ( le.MouseClickLeft( pos ) ) {
        NextState();
        setRedraw();
    }
    else if ( le.isMouseRightButtonPressedInArea( GetRect() ) ) {
        if ( H1::isGraphicsMode() ) {
            // homm1-remaster: right click on the status window (items 2000..2200, decompile line 37227): FUN_00422fc2( text, 4 ) =
            // the HoMM1 popup with the one original text.
            H1::UI::messageBox( "Status Window\n\nThis window provides information on the status of your hero or kingdom, and shows the date.  "
                                "Left click here to cycle through these windows.",
                                H1::UI::MESSAGE_POPUP );
            return;
        }
        const fheroes2::Sprite & ston = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::STONBAKE : ICN::STONBACK, 0 );
        const bool isFullInfo = StatusType::STATUS_UNKNOWN != _state && pos.height >= ( ston.height() * 3 + 15 );
        if ( isFullInfo ) {
            fheroes2::showStandardTextMessage( _( "Status Window" ), _( "This window provides information on the status of your hero or kingdom, and shows the date." ),
                                               Dialog::ZERO );
        }
        else {
            fheroes2::showStandardTextMessage(
                _( "Status Window" ),
                _( "This window provides information on the status of your hero or kingdom, and shows the date. Left click here to cycle through these windows." ),
                Dialog::ZERO );
        }
    }
}

void Interface::StatusPanel::TimerEventProcessing()
{
    if ( H1::isGraphicsMode() && _state != StatusType::STATUS_AITURN && _state != StatusType::STATUS_RESOURCE && _state != StatusType::STATUS_UNKNOWN ) {
        const auto now = std::chrono::steady_clock::now();
        if ( _state != h1LastState ) {
            h1LastState = _state;
            h1StateSince = now;
        }
        const StatusType usual = h1DefaultState();
        if ( _state != usual && now - h1StateSince >= std::chrono::milliseconds( 3000 ) ) {
            _state = usual;
            h1LastState = _state;
            setRedraw();
        }
        return;
    }

    if ( _state != StatusType::STATUS_RESOURCE || !_showLastResourceDelay.isPassed() ) {
        return;
    }

    switch ( GetFocusType() ) {
    case GameFocus::HEROES:
        _state = StatusType::STATUS_ARMY;
        break;
    case GameFocus::CASTLE:
        _state = StatusType::STATUS_FUNDS;
        break;
    default:
        _state = StatusType::STATUS_DAY;
        break;
    }

    setRedraw();
}

void Interface::StatusPanel::drawAITurnProgress( const uint32_t progressValue )
{
    // Even if there is no need to draw anything, we still need to pump the event queue to
    // update the position of the software-emulated mouse cursor, feed the music player by
    // another music chunk on some platforms (e.g. WebAssembly), etc.
    LocalEvent::Get().HandleEvents( false );

    const bool updateProgress = ( progressValue != _aiTurnProgress );
    const bool isMapAnimation = Game::validateAnimationDelay( Game::DelayType::MAPS_DELAY );

    if ( !updateProgress && !isMapAnimation ) {
        return;
    }

    if ( updateProgress ) {
        if ( progressValue == 0 ) {
            // If turn progress is just started start the grain animation from the beginning.
            _grainsAnimationIndexOffset = Game::getAdventureMapAnimationIndex();
        }

        _aiTurnProgress = progressValue;
    }

    if ( isMapAnimation ) {
        Game::updateAdventureMapAnimationIndex();

        _interface.setRedraw( REDRAW_GAMEAREA );
    }

    _interface.redraw( REDRAW_STATUS );

    fheroes2::Display::instance().render();
}
