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

#include "interface_icons.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "castle.h"
#include "dialog.h"
#include "game.h"
#include "game_assets.h"
#include "game_hotkeys.h"
#include "game_interface.h"
#include "heroes.h"
#include "heroes_base.h"
#include "icn.h"
#include "interface_base.h"
#include "h1_castle.h"
#include "h1_gfx.h"
#include "h1_quickinfo.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "kingdom.h"
#include "localevent.h"
#include "players.h"
#include "screen.h"
#include "settings.h"
#include "ui_castle.h"
#include "ui_constants.h"
#include "ui_scrollbar.h"
#include "world.h"

enum class PlayerColor : uint8_t;

namespace
{
    const int32_t iconsCursorWidth = 56;
    const int32_t iconsCursorHeight = 32;
}

bool Interface::IconsBar::isVisible()
{
    const Settings & conf = Settings::Get();
    return !conf.isHideInterfaceEnabled() || conf.ShowIcons();
}

void Interface::redrawCastleIcon( const Castle & castle, const int32_t posX, const int32_t posY )
{
    fheroes2::drawCastleIcon( castle, fheroes2::Display::instance(), { posX, posY } );
}

void Interface::redrawHeroesIcon( const Heroes & hero, const int32_t posX, const int32_t posY )
{
    hero.PortraitRedraw( posX, posY, PORT_SMALL, fheroes2::Display::instance() );
}

void Interface::IconsBar::redrawBackground( fheroes2::Image & output, const fheroes2::Point & offset, const int32_t validItemCount ) const
{
    const bool isEvilInterface = Settings::Get().isEvilInterfaceEnabled();

    const fheroes2::Sprite & icnadv = Assets::getImage( isEvilInterface ? ICN::ADVBORDE : ICN::ADVBORD, 0 );
    fheroes2::Rect srcrt( icnadv.width() - fheroes2::radarWidthPx - fheroes2::borderWidthPx, fheroes2::radarWidthPx + 2 * fheroes2::borderWidthPx,
                          fheroes2::radarWidthPx / 2, 32 );

    fheroes2::Blit( icnadv, srcrt.x, srcrt.y, output, offset.x, offset.y, srcrt.width, srcrt.height );

    srcrt.y = srcrt.y + srcrt.height;

    int32_t internalOffsetY = offset.y + srcrt.height;
    srcrt.height = 32;

    if ( _iconsCount > 2 ) {
        for ( int32_t i = 0; i < _iconsCount - 2; ++i ) {
            fheroes2::Blit( icnadv, srcrt.x, srcrt.y, output, offset.x, internalOffsetY, srcrt.width, srcrt.height );
            internalOffsetY += srcrt.height;
        }
    }

    srcrt.y = srcrt.y + 64;
    srcrt.height = 32;
    fheroes2::Blit( icnadv, srcrt.x, srcrt.y, output, offset.x, internalOffsetY, srcrt.width, srcrt.height );

    if ( H1::isGraphicsMode() ) {
        // homm1-remaster: Heroes I slots (adv_wind.bin, note v56): a black 56 x 32 frame (locators.icn 20, one colour 0) and a
        // 54 x 30 background at +1, +1: frames 0..3 for the hero list, 4..7 for the castle list, every 32 pixels (also under filled slots).
        const std::vector<fheroes2::Sprite> & locators = H1::getIcn( "locators.icn" );
        const std::vector<fheroes2::Sprite> & locatorsMono = H1::getIcnMono( "locators.icn" );
        const int firstFrame = ( dynamic_cast<const HeroesIcons *>( this ) != nullptr ) ? 0 : 4;
        for ( int32_t i = 0; i < _iconsCount; ++i ) {
            const int32_t y = offset.y + i * 32;
            if ( locatorsMono.size() > 20 ) {
                const fheroes2::Sprite & frame = locatorsMono[20];
                fheroes2::Fill( output, offset.x + frame.x(), y + frame.y(), frame.width(), frame.height(), 0 );
            }
            if ( static_cast<int>( locators.size() ) > firstFrame + i % 4 ) {
                const fheroes2::Sprite & background = locators[firstFrame + i % 4];
                fheroes2::Blit( background, output, offset.x + 1 + background.x(), y + 1 + background.y() );
            }
        }
        return;
    }

    for ( int32_t i = validItemCount; i < _iconsCount; ++i ) {
        const fheroes2::Sprite & background = Assets::getImage( isEvilInterface ? ICN::LOCATORE : ICN::LOCATORS, 1 + i % 8 );
        fheroes2::Copy( background, 0, 0, output, offset.x + 5, offset.y + 5 + i * ( IconsBar::getItemHeight() + 10 ), background.width(), background.height() );
    }
}

void Interface::HeroesIcons::rotateHeroPortraits( Heroes * currentHero )
{
    assert( currentHero != nullptr );
    const PlayerColor currentColor = Settings::Get().CurrentColor();
    VecHeroes & heroes = world.GetKingdom( currentColor ).GetHeroes();
    if ( heroes.size() <= 1 ) {
        return;
    }

    auto heroIterator = std::find( heroes.begin(), heroes.end(), currentHero );
    assert( heroIterator != heroes.end() );
    const auto iteratorDistance = std::distance( heroes.begin(), heroIterator );
    std::rotate( heroes.begin(), heroes.begin() + iteratorDistance, heroes.begin() + iteratorDistance + 1 );

    Interface::AdventureMap & I = Interface::AdventureMap::Get();

    I.SetFocus( currentHero, false );
    I.RedrawFocus();
}

void Interface::CastleIcons::RedrawItem( const CASTLE & item, int32_t ox, int32_t oy, bool current )
{
    if ( item && _show && H1::isGraphicsMode() ) {
        // homm1-remaster (FUN_0045a360, note v57): slot frame in colour 0xc5 for the selected castle (0 otherwise); picture at +1 =
        // locators.icn 0xc + race (town) or 0x10 + race (castle built, flag 0x40), race in Heroes I numbering.
        fheroes2::Display & display = fheroes2::Display::instance();
        const std::vector<fheroes2::Sprite> & locators = H1::getIcn( "locators.icn" );
        const std::vector<fheroes2::Sprite> & locatorsMono = H1::getIcnMono( "locators.icn" );
        if ( locatorsMono.size() > 20 ) {
            H1::UI::drawMonoSprite( display, locatorsMono[20], ox, oy, current ? 0xc5 : 0 );
        }
        const int race = H1::h1RaceFromEngine( item->GetRace() );
        // H1-INTENDED(D16): the necromancer (race 4) pictures are appended as frames 22 (town) and 23 (castle) (make_ext_necro_locators.py).
        const size_t frame = static_cast<size_t>( race == 4 ? ( item->isCastle() ? 23 : 22 ) : ( item->isCastle() ? 0x10 : 0xc ) + ( race < 0 ? 0 : race ) );
        if ( frame < locators.size() ) {
            fheroes2::Blit( locators[frame], display, ox + 1 + locators[frame].x(), oy + 1 + locators[frame].y() );
        }
        // H1-INTENDED(D12): the Heroes II "built here today" mark (LOCATORS frame 24) at the top right of the castle picture.
        // The original (FUN_0045a360) draws no mark; owner decision 2026-11-17 18:50.
        // Red X (owner 19:08 "지을 수 없는 것도"): as the Heroes II engine shows it (Castle::GetAllBuildingStatus REQUIRES_BUILD), on HoMM1 rules
        // (FUN_00422d50 = H1::canBuildBuilding): nothing can be built today, while a dwelling is still missing for want of the buildings it needs.
        // Buildings 0..12 = guild, thieves' guild, tavern, shipyard, well, tent, castle, dwellings 1..6.
        int markFrame = -1;
        if ( Castle::GetAllBuildingStatus( *item ) == BuildingStatus::NOT_TODAY ) {
            markFrame = 0;
        }
        else {
            bool any = false;
            bool missing = false;
            for ( int number = 0; number <= 12; ++number ) {
                const bool built = H1::isBuildingBuilt( *item, number );
                if ( H1::canBuildBuilding( *item, number ) && ( number == 0 || !built ) ) {
                    any = true;
                }
                if ( number >= 7 && !built ) {
                    missing = true;
                }
            }
            if ( !any && missing ) {
                markFrame = 1;
            }
        }
        if ( markFrame >= 0 ) {
            // The engine makes frame 1 by turning HoMM2 colour 0x0A into 0xD6 in LOCATORS 24; here that picture is already in HoMM1
            // colours, so the same two colours are taken through the HoMM2 -> HoMM1 colour table first.
            static fheroes2::Sprite redMark;
            if ( redMark.empty() ) {
                redMark = Assets::getImage( ICN::LOCATORS, 24 );
                fheroes2::Image from( 1, 1 );
                fheroes2::Image to( 1, 1 );
                from.image()[0] = 0x0A;
                to.image()[0] = 0xD6;
                H1::remapHoMM2Image( from );
                H1::remapHoMM2Image( to );
                fheroes2::ReplaceColorId( redMark, from.image()[0], to.image()[0] );
            }
            const fheroes2::Sprite & mark = markFrame == 0 ? Assets::getImage( ICN::CSLMARKER, 0 ) : redMark;
            if ( frame < locators.size() ) {
                // Top right corner inside the castle picture.
                fheroes2::Blit( mark, display, ox + 1 + locators[frame].x() + locators[frame].width() - mark.width() - 1, oy + 1 + locators[frame].y() + 1 );
            }
        }
        return;
    }
    if ( item && _show ) {
        redrawCastleIcon( *item, ox + 5, oy + 5 );

        if ( current ) {
            fheroes2::Blit( _marker, fheroes2::Display::instance(), ox, oy );
        }
    }
}

void Interface::CastleIcons::RedrawBackground( const fheroes2::Point & pos )
{
    redrawBackground( fheroes2::Display::instance(), pos, _show ? _size() : 0 );
}

void Interface::CastleIcons::ActionCurrentUp()
{
    Interface::AdventureMap::Get().SetFocus( GetCurrent() );
}

void Interface::CastleIcons::ActionCurrentDn()
{
    Interface::AdventureMap::Get().SetFocus( GetCurrent() );
}

void Interface::CastleIcons::ActionListDoubleClick( CASTLE & item )
{
    if ( item ) {
        Game::OpenCastleDialog( *item );
    }
}

void Interface::CastleIcons::ActionListSingleClick( CASTLE & item )
{
    if ( item ) {
        Interface::AdventureMap & I = Interface::AdventureMap::Get();

        I.SetFocus( item );
        I.RedrawFocus();
    }
}

void Interface::CastleIcons::ActionListPressRight( CASTLE & item )
{
    if ( item && H1::showCastleListQuickInfo( *item ) ) {
        return;
    }
    if ( item ) {
        Dialog::QuickInfoAtPosition( *item, { _topLeftCorner.x - 1, _topLeftCorner.y } );
    }
}

void Interface::CastleIcons::showIcons( const bool show )
{
    IconsBar::setShow( show );

    if ( IconsBar::isVisible() ) {
        if ( show ) {
            GetScrollbar().show();
        }
        else {
            GetScrollbar().hide();
        }
    }
}

void Interface::CastleIcons::setPos( const int32_t px, const int32_t py )
{
    Castle * selectedCastle = isSelected() ? GetCurrent() : nullptr;

    const int icnscroll = Settings::Get().isEvilInterfaceEnabled() ? ICN::SCROLLE : ICN::SCROLL;

    _topLeftCorner = fheroes2::Point( px, py );
    SetTopLeft( _topLeftCorner );
    setScrollBarArea( { px + iconsCursorWidth + 3, py + 19, 10, iconsCursorHeight * _iconsCount - 38 } );

    VecCastles & castles = world.GetKingdom( Settings::Get().CurrentColor() ).GetCastles();

    const fheroes2::Sprite & originalSlider = Assets::getImage( icnscroll, 4 );
    fheroes2::Image scrollbarSlider
        = fheroes2::generateScrollbarSlider( originalSlider, false, iconsCursorHeight * _iconsCount - 38, _iconsCount, static_cast<int32_t>( castles.size() ),
                                             { 0, 0, originalSlider.width(), 8 }, { 0, 7, originalSlider.width(), 8 } );

    setScrollBarImage( std::move( scrollbarSlider ) );
    SetScrollButtonUp( icnscroll, 0, 1, { px + iconsCursorWidth + 1, py + 1 } );
    SetScrollButtonDn( icnscroll, 2, 3, { px + iconsCursorWidth + 1, py + _iconsCount * iconsCursorHeight - 15 } );
    SetAreaMaxItems( _iconsCount );
    SetAreaItems( { px, py, iconsCursorWidth, _iconsCount * iconsCursorHeight } );
    DisableHotkeys( true );

    SetListContent( castles );
    Reset();

    if ( selectedCastle ) {
        SetCurrent( selectedCastle );
    }
}

void Interface::HeroesIcons::RedrawItem( const HEROES & item, int32_t ox, int32_t oy, bool current )
{
    if ( item && _show && H1::isGraphicsMode() ) {
        // homm1-remaster (FUN_00459eac, note v57): slot frame in colour 0xc5 for the selected hero (0 otherwise), background
        // locators.icn 8 at +1, then the hero picture at +5.
        fheroes2::Display & display = fheroes2::Display::instance();
        const std::vector<fheroes2::Sprite> & locators = H1::getIcn( "locators.icn" );
        const std::vector<fheroes2::Sprite> & locatorsMono = H1::getIcnMono( "locators.icn" );
        if ( locatorsMono.size() > 20 ) {
            H1::UI::drawMonoSprite( display, locatorsMono[20], ox, oy, current ? 0xc5 : 0 );
        }
        if ( locators.size() > 8 ) {
            fheroes2::Blit( locators[8], display, ox + 1 + locators[8].x(), oy + 1 + locators[8].y() );
        }
        redrawHeroesIcon( *item, ox + 5, oy + 5 );
        return;
    }
    if ( item && _show ) {
        redrawHeroesIcon( *item, ox + 5, oy + 5 );

        if ( current ) {
            fheroes2::Blit( _marker, fheroes2::Display::instance(), ox, oy );
        }
    }
}

void Interface::HeroesIcons::RedrawBackground( const fheroes2::Point & pos )
{
    redrawBackground( fheroes2::Display::instance(), pos, _show ? _size() : 0 );
}

void Interface::HeroesIcons::ActionCurrentUp()
{
    Interface::AdventureMap::Get().SetFocus( GetCurrent(), false );
}

void Interface::HeroesIcons::ActionCurrentDn()
{
    Interface::AdventureMap::Get().SetFocus( GetCurrent(), false );
}

void Interface::HeroesIcons::ActionListDoubleClick( HEROES & item )
{
    if ( item ) {
        if ( Game::HotKeyHoldEvent( Game::HotKeyEvent::WORLD_MOVE_HERO_PORTRAIT_UP_MODIFIER ) ) {
            rotateHeroPortraits( item );
        }
        else {
            Game::OpenHeroesDialog( *item, false, true );
        }
    }
}

void Interface::HeroesIcons::ActionListSingleClick( HEROES & item )
{
    if ( item ) {
        if ( Game::HotKeyHoldEvent( Game::HotKeyEvent::WORLD_MOVE_HERO_PORTRAIT_UP_MODIFIER ) ) {
            rotateHeroPortraits( item );
        }
        else {
            Interface::AdventureMap & I = Interface::AdventureMap::Get();

            I.SetFocus( item, false );
            I.RedrawFocus();
        }
    }
}

void Interface::HeroesIcons::ActionListPressRight( HEROES & item )
{
    // homm1-remaster: list slot = 32-pixel row under the mouse (hero list items 0x69 + slot x 7, FUN_00459eac).
    if ( item && H1::showHeroListQuickInfo( *item, ( LocalEvent::Get().getMouseCursorPos().y - _topLeftCorner.y ) / 32 ) ) {
        return;
    }
    if ( item ) {
        Dialog::QuickInfoAtPosition( *item, { _topLeftCorner.x - 1, _topLeftCorner.y } );
    }
}

void Interface::HeroesIcons::ActionListLongPress( HEROES & item )
{
    if ( item ) {
        rotateHeroPortraits( item );
    }
}

void Interface::HeroesIcons::showIcons( const bool show )
{
    IconsBar::setShow( show );

    if ( IconsBar::isVisible() ) {
        if ( show ) {
            GetScrollbar().show();
        }
        else {
            GetScrollbar().hide();
        }
    }
}

void Interface::HeroesIcons::setPos( const int32_t px, const int32_t py )
{
    Heroes * selectedHero = isSelected() ? GetCurrent() : nullptr;

    const int icnscroll = Settings::Get().isEvilInterfaceEnabled() ? ICN::SCROLLE : ICN::SCROLL;

    _topLeftCorner = fheroes2::Point( px, py );
    SetTopLeft( _topLeftCorner );
    setScrollBarArea( { px + iconsCursorWidth + 3, py + 19, 10, iconsCursorHeight * _iconsCount - 38 } );

    VecHeroes & heroes = world.GetKingdom( Settings::Get().CurrentColor() ).GetHeroes();

    const fheroes2::Sprite & originalSlider = Assets::getImage( icnscroll, 4 );
    fheroes2::Image scrollbarSlider
        = fheroes2::generateScrollbarSlider( originalSlider, false, iconsCursorHeight * _iconsCount - 38, _iconsCount, static_cast<int32_t>( heroes.size() ),
                                             { 0, 0, originalSlider.width(), 8 }, { 0, 7, originalSlider.width(), 8 } );

    setScrollBarImage( std::move( scrollbarSlider ) );
    SetScrollButtonUp( icnscroll, 0, 1, { px + iconsCursorWidth + 1, py + 1 } );
    SetScrollButtonDn( icnscroll, 2, 3, { px + iconsCursorWidth + 1, py + _iconsCount * iconsCursorHeight - 15 } );
    SetAreaMaxItems( _iconsCount );
    SetAreaItems( { px, py, iconsCursorWidth, _iconsCount * iconsCursorHeight } );
    DisableHotkeys( true );

    SetListContent( heroes );
    Reset();

    if ( selectedHero ) {
        SetCurrent( selectedHero );
    }
}

Interface::IconsPanel::IconsPanel( AdventureMap & interface )
    : BorderWindow( { 0, 0, 144, 128 } )
    , _interface( interface )
{
    _sfMarker.resize( iconsCursorWidth, iconsCursorHeight );
    _sfMarker.reset();

    fheroes2::DrawBorder( _sfMarker, fheroes2::GetColorId( 0xA0, 0xE0, 0xE0 ) );
}

void Interface::IconsPanel::SavePosition()
{
    Settings & conf = Settings::Get();

    conf.SetPosIcons( GetRect().getPosition() );
    conf.Save( Settings::configFileName );
}

void Interface::IconsPanel::setRedraw( const HeroesCastlesIcons type ) const
{
    if ( !IconsBar::isVisible() ) {
        return;
    }

    switch ( type ) {
    case ICON_HEROES:
        _interface.setRedraw( REDRAW_HEROES );
        break;
    case ICON_CASTLES:
        _interface.setRedraw( REDRAW_CASTLES );
        break;
    case ICON_ANY:
        _interface.setRedraw( REDRAW_ICONS );
        break;
    default:
        break;
    }
}

void Interface::IconsPanel::SetPos( int32_t x, int32_t y )
{
    int32_t iconsCount = 0;

    if ( Settings::Get().isHideInterfaceEnabled() ) {
        iconsCount = 2;
    }
    else {
        const int32_t count_h = ( fheroes2::Display::instance().height() - fheroes2::Display::DEFAULT_HEIGHT ) / fheroes2::tileWidthPx;
        iconsCount = count_h > 3 ? 8 : ( count_h < 3 ? 4 : 7 );
    }

    BorderWindow::SetPosition( x, y, 144, iconsCount * iconsCursorHeight );

    _heroesIcons.setIconsCount( iconsCount );
    _castleIcons.setIconsCount( iconsCount );

    const fheroes2::Rect & rect = GetArea();

    _heroesIcons.setPos( rect.x, rect.y );
    _castleIcons.setPos( rect.x + 72, rect.y );
}

void Interface::IconsPanel::_redraw()
{
    if ( !IconsBar::isVisible() ) {
        return;
    }

    if ( Settings::Get().isHideInterfaceEnabled() ) {
        BorderWindow::Redraw();
    }

    _heroesIcons.Redraw();
    _castleIcons.Redraw();
}

void Interface::IconsPanel::queueEventProcessing()
{
    captureMouse();

    // Move the window border
    if ( Settings::Get().ShowIcons() && BorderWindow::QueueEventProcessing() ) {
        setRedraw();
    }
    else {
        if ( _heroesIcons.QueueEventProcessing() ) {
            if ( _heroesIcons.isSelected() ) {
                _castleIcons.Unselect();
            }

            setRedraw();
        }
        else if ( _castleIcons.QueueEventProcessing() ) {
            if ( _castleIcons.isSelected() ) {
                _heroesIcons.Unselect();
            }

            setRedraw();
        }
    }
}

void Interface::IconsPanel::select( Heroes * const hero )
{
    _castleIcons.Unselect();
    _heroesIcons.SetCurrent( hero );
}

void Interface::IconsPanel::select( Castle * const castle )
{
    _heroesIcons.Unselect();
    _castleIcons.SetCurrent( castle );
}

void Interface::IconsPanel::resetIcons( const HeroesCastlesIcons type )
{
    const PlayerColor color = Settings::Get().CurrentColor();
    Kingdom & kingdom = world.GetKingdom( color );

    Player * player = Players::Get( color );
    assert( player != nullptr );

    if ( !kingdom.isControlAI() || player->isAIAutoControlMode() ) {
        const int icnscroll = Settings::Get().isEvilInterfaceEnabled() ? ICN::SCROLLE : ICN::SCROLL;

        const fheroes2::Sprite & originalSlider = Assets::getImage( icnscroll, 4 );

        if ( type & ICON_HEROES ) {
            fheroes2::Image scrollbarSlider = fheroes2::generateScrollbarSlider( originalSlider, false, iconsCursorHeight * _heroesIcons.getIconsCount() - 38,
                                                                                 _heroesIcons.getIconsCount(), static_cast<int32_t>( kingdom.GetHeroes().size() ),
                                                                                 { 0, 0, originalSlider.width(), 8 }, { 0, 7, originalSlider.width(), 8 } );
            _heroesIcons.setScrollBarImage( std::move( scrollbarSlider ) );
            _heroesIcons.SetListContent( kingdom.GetHeroes() );

            // SetListContent() selects the first item (hero) in the list. We reset it by unselecting.
            _heroesIcons.Unselect();
        }

        if ( type & ICON_CASTLES ) {
            fheroes2::Image scrollbarSlider = fheroes2::generateScrollbarSlider( originalSlider, false, iconsCursorHeight * _castleIcons.getIconsCount() - 38,
                                                                                 _castleIcons.getIconsCount(), static_cast<int32_t>( kingdom.GetCastles().size() ),
                                                                                 { 0, 0, originalSlider.width(), 8 }, { 0, 7, originalSlider.width(), 8 } );
            _castleIcons.setScrollBarImage( std::move( scrollbarSlider ) );
            _castleIcons.SetListContent( kingdom.GetCastles() );

            // SetListContent() selects the first item (castle/town) in the list. We reset it by unselecting.
            _castleIcons.Unselect();
        }
    }
}

void Interface::IconsPanel::hideIcons( const HeroesCastlesIcons type )
{
    if ( type & ICON_HEROES ) {
        _heroesIcons.showIcons( false );
    }
    if ( type & ICON_CASTLES ) {
        _castleIcons.showIcons( false );
    }
}

void Interface::IconsPanel::showIcons( const HeroesCastlesIcons type )
{
    if ( type & ICON_HEROES ) {
        _heroesIcons.showIcons( true );
    }
    if ( type & ICON_CASTLES ) {
        _castleIcons.showIcons( true );
    }
}

void Interface::IconsPanel::_redrawIcons( const HeroesCastlesIcons type )
{
    if ( type & ICON_HEROES ) {
        _heroesIcons.Redraw();
    }
    if ( type & ICON_CASTLES ) {
        _castleIcons.Redraw();
    }
}

bool Interface::IconsPanel::isSelected( const HeroesCastlesIcons type ) const
{
    if ( type & ICON_HEROES ) {
        return _heroesIcons.isSelected();
    }
    if ( type & ICON_CASTLES ) {
        return _castleIcons.isSelected();
    }

    return false;
}
