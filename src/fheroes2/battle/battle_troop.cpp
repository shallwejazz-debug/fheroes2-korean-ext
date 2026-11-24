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

#include "battle_troop.h"

#include "h2_ext_items.h" // H2-EXT
#include "h2_ext_sets.h" // H2-EXT
#include "luck.h" // H2-EXT

#include "h2_ext.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>

#include "army.h"
#include "artifact.h"
#include "artifact_info.h"
#include "battle.h"
#include "battle_arena.h"
#include "battle_army.h"
#include "battle_board.h"
#include "battle_cell.h"
#include "battle_grave.h"
#include "battle_interface.h"
#include "battle_tower.h"
#include "castle.h"
#include "color.h"
#include "game_assets.h"
#include "game_static.h"
#include "heroes_base.h"
#include "image.h"
#include "logging.h"
#include "m82.h"
#include "monster.h"
#include "monster_anim.h"
#include "monster_info.h"
#include "morale.h"
#include "rand.h"
#include "resource.h"
#include "skill.h"
#include "speed.h"
#include "spell.h"
#include "spell_info.h"
#include "tools.h"
#include "translations.h"

#include "h1_rules.h"
#include "h1_mode.h"

namespace
{
    Artifact getImmunityArtifactForSpell( const HeroBase * hero, const Spell & spell )
    {
        if ( hero == nullptr ) {
            return { Artifact::UNKNOWN };
        }

        switch ( spell.GetID() ) {
        case Spell::CURSE:
        case Spell::MASSCURSE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::CURSE_SPELL_IMMUNITY );
        case Spell::HYPNOTIZE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::HYPNOTIZE_SPELL_IMMUNITY );
        case Spell::DEATHRIPPLE:
        case Spell::DEATHWAVE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::DEATH_SPELL_IMMUNITY );
        case Spell::BERSERKER:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::BERSERK_SPELL_IMMUNITY );
        case Spell::BLIND:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::BLIND_SPELL_IMMUNITY );
        case Spell::PARALYZE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::PARALYZE_SPELL_IMMUNITY );
        case Spell::HOLYWORD:
        case Spell::HOLYSHOUT:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::HOLY_SPELL_IMMUNITY );
        case Spell::DISPEL:
        case Spell::MASSDISPEL:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::DISPEL_SPELL_IMMUNITY );
        default:
            break;
        }

        return { Artifact::UNKNOWN };
    }
}

uint32_t Battle::ModesAffected::GetMode( const uint32_t mode ) const
{
    const const_iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    return it == end() ? 0 : it->second;
}

void Battle::ModesAffected::AddMode( const uint32_t mode, const uint32_t duration )
{
    const iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    if ( it == end() ) {
        emplace_back( mode, duration );
    }
    else {
        it->second = duration;
    }
}

void Battle::ModesAffected::RemoveMode( const uint32_t mode )
{
    erase( std::remove_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } ), end() );
}

void Battle::ModesAffected::DecreaseDuration()
{
    std::for_each( begin(), end(), []( Battle::ModeDuration & v ) { v.DecreaseDuration(); } );
}

uint32_t Battle::ModesAffected::FindZeroDuration() const
{
    const const_iterator it = std::find_if( begin(), end(), []( const Battle::ModeDuration & v ) { return v.isZeroDuration(); } );
    return it == end() ? 0 : ( *it ).first;
}

Battle::Unit::Unit( const Troop & troop, const Position & pos, const bool isReflected, const uint32_t uid )
    : ArmyTroop( nullptr, troop )
    , animation( id )
    , _uid( uid )
    , _hitPoints( troop.GetHitPoints() )
    , _initialCount( troop.GetCount() )
    , _maxCount( troop.GetCount() )
    , _shotsLeft( troop.GetShots() )
    , _idleTimer( animation.getIdleDelay() )
    , _isReflected( isReflected )
{
    SetPosition( pos );
}

void Battle::Unit::SetPosition( const int32_t index )
{
    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( nullptr );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( nullptr );
    }

    _position.Set( index, isWide(), _isReflected );

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( this );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( this );
    }
}

void Battle::Unit::SetPosition( const Position & pos )
{
    // Position may be empty if this unit is a castle tower
    assert( pos.isValidForUnit( this ) || pos.isEmpty() );

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( nullptr );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( nullptr );
    }

    _position = pos;

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( this );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( this );
    }

    if ( isWide() && _position.GetHead() && _position.GetTail() ) {
        _isReflected = GetHeadIndex() < GetTailIndex();
    }
}

void Battle::Unit::SetReflection( const bool isReflected )
{
    if ( _isReflected != isReflected ) {
        _position.Swap();

        _isReflected = isReflected;
    }
}

void Battle::Unit::UpdateDirection()
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    SetReflection( arena->getAttackingArmyColor() != GetArmyColor() );
}

bool Battle::Unit::UpdateDirection( const fheroes2::Rect & pos )
{
    if ( _position.GetRect().x == pos.x ) {
        return false;
    }

    const bool isReflected = _position.GetRect().x > pos.x;
    if ( isReflected != _isReflected ) {
        SetReflection( isReflected );
        return true;
    }

    return false;
}

bool Battle::Unit::isBattle() const
{
    return true;
}

bool Battle::Unit::isModes( const uint32_t modes_ ) const
{
    return Modes( modes_ );
}

std::string Battle::Unit::GetShotString() const
{
    if ( Troop::GetShots() == GetShots() ) {
        return std::to_string( Troop::GetShots() );
    }

    std::string output( std::to_string( Troop::GetShots() ) );
    output += " (";
    output += std::to_string( GetShots() );
    output += ')';

    return output;
}

std::string Battle::Unit::GetSpeedString() const
{
    const uint32_t speed = GetSpeed( true, false );
    return Troop::GetSpeedString( speed );
}

uint32_t Battle::Unit::GetHitPointsLeft() const
{
    return GetHitPoints() - ( GetCount() - 1 ) * Monster::GetHitPoints();
}

uint32_t Battle::Unit::GetMissingHitPoints() const
{
    const uint32_t totalHitPoints = _maxCount * Monster::GetHitPoints();
    assert( totalHitPoints >= _hitPoints );
    return totalHitPoints - _hitPoints;
}

uint32_t Battle::Unit::GetAffectedDuration( const uint32_t mode ) const
{
    return _affected.GetMode( mode );
}

uint32_t Battle::Unit::GetSpeed() const
{
    return GetSpeed( false, false );
}

int Battle::Unit::GetMorale() const
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    int armyTroopMorale = ArmyTroop::GetMorale();

    // enemy Bone dragons affect morale
    if ( H2Ext::isOn() ) {
        // H2-EXT: every enemy creature with the morale decrement counts with its own value (Bone Dragon 1, Nosferatu 1, Plague Dragon 2),
        // the biggest one once, as several Bone Dragons count once (v288: the engine looked only for the Bone Dragon) [제안: biggest, not added]
        int decrement = 0;
        for ( const Unit * unit : arena->getEnemyForce( GetArmyColor() ) ) {
            for ( const fheroes2::MonsterAbility & ability : fheroes2::getMonsterData( unit->GetID() ).battleStats.abilities ) {
                if ( ability.type == fheroes2::MonsterAbilityType::MORAL_DECREMENT ) {
                    decrement = std::max( decrement, static_cast<int>( ability.value ) );
                }
            }
        }
        if ( isAffectedByMorale() && decrement > 0 && armyTroopMorale > Morale::TREASON ) {
            armyTroopMorale = std::max( armyTroopMorale - decrement, static_cast<int>( Morale::TREASON ) );
        }
    }
    else if ( isAffectedByMorale() && arena->getEnemyForce( GetArmyColor() ).HasMonster( Monster::BONE_DRAGON ) && armyTroopMorale > Morale::TREASON ) {
        --armyTroopMorale;
    }

    // H2-EXT: the enemy hero's Four Medals set lowers morale by 1
    if ( isAffectedByMorale() && armyTroopMorale > Morale::TREASON
         && H2Ext::heroHasSetSpecial( arena->getEnemyForce( GetArmyColor() ).GetCommander(), H2Ext::SetSpecial::ENEMY_MORALE_MINUS_1 ) ) {
        --armyTroopMorale;
    }

    return armyTroopMorale;
}

int32_t Battle::Unit::GetHeadIndex() const
{
    return _position.GetHead() ? _position.GetHead()->GetIndex() : -1;
}

int32_t Battle::Unit::GetTailIndex() const
{
    return _position.GetTail() ? _position.GetTail()->GetIndex() : -1;
}

void Battle::Unit::SetRandomMorale( Rand::PCG32 & randomGenerator )
{
    const int morale = GetMorale();

    if ( morale > 0 && static_cast<int>( Rand::GetWithGen( 1, 24, randomGenerator ) ) <= morale ) {
        SetModes( MORALE_GOOD );
    }
    else if ( morale < 0 && static_cast<int>( Rand::GetWithGen( 1, 12, randomGenerator ) ) <= -morale ) {
        // AI is given a cheeky 25% chance to avoid it - because they build armies from random troops
        if ( isControlHuman() || ( Rand::GetWithGen( 1, 4, randomGenerator ) != 1 ) ) {
            SetModes( MORALE_BAD );
        }
    }
}

void Battle::Unit::SetRandomLuck( Rand::PCG32 & randomGenerator )
{
    // homm1-remaster: HoMM1 FUN_0040a771 (note v19 section 1, port note v43).
    // No roll at all for a side without a hero; one 1..12 roll; good luck if roll <= luck, bad luck if roll < -luck.
    if ( H1::isH1Mode() ) {
        if ( GetCommander() == nullptr ) {
            return;
        }
        const int32_t h1Luck = GetLuck();
        const int32_t h1Roll = static_cast<int32_t>( Rand::GetWithGen( 1, 12, randomGenerator ) );
        if ( h1Luck > 0 && h1Roll <= h1Luck ) {
            SetModes( LUCK_GOOD );
        }
        else if ( h1Luck < 0 && h1Roll < -h1Luck ) {
            SetModes( LUCK_BAD );
        }
        return;
    }

    int32_t luck = GetLuck();
    // H2-EXT: the enemy hero's Four Charms set lowers luck by 1
    if ( luck > Luck::CURSED
         && H2Ext::heroHasSetSpecial( GetArena()->getEnemyForce( GetArmyColor() ).GetCommander(), H2Ext::SetSpecial::ENEMY_LUCK_MINUS_1 ) ) {
        --luck;
    }
    const int32_t chance = static_cast<int32_t>( Rand::GetWithGen( 1, 24, randomGenerator ) );

    if ( luck > 0 && chance <= luck ) {
        SetModes( LUCK_GOOD );
    }
    else if ( luck < 0 && chance <= -luck ) {
        SetModes( LUCK_BAD );
    }

    // Bless, Curse and Luck do stack
}

bool Battle::Unit::isFlying() const
{
    return ( ArmyTroop::isFlying() && !Modes( SP_SLOW ) ) || Modes( TELEPORT_ABILITY );
}

bool Battle::Unit::isOutOfCastleWalls() const
{
    return Board::isOutOfWallsIndex( GetHeadIndex() ) || ( isWide() && Board::isOutOfWallsIndex( GetTailIndex() ) );
}

bool Battle::Unit::isHandFighting() const
{
    assert( isValid() );

    // Towers never fight in close combat
    if ( Modes( CAP_TOWER ) ) {
        return false;
    }

    for ( const int32_t nearbyIdx : Board::GetAroundIndexes( *this ) ) {
        const Unit * nearbyUnit = Board::GetCell( nearbyIdx )->GetUnit();
        if ( nearbyUnit == nullptr ) {
            continue;
        }

        if ( nearbyUnit->GetColor() == GetCurrentColor() ) {
            continue;
        }

        return true;
    }

    return false;
}

bool Battle::Unit::isHandFighting( const Unit & attacker, const Unit & defender )
{
    assert( attacker.isValid() );

    // Towers never fight in close combat
    if ( attacker.Modes( CAP_TOWER ) || defender.Modes( CAP_TOWER ) ) {
        return false;
    }

    const uint32_t distance = Board::GetDistance( attacker.GetPosition(), defender.GetPosition() );
    assert( distance > 0 );

    // If the attacker and the defender are next to each other, then this is a melee attack, otherwise it's a shot
    return ( distance == 1 );
}

bool Battle::Unit::isIdling() const
{
    return GetAnimationState() == Monster_Info::IDLE;
}

void Battle::Unit::NewTurn()
{
    if ( isRegenerating() ) {
        _hitPoints = ArmyTroop::GetHitPoints();
    }

    ResetModes( TR_RETALIATED );
    ResetModes( TR_MOVED );
    ResetModes( TR_SKIP );
    _extWaiting = false; // H2-EXT: a new round
    _extWaitUsed = false;
    ResetModes( LUCK_GOOD );
    ResetModes( LUCK_BAD );
    ResetModes( MORALE_GOOD );
    ResetModes( MORALE_BAD );

    _affected.DecreaseDuration();

    for ( uint32_t mode = _affected.FindZeroDuration(); mode != 0; mode = _affected.FindZeroDuration() ) {
        assert( Modes( mode ) );

        if ( mode == CAP_MIRROROWNER ) {
            assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

            if ( Arena::GetInterface() ) {
                Arena::GetInterface()->RedrawActionRemoveMirrorImage( { _mirrorUnit } );
            }

            _mirrorUnit->_hitPoints = 0;
            _mirrorUnit->SetCount( 0 );
            // Affection will be removed here
            _mirrorUnit->PostKilledAction();
        }
        else {
            removeAffection( mode );
        }
    }
}

uint32_t Battle::Unit::GetSpeed( const bool skipStandingCheck, const bool skipMovedCheck ) const
{
    if ( !skipStandingCheck ) {
        uint32_t modesToCheck = SP_BLIND | IS_PARALYZE_MAGIC;
        if ( !skipMovedCheck ) {
            modesToCheck |= TR_MOVED;
        }

        if ( GetCount() == 0 || Modes( modesToCheck ) ) {
            return Speed::STANDING;
        }
    }

    const uint32_t speed = Monster::GetSpeed();

    if ( Modes( SP_HASTE ) ) {
        return Speed::getHasteSpeedFromSpell( speed );
    }
    if ( Modes( SP_SLOW ) ) {
        return Speed::getSlowSpeedFromSpell( speed );
    }

    return speed;
}

uint32_t Battle::Unit::EstimateRetaliatoryDamage( const uint32_t damageTaken ) const
{
    // The entire unit is destroyed, no retaliation
    if ( damageTaken >= _hitPoints ) {
        return 0;
    }

    // Mirror images are destroyed anyway and hypnotized units never respond to an attack
    if ( Modes( TR_RETALIATED | CAP_MIRRORIMAGE | SP_HYPNOTIZE ) ) {
        return 0;
    }

    // Units with this ability retaliate even when under the influence of paralyzing spells
    if ( Modes( IS_PARALYZE_MAGIC ) && !isAbilityPresent( fheroes2::MonsterAbilityType::UNLIMITED_RETALIATION ) ) {
        return 0;
    }

    const uint32_t unitsLeft = GetCountFromHitPoints( *this, _hitPoints - damageTaken );
    assert( unitsLeft > 0 );

    const uint32_t damagePerUnit = [this]() {
        if ( Modes( SP_CURSE ) ) {
            return Monster::GetDamageMin();
        }

        if ( Modes( SP_BLESS ) ) {
            return Monster::GetDamageMax();
        }

        return ( Monster::GetDamageMin() + Monster::GetDamageMax() ) / 2;
    }();

    const uint32_t retaliatoryDamage = unitsLeft * damagePerUnit;

    if ( !Modes( SP_BLIND ) ) {
        return retaliatoryDamage;
    }

    // The retaliatory damage of a blinded unit is reduced
    const uint32_t reductionPercent = Spell( Spell::BLIND ).ExtraValue();
    assert( reductionPercent <= 100 );

    return retaliatoryDamage * ( 100 - reductionPercent ) / 100;
}

uint32_t Battle::Unit::CalculateMinDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMin() );
}

uint32_t Battle::Unit::CalculateMaxDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMax() );
}

uint32_t Battle::Unit::getPotentialDamage( const Unit & enemy ) const
{
    if ( Modes( Battle::SP_CURSE ) ) {
        return CalculateMinDamage( enemy );
    }

    if ( Modes( Battle::SP_BLESS ) ) {
        return CalculateMaxDamage( enemy );
    }

    return ( CalculateMinDamage( enemy ) + CalculateMaxDamage( enemy ) ) / 2;
}

bool Battle::Unit::h1IsShooting( const Unit & enemy ) const
{
    // Same test fheroes2 uses below: a unit with shots left that is not in hand-to-hand fighting shoots.
    return isArchers() && !isHandFighting() && !isHandFighting( *this, enemy );
}

uint32_t Battle::Unit::h1FinishDamage( const Unit & enemy, const double baseDamage, const int luck ) const
{
    const bool shooting = h1IsShooting( enemy );
    // HoMM1 v19: shooting over the castle wall adds 4 to the target's defense (5th argument of FUN_0040a9ba).
    // The wall test itself is still fheroes2's (IsShootingPenalty) until the battlefield is ported.
    int extraDefense = 0;
    if ( shooting ) {
        const Arena * arena = GetArena();
        if ( arena != nullptr && arena->IsShootingPenalty( *this, enemy ) ) {
            extraDefense = 4;
        }
    }
    // HoMM1 v15 step 4: creature flag 4 (a shooter by its table entry, even with no shots left) attacking in melee -> half.
    const bool shooterInMelee = ( Monster::GetShots() > 0 ) && !shooting;
    return H1::finishDamage( baseDamage, GetAttack(), enemy.GetDefense(), extraDefense, luck, shooterInMelee );
}

bool Battle::Unit::isExtShieldWalled( const Unit & target )
{
    if ( target.GetID() == Monster::EXT_SHIELDBEARER ) {
        return true;
    }
    for ( const int32_t nearbyIdx : Board::GetAroundIndexes( target ) ) {
        const Unit * nearbyUnit = Board::GetCell( nearbyIdx )->GetUnit();
        if ( nearbyUnit != nullptr && nearbyUnit != &target && nearbyUnit->GetColor() == target.GetColor() && nearbyUnit->GetID() == Monster::EXT_SHIELDBEARER ) {
            return true;
        }
    }
    return false;
}

uint32_t Battle::Unit::CalculateDamageUnit( const Unit & enemy, double dmg ) const
{
    // homm1-remaster: HoMM1 damage formula (FUN_0040a9ba, notes v15/v43). Castle towers keep the fheroes2 formula (open question).
    if ( H1::isH1Mode() && !Modes( CAP_TOWER ) ) {
        return h1FinishDamage( enemy, dmg, 0 );
    }

    if ( isArchers() ) {
        // Melee penalty can be applied either if the archer is blocked by enemy units and cannot shoot,
        // or if he is not blocked, but was attacked by a friendly unit (for example, in the case of using
        // Berserk or Hypnotize spells) and deals retaliatory damage
        if ( !isHandFighting() && !isHandFighting( *this, enemy ) ) {
            // Hero's Archery skill may increase damage
            if ( GetCommander() ) {
                dmg += ( dmg * GetCommander()->GetSecondarySkillValue( Skill::Secondary::ARCHERY ) / 100 );
            }

            const Arena * arena = GetArena();
            assert( arena != nullptr );

            // Penalty for damage to castle defenders behind the castle walls
            // (H2-EXT: the Crossbowman shoots over the castle walls without the penalty)
            if ( arena->IsShootingPenalty( *this, enemy ) && GetID() != Monster::EXT_CROSSBOWMAN ) {
                dmg *= 1 - ( GameStatic::getCastleWallRangedPenalty() / 100.0 );
            }

            // The Shield spell does not affect the damage of the castle towers
            if ( !Modes( CAP_TOWER ) && enemy.Modes( SP_SHIELD ) ) {
                dmg /= Spell( Spell::SHIELD ).ExtraValue();
            }

            // H2-EXT: the Stone Gargoyle takes half damage from shots
            if ( enemy.GetID() == Monster::EXT_STONE_GARGOYLE ) {
                dmg /= 2;
            }

            // H2-EXT: shield wall - a Shieldbearer and the units next to it take 30% less damage from shots
            if ( isExtShieldWalled( enemy ) ) {
                dmg = dmg * 70 / 100;
            }

            // H2-EXT: Archer's Tools set - shooting troops +20% damage
            if ( H2Ext::heroHasSetSpecial( GetCommander(), H2Ext::SetSpecial::RANGED_20 ) ) {
                dmg = dmg * 120 / 100;
            }
        }
        else if ( !isAbilityPresent( fheroes2::MonsterAbilityType::NO_MELEE_PENALTY ) ) {
            dmg /= 2;
        }
    }

    // The retaliatory damage of a blinded unit is reduced
    if ( _blindRetaliation ) {
        // Petrified units cannot attack, respectively, there should be no retaliation
        assert( !enemy.Modes( SP_STONE ) );

        const uint32_t reductionPercent = Spell( Spell::BLIND ).ExtraValue();
        assert( reductionPercent <= 100 );

        dmg = dmg * ( 100 - reductionPercent ) / 100;
    }

    // A petrified unit takes only half of the damage
    if ( enemy.Modes( SP_STONE ) ) {
        // Petrified units cannot attack, respectively, there should be no retaliation
        assert( !_blindRetaliation );

        dmg /= 2;
    }

    // If multiple options are suitable at the same time, the damage should be doubled only once
    if ( ( isAbilityPresent( fheroes2::MonsterAbilityType::DOUBLE_DAMAGE_TO_UNDEAD ) && enemy.isAbilityPresent( fheroes2::MonsterAbilityType::UNDEAD ) )
         || ( isAbilityPresent( fheroes2::MonsterAbilityType::EARTH_CREATURE )
              && enemy.isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_EARTH_CREATURES ) )
         || ( isAbilityPresent( fheroes2::MonsterAbilityType::AIR_CREATURE )
              && enemy.isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_AIR_CREATURES ) )
         || ( isAbilityPresent( fheroes2::MonsterAbilityType::FIRE_CREATURE )
              && enemy.isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_FIRE_CREATURES ) )
         || ( isAbilityPresent( fheroes2::MonsterAbilityType::WATER_CREATURE )
              && enemy.isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_WATER_CREATURES ) ) ) {
        dmg *= 2;
    }

    // H2-EXT: Halberdier +50% against two-cell creatures; charge of the Lancer and the Boar Rider +5% for each cell walked in its own turn
    if ( GetID() == Monster::EXT_HALBERDIER && enemy.isWide() ) {
        dmg = dmg * 3 / 2;
    }
    if ( ( GetID() == Monster::EXT_LANCER || GetID() == Monster::EXT_BOAR_RIDER ) && _extMovedCells > 0 ) {
        dmg *= 1 + 0.05 * _extMovedCells;
    }

    int r = GetAttack() - enemy.GetDefense();
    // H2-EXT: the Forest Sniper's aimed shot counts half of the target's defense
    if ( GetID() == Monster::EXT_FOREST_SNIPER && isArchers() && !isHandFighting() && !isHandFighting( *this, enemy ) ) {
        r = static_cast<int>( GetAttack() ) - static_cast<int>( enemy.GetDefense() / 2 );
    }
    if ( enemy.isDragons() && Modes( SP_DRAGONSLAYER ) ) {
        r += Spell( Spell::DRAGONSLAYER ).ExtraValue();
    }
    // H2-EXT: Hunter's Quiver - attack +1 when the troop shoots (docs/H2_ITEM_CONCEPT_v5.md 3)
    if ( isArchers() && !isHandFighting() && !isHandFighting( *this, enemy ) && H2Ext::wears( GetCommander(), Artifact::EXT_HUNTER_QUIVER ) ) {
        ++r;
    }

    // Attack bonus is 20% to 300%
    dmg *= 1 + ( 0 < r ? 0.1 * std::min( r, 20 ) : 0.05 * std::max( r, -16 ) );

    // H2-EXT: Elixir of Life set - living troops take as if they had 15% more hit points
    if ( enemy.isExtLivingHardy() ) {
        dmg = dmg * 100 / 115;
    }

    return std::max<uint32_t>( fheroes2::checkedCast<uint32_t>( dmg ).value(), 1U );
}

uint32_t Battle::Unit::GetDamage( const Unit & enemy, Rand::PCG32 & randomGenerator ) const
{
    // homm1-remaster: HoMM1 FUN_0040a9ba step 1 - one roll per creature in the stack and the sum is the base damage.
    // Damage state 3 (Bless) = always maximum, state 1 (Curse) = always minimum (v16).
    if ( H1::isH1Mode() && !Modes( CAP_TOWER ) ) {
        const uint32_t perMin = Monster::GetDamageMin();
        const uint32_t perMax = Monster::GetDamageMax();
        double base = 0;
        for ( uint32_t i = 0; i < GetCount(); ++i ) {
            if ( Modes( SP_BLESS ) ) {
                base += perMax;
            }
            else if ( Modes( SP_CURSE ) ) {
                base += perMin;
            }
            else {
                base += Rand::GetWithGen( perMin, perMax, randomGenerator );
            }
        }
        const int luck = Modes( LUCK_GOOD ) ? 1 : ( Modes( LUCK_BAD ) ? -1 : 0 );
        return h1FinishDamage( enemy, base, luck );
    }

    uint32_t res = 0;

    if ( Modes( SP_BLESS ) ) {
        res = CalculateMaxDamage( enemy );
    }
    else if ( Modes( SP_CURSE ) ) {
        res = CalculateMinDamage( enemy );
    }
    else {
        res = Rand::GetWithGen( CalculateMinDamage( enemy ), CalculateMaxDamage( enemy ), randomGenerator );
    }

    if ( Modes( LUCK_GOOD ) ) {
        res *= 2;
    }
    else if ( Modes( LUCK_BAD ) ) {
        res /= 2;
    }

    return std::max<uint32_t>( res, 1U );
}

uint32_t Battle::Unit::HowManyWillBeKilled( const uint32_t dmg ) const
{
    if ( Modes( CAP_MIRRORIMAGE ) ) {
        return GetCount();
    }

    return dmg >= _hitPoints ? GetCount() : GetCount() - Monster::GetCountFromHitPoints( *this, _hitPoints - dmg );
}

uint32_t Battle::Unit::_applyDamage( const uint32_t dmg )
{
    assert( !AllModes( CAP_MIRROROWNER | CAP_MIRRORIMAGE ) );

    if ( dmg == 0 || GetCount() == 0 ) {
        return 0;
    }

    const uint32_t killed = HowManyWillBeKilled( dmg );

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, dmg << " to " << String() << " and killed: " << killed )

    if ( Modes( IS_PARALYZE_MAGIC ) ) {
        // Units with this ability retaliate even when under the influence of paralyzing spells
        if ( !isAbilityPresent( fheroes2::MonsterAbilityType::UNLIMITED_RETALIATION ) ) {
            SetModes( TR_RETALIATED );
        }

        SetModes( TR_MOVED );

        removeAffection( IS_PARALYZE_MAGIC );
    }

    if ( Modes( SP_BLIND ) ) {
        SetModes( TR_MOVED );

        removeAffection( SP_BLIND );
    }

    if ( killed >= GetCount() ) {
        _deadCount += GetCount();

        SetCount( 0 );
    }
    else {
        _deadCount += killed;

        SetCount( GetCount() - killed );
    }

    if ( Modes( CAP_MIRRORIMAGE ) ) {
        _hitPoints = 0;
    }
    else {
        _hitPoints -= std::min( _hitPoints, dmg );
    }

    if ( Modes( CAP_MIRROROWNER ) && !isValid() ) {
        assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

        _mirrorUnit->_applyDamage( _mirrorUnit->_hitPoints );
    }

    return killed;
}

void Battle::Unit::PostKilledAction()
{
    assert( !AllModes( CAP_MIRROROWNER | CAP_MIRRORIMAGE ) );

    if ( Modes( CAP_MIRROROWNER ) ) {
        assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

        _mirrorUnit = nullptr;

        removeAffection( CAP_MIRROROWNER );
    }

    if ( Modes( CAP_MIRRORIMAGE ) ) {
        // CAP_MIRROROWNER may have already been removed from the mirror owner,
        // since this method may already have been called for it
        assert( _mirrorUnit != nullptr );

        // But we still need to remove it if it is present
        if ( _mirrorUnit->Modes( CAP_MIRROROWNER ) ) {
            assert( _mirrorUnit->_mirrorUnit == this );

            _mirrorUnit->_mirrorUnit = nullptr;
            _mirrorUnit->removeAffection( CAP_MIRROROWNER );
        }

        _mirrorUnit = nullptr;
    }

    // Remove all spells
    removeAffection( IS_MAGIC );
    assert( _affected.empty() );

    // Save to the graveyard if possible
    if ( !Modes( CAP_MIRRORIMAGE ) && !isElemental() ) {
        Graveyard * graveyard = Arena::GetGraveyard();
        assert( graveyard != nullptr );

        graveyard->addUnit( this );
    }

    Cell * head = _position.GetHead();
    assert( head != nullptr );

    head->SetUnit( nullptr );

    if ( isWide() ) {
        Cell * tail = _position.GetTail();
        assert( tail != nullptr );

        tail->SetUnit( nullptr );
    }

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() )
}

uint32_t Battle::Unit::_resurrect( const uint32_t points, const bool allowToExceedMaxCount, const bool isTemporary )
{
    uint32_t resurrect = Monster::GetCountFromHitPoints( *this, _hitPoints + points ) - GetCount();

    SetCount( GetCount() + resurrect );
    _hitPoints += points;

    if ( allowToExceedMaxCount ) {
        _maxCount = std::max( _maxCount, GetCount() );
    }
    else if ( GetCount() > _maxCount ) {
        resurrect -= GetCount() - _maxCount;
        SetCount( _maxCount );
        _hitPoints = ArmyTroop::GetHitPoints();
    }

    if ( !isTemporary ) {
        _deadCount -= ( resurrect < _deadCount ? resurrect : _deadCount );
    }

    return resurrect;
}

bool Battle::Unit::isImmovable() const
{
    return Modes( SP_BLIND | IS_PARALYZE_MAGIC );
}

uint32_t Battle::Unit::ApplyDamage( Unit & enemy, const uint32_t dmg, uint32_t & killed, uint32_t * ptrResurrected )
{
    killed = _applyDamage( dmg );

    // H2-EXT: the Great Phoenix rises once a battle with a third of its stack when it is destroyed [제안: 1/3]
    if ( GetCount() == 0 && GetID() == Monster::EXT_GREAT_PHOENIX && !_extRebirthUsed && !Modes( CAP_MIRRORIMAGE ) ) {
        _extRebirthUsed = true;
        const uint32_t rise = std::max<uint32_t>( 1, _maxCount / 3 );
        _resurrect( rise * Monster::GetHitPoints(), false, false );
    }

    if ( killed == 0 ) {
        if ( ptrResurrected != nullptr ) {
            *ptrResurrected = 0;
        }
        return killed;
    }

    uint32_t resurrected = 0;

    if ( enemy.isAbilityPresent( fheroes2::MonsterAbilityType::SOUL_EATER ) ) {
        resurrected = enemy._resurrect( killed * enemy.Monster::GetHitPoints(), true, false );
    }
    else if ( enemy.isAbilityPresent( fheroes2::MonsterAbilityType::HP_DRAIN ) ) {
        resurrected = enemy._resurrect( killed * Monster::GetHitPoints(), false, false );
    }

    if ( resurrected > 0 ) {
        DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() << ", enemy: " << enemy.String() << ", resurrected: " << resurrected )
    }

    if ( ptrResurrected != nullptr ) {
        *ptrResurrected = resurrected;
    }

    return killed;
}

bool Battle::Unit::AllowApplySpell( const Spell & spell, const HeroBase * applyingHero, const bool forceApplyToAlly /* = false */ ) const
{
    if ( Modes( CAP_MIRRORIMAGE ) && ( spell == Spell::ANTIMAGIC || spell == Spell::MIRRORIMAGE ) ) {
        return false;
    }

    if ( Modes( CAP_MIRROROWNER ) && spell == Spell::MIRRORIMAGE ) {
        return false;
    }

    if ( applyingHero && spell.isApplyToFriends() && GetColor() != applyingHero->GetColor() ) {
        return false;
    }
    if ( applyingHero && spell.isApplyToEnemies() && GetColor() == applyingHero->GetColor() && !forceApplyToAlly ) {
        return false;
    }

    return ( GetMagicResist( spell, applyingHero ) < 100 );
}

bool Battle::Unit::ApplySpell( const Spell & spell, const HeroBase * applyingHero, TargetInfo & target )
{
    // HACK!!! Chain lightning is the only spell which can't be cast on allies but could be applied on them
    const bool isForceApply = ( spell.GetID() == Spell::CHAINLIGHTNING );

    if ( !AllowApplySpell( spell, applyingHero, isForceApply ) ) {
        return false;
    }

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, spell.GetName() << " to " << String() )

    const uint32_t spoint = applyingHero ? applyingHero->GetPower() : fheroes2::spellPowerForBuiltinMonsterSpells;

    if ( spell.isDamage() ) {
        _spellApplyDamage( spell, spoint, applyingHero, target );
    }
    else if ( spell.isRestore() || spell.isResurrect() ) {
        _spellRestoreAction( spell, spoint, applyingHero );
    }
    else {
        _spellModesAction( spell, spoint, applyingHero );
    }

    return true;
}

std::vector<Spell> Battle::Unit::getCurrentSpellEffects() const
{
    std::vector<Spell> spellList;

    if ( Modes( SP_BLESS ) ) {
        spellList.emplace_back( Spell::BLESS );
    }
    if ( Modes( SP_CURSE ) ) {
        spellList.emplace_back( Spell::CURSE );
    }
    if ( Modes( SP_HASTE ) ) {
        spellList.emplace_back( Spell::HASTE );
    }
    if ( Modes( SP_SLOW ) ) {
        spellList.emplace_back( Spell::SLOW );
    }
    if ( Modes( SP_SHIELD ) ) {
        spellList.emplace_back( Spell::SHIELD );
    }
    if ( Modes( SP_BLOODLUST ) ) {
        spellList.emplace_back( Spell::BLOODLUST );
    }
    if ( Modes( SP_STONESKIN ) ) {
        spellList.emplace_back( Spell::STONESKIN );
    }
    if ( Modes( SP_STEELSKIN ) ) {
        spellList.emplace_back( Spell::STEELSKIN );
    }
    if ( Modes( SP_BLIND ) ) {
        spellList.emplace_back( Spell::BLIND );
    }
    if ( Modes( SP_PARALYZE ) ) {
        spellList.emplace_back( Spell::PARALYZE );
    }
    if ( Modes( SP_STONE ) ) {
        spellList.emplace_back( Spell::PETRIFY );
    }
    if ( Modes( SP_DRAGONSLAYER ) ) {
        spellList.emplace_back( Spell::DRAGONSLAYER );
    }
    if ( Modes( SP_BERSERKER ) ) {
        spellList.emplace_back( Spell::BERSERKER );
    }
    if ( Modes( SP_HYPNOTIZE ) ) {
        spellList.emplace_back( Spell::HYPNOTIZE );
    }
    if ( Modes( CAP_MIRROROWNER ) ) {
        spellList.emplace_back( Spell::MIRRORIMAGE );
    }

    return spellList;
}

std::string Battle::Unit::String( const bool more /* = false */ ) const
{
    std::stringstream ss;

    ss << "Unit: "
       << "[ " <<
        // info
        GetCount() << " " << GetName() << ", " << Color::String( GetColor() ) << ", pos: " << GetHeadIndex() << ", " << GetTailIndex()
       << ( _isReflected ? ", reflect" : "" );

    if ( more )
        ss << ", mode(" << GetHexString( modes ) << ")"
           << ", uid(" << GetHexString( _uid ) << ")"
           << ", speed(" << Speed::String( GetSpeed() ) << ", " << GetSpeed() << ")"
           << ", hp(" << _hitPoints << ")"
           << ", died(" << _deadCount << ")";

    ss << " ]";

    return ss.str();
}

bool Battle::Unit::isRetaliationAllowed() const
{
    // Hypnotized units never respond to an attack
    if ( Modes( SP_HYPNOTIZE ) ) {
        return false;
    }

    // Blindness can be cast by an attacking unit. There should never be any retaliation in this case.
    if ( Modes( SP_BLIND ) && !_blindRetaliation ) {
        return false;
    }

    // Paralyzing magic can be cast by an attacking unit. There should never be any retaliation in this case.
    if ( Modes( IS_PARALYZE_MAGIC ) ) {
        return false;
    }

    return ( !Modes( TR_RETALIATED ) );
}

void Battle::Unit::setRetaliationAsCompleted()
{
    if ( isAbilityPresent( fheroes2::MonsterAbilityType::UNLIMITED_RETALIATION ) ) {
        return;
    }

    SetModes( TR_RETALIATED );
}

void Battle::Unit::PostAttackAction( const Unit & enemy )
{
    if ( isArchers() && !isHandFighting( *this, enemy ) ) {
        const HeroBase * hero = GetCommander();

        if ( hero == nullptr || !hero->GetBagArtifacts().isArtifactBonusPresent( fheroes2::ArtifactBonusType::ENDLESS_AMMUNITION ) ) {
            assert( !Modes( CAP_TOWER ) && _shotsLeft > 0 );

            --_shotsLeft;
        }
    }

    ResetModes( LUCK_GOOD | LUCK_BAD );
}

uint32_t Battle::Unit::GetAttack() const
{
    uint32_t res = ArmyTroop::GetAttack();

    if ( Modes( SP_BLOODLUST ) ) {
        res += Spell( Spell::BLOODLUST ).ExtraValue();
    }

    res += _extThirst; // H2-EXT: Nosferatu blood thirst

    if ( _extPlagueNum ) {
        const uint32_t step = _extPlagueNum * 2; // H2-EXT
        res = step >= res ? 1 : res - step;
    }

    return res;
}

uint32_t Battle::Unit::GetDefense() const
{
    uint32_t res = ArmyTroop::GetDefense();

    if ( Modes( SP_STONESKIN ) ) {
        res += Spell( Spell::STONESKIN ).ExtraValue();
    }
    else if ( Modes( SP_STEELSKIN ) ) {
        res += Spell( Spell::STEELSKIN ).ExtraValue();
    }

    if ( _extAcidNum ) {
        const uint32_t step = _extAcidNum * 2; // H2-EXT
        res = step >= res ? 1 : res - step;
    }

    if ( _disruptingRaysNum ) {
        const uint32_t step = _disruptingRaysNum * Spell( Spell::DISRUPTINGRAY ).ExtraValue();

        if ( step >= res ) {
            res = 1;
        }
        else {
            res -= step;
        }
    }

    const Castle * castle = Arena::GetCastle();

    if ( castle && castle->isBuild( BUILD_MOAT ) && ( Board::isMoatIndex( GetHeadIndex(), *this ) || Board::isMoatIndex( GetTailIndex(), *this ) ) ) {
        const uint32_t step = GameStatic::GetBattleMoatReduceDefense();

        if ( step >= res ) {
            res = 1;
        }
        else {
            res -= step;
        }
    }

    // H2-EXT: defend (skip in an extension game) - defense +20%, at least +1, until the unit's next turn
    if ( _extDefending ) {
        res += std::max<uint32_t>( 1, res / 5 );
    }

    return res;
}

double Battle::Unit::evaluateThreatForUnit( const Unit & defender ) const
{
    const Unit & attacker = *this;

    const uint32_t attackerDamageToDefender = attacker.getPotentialDamage( defender );
    double attackerThreat = attackerDamageToDefender;

    {
        const double distanceModifier = [&defender, &attacker]() {
            if ( defender.Modes( CAP_TOWER ) ) {
                return 1.0;
            }

            if ( attacker.isFlying() || attacker.isArchers() ) {
                return 1.0;
            }

            const uint32_t attackerSpeed = attacker.GetSpeed( true, false );
            assert( attackerSpeed > Speed::STANDING );

            const uint32_t attackRange = attackerSpeed + 1;

            const uint32_t distance = Board::GetDistance( attacker.GetPosition(), defender.GetPosition() );
            assert( distance > 0 );

            if ( distance <= attackRange ) {
                return 1.0;
            }

            return 1.5 * static_cast<double>( distance ) / static_cast<double>( attackerSpeed );
        }();

        attackerThreat /= distanceModifier;
    }

    if ( attacker.isDoubleAttack() ) {
        // The following logic of accounting for potential retaliatory damage is intended only for the case when one unit directly attacks another, and will not work in
        // cases where one unit attacks several units at once
        assert( !attacker.isAbilityPresent( fheroes2::MonsterAbilityType::TWO_CELL_MELEE_ATTACK )
                && !attacker.isAbilityPresent( fheroes2::MonsterAbilityType::ALL_ADJACENT_CELL_MELEE_ATTACK )
                && !attacker.isAbilityPresent( fheroes2::MonsterAbilityType::AREA_SHOT ) );

        const uint32_t retaliatoryDamage = [&defender, &attacker, attackerDamageToDefender]() -> uint32_t {
            if ( defender.Modes( CAP_TOWER ) ) {
                return 0;
            }

            if ( attacker.isIgnoringRetaliation() ) {
                return 0;
            }

            if ( attacker.isArchers() && !attacker.isHandFighting() ) {
                return 0;
            }

            return defender.EstimateRetaliatoryDamage( attackerDamageToDefender );
        }();

        // If the defender is able to retaliate the attacker after his first attack, then the attacker's second attack can cause less damage than the first
        if ( retaliatoryDamage > 0 ) {
            assert( attacker.GetHitPoints() > 0 );

            // Rough but quick estimate
            attackerThreat += attackerThreat * ( 1.0 - std::min( static_cast<double>( retaliatoryDamage ) / static_cast<double>( attacker.GetHitPoints() ), 1.0 ) );
        }
        // Otherwise, estimate the second attack as approximately equal to the first in damage
        else {
            attackerThreat *= 2;
        }
    }

    if ( attacker.isAbilityPresent( fheroes2::MonsterAbilityType::ENEMY_HALVING ) ) {
        attackerThreat *= 2;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonsterAbilityType::SOUL_EATER ) ) {
        attackerThreat *= 3;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonsterAbilityType::HP_DRAIN ) ) {
        attackerThreat *= 1.3;
    }

    {
        const std::vector<fheroes2::MonsterAbility> & attackerAbilities = fheroes2::getMonsterData( id ).battleStats.abilities;

        if ( const auto abilityIter = std::find( attackerAbilities.begin(), attackerAbilities.end(), fheroes2::MonsterAbilityType::SPELL_CASTER );
             abilityIter != attackerAbilities.end() ) {
            const auto getDefenderDamage = [&defender]() {
                if ( defender.Modes( SP_CURSE ) ) {
                    return defender.GetDamageMin();
                }

                if ( defender.Modes( SP_BLESS ) ) {
                    return defender.GetDamageMax();
                }

                return ( defender.GetDamageMin() + defender.GetDamageMax() ) / 2;
            };

            switch ( abilityIter->value ) {
            case Spell::BLIND:
            case Spell::PARALYZE:
            case Spell::PETRIFY:
                // Creature's built-in magic resistance (not 100% immunity but resistance, as, for example, with Dwarves) never works against the built-in magic of
                // another creature (for example, Unicorn's Blind ability). Only the probability of triggering the built-in magic matters.
                if ( defender.AllowApplySpell( static_cast<int32_t>( abilityIter->value ), nullptr ) ) {
                    attackerThreat += static_cast<double>( getDefenderDamage() ) * abilityIter->percentage / 100.0;
                }
                break;
            case Spell::DISPEL:
                // TODO: add the logic to evaluate this spell value.
                break;
            case Spell::CURSE:
                // Creature's built-in magic resistance (not 100% immunity but resistance, as, for example, with Dwarves) never works against the built-in magic of
                // another creature (for example, Unicorn's Blind ability). Only the probability of triggering the built-in magic matters.
                if ( defender.AllowApplySpell( static_cast<int32_t>( abilityIter->value ), nullptr ) ) {
                    attackerThreat += static_cast<double>( getDefenderDamage() ) * abilityIter->percentage / 100.0 / 10.0;
                }
                break;
            default:
                // Did you add a new spell casting ability? Add the logic above!
                assert( 0 );
                break;
            }
        }
    }

    // Give the mirror images a higher priority, as they can be destroyed in 1 hit
    if ( attacker.Modes( CAP_MIRRORIMAGE ) ) {
        attackerThreat *= 10;
    }

    // Heavy penalty for hitting our own units
    if ( attacker.GetColor() == defender.GetCurrentColor() ) {
        // TODO: remove this temporary assertion
        assert( dynamic_cast<const Battle::Tower *>( this ) == nullptr );

        attackerThreat *= -2;
    }
    // Negative value of units that changed the side
    else if ( attacker.GetColor() != attacker.GetCurrentColor() ) {
        attackerThreat *= -1;
    }
    // Ignore disabled enemy units
    else if ( attacker.isImmovable() ) {
        attackerThreat = 0;
    }
    // Reduce the priority of those enemy units that have already got their turn
    else if ( attacker.Modes( TR_MOVED ) ) {
        attackerThreat /= 1.25;
    }

    return attackerThreat;
}

Funds Battle::Unit::GetSurrenderCost() const
{
    // Resurrected (not truly resurrected) units should not be taken into account when calculating the cost of surrender
    return GetCost() * ( GetDead() > GetMaxCount() ? 0 : GetMaxCount() - GetDead() );
}

int Battle::Unit::GetControl() const
{
    return !GetArmy() ? CONTROL_AI : GetArmy()->GetControl();
}

void Battle::Unit::_spellModesAction( const Spell & spell, uint32_t duration, const HeroBase * applyingHero )
{
    if ( applyingHero ) {
        duration += applyingHero->GetBagArtifacts().getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::EVERY_COMBAT_SPELL_DURATION );
    }

    switch ( spell.GetID() ) {
    case Spell::BLESS:
    case Spell::MASSBLESS:
        _replaceAffection( SP_CURSE, SP_BLESS, duration );
        break;

    case Spell::BLOODLUST:
        _addAffection( SP_BLOODLUST, 3 );
        break;

    case Spell::CURSE:
    case Spell::MASSCURSE:
        _replaceAffection( SP_BLESS, SP_CURSE, duration );
        break;

    case Spell::HASTE:
    case Spell::MASSHASTE:
        _replaceAffection( SP_SLOW, SP_HASTE, duration );
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        extDispel( IS_MAGIC ); // H2-EXT: removeAffection( IS_MAGIC ), Time Magic set
        break;

    case Spell::SHIELD:
    case Spell::MASSSHIELD:
        _addAffection( SP_SHIELD, duration );
        break;

    case Spell::SLOW:
    case Spell::MASSSLOW:
        _replaceAffection( SP_HASTE, SP_SLOW, duration );
        break;

    case Spell::STONESKIN:
        _replaceAffection( SP_STEELSKIN, SP_STONESKIN, duration );
        break;

    case Spell::BLIND:
        _addAffection( SP_BLIND, duration );
        // Blindness can be cast by an attacking unit. In this case, there should be no response to the attack.
        _blindRetaliation = false;
        break;

    case Spell::DRAGONSLAYER:
        _addAffection( SP_DRAGONSLAYER, duration );
        break;

    case Spell::STEELSKIN:
        _replaceAffection( SP_STONESKIN, SP_STEELSKIN, duration );
        break;

    case Spell::ANTIMAGIC:
        _replaceAffection( IS_MAGIC, SP_ANTIMAGIC, duration );
        break;

    case Spell::PARALYZE:
        _addAffection( SP_PARALYZE, duration );
        break;

    case Spell::BERSERKER:
        _replaceAffection( SP_HYPNOTIZE, SP_BERSERKER, duration );
        break;

    case Spell::HYPNOTIZE:
        _replaceAffection( SP_BERSERKER, SP_HYPNOTIZE, duration );
        break;

    case Spell::PETRIFY:
        _addAffection( SP_STONE, duration );
        break;

    case Spell::MIRRORIMAGE:
        // Special case, CAP_MIRROROWNER mode will be set when mirror image unit will be created
        _affected.AddMode( CAP_MIRROROWNER, duration );
        break;

    case Spell::DISRUPTINGRAY:
        ++_disruptingRaysNum;
        break;

    default:
        assert( 0 );
        break;
    }
}

void Battle::Unit::_spellApplyDamage( const Spell & spell, const uint32_t spellPower, const HeroBase * applyingHero, TargetInfo & target )
{
    assert( spell.isDamage() );

    const uint32_t dmg = CalculateSpellDamage( spell, spellPower, applyingHero, target.damage, false /* ignore defending hero */ );

    // apply damage
    if ( dmg ) {
        target.damage = dmg;
        target.killed = _applyDamage( dmg );
    }
}

uint32_t Battle::Unit::CalculateSpellDamage( const Spell & spell, uint32_t spellPower, const HeroBase * applyingHero, const uint32_t targetDamage,
                                             const bool ignoreDefendingHero ) const
{
    assert( spell.isDamage() );

    uint32_t dmg = spell.Damage() * spellPower;

    // If multiple options are suitable at the same time, then the abilities are considered first (in order from more specific to less specific),
    // and then the weaknesses are considered (also in order from more specific to less specific)
    {
        const std::vector<fheroes2::MonsterAbility> & abilities = fheroes2::getMonsterData( GetID() ).battleStats.abilities;
        const std::vector<fheroes2::MonsterWeakness> & weaknesses = fheroes2::getMonsterData( GetID() ).battleStats.weaknesses;

        // Abilities
        //
        if ( const auto certainSpellDmgRedIter
             = std::find( abilities.begin(), abilities.end(),
                          std::make_pair( fheroes2::MonsterAbilityType::CERTAIN_SPELL_DAMAGE_REDUCTION, static_cast<uint32_t>( spell.GetID() ) ) );
             certainSpellDmgRedIter != abilities.end() ) {
            dmg = dmg * certainSpellDmgRedIter->percentage / 100;
        }
        else if ( GetID() == Monster::EXT_MITHRIL_GOLEM ) {
            dmg = dmg * 25 / 100; // H2-EXT: Mithril Golem, every damage spell -75%
        }
        else if ( const auto elementalSpellDmgRedIter = std::find( abilities.begin(), abilities.end(), fheroes2::MonsterAbilityType::ELEMENTAL_SPELL_DAMAGE_REDUCTION );
                  elementalSpellDmgRedIter != abilities.end() && spell.isElementalSpell() ) {
            dmg = dmg * elementalSpellDmgRedIter->percentage / 100;
        }
        //
        // Weaknesses
        //
        else if ( const auto certainSpellExtraDmgIter
                  = std::find( weaknesses.begin(), weaknesses.end(),
                               std::make_pair( fheroes2::MonsterWeaknessType::EXTRA_DAMAGE_FROM_CERTAIN_SPELL, static_cast<uint32_t>( spell.GetID() ) ) );
                  certainSpellExtraDmgIter != weaknesses.end() ) {
            dmg = dmg * ( 100 + certainSpellExtraDmgIter->percentage ) / 100;
        }
        else if ( ( isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_FIRE_SPELLS ) && spell.isFire() )
                  || ( isWeaknessPresent( fheroes2::MonsterWeaknessType::DOUBLE_DAMAGE_FROM_COLD_SPELLS ) && spell.isCold() ) ) {
            dmg *= 2;
        }
    }

    // H2-EXT: a Red Dragon in the casting hero's army: Fireball and Fire Blast +20% (owner 20:14 "오벨 안 되게": not Armageddon, once)
    if ( applyingHero && H2Ext::isOn() && ( spell.GetID() == Spell::FIREBALL || spell.GetID() == Spell::FIREBLAST )
         && applyingHero->GetArmy().HasMonster( Monster( Monster::RED_DRAGON ) ) ) {
        dmg = dmg * 120 / 100;
    }

    if ( applyingHero ) {
        const HeroBase * defendingHero = GetCommander();
        const bool useDefendingHeroArts = defendingHero && !ignoreDefendingHero;

        switch ( spell.GetID() ) {
        case Spell::COLDRAY:
        case Spell::COLDRING: {
            std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::COLD_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = dmg * ( 100 + value ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::COLD_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = dmg * ( 100 - value ) / 100;
                }

                extraDamagePercent = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactCurseType::COLD_SPELL_EXTRA_DAMAGE_PERCENT );
                for ( const int32_t value : extraDamagePercent ) {
                    dmg = dmg * ( 100 + value ) / 100;
                }
            }

            break;
        }
        case Spell::FIREBALL:
        case Spell::FIREBLAST: {
            std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::FIRE_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = dmg * ( 100 + value ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::FIRE_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = dmg * ( 100 - value ) / 100;
                }

                extraDamagePercent = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactCurseType::FIRE_SPELL_EXTRA_DAMAGE_PERCENT );
                for ( const int32_t value : extraDamagePercent ) {
                    dmg = dmg * ( 100 + value ) / 100;
                }
            }

            break;
        }
        case Spell::LIGHTNINGBOLT:
        case Spell::CHAINLIGHTNING: {
            const std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::LIGHTNING_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = dmg * ( 100 + value ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::LIGHTNING_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = dmg * ( 100 - value ) / 100;
                }
            }

            if ( spell.GetID() == Spell::CHAINLIGHTNING ) {
                switch ( targetDamage ) {
                case 0:
                    break;
                case 1:
                    dmg /= 2;
                    break;
                case 2:
                    dmg /= 4;
                    break;
                case 3:
                    dmg /= 8;
                    break;
                default:
                    break;
                }
            }

            break;
        }
        case Spell::ELEMENTALSTORM:
        case Spell::ARMAGEDDON: {
            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::ELEMENTAL_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = dmg * ( 100 - value ) / 100;
                }
            }

            break;
        }
        default:
            break;
        }
    }

    // H2-EXT: Elixir of Life set - living troops take as if they had 15% more hit points
    if ( isExtLivingHardy() ) {
        dmg = dmg * 100 / 115;
    }

    return dmg;
}

void Battle::Unit::_spellRestoreAction( const Spell & spell, const uint32_t spellPoints, const HeroBase * applyingHero )
{
    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        // clear bad magic
        removeAffection( IS_BAD_MAGIC );

        // restore
        _hitPoints += fheroes2::getHPRestorePoints( spell, spellPoints, applyingHero );

        _hitPoints = std::min( _hitPoints, ArmyTroop::GetHitPoints() );
        break;

    case Spell::RESURRECT:
    case Spell::ANIMATEDEAD:
    case Spell::RESURRECTTRUE: {
        if ( !isValid() ) {
            Graveyard * graveyard = Arena::GetGraveyard();
            assert( graveyard != nullptr );

            graveyard->removeUnit( this );
        }

        const uint32_t restore = fheroes2::getResurrectPoints( spell, spellPoints, applyingHero );
        const uint32_t resurrect = _resurrect( restore, false, ( spell == Spell::RESURRECT ) );

        // Put the unit back on the board
        SetPosition( GetPosition() );

        if ( Arena::GetInterface() ) {
            std::string str( _n( "%{count} %{name} rises from the dead!", "%{count} %{name} rise from the dead!", resurrect ) );
            StringReplace( str, "%{count}", resurrect );
            StringReplace( str, "%{name}", Monster::GetPluralName( resurrect ) );
            Arena::GetInterface()->setStatus( str, true );
        }
        break;
    }

    default:
        assert( 0 );
        break;
    }
}

bool Battle::Unit::isDoubleAttack() const
{
    if ( !isArchers() || isHandFighting() ) {
        return isAbilityPresent( fheroes2::MonsterAbilityType::DOUBLE_MELEE_ATTACK );
    }

    // Archers with double shooting ability can only fire a second shot if they have enough ammo
    if ( isAbilityPresent( fheroes2::MonsterAbilityType::DOUBLE_SHOOTING ) ) {
        return GetShots() > 1;
    }

    return false;
}

uint32_t Battle::Unit::GetMagicResist( const Spell & spell, const HeroBase * applyingHero ) const
{
    if ( Modes( SP_ANTIMAGIC ) ) {
        return 100;
    }

    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        if ( !isHaveDamage() && !( modes & IS_BAD_MAGIC ) ) {
            return 100;
        }
        break;

    case Spell::RESURRECT:
    case Spell::RESURRECTTRUE:
    case Spell::ANIMATEDEAD:
        if ( GetCount() == _maxCount ) {
            return 100;
        }
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        if ( !( modes & IS_MAGIC ) ) {
            return 100;
        }
        break;

    case Spell::HYPNOTIZE:
        assert( applyingHero != nullptr );

        if ( fheroes2::getHypnotizeMonsterHPPoints( spell, applyingHero->GetPower(), applyingHero ) < _hitPoints ) {
            return 100;
        }
        break;

    default:
        break;
    }

    const Artifact spellImmunityArt = getImmunityArtifactForSpell( GetCommander(), spell );
    if ( spellImmunityArt.isValid() ) {
        return 100;
    }

    return fheroes2::getSpellResistance( id, spell.GetID() );
}

Spell Battle::Unit::GetSpellMagic( Rand::PCG32 & randomGenerator ) const
{
    const std::vector<fheroes2::MonsterAbility> & abilities = fheroes2::getMonsterData( GetID() ).battleStats.abilities;

    const auto abilityIter = std::find( abilities.begin(), abilities.end(), fheroes2::MonsterAbilityType::SPELL_CASTER );
    if ( abilityIter == abilities.end() ) {
        // Not a spell caster.
        return Spell::NONE;
    }

    // H2-EXT: Shaman's Skull - the creature's spell chance +10 (docs/H2_ITEM_CONCEPT_v5.md 3)
    const uint32_t percentage = abilityIter->percentage + ( H2Ext::wears( GetCommander(), Artifact::EXT_SHAMAN_SKULL ) ? 10U : 0U );
    if ( Rand::GetWithGen( 1, 100, randomGenerator ) > percentage ) {
        // No luck to cast the spell.
        return Spell::NONE;
    }

    return { static_cast<int32_t>( abilityIter->value ) };
}

bool Battle::Unit::isHaveDamage() const
{
    return _hitPoints < _maxCount * Monster::GetHitPoints();
}

bool Battle::Unit::SwitchAnimation( const int rule, const bool reverse /* = false */ )
{
    if ( rule == Monster_Info::STATIC && GetAnimationState() != Monster_Info::IDLE ) {
        // Reset the delay before switching to the 'IDLE' animation from 'STATIC'.
        checkIdleDelay();
    }

    return ( animation.switchAnimation( rule, reverse ) && animation.isValid() );
}

bool Battle::Unit::SwitchAnimation( const std::vector<int> & animationList, const bool reverse /* = false */ )
{
    return ( animation.switchAnimation( animationList, reverse ) && animation.isValid() );
}

int Battle::Unit::M82Attk( const Unit & enemy ) const
{
    const fheroes2::MonsterSound & sounds = fheroes2::getMonsterData( id ).sounds;

    if ( isArchers() && !isHandFighting( *this, enemy ) ) {
        // Added a new shooter without sound? Grant him a voice!
        assert( sounds.rangeAttack != M82::UNKNOWN );
        return sounds.rangeAttack;
    }

    assert( sounds.meleeAttack != M82::UNKNOWN );
    return sounds.meleeAttack;
}

int Battle::Unit::M82Kill() const
{
    return fheroes2::getMonsterData( id ).sounds.death;
}

int Battle::Unit::M82Move() const
{
    return fheroes2::getMonsterData( id ).sounds.movement;
}

int Battle::Unit::M82Wnce() const
{
    return fheroes2::getMonsterData( id ).sounds.wince;
}

int Battle::Unit::M82Expl() const
{
    return fheroes2::getMonsterData( id ).sounds.explosion;
}

int Battle::Unit::M82Tkof() const
{
    return fheroes2::getMonsterData( id ).sounds.takeoff;
}

int Battle::Unit::M82Land() const
{
    return fheroes2::getMonsterData( id ).sounds.landing;
}

fheroes2::Point Battle::Unit::GetBackPoint() const
{
    const fheroes2::Rect & rt = _position.GetRect();
    return _isReflected ? fheroes2::Point( rt.x + rt.width, rt.y + rt.height / 2 ) : fheroes2::Point( rt.x, rt.y + rt.height / 2 );
}

fheroes2::Point Battle::Unit::GetCenterPoint() const
{
    const fheroes2::Sprite & sprite = Assets::getImage( GetMonsterSprite(), GetFrame() );

    const fheroes2::Rect & pos = _position.GetRect();
    const int32_t centerY = pos.y + pos.height + sprite.y() / 2 - 10;

    return { pos.x + pos.width / 2, centerY };
}

fheroes2::Point Battle::Unit::GetStartMissileOffset( const size_t direction ) const
{
    return animation.getProjectileOffset( direction );
}

PlayerColor Battle::Unit::GetCurrentColor() const
{
    if ( Modes( SP_BERSERKER ) ) {
        return PlayerColor::UNUSED; // Be aware of unknown color
    }

    if ( Modes( SP_HYPNOTIZE ) ) {
        const Arena * arena = GetArena();
        assert( arena != nullptr );

        return arena->GetOppositeColor( GetArmyColor() );
    }

    return GetColor();
}

PlayerColor Battle::Unit::GetCurrentOrArmyColor() const
{
    const PlayerColor color = GetCurrentColor();

    // Unknown color in case of SP_BERSERKER mode
    if ( color == PlayerColor::UNUSED ) {
        return GetArmyColor();
    }

    return color;
}

int Battle::Unit::GetCurrentControl() const
{
    // Let's say that berserkers belong to AI, which is not present in the battle
    if ( Modes( SP_BERSERKER ) ) {
        return CONTROL_AI;
    }

    if ( Modes( SP_HYPNOTIZE ) ) {
        const Arena * arena = GetArena();
        assert( arena != nullptr );

        return arena->getForce( GetCurrentColor() ).GetControl();
    }

    return GetControl();
}

const HeroBase * Battle::Unit::GetCommander() const
{
    return GetArmy() ? GetArmy()->GetCommander() : nullptr;
}

const HeroBase * Battle::Unit::GetCurrentOrArmyCommander() const
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    return arena->getCommander( GetCurrentOrArmyColor() );
}

void Battle::Unit::_addAffection( const uint32_t mode, const uint32_t duration )
{
    assert( CountBits( mode ) == 1 );

    SetModes( mode );

    _affected.AddMode( mode, duration );
}

void Battle::Unit::removeAffection( const uint32_t mode )
{
    ResetModes( mode );

    _affected.RemoveMode( mode );
}

void Battle::Unit::_replaceAffection( const uint32_t modeToReplace, const uint32_t replacementMode, const uint32_t duration )
{
    removeAffection( modeToReplace );
    _addAffection( replacementMode, duration );
}

bool Battle::Unit::isExtLivingHardy() const
{
    return !isAbilityPresent( fheroes2::MonsterAbilityType::UNDEAD ) && !isAbilityPresent( fheroes2::MonsterAbilityType::ELEMENTAL )
           && H2Ext::heroHasSetSpecial( GetCommander(), H2Ext::SetSpecial::LIVING_HP_15 );
}

void Battle::Unit::extRegenerate()
{
    if ( !isValid() || !isExtLivingHardy() ) {
        return;
    }
    // only the top creature heals: no creature comes back
    const uint32_t full = GetCount() * Monster::GetHitPoints();
    if ( _hitPoints >= full ) {
        return;
    }
    _hitPoints = std::min( full, _hitPoints + std::max( 1U, Monster::GetHitPoints() / 10 ) );
}

void Battle::Unit::extDispel( const uint32_t magicToRemove )
{
    if ( Modes( IS_GOOD_MAGIC ) && H2Ext::heroHasSetSpecial( GetCommander(), H2Ext::SetSpecial::DISPEL_RESIST_50 ) ) {
        Arena * arena = GetArena();
        if ( arena != nullptr && Rand::GetWithGen( 0, 1, arena->extRandomGenerator() ) == 1 ) {
            removeAffection( magicToRemove & ~static_cast<uint32_t>( IS_GOOD_MAGIC ) );
            return;
        }
    }
    removeAffection( magicToRemove );
}

