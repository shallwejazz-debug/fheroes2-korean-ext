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

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "battle_cell.h"
#include "bin_info.h" // H2-EXT
#include "castle.h" // IWYU pragma: associated
#include "cursor.h"
#include "dialog.h"
#include "game_assets.h"
#include "game_delays.h"
#include "game_hotkeys.h"
#include "h2_ext.h" // H2-EXT
#include "h2_ext_monsters.h" // H2-EXT
#include "heroes.h"
#include "icn.h"
#include "image.h"
#include "image_palette.h" // H2-EXT
#include "kingdom.h"
#include "localevent.h"
#include "math_base.h"
#include "pal.h" // H2-EXT
#include "monster.h"
#include "monster_anim.h"
#include "resource.h"
#include "screen.h"
#include "settings.h"
#include "speed.h"
#include "tools.h" // H2-EXT
#include "translations.h"
#include "ui_button.h"
#include "ui_dialog.h"
#include "ui_text.h"

namespace
{
    const int32_t bottomBarOffsetY = 461;

    // H2-EXT: shrink a creature picture for the extension well by averaging each block of pixels and taking the nearest palette colour
    // (owner 2026-11-23 16:46 "고치긴 해야겠다": big creatures looked jagged when every other pixel was simply dropped).
    // A block is opaque when at least half of it is; otherwise it keeps the shadow or stays transparent.
    fheroes2::Sprite shrinkSmooth( const fheroes2::Sprite & in, const int32_t outWidth, const int32_t outHeight )
    {
        fheroes2::Sprite out( outWidth, outHeight );
        out.reset();

        const int32_t inWidth = in.width();
        const int32_t inHeight = in.height();
        const uint8_t * inImage = in.image();
        const uint8_t * inTransform = in.singleLayer() ? nullptr : in.transform();
        uint8_t * outImage = out.image();
        uint8_t * outTransform = out.transform();
        const fheroes2::RGB * palette = fheroes2::getRGBGamePalette();

        for ( int32_t oy = 0; oy < outHeight; ++oy ) {
            const int32_t y0 = oy * inHeight / outHeight;
            const int32_t y1 = std::max( y0 + 1, ( oy + 1 ) * inHeight / outHeight );
            for ( int32_t ox = 0; ox < outWidth; ++ox ) {
                const int32_t x0 = ox * inWidth / outWidth;
                const int32_t x1 = std::max( x0 + 1, ( ox + 1 ) * inWidth / outWidth );

                uint32_t red = 0;
                uint32_t green = 0;
                uint32_t blue = 0;
                int32_t opaque = 0;
                int32_t shadow = 0;
                int32_t total = 0;
                uint8_t shadowId = 0;
                for ( int32_t y = y0; y < y1; ++y ) {
                    for ( int32_t x = x0; x < x1; ++x ) {
                        const int32_t pos = y * inWidth + x;
                        const uint8_t transformId = inTransform == nullptr ? 0 : inTransform[pos];
                        ++total;
                        if ( transformId == 0 ) {
                            const fheroes2::RGB & color = palette[inImage[pos]];
                            red += color.r;
                            green += color.g;
                            blue += color.b;
                            ++opaque;
                        }
                        else if ( transformId > 1 ) {
                            ++shadow;
                            shadowId = transformId;
                        }
                    }
                }

                const int32_t outPos = oy * outWidth + ox;
                if ( opaque * 2 >= total ) {
                    // the game palette is in 6-bit steps; GetColorId takes 8-bit values
                    outImage[outPos] = fheroes2::GetColorId( static_cast<uint8_t>( red * 4 / opaque ), static_cast<uint8_t>( green * 4 / opaque ),
                                                             static_cast<uint8_t>( blue * 4 / opaque ) );
                    outTransform[outPos] = 0;
                }
                else if ( shadow * 2 >= total - opaque && shadow > 0 ) {
                    outTransform[outPos] = shadowId;
                }
            }
        }
        return out;
    }

    const std::array<uint32_t, Castle::maxNumOfDwellings> castleDwellings
        = { DWELLING_MONSTER1, DWELLING_MONSTER2, DWELLING_MONSTER3, DWELLING_MONSTER4, DWELLING_MONSTER5, DWELLING_MONSTER6 };

    uint32_t howManyRecruitMonster( const Castle & castle, Troops & tempCastleArmy, Troops & tempHeroArmy, const uint32_t dw, const Funds & add, Funds & res )
    {
        const Monster monsters( castle.GetRace(), castle.GetActualDwelling( dw ) );
        if ( !tempCastleArmy.CanJoinTroop( monsters ) && !tempHeroArmy.CanJoinTroop( monsters ) ) {
            return 0;
        }

        uint32_t count = castle.getMonstersInDwelling( dw );
        Funds payment;

        const Kingdom & kingdom = castle.GetKingdom();

        while ( count ) {
            payment = monsters.GetCost() * count;
            res = payment;
            payment += add;
            if ( kingdom.AllowPayment( payment ) ) {
                break;
            }
            --count;
        }

        if ( count > 0 ) {
            if ( tempCastleArmy.CanJoinTroop( monsters ) ) {
                tempCastleArmy.JoinTroop( monsters, count, false );
            }
            else if ( tempHeroArmy.CanJoinTroop( monsters ) ) {
                tempHeroArmy.JoinTroop( monsters, count, false );
            }
        }

        return count;
    }

    BuildingType getPressedBuildingHotkey()
    {
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_1 ) ) {
            return DWELLING_MONSTER1;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_2 ) ) {
            return DWELLING_MONSTER2;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_3 ) ) {
            return DWELLING_MONSTER3;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_4 ) ) {
            return DWELLING_MONSTER4;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_5 ) ) {
            return DWELLING_MONSTER5;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_6 ) ) {
            return DWELLING_MONSTER6;
        }

        return BUILD_NOTHING;
    }
}

bool Castle::_recruitCastleMax( const Troops & currentCastleArmy )
{
    std::vector<Troop> totalRecruitmentResult;
    Funds currentMonsterCost;
    Funds totalMonstersCost;
    std::string monstersRecruitedText;

    Troops tempCastleArmy( currentCastleArmy );
    Troops tempGuestArmy;

    const Heroes * hero = GetHero();

    if ( hero ) {
        tempGuestArmy.Insert( hero->GetArmy().getTroops() );
    }

    // In this loop we should go in reverse order - from strongest monsters to weakest in order to purchase most of the best monsters.
    for ( size_t id = 0; id < maxNumOfDwellings; ++id ) {
        const uint32_t dwellingType = castleDwellings[maxNumOfDwellings - id - 1];
        const uint32_t recruitableNumber = howManyRecruitMonster( *this, tempCastleArmy, tempGuestArmy, dwellingType, totalMonstersCost, currentMonsterCost );

        if ( recruitableNumber == 0 ) {
            continue;
        }
        const Monster recruitableMonster( _race, GetActualDwelling( dwellingType ) );

        totalRecruitmentResult.emplace_back( recruitableMonster, recruitableNumber );
        totalMonstersCost += currentMonsterCost;

        monstersRecruitedText += recruitableMonster.GetPluralName( recruitableNumber );
        monstersRecruitedText += " : ";
        monstersRecruitedText += std::to_string( recruitableNumber );
        monstersRecruitedText += '\n';
    }

    const fheroes2::FontType normalWhite = fheroes2::FontType::normalWhite();

    if ( monstersRecruitedText.empty() ) {
        bool isCreaturePresent = false;
        bool canAffordOneCreature = false;

        for ( const uint32_t dwellingType : castleDwellings ) {
            if ( getMonstersInDwelling( dwellingType ) > 0 ) {
                const Monster monsters( _race, dwellingType );
                const Funds payment = monsters.GetCost();

                if ( GetKingdom().AllowPayment( payment ) ) {
                    canAffordOneCreature = true;
                }
                isCreaturePresent = true;
                break;
            }
        }
        if ( isCreaturePresent ) {
            if ( !canAffordOneCreature ) {
                fheroes2::showStandardTextMessage( {}, _( "Not enough resources to recruit creatures." ), Dialog::OK );
            }
            else {
                fheroes2::showStandardTextMessage( {}, _( "You are unable to recruit at this time, your ranks are full." ), Dialog::OK );
            }
        }
        else {
            fheroes2::showStandardTextMessage( {}, _( "No creatures available for purchase." ), Dialog::OK );
        }
    }
    else if ( fheroes2::showResourceMessage( fheroes2::Text( _( "Recruit Creatures" ), fheroes2::FontType::normalYellow() ),
                                             fheroes2::Text( std::move( monstersRecruitedText ), normalWhite ), Dialog::YES | Dialog::NO, totalMonstersCost )
              == Dialog::YES ) {
        for ( const Troop & troop : totalRecruitmentResult ) {
            RecruitMonster( troop, false );
        }

        return true;
    }

    return false;
}

void Castle::_openWell()
{
    if ( H2Ext::isOn() ) {
        _openExtensionWell();
        return;
    }

    fheroes2::Display & display = fheroes2::Display::instance();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const fheroes2::ImageRestorer restorer( display, ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2,
                                            ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2, fheroes2::Display::DEFAULT_WIDTH,
                                            fheroes2::Display::DEFAULT_HEIGHT );

    const fheroes2::Rect roi = restorer.rect();

    const int32_t buttonOffsetY = roi.y + bottomBarOffsetY;

    // MAX button.
    fheroes2::Button buttonMax( roi.x, buttonOffsetY, ICN::BUTTON_WELL_MAX, 0, 1 );

    // EXIT button.
    fheroes2::Button buttonExit( roi.x + roi.width - Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width(), buttonOffsetY, ICN::BUTTON_GUILDWELL_EXIT, 0, 1 );

    const std::array<fheroes2::Rect, maxNumOfDwellings> rectMonster
        = { fheroes2::Rect( roi.x + 20, roi.y + 18, 288, 124 ),   fheroes2::Rect( roi.x + 20, roi.y + 168, 288, 124 ),
            fheroes2::Rect( roi.x + 20, roi.y + 318, 288, 124 ),  fheroes2::Rect( roi.x + 334, roi.y + 18, 288, 124 ),
            fheroes2::Rect( roi.x + 334, roi.y + 168, 288, 124 ), fheroes2::Rect( roi.x + 334, roi.y + 318, 288, 124 ) };

    const std::array<Monster, maxNumOfDwellings> castleMonster
        = { Monster( _race, GetActualDwelling( castleDwellings[0] ) ), Monster( _race, GetActualDwelling( castleDwellings[1] ) ),
            Monster( _race, GetActualDwelling( castleDwellings[2] ) ), Monster( _race, GetActualDwelling( castleDwellings[3] ) ),
            Monster( _race, GetActualDwelling( castleDwellings[4] ) ), Monster( _race, GetActualDwelling( castleDwellings[5] ) ) };

    std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> monsterAnimInfo
        = { fheroes2::RandomMonsterAnimation( castleMonster[0], false ), fheroes2::RandomMonsterAnimation( castleMonster[1], false ),
            fheroes2::RandomMonsterAnimation( castleMonster[2], false ), fheroes2::RandomMonsterAnimation( castleMonster[3], false ),
            fheroes2::RandomMonsterAnimation( castleMonster[4], false ), fheroes2::RandomMonsterAnimation( castleMonster[5], false ) };

    fheroes2::Image background;
    background._disableTransformLayer();
    background.resize( roi.width, roi.height );
    _wellRedrawBackground( background );

    LocalEvent & le = LocalEvent::Get();

    Game::passAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY );

    while ( le.HandleEvents( Game::isDelayNeeded( { Game::DelayType::CASTLE_UNIT_DELAY } ) ) ) {
        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );

        buttonMax.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonMax.area() ) );
        const BuildingType pressedHotkeyBuildingID = getPressedBuildingHotkey();

        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            break;
        }
        if ( le.MouseClickLeft( buttonMax.area() ) || HotKeyPressEvent( Game::HotKeyEvent::TOWN_WELL_BUY_ALL ) ) {
            const Troops & currentArmy = GetArmy();
            if ( _recruitCastleMax( currentArmy ) ) {
                // Update available monster count on background.

                for ( const uint32_t dwellingType : castleDwellings ) {
                    _wellRedrawAvailableMonsters( dwellingType, true, background );
                }
            }
        }

        for ( size_t dwellingId = 0; dwellingId < maxNumOfDwellings; ++dwellingId ) {
            if ( _constructedBuildings & castleDwellings[dwellingId] ) {
                if ( le.MouseClickLeft( rectMonster[dwellingId] ) || pressedHotkeyBuildingID == castleDwellings[dwellingId] ) {
                    if ( RecruitMonster( Dialog::RecruitMonster( castleMonster[dwellingId], _dwelling[dwellingId], true, 2 ) ) ) {
                        // Update available monster count on background.

                        _wellRedrawAvailableMonsters( castleDwellings[dwellingId], true, background );
                    }

                    break;
                }

                if ( le.isMouseRightButtonPressedInArea( rectMonster[dwellingId] ) ) {
                    Dialog::DwellingInfo( castleMonster[dwellingId], _dwelling[dwellingId] );

                    break;
                }
            }
        }

        if ( Game::validateAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY ) ) {
            fheroes2::Copy( background, 0, 0, display, roi.x, roi.y, roi.width, roi.height );

            _wellRedrawMonsterAnimation( roi, monsterAnimInfo );

            buttonMax.draw();
            buttonExit.draw();
            display.render( roi );
        }

        if ( le.isMouseRightButtonPressedInArea( buttonExit.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Exit" ), _( "Exit this menu." ), Dialog::ZERO );
        }

        if ( le.isMouseRightButtonPressedInArea( buttonMax.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Max" ), _( "Hire all creatures in the town." ), Dialog::ZERO );
        }
    }
}

void Castle::_wellRedrawAvailableMonsters( const uint32_t dwellingType, const bool restoreBackground, fheroes2::Image & background ) const
{
    if ( !( _constructedBuildings & dwellingType ) ) {
        // This building has not been built.
        return;
    }

    uint32_t population = 0;
    fheroes2::Point offset{ 20, 121 };

    switch ( dwellingType ) {
    case DWELLING_MONSTER1:
        population = _dwelling[0];
        break;
    case DWELLING_MONSTER2:
        population = _dwelling[1];
        offset.y += 150;
        break;
    case DWELLING_MONSTER3:
        population = _dwelling[2];
        offset.y += 300;
        break;
    case DWELLING_MONSTER4:
        offset.x += 314;
        population = _dwelling[3];
        break;
    case DWELLING_MONSTER5:
        population = _dwelling[4];
        offset.x += 314;
        offset.y += 150;
        break;
    case DWELLING_MONSTER6:
        population = _dwelling[5];
        offset.x += 314;
        offset.y += 300;
        break;
    default:
        // Have you added a new dwelling?
        assert( 0 );
        break;
    }

    if ( restoreBackground ) {
        // Restore background under "Available: <number>" text.
        const fheroes2::Sprite & wellBackground = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::WELLBKG_EVIL : ICN::WELLBKG, 0 );

        fheroes2::Copy( wellBackground, offset.x, offset.y, background, offset.x, offset.y, 137, fheroes2::getFontHeight( fheroes2::FontSize::NORMAL ) );
    }

    std::string textString = _( "Available" );
    textString += ':';

    fheroes2::Text text( std::to_string( population ), fheroes2::FontType::normalYellow() );
    const int32_t populationOffsetX{ 109 - text.width() / 2 };
    text.draw( offset.x + populationOffsetX, offset.y, background );

    const int32_t textLength{ 106 };
    const int32_t allowedTextLength{ std::min( populationOffsetX, textLength ) };

    text.set( std::move( textString ), fheroes2::FontType::smallWhite() );
    if ( text.width() > allowedTextLength ) {
        text.fitToOneRow( allowedTextLength );
        text.draw( offset.x, offset.y + 2, background );
    }
    else {
        text.draw( offset.x + ( textLength - text.width() ) / 2, offset.y + 2, background );
    }
}

void Castle::_wellRedrawBackground( fheroes2::Image & background ) const
{
    const fheroes2::Sprite & wellBackground = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::WELLBKG_EVIL : ICN::WELLBKG, 0 );
    const int32_t backgroundWidth = wellBackground.width();

    fheroes2::Copy( wellBackground, 0, 0, background, 0, 0, backgroundWidth, bottomBarOffsetY );

    // TODO: Make Well bottom bar and buttons for Evil interface.

    // The original ICN::WELLBKG image has incorrect bottom message bar with no yellow outline. Also the original graphics did not have MAX button.
    const fheroes2::Sprite & bottomBar = Assets::getImage( ICN::SMALLBAR, 0 );
    const int32_t barHeight = bottomBar.height();
    const int32_t exitWidth = Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width();
    const int32_t buttonMaxWidth = Assets::getImage( ICN::BUTTON_WELL_MAX, 0 ).width();
    // ICN::SMALLBAR image's first column contains all black pixels. This should not be drawn.
    fheroes2::Copy( bottomBar, 1, 0, background, buttonMaxWidth, bottomBarOffsetY, backgroundWidth / 2 - buttonMaxWidth, barHeight );
    fheroes2::Copy( bottomBar, bottomBar.width() - backgroundWidth / 2 + exitWidth - 1, 0, background, backgroundWidth / 2, bottomBarOffsetY,
                    backgroundWidth / 2 - exitWidth + 1, barHeight );

    // The original assets Well background has a transparent line to the right of EXIT button and it is not covered by any other image. Fill it with the black color.
    fheroes2::Fill( background, background.width() - 1, bottomBarOffsetY, 1, bottomBar.height(), static_cast<uint8_t>( 0 ) );

    const fheroes2::Rect textRoi{ 0, bottomBarOffsetY, backgroundWidth, bottomBar.height() - 1 };
    fheroes2::Text text( _( "Town Population Information and Statistics" ), fheroes2::FontType::normalWhite() );
    text.drawInRoi( 315 - text.width() / 2, bottomBarOffsetY + 3, background, textRoi );

    const fheroes2::FontType statsFontType = fheroes2::FontType::smallWhite();

    for ( const uint32_t dwellingType : castleDwellings ) {
        // By default the 'icnIndex' and 'offset' values are set for DWELLING_MONSTER1.
        uint32_t icnIndex = 19;
        fheroes2::Point offset{ 0, 1 };

        switch ( dwellingType ) {
        case DWELLING_MONSTER1:
            break;
        case DWELLING_MONSTER2:
            offset.y = 151;
            icnIndex = DWELLING_UPGRADE2 & _constructedBuildings ? 25 : 20;
            break;
        case DWELLING_MONSTER3:
            offset.y = 301;
            icnIndex = DWELLING_UPGRADE3 & _constructedBuildings ? 26 : 21;
            break;
        case DWELLING_MONSTER4:
            offset.x = 314;
            icnIndex = DWELLING_UPGRADE4 & _constructedBuildings ? 27 : 22;
            break;
        case DWELLING_MONSTER5:
            offset.x = 314;
            offset.y = 151;
            icnIndex = DWELLING_UPGRADE5 & _constructedBuildings ? 28 : 23;
            break;
        case DWELLING_MONSTER6:
            offset.x = 314;
            offset.y = 301;
            icnIndex = DWELLING_UPGRADE7 & _constructedBuildings ? 30 : ( DWELLING_UPGRADE6 & _constructedBuildings ? 29 : 24 );
            break;
        default:
            // Have you added a new dwelling?
            assert( 0 );
            break;
        }

        const uint32_t actualDwellindType = GetActualDwelling( dwellingType );
        const Monster monster( _race, actualDwellindType );

        // Dwelling building image.
        fheroes2::Point renderPoint( offset.x + 21, offset.y + 35 );
        const fheroes2::Sprite & dwellingImage = Assets::getImage( ICN::getBuildingIcnId( _race ), icnIndex );
        fheroes2::Copy( dwellingImage, 0, 0, background, renderPoint.x, renderPoint.y, dwellingImage.width(), dwellingImage.height() );

        // Dwelling name.
        text.set( GetStringBuilding( actualDwellindType, _race ), statsFontType );
        renderPoint.x = offset.x + 86 - text.width() / 2;
        renderPoint.y = offset.y + 104;
        text.draw( renderPoint.x, renderPoint.y, background );

        // Creature name.
        text.set( monster.GetMultiName(), statsFontType );
        renderPoint.x = offset.x + 122 - text.width() / 2;
        renderPoint.y = offset.y + 19;
        text.draw( renderPoint.x, renderPoint.y, background );

        // attack
        std::string textString;
        textString = _( "Attack" );
        textString += ": ";
        textString += std::to_string( monster.GetAttack() );

        text.set( textString, statsFontType );

        // Set initial X and Y offset.
        renderPoint.x = offset.x + 269;
        renderPoint.y = offset.y + 22;

        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // defense
        textString = _( "Defense" );
        textString += ": ";
        textString += std::to_string( monster.GetDefense() );

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // damage
        textString = _( "Damage" );
        textString += ": ";

        const uint32_t monsterMinDamage = monster.GetDamageMin();
        const uint32_t monsterMaxDamage = monster.GetDamageMax();

        textString += std::to_string( monsterMinDamage );

        if ( monsterMinDamage != monsterMaxDamage ) {
            textString += '-';
            textString += std::to_string( monsterMaxDamage );
        }

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // hp
        textString = _( "HP" );
        textString += ": ";
        textString += std::to_string( monster.GetHitPoints() );

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += 2 * ( text.height( text.width() ) ); // skip a line

        // speed
        textString = _( "Speed" );
        textString += ':';

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        text.set( Speed::String( monster.GetSpeed() ), statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += 2 * ( text.height( text.width() ) ); // skip a line

        // If dwelling is built show growth and available population.
        if ( _constructedBuildings & dwellingType ) {
            uint32_t monsterGrown = monster.GetGrown();
            monsterGrown += _constructedBuildings & BUILD_WELL ? GetGrownWell() : 0;

            if ( DWELLING_MONSTER1 & dwellingType ) {
                monsterGrown += _constructedBuildings & BUILD_WEL2 ? GetGrownWel2() : 0;
            }

            text.set( _( "Growth" ), statsFontType );
            text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
            renderPoint.y += text.height( text.width() );

            textString = "+ ";
            textString += std::to_string( monsterGrown );
            textString += " / ";
            textString += _( "week" );

            text.set( textString, statsFontType );
            text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );

            _wellRedrawAvailableMonsters( dwellingType, false, background );
        }
    }
}

void Castle::_wellRedrawMonsterAnimation( const fheroes2::Rect & roi, std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> & monsterAnimInfo ) const
{
    fheroes2::Display & display = fheroes2::Display::instance();

    for ( size_t monsterId = 0; monsterId < maxNumOfDwellings; ++monsterId ) {
        fheroes2::Point outPos( roi.x, roi.y + 1 );

        switch ( monsterId ) {
        case 0:
            break;
        case 1:
            outPos.y += 150;
            break;
        case 2:
            outPos.y += 300;
            break;
        case 3:
            outPos.x += 314;
            break;
        case 4:
            outPos.x += 314;
            outPos.y += 150;
            break;
        case 5:
            outPos.x += 314;
            outPos.y += 300;
            break;
        default:
            // Have you added a new dwelling?
            assert( 0 );
            break;
        }

        const Monster monster( _race, GetActualDwelling( castleDwellings[monsterId] ) );

        // monster
        const bool flipMonsterSprite = ( monsterId > 2 );

        const fheroes2::Sprite & smonster = Assets::getImage( monsterAnimInfo[monsterId].icnFile(), monsterAnimInfo[monsterId].frameId() );
        if ( flipMonsterSprite ) {
            outPos.x += 193 - ( smonster.x() + smonster.width() ) + ( monster.isWide() ? Battle::Cell::widthPx / 2 : 0 ) + monsterAnimInfo[monsterId].offset();
        }
        else {
            outPos.x += 193 + smonster.x() - ( monster.isWide() ? Battle::Cell::widthPx / 2 : 0 ) - monsterAnimInfo[monsterId].offset();
        }

        outPos.y += 124 + smonster.y();

        fheroes2::Point inPos( 0, 0 );
        fheroes2::Size inSize( smonster.width(), smonster.height() );

        if ( fheroes2::FitToRoi( smonster, inPos, display, outPos, inSize, roi ) ) {
            fheroes2::Blit( smonster, inPos, display, outPos, inSize, flipMonsterSprite );
        }

        monsterAnimInfo[monsterId].increment();
    }
}

// H2-EXT: the Well of an extension game (owner 2026-11-20 14:54). One row per tier: the base creature, then upgrade branch A and B.
// Tier 1 has no upgrade. Before the upgrade dwelling is built only the base creature can be hired; after it only the two branches
// (the base creature is shown greyed). A tier whose dwelling is not built is greyed. Clicking a creature hires it, right click shows it.
// MAX hires as before (the engine's upgrade, branch A, from an upgraded dwelling).
void Castle::_openExtensionWell()
{
    fheroes2::Display & display = fheroes2::Display::instance();
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const fheroes2::ImageRestorer restorer( display, ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2,
                                            ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2, fheroes2::Display::DEFAULT_WIDTH,
                                            fheroes2::Display::DEFAULT_HEIGHT );
    const fheroes2::Rect roi = restorer.rect();
    const int32_t buttonOffsetY = roi.y + bottomBarOffsetY;

    fheroes2::Button buttonMax( roi.x, buttonOffsetY, ICN::BUTTON_WELL_MAX, 0, 1 );
    fheroes2::Button buttonExit( roi.x + roi.width - Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width(), buttonOffsetY, ICN::BUTTON_GUILDWELL_EXIT, 0, 1 );

    struct Cell
    {
        Monster monster;
        size_t dwellingId{ 0 };
        bool isBase{ true };
        fheroes2::Rect area;
    };

    constexpr int32_t cellWidth = 204;
    constexpr int32_t cellHeight = 72;
    constexpr int32_t cellGap = 4;
    constexpr int32_t leftOffset = 8;
    constexpr int32_t topOffset = 4;

    std::vector<Cell> cells;
    for ( size_t id = 0; id < maxNumOfDwellings; ++id ) {
        const Monster base( _race, castleDwellings[id] );
        const int32_t rowY = topOffset + static_cast<int32_t>( id ) * ( cellHeight + cellGap );
        cells.push_back( { base, id, true, { leftOffset, rowY, cellWidth, cellHeight } } );

        std::vector<int> branches = H2Ext::upgradeChoices( base.GetID() );
        if ( branches.empty() && base.GetUpgrade() != base ) {
            branches.push_back( base.GetUpgrade().GetID() );
        }
        int32_t column = 1;
        for ( const int branch : branches ) {
            cells.push_back( { Monster( branch ), id, false, { leftOffset + column * ( cellWidth + cellGap ), rowY, cellWidth, cellHeight } } );
            ++column;
        }
    }

    const auto isDwellingBuilt = [this]( const size_t id ) { return ( _constructedBuildings & castleDwellings[id] ) != 0; };
    const auto isUpgraded = [this]( const size_t id ) { return GetActualDwelling( castleDwellings[id] ) != castleDwellings[id]; };
    const auto isActive = [&]( const Cell & cell ) {
        if ( !isDwellingBuilt( cell.dwellingId ) ) {
            return false;
        }
        return cell.isBase ? !isUpgraded( cell.dwellingId ) : isUpgraded( cell.dwellingId );
    };

    fheroes2::Image background;
    background._disableTransformLayer();
    background.resize( roi.width, roi.height );

    const auto redrawBackground = [&]() {
        _wellRedrawBackground( background );

        // Cover the original six boxes.
        const fheroes2::Sprite & texture = Assets::getImage( ICN::STONEBAK, 0 );
        for ( int32_t y = 0; y < bottomBarOffsetY; y += texture.height() ) {
            for ( int32_t x = 0; x < roi.width; x += texture.width() ) {
                fheroes2::Copy( texture, 0, 0, background, x, y, std::min( texture.width(), roi.width - x ), std::min( texture.height(), bottomBarOffsetY - y ) );
            }
        }

        const fheroes2::Sprite & leather = Assets::getImage( ICN::RECRBKG, 0 );

        for ( const Cell & cell : cells ) {
            const fheroes2::Rect & r = cell.area;
            // Plain leather from the hiring window (its left part has no boxes).
            fheroes2::Copy( leather, 20, 20, background, r.x, r.y, r.width / 2, r.height );
            fheroes2::Copy( leather, 20, 20, background, r.x + r.width / 2, r.y, r.width - r.width / 2, r.height );
            // palette 210 / 115 (dark gold / gold): 0xDD / 0xDB used before are in the colour-cycling range (214-241), so the frames
            // flashed while the well was open (owner 2026-11-21 00:21 "이 화면 자꾸 번쩍거린다")
            fheroes2::DrawRect( background, r, 210 );
            fheroes2::DrawRect( background, { r.x + 1, r.y + 1, r.width - 2, r.height - 2 }, 115 );

            // Creature picture, scaled down to the picture box when needed.
            const Bin_Info::MonsterAnimInfo & monsterInfo = Bin_Info::GetMonsterInfo( cell.monster.GetID() );
            const fheroes2::Sprite & sprite
                = Assets::getImage( cell.monster.GetMonsterSprite(),
                                    monsterInfo.animationFrames[Bin_Info::MonsterAnimInfo::STATIC].empty() ? 0 : monsterInfo.animationFrames[Bin_Info::MonsterAnimInfo::STATIC][0] );
            const int32_t boxWidth = 62;
            const int32_t boxHeight = cellHeight - 6;
            if ( !sprite.empty() ) {
                int32_t width = sprite.width();
                int32_t height = sprite.height();
                if ( width > boxWidth || height > boxHeight ) {
                    const double scale = std::min( static_cast<double>( boxWidth ) / width, static_cast<double>( boxHeight ) / height );
                    width = std::max( 1, static_cast<int32_t>( width * scale ) );
                    height = std::max( 1, static_cast<int32_t>( height * scale ) );
                    const fheroes2::Sprite scaled = shrinkSmooth( sprite, width, height );
                    fheroes2::Blit( scaled, background, r.x + 3 + ( boxWidth - width ) / 2, r.y + 3 + boxHeight - height );
                }
                else {
                    fheroes2::Blit( sprite, background, r.x + 3 + ( boxWidth - width ) / 2, r.y + 3 + boxHeight - height );
                }
            }

            const int32_t textX = r.x + 68;
            const int32_t textWidth = r.width - 72;
            const fheroes2::FontType small = fheroes2::FontType::smallWhite();
            constexpr int32_t lineStep = 11;
            int32_t lineY = r.y + 20;

            // Line 1: name (owner 15:19: more information beside the picture).
            fheroes2::Text available;
            if ( isDwellingBuilt( cell.dwellingId ) ) {
                std::string availableText = _( "Available: %{count}" );
                StringReplace( availableText, "%{count}", _dwelling[cell.dwellingId] );
                available.set( std::move( availableText ), fheroes2::FontType::smallWhite() );
            }
            fheroes2::Text text( cell.monster.GetMultiName(), fheroes2::FontType::normalYellow() );
            text.fitToOneRow( textWidth );
            text.draw( textX, r.y + 5, background );

            const auto drawLine = [&]( std::string line ) {
                text.set( std::move( line ), small );
                text.fitToOneRow( textWidth );
                text.draw( textX, lineY, background );
                lineY += lineStep;
            };

            // Lines 2-4: the numbers of the original Well (attack, defense, damage, hit points, speed, growth).
            std::string line = std::string( _( "Attack" ) ) + ' ' + std::to_string( cell.monster.GetAttack() ) + "  " + _( "Defense" ) + ' '
                               + std::to_string( cell.monster.GetDefense() );
            drawLine( std::move( line ) );

            line = std::string( _( "Damage" ) ) + ' ' + std::to_string( cell.monster.GetDamageMin() );
            if ( cell.monster.GetDamageMin() != cell.monster.GetDamageMax() ) {
                line += '-';
                line += std::to_string( cell.monster.GetDamageMax() );
            }
            line += "  ";
            line += _( "HP" );
            line += ' ';
            line += std::to_string( cell.monster.GetHitPoints() );
            drawLine( std::move( line ) );

            line = std::string( _( "Speed" ) ) + ' ' + Speed::String( cell.monster.GetSpeed() );
            if ( isDwellingBuilt( cell.dwellingId ) ) {
                uint32_t grown = cell.monster.GetGrown();
                grown += ( _constructedBuildings & BUILD_WELL ) ? GetGrownWell() : 0;
                if ( cell.dwellingId == 0 ) {
                    grown += ( _constructedBuildings & BUILD_WEL2 ) ? GetGrownWel2() : 0;
                }
                line += "  +";
                line += std::to_string( grown );
                line += '/';
                line += _( "week" );
            }
            drawLine( std::move( line ) );

            // Price of one creature.
            const Funds cost = cell.monster.GetCost();
            std::string price = std::to_string( cost.gold ) + ' ' + Resource::String( Resource::GOLD );
            for ( const int res : { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS } ) {
                if ( const int32_t value = cost.Get( res ); value > 0 ) {
                    price += ", ";
                    price += std::to_string( value );
                    price += ' ';
                    price += Resource::String( res );
                }
            }
            // Line 5: price, and the number left in the dwelling at the right end.
            text.set( std::move( price ), fheroes2::FontType::smallYellow() );
            text.fitToOneRow( textWidth - available.width() - 6 );
            text.draw( textX, lineY, background );
            available.draw( r.x + r.width - 5 - available.width(), lineY, background );

            if ( !isActive( cell ) ) {
                // Greyed out: darkened twice so that it is clearly told apart.
                for ( int i = 0; i < 2; ++i ) {
                    fheroes2::ApplyPalette( background, r.x, r.y, background, r.x, r.y, r.width, r.height, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
                }
            }
        }
    };

    redrawBackground();

    LocalEvent & le = LocalEvent::Get();
    bool redraw = true;

    while ( le.HandleEvents() ) {
        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );
        buttonMax.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonMax.area() ) );

        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            break;
        }
        if ( le.MouseClickLeft( buttonMax.area() ) || HotKeyPressEvent( Game::HotKeyEvent::TOWN_WELL_BUY_ALL ) ) {
            if ( _recruitCastleMax( GetArmy() ) ) {
                redrawBackground();
                redraw = true;
            }
        }
        if ( le.isMouseRightButtonPressedInArea( buttonExit.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Exit" ), _( "Exit this menu." ), Dialog::ZERO );
        }
        if ( le.isMouseRightButtonPressedInArea( buttonMax.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Max" ), _( "Hire all creatures in the town." ), Dialog::ZERO );
        }

        // Hotkeys 1-6 open the hiring window of the tier as before (an upgraded dwelling with the arrows between the branches).
        const BuildingType pressedHotkeyBuildingID = getPressedBuildingHotkey();
        for ( size_t id = 0; id < maxNumOfDwellings; ++id ) {
            if ( pressedHotkeyBuildingID == castleDwellings[id] && isDwellingBuilt( id ) ) {
                const Monster monster( _race, GetActualDwelling( castleDwellings[id] ) );
                if ( RecruitMonster( Dialog::RecruitMonster( monster, _dwelling[id], true, 2 ) ) ) {
                    redrawBackground();
                }
                redraw = true;
            }
        }

        for ( const Cell & cell : cells ) {
            const fheroes2::Rect area( roi.x + cell.area.x, roi.y + cell.area.y, cell.area.width, cell.area.height );
            if ( isActive( cell ) && le.MouseClickLeft( area ) ) {
                if ( RecruitMonster( Dialog::RecruitMonster( cell.monster, _dwelling[cell.dwellingId], false, 2 ) ) ) {
                    redrawBackground();
                }
                redraw = true;
                break;
            }
            if ( le.isMouseRightButtonPressedInArea( area ) ) {
                // The creature window with its abilities explained (owner 15:35: the abilities must be readable), shown while the button is held.
                Dialog::ArmyInfo( Troop( cell.monster, 0 ), Dialog::ZERO );
                redraw = true;
                break;
            }
        }

        if ( redraw ) {
            fheroes2::Copy( background, 0, 0, display, roi.x, roi.y, roi.width, roi.height );
            buttonMax.draw();
            buttonExit.draw();
            display.render( roi );
            redraw = false;
        }
    }
}
