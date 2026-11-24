/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2010 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "audio_manager.h"
#include "castle.h"
#include "color.h"
#include "dialog.h"
#include "dialog_selectitems.h"
#include "direction.h"
#include "game_interface.h"
#include "h1_gfx.h"
#include "h1_ui.h"
#include "h1_smallwin.h"
#include "h1_heroes.h"
#include "heroes.h" // IWYU pragma: associated
#include "interface_base.h"
#include "interface_gamearea.h"
#include "interface_radar.h"
#include "kingdom.h"
#include "logging.h"
#include "m82.h"
#include "maps.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "math_base.h"
#include "monster.h"
#include "mp2.h"
#include "resource.h"
#include "route.h"
#include "settings.h"
#include "spell.h"
#include "spell_info.h"
#include "tools.h"
#include "translations.h"
#include "ui_dialog.h"
#include "view_world.h"
#include "world.h"

namespace
{
    void HeroesTownGate( Heroes & hero, const Castle * castle )
    {
        assert( castle && castle->GetHero() == nullptr );

        Interface::AdventureMap & I = Interface::AdventureMap::Get();

        const fheroes2::Point fromPosition = hero.GetCenter();
        // Position of Hero on radar before casting the spell to clear it after casting.
        const fheroes2::Rect fromRoi( fromPosition.x, fromPosition.y, 1, 1 );

        // Before casting the spell, make sure that the game area is centered on the hero
        I.getGameArea().SetCenter( fromPosition );
        I.redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR_CURSOR );

        const int32_t dst = castle->GetIndex();
        assert( Maps::isValidAbsIndex( dst ) );

        AudioManager::PlaySound( M82::KILLFADE );
        hero.ShowPath( false );
        hero.FadeOut();

        hero.Scout( dst );
        hero.Move2Dest( dst );
        hero.GetPath().Reset();

        // Clear previous hero position on radar.
        I.getRadar().SetRenderArea( fromRoi );
        I.redraw( Interface::REDRAW_RADAR );

        I.getGameArea().SetCenter( hero.GetCenter() );

        // Update radar image in scout area around Hero after teleport.
        I.getRadar().SetRenderArea( hero.GetScoutRoi() );
        I.setRedraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );

        AudioManager::PlaySound( M82::KILLFADE );
        hero.FadeIn();
        hero.ShowPath( true );

        castle->trainHeroInMageGuild( hero );
    }

    // homm1-remaster (note v134, v139): FUN_0045ed0a for Dimension Door / Town Gate: "too tired" when no movement is left, else 12 movement
    // points (0 when fewer are left) are taken first.
    bool h1SpendCastMovement( Heroes & hero )
    {
        if ( hero.GetMovePoints() == 0 ) {
            H1::UI::messageBox( "Your hero is too tired to cast this spell today.  Try again tomorrow.", H1::UI::MESSAGE_OKAY );
            return false;
        }
        const uint32_t cost = 12 * H1::movementScale;
        const uint32_t left = ( hero.GetMovePoints() < cost ) ? 0 : hero.GetMovePoints() - cost;
        hero.ResetMovePoints();
        hero.IncreaseMovePoints( left );
        return true;
    }

    // FUN_00462d25: Town Gate. Returns true when the hero moved (the spell is used up).
    bool h1TownGate( Heroes & hero )
    {
        if ( !h1SpendCastMovement( hero ) ) {
            return false;
        }
        if ( hero.isShipMaster() ) {
            H1::UI::messageBox( "Town Gate Failed!!!  You must be on land for this spell to work.", H1::UI::MESSAGE_OKAY );
            return false;
        }
        // The nearest own town by |dx| + |dy| from the hero.
        const fheroes2::Point here = hero.GetCenter();
        const Castle * nearest = nullptr;
        int best = 1000;
        for ( const Castle * castle : hero.GetKingdom().GetCastles() ) {
            const fheroes2::Point c = castle->GetCenter();
            const int d = std::abs( c.x - here.x ) + std::abs( c.y - here.y );
            if ( d < best ) {
                best = d;
                nearest = castle;
            }
        }
        if ( nearest == nullptr ) {
            H1::UI::messageBox( "No available town.  Town Gate Failed!!!", H1::UI::MESSAGE_OKAY );
            return false;
        }
        if ( nearest->GetHero() != nullptr ) {
            H1::UI::messageBox( "Nearest town occupied.  Town Gate Failed!!!", H1::UI::MESSAGE_OKAY, 0x61, -1 );
            return false;
        }
        HeroesTownGate( hero, nearest );
        return true;
    }

    // FUN_00462fcb: Summon Boat - on land, the first empty water tile around the hero (N, NE, E, SE, S, SW, W, NW: table 0x491660)
    // gets the player's boat; otherwise "Summon Boat failed!!!" at (0x61, 0x91). The spell is used up either way.
    void h1SummonBoat( Heroes & hero )
    {
        const fheroes2::Point here = hero.GetCenter();
        static const int offsets[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
        int32_t destination = -1;
        if ( !hero.isShipMaster() && !world.getTile( hero.GetIndex() ).isWater() ) {
            for ( const auto & o : offsets ) {
                const int x = here.x + o[0];
                const int y = here.y + o[1];
                if ( !Maps::isValidAbsPoint( x, y ) ) {
                    continue;
                }
                const int32_t index = Maps::GetIndexFromAbsPoint( x, y );
                if ( world.getTile( index ).isSuitableForSummoningBoat() ) {
                    destination = index;
                    break;
                }
            }
        }
        // [Which boat comes: the engine's search for a summonable boat stands in for the boat table +0x1448c (own boats first).]
        const int32_t source = destination >= 0 ? fheroes2::getSummonableBoat( hero ) : -1;
        if ( destination < 0 || source < 0 ) {
            H1::UI::messageBox( "Summon Boat failed!!!", H1::UI::MESSAGE_OKAY, 0x61, 0x91 );
            return;
        }
        Maps::Tile & tileSource = world.getTile( source );
        Interface::GameArea & gameArea = Interface::AdventureMap::Get().getGameArea();
        gameArea.runSingleObjectAnimation( std::make_shared<Interface::ObjectFadingOutInfo>( tileSource.getMainObjectPart()._uid, source, MP2::OBJ_BOAT ) );
        Maps::Tile & tileDest = world.getTile( destination );
        tileDest.setBoat( Direction::RIGHT, hero.GetColor() );
        tileSource.resetBoatOwnerColor();
        gameArea.runSingleObjectAnimation( std::make_shared<Interface::ObjectFadingInInfo>( tileDest.getMainObjectPart()._uid, destination, MP2::OBJ_BOAT ) );
    }

    bool ActionSpellViewMines()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewMines, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellViewResources()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewResources, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellViewArtifacts()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewArtifacts, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellViewTowns()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewTowns, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellViewHeroes()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewHeroes, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellViewAll()
    {
        ViewWorld::ViewWorldWindow( Settings::Get().CurrentColor(), ViewWorldMode::ViewAll, Interface::AdventureMap::Get() );
        return true;
    }

    bool ActionSpellIdentifyHero( const Heroes & hero )
    {
        assert( !hero.GetKingdom().Modes( Kingdom::IDENTIFYHERO ) );

        hero.GetKingdom().SetModes( Kingdom::IDENTIFYHERO );
        if ( H1::isGraphicsMode() ) {
            // FUN_0045ed0a case 0x19: HoMM1 text at (0x61, 0x91).
            H1::UI::messageBox( "Enemy Heroes are now fully identifiable.", H1::UI::MESSAGE_OKAY, 0x61, 0x91 );
            return true;
        }
        fheroes2::showStandardTextMessage( _( "Identify Hero" ), _( "Enemy heroes are now fully identifiable." ), Dialog::OK );

        return true;
    }

    bool ActionSpellSummonBoat( const Heroes & hero )
    {
        assert( !hero.isShipMaster() );

        const int32_t boatDestination = fheroes2::getPossibleBoatPosition( hero );
        assert( Maps::isValidAbsIndex( boatDestination ) );

        const int32_t boatSource = fheroes2::getSummonableBoat( hero );

        // Player should have a summonable boat before calling this function.
        assert( Maps::isValidAbsIndex( boatSource ) );

        const PlayerColor heroColor = hero.GetColor();

        Interface::GameArea & gameArea = Interface::AdventureMap::Get().getGameArea();

        Maps::Tile & tileSource = world.getTile( boatSource );

        gameArea.runSingleObjectAnimation( std::make_shared<Interface::ObjectFadingOutInfo>( tileSource.getMainObjectPart()._uid, boatSource, MP2::OBJ_BOAT ) );

        Maps::Tile & tileDest = world.getTile( boatDestination );

        tileDest.setBoat( Direction::RIGHT, heroColor );
        tileSource.resetBoatOwnerColor();

        gameArea.runSingleObjectAnimation( std::make_shared<Interface::ObjectFadingInInfo>( tileDest.getMainObjectPart()._uid, boatDestination, MP2::OBJ_BOAT ) );

        return true;
    }

    bool ActionSpellDimensionDoor( Heroes & hero )
    {
        Interface::AdventureMap & I = Interface::AdventureMap::Get();

        const fheroes2::Point fromPosition = hero.GetCenter();
        // Position of Hero on radar before casting the spell to clear it after casting.
        const fheroes2::Rect fromRoi( fromPosition.x, fromPosition.y, 1, 1 );

        int32_t dst = -1;
        if ( H1::isGraphicsMode() ) {
            // homm1-remaster: Heroes I target window in the shown view (dimdoor.bin, note v123); a target that does not suit uses the spell up.
            // FUN_0045ed0a first: "too tired" (note v134).
            if ( !h1SpendCastMovement( hero ) ) {
                return false;
            }
            dst = H1::dimensionDoorDestination( hero.isShipMaster() );
            if ( dst == -2 ) {
                hero.SpellCasted( Spell::DIMENSIONDOOR );
                return false;
            }
            if ( !Maps::isValidAbsIndex( dst ) ) {
                return false;
            }
        }
        else {
            // Before casting the spell, make sure that the game area is centered on the hero
            I.getGameArea().SetCenter( hero.GetCenter() );
            I.redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR_CURSOR );

            const int32_t src = hero.GetIndex();
            assert( Maps::isValidAbsIndex( src ) );

            dst = I.GetDimensionDoorDestination( src, Spell::CalculateDimensionDoorDistance(), hero.isShipMaster() );
            if ( !Maps::isValidAbsIndex( dst ) ) {
                return false;
            }
        }

        AudioManager::PlaySound( M82::KILLFADE );
        hero.ShowPath( false );
        hero.FadeOut();

        hero.Scout( dst );
        hero.Move2Dest( dst );
        hero.SpellCasted( Spell::DIMENSIONDOOR );
        hero.calculatePath( -1 );

        // Clear previous hero position on radar.
        I.getRadar().SetRenderArea( fromRoi );
        I.redraw( Interface::REDRAW_RADAR );

        I.getGameArea().SetCenter( hero.GetCenter() );

        // Update radar image in scout area around Hero after teleport.
        I.getRadar().SetRenderArea( hero.GetScoutRoi() );
        I.setRedraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );

        AudioManager::PlaySound( M82::KILLFADE );
        hero.FadeIn();
        hero.ShowPath( true );

        hero.ActionNewPosition( false );

        // SpellCasted() has already been called, we should not call it once again
        return false;
    }

    bool ActionSpellTownGate( Heroes & hero )
    {
        assert( !hero.isShipMaster() );

        const Castle * castle = fheroes2::getNearestCastleTownGate( hero );
        if ( !castle ) {
            // A hero must be able to have a destination castle. Something is wrong with the logic!
            assert( 0 );
            return false;
        }

        if ( castle->GetHero() ) {
            // The nearest town occupation must be checked before casting this spell. Something is wrong with the logic!
            assert( 0 );
            return false;
        }

        HeroesTownGate( hero, castle );

        return true;
    }

    bool ActionSpellTownPortal( Heroes & hero )
    {
        assert( !hero.isShipMaster() );

        const int32_t townPositionIndex = Dialog::selectKingdomCastle( hero.GetKingdom(), true, _( "Town Portal" ), _( "Select town to port to." ) );

        if ( townPositionIndex == -1 ) {
            return false;
        }

        HeroesTownGate( hero, world.getCastleEntrance( Maps::GetPoint( townPositionIndex ) ) );

        return true;
    }

    bool ActionSpellVisions( Heroes & hero )
    {
        const MapsIndexes monsters = Maps::getVisibleMonstersAroundHero( hero );

        assert( !monsters.empty() );

        hero.GetKingdom().addMonstersUnderVisionSpell( monsters );

        for ( const int32_t monsterIndex : monsters ) {
            const Maps::Tile & tile = world.getTile( monsterIndex );

            const Troop troop = getTroopFromTile( tile );
            const NeutralMonsterJoiningCondition join = Army::GetJoinSolution( hero, tile, troop );

            std::string hdr;
            std::string msg;

            hdr = std::string( "%{count} " ) + troop.GetPluralName( join.monsterCount );
            StringReplace( hdr, "%{count}", troop.GetCount() );

            switch ( join.reason ) {
            case NeutralMonsterJoiningCondition::Reason::Free:
            case NeutralMonsterJoiningCondition::Reason::Alliance:
                msg = _( "The creatures are willing to join us!" );
                break;

            case NeutralMonsterJoiningCondition::Reason::ForMoney:
                if ( join.monsterCount == troop.GetCount() )
                    msg = _( "All the creatures will join us..." );
                else {
                    msg = _n( "The creature will join us...", "%{count} of the creatures will join us...", join.monsterCount );
                    StringReplace( msg, "%{count}", join.monsterCount );
                }

                msg += '\n';
                msg.append( _( "\n for a fee of %{gold} gold." ) );
                StringReplace( msg, "%{gold}", troop.GetTotalCost().gold );
                break;

            case NeutralMonsterJoiningCondition::Reason::RunAway:
            case NeutralMonsterJoiningCondition::Reason::Bane:
                msg = _( "These weak creatures will surely flee before us." );
                break;
            default:
                msg = _( "I fear these creatures are in the mood for a fight." );
                break;
            }

            fheroes2::MonsterDialogElement monsterUI( troop );
            const std::vector<const fheroes2::DialogElement *> uiElements{ &monsterUI };

            fheroes2::showStandardTextMessage( hdr, msg, Dialog::OK, uiElements );
        }

        hero.SetModes( Heroes::VISIONS );

        return true;
    }

    bool ActionSpellSetGuardian( const Heroes & hero, const Spell & spell )
    {
        Maps::Tile & tile = world.getTile( hero.GetIndex() );
        assert( MP2::OBJ_MINE == tile.getMainObjectType( false ) );

        const uint32_t count = fheroes2::getGuardianMonsterCount( spell, hero.GetPower() );

        if ( count == 0 ) {
            return false;
        }

        Maps::setMineSpellOnTile( tile, spell.GetID() );

        if ( spell == Spell::HAUNT && hero.isControlHuman() ) {
            world.CaptureObject( tile.GetIndex(), PlayerColor::NONE );

            // Update the color of haunted mine on radar.
            Interface::AdventureMap & I = Interface::AdventureMap::Get();
            const fheroes2::Point heroPosition = hero.GetCenter();
            I.getRadar().SetRenderArea( { heroPosition.x - 1, heroPosition.y - 1, 3, 2 } );

            I.setRedraw( Interface::REDRAW_RADAR );
        }

        world.GetCapturedObject( tile.GetIndex() ).GetTroop().Set( Monster( spell ), count );

        return true;
    }
}

bool Heroes::ActionSpellCast( const Spell & spell )
{
    assert( spell.isValid() && !spell.isCombat() && CanCastSpell( spell ) );

    bool apply = false;

    switch ( spell.GetID() ) {
    case Spell::VIEWMINES:
        apply = ActionSpellViewMines();
        break;
    case Spell::VIEWRESOURCES:
        apply = ActionSpellViewResources();
        break;
    case Spell::VIEWARTIFACTS:
        apply = ActionSpellViewArtifacts();
        break;
    case Spell::VIEWTOWNS:
        apply = ActionSpellViewTowns();
        break;
    case Spell::VIEWHEROES:
        apply = ActionSpellViewHeroes();
        break;
    case Spell::VIEWALL:
        apply = ActionSpellViewAll();
        break;
    case Spell::IDENTIFYHERO:
        apply = ActionSpellIdentifyHero( *this );
        break;
    case Spell::SUMMONBOAT:
        if ( H1::isGraphicsMode() ) {
            h1SummonBoat( *this );
            apply = true;
            break;
        }
        apply = ActionSpellSummonBoat( *this );
        break;
    case Spell::DIMENSIONDOOR:
        apply = ActionSpellDimensionDoor( *this );
        break;
    case Spell::TOWNGATE:
        if ( H1::isGraphicsMode() ) {
            apply = h1TownGate( *this );
            break;
        }
        apply = ActionSpellTownGate( *this );
        break;
    case Spell::TOWNPORTAL:
        apply = ActionSpellTownPortal( *this );
        break;
    case Spell::VISIONS:
        apply = ActionSpellVisions( *this );
        break;
    case Spell::HAUNT:
    case Spell::SETEGUARDIAN:
    case Spell::SETAGUARDIAN:
    case Spell::SETFGUARDIAN:
    case Spell::SETWGUARDIAN:
        apply = ActionSpellSetGuardian( *this, spell );
        break;
    default:
        break;
    }

    if ( !apply ) {
        return false;
    }

    DEBUG_LOG( DBG_GAME, DBG_INFO, GetName() << " cast spell: " << spell.GetName() )

    SpellCasted( spell );
    return true;
}
