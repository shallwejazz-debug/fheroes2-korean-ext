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

#include "cursor.h"

#include <algorithm>
#include <vector>

#include "h1_gfx.h" // homm1-remaster

#include "game_assets.h"
#include "icn.h"
#include "image.h"
#include "localevent.h"

Cursor & Cursor::Get()
{
    static Cursor _cursor;
    return _cursor;
}

void Cursor::SetThemes( const int theme, const bool force /* = false */ )
{
    if ( _theme == theme && !force ) {
        return;
    }

    _theme = theme;

    // homm1-remaster (note v159): the adventure map cursors from the HoMM1 advmice.icn (32 frames: 0 pointer, 1 wait, 2 hero, 3 castle,
    // then for days 0..3 (frame + 6 x day) 4 move, 5 fight, 6 boat, 7 anchor, 8 meet, 9 action; 0x1c + day boat action), as the original
    // FUN_00456dde picks them (FUN_00473460), with the hotspot of advmice.mse. The scroll cursors stay the engine's.
    if ( H1::isGraphicsMode() ) {
        int frame = -1;
        if ( theme == POINTER ) {
            frame = 0;
        }
        else if ( theme == WAIT ) {
            frame = 1;
        }
        else if ( theme == HEROES ) {
            frame = 2;
        }
        else if ( theme == CASTLE ) {
            frame = 3;
        }
        else if ( theme >= CURSOR_HERO_MOVE && theme < CURSOR_HERO_MOVE + 7 * 8 ) {
            const int kind = ( theme - CURSOR_HERO_MOVE ) / 8;
            const int day = std::min( ( theme - CURSOR_HERO_MOVE ) % 8, 3 );
            frame = ( kind == 6 ) ? 0x1c + day : day * 6 + 4 + kind;
        }
        if ( frame >= 0 && H1::setMouseCursor( "advmice.mse", frame ) ) {
            // The engine's offset is used only by the scroll cursors' screen check.
            _offset = { 0, 0 };
            fheroes2::cursor().keepInScreenArea( false );
            const fheroes2::Point & pos = LocalEvent::Get().getMouseCursorPos();
            Move( pos.x, pos.y );
            return;
        }
    }

    // Video pointer cannot be properly rendered in black-white so we have to force to use color cursor.
    int icnID = ( _monochromeCursorThemes && ( theme != Cursor::POINTER_VIDEO ) ) ? ICN::MONO_CURSOR_ADVMBW : ICN::ADVMCO;
    switch ( 0xF000 & theme ) {
    case 0x3000:
        icnID = _monochromeCursorThemes ? ICN::MONO_CURSOR_SPELBW : ICN::SPELCO;
        break;
    case 0x2000:
        icnID = _monochromeCursorThemes ? ICN::MONO_CURSOR_CMSSBW : ICN::CMSECO;
        break;
    case 0x4000:
        icnID = _monochromeCursorThemes ? ICN::MONO_CURSOR_ADVENTURE_MAP : ICN::COLOR_CURSOR_ADVENTURE_MAP;
        break;
    default:
        break;
    }
    const fheroes2::Sprite & spr = Assets::getImage( icnID, 0xFF & theme );
    SetOffset( theme, { ( spr.width() - spr.x() ) / 2, ( spr.height() - spr.y() ) / 2 } );

    fheroes2::Cursor & cursor = fheroes2::cursor();
    cursor.update( spr, -_offset.x, -_offset.y );
    // Force the scroll cursor to stay in screen boundaries.
    cursor.keepInScreenArea( theme >= SCROLL_TOP && theme <= SCROLL_TOPLEFT );

    // Apply new offset.
    const fheroes2::Point & currentPos = LocalEvent::Get().getMouseCursorPos();
    Move( currentPos.x, currentPos.y );
}

void Cursor::setCustomImage( const fheroes2::Image & image, const fheroes2::Point & offset )
{
    _theme = NONE;

    fheroes2::Cursor & cursor = fheroes2::cursor();
    cursor.update( image, -offset.x, -offset.y );
    cursor.keepInScreenArea( false );

    // Immediately apply new mouse offset.
    const fheroes2::Point & currentPos = LocalEvent::Get().getMouseCursorPos();
    _offset = offset;

    Move( currentPos.x, currentPos.y );
}

fheroes2::Rect Cursor::updateCursorPosition( const int32_t x, const int32_t y )
{
    if ( fheroes2::cursor().isSoftwareEmulation() ) {
        Cursor::Get().Move( x, y );
        if ( fheroes2::cursor().isVisible() ) {
            return { x, y, 1, 1 };
        }
    }

    return {};
}

void Cursor::Move( int32_t x, int32_t y ) const
{
    fheroes2::cursor().setPosition( x + _offset.x, y + _offset.y );
}

void Cursor::SetOffset( const int name, const fheroes2::Point & defaultOffset )
{
    switch ( name ) {
    case Cursor::POINTER:
    case Cursor::POINTER_VIDEO:
    case Cursor::WAR_POINTER:
        _offset = { 0, 0 };
        break;

    case Cursor::CURSOR_HERO_FIGHT:
    case Cursor::CURSOR_HERO_FIGHT_2:
    case Cursor::CURSOR_HERO_FIGHT_3:
    case Cursor::CURSOR_HERO_FIGHT_4:
    case Cursor::CURSOR_HERO_FIGHT_5:
    case Cursor::CURSOR_HERO_FIGHT_6:
    case Cursor::CURSOR_HERO_FIGHT_7:
    case Cursor::CURSOR_HERO_FIGHT_8:
        _offset = { -10, -11 };
        break;

    case Cursor::SCROLL_TOPRIGHT:
    case Cursor::SCROLL_RIGHT:
        _offset = { -15, 0 };
        break;

    case Cursor::SCROLL_BOTTOM:
    case Cursor::SCROLL_BOTTOMLEFT:
        _offset = { 0, -15 };
        break;

    case Cursor::SCROLL_BOTTOMRIGHT:
    case Cursor::SWORD_BOTTOMRIGHT:
        _offset = { -20, -20 };
        break;

    case Cursor::SWORD_BOTTOMLEFT:
        _offset = { -5, -20 };
        break;

    case Cursor::SWORD_TOPLEFT:
        _offset = { -5, -5 };
        break;

    case Cursor::SWORD_TOPRIGHT:
        _offset = { -20, -5 };
        break;

    case Cursor::SWORD_TOP:
        _offset = { -defaultOffset.x, -5 };
        break;

    case Cursor::SWORD_BOTTOM:
        _offset = { -defaultOffset.x, -25 };
        break;

    case Cursor::SWORD_LEFT:
        _offset = { -5, -7 };
        break;

    case Cursor::SWORD_RIGHT:
        _offset = { -25, -7 };
        break;

    default:
        _offset = { -defaultOffset.x, -defaultOffset.y };
        break;
    }
}

void Cursor::setVideoPlaybackCursor()
{
    if ( fheroes2::cursor().isSoftwareEmulation() ) {
        SetThemes( Cursor::POINTER_VIDEO );
    }
}

void Cursor::Refresh()
{
    Get().SetThemes( Get().Themes(), true );
}

int Cursor::DistanceThemes( const int theme, uint32_t distance )
{
    if ( distance == 0 ) {
        return POINTER;
    }

    if ( distance > 8 ) {
        distance = 8;
    }

    switch ( theme ) {
    case CURSOR_HERO_MOVE:
    case CURSOR_HERO_FIGHT:
    case CURSOR_HERO_BOAT:
    case CURSOR_HERO_ANCHOR:
    case CURSOR_HERO_MEET:
    case CURSOR_HERO_ACTION:
    case CURSOR_HERO_BOAT_ACTION:
        return theme + static_cast<int>( distance ) - 1;
    default:
        break;
    }

    return theme;
}

int Cursor::WithoutDistanceThemes( const int theme )
{
    if ( theme > CURSOR_HERO_MOVE && theme <= CURSOR_HERO_MOVE_8 ) {
        return CURSOR_HERO_MOVE;
    }
    if ( theme > CURSOR_HERO_FIGHT && theme <= CURSOR_HERO_FIGHT_8 ) {
        return CURSOR_HERO_FIGHT;
    }
    if ( theme > CURSOR_HERO_BOAT && theme <= CURSOR_HERO_BOAT_8 ) {
        return CURSOR_HERO_BOAT;
    }
    if ( theme > CURSOR_HERO_ANCHOR && theme <= CURSOR_HERO_ANCHOR_8 ) {
        return CURSOR_HERO_ANCHOR;
    }
    if ( theme > CURSOR_HERO_MEET && theme <= CURSOR_HERO_MEET_8 ) {
        return CURSOR_HERO_MEET;
    }
    if ( theme > CURSOR_HERO_ACTION && theme <= CURSOR_HERO_ACTION_8 ) {
        return CURSOR_HERO_ACTION;
    }
    if ( theme > CURSOR_HERO_BOAT_ACTION && theme <= CURSOR_HERO_BOAT_ACTION_8 ) {
        return CURSOR_HERO_BOAT_ACTION;
    }

    return theme;
}

CursorRestorer::CursorRestorer( const bool visible )
{
    fheroes2::cursor().show( visible );
}

CursorRestorer::CursorRestorer( const bool visible, const int theme )
{
    Cursor::Get().SetThemes( theme );

    fheroes2::cursor().show( visible );
}

CursorRestorer::~CursorRestorer()
{
    fheroes2::Cursor & cursorRenderer = fheroes2::cursor();

    const bool isShown = _visible && !cursorRenderer.isVisible();

    cursorRenderer.show( _visible );

    Cursor & cursor = Cursor::Get();

    const bool noThemeChange = ( cursor.Themes() == _theme );

    cursor.SetThemes( _theme );

    // In case of software emulated cursor when cursor theme is not changed and it is shown after it was hidden
    // we force render the cursor area. It is needed to reduce the cursor show delay.
    if ( isShown && noThemeChange && cursorRenderer.isSoftwareEmulation() ) {
        const fheroes2::Point & pos = LocalEvent::Get().getMouseCursorPos();
        fheroes2::Display::instance().render( { pos.x, pos.y, 1, 1 } );
    }
}

