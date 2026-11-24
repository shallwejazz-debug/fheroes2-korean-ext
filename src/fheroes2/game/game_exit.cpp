/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
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

#include "game_exit.h"

#include "dialog.h"
#include "game_mode.h"
#include "h1_gfx.h"
#include "h1_ui.h"
#include "translations.h"
#include "ui_dialog.h"

namespace Game
{
    fheroes2::GameMode processExitEvent()
    {
#if defined( __IPHONEOS__ )
        // iOS discourages to exit a running application.
        fheroes2::showStandardTextMessage( _( "Quit" ), _( "To exit fheroes2, press the Home button or swipe up." ), Dialog::OK );
#else
        if ( H1::isGraphicsMode() ) {
            // homm1-remaster: the original asks with its own Yes / No box (window close, decompile line 21571, and the Q key, line 36523:
            // FUN_00422fc2( "Are you sure you want to quit?", 2 )).
            return H1::UI::messageBox( "Are you sure you want to quit?", H1::UI::MESSAGE_YES_NO ) == 0x8005 ? fheroes2::GameMode::QUIT_GAME
                                                                                                       : fheroes2::GameMode::CANCEL;
        }
        if ( Dialog::YES & fheroes2::showStandardTextMessage( _( "Quit" ), _( "Are you sure you want to quit?" ), Dialog::YES | Dialog::NO ) ) {
            return fheroes2::GameMode::QUIT_GAME;
        }
#endif

        return fheroes2::GameMode::CANCEL;
    }
}
