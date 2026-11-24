/***************************************************************************
 *   H2-EXT: equipment slots for the HoMM2 extension mode (h2_ext_items.h)  *
 ***************************************************************************/

#include "h2_ext_items.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "artifact.h"
#include "h2_ext.h"
#include "h2_ext_sets.h"
#include "heroes_base.h"

namespace H2Ext
{
    size_t newBagSize()
    {
        return isOn() ? EXT_BAG_SIZE : BagArtifacts::maxCapacity;
    }

    bool isExtBag( const BagArtifacts & bag )
    {
        return bag.size() == EXT_BAG_SIZE;
    }

    size_t activeCount( const BagArtifacts & bag )
    {
        return isExtBag( bag ) ? WORN_COUNT : bag.size();
    }

    Slot slotOf( const int artifactId )
    {
        // [제안] docs/H2_ITEM_CONCEPT_v5.md 5-2 (HoMM2 has no body parts)
        switch ( artifactId ) {
        case Artifact::ULTIMATE_CROWN:
        case Artifact::DEFENDER_HELM:
        case Artifact::SKULLCAP:
        case Artifact::LIGHTNING_HELM:
        case Artifact::WIZARD_HAT:
        case Artifact::HIDEOUS_MASK:
        case Artifact::SPIKED_HELM:
        case Artifact::HELMET_ANDURAN:
        case Artifact::EXT_SAGE_SPECTACLES:
            return Slot::HEAD;
        case Artifact::ARCANE_NECKLACE:
        case Artifact::HOLY_PENDANT:
        case Artifact::PENDANT_FREE_WILL:
        case Artifact::PENDANT_LIFE:
        case Artifact::SERENITY_PENDANT:
        case Artifact::SEEING_EYE_PENDANT:
        case Artifact::KINETIC_PENDANT:
        case Artifact::PENDANT_DEATH:
            return Slot::NECK;
        case Artifact::ULTIMATE_CLOAK:
        case Artifact::ICE_CLOAK:
        case Artifact::FIRE_CLOAK:
        case Artifact::EXT_WIND_CLOAK:
            return Slot::CLOAK;
        case Artifact::ULTIMATE_SWORD:
        case Artifact::ULTIMATE_WAND:
        case Artifact::ULTIMATE_STAFF:
        case Artifact::THUNDER_MACE:
        case Artifact::GIANT_FLAIL:
        case Artifact::DRAGON_SWORD:
        case Artifact::POWER_AXE:
        case Artifact::WAND_NEGATION:
        case Artifact::GOLDEN_BOW:
        case Artifact::HOLY_HAMMER:
        case Artifact::LEGENDARY_SCEPTER:
        case Artifact::STAFF_WIZARDRY:
        case Artifact::SWORD_ANDURAN:
            return Slot::WEAPON;
        case Artifact::ULTIMATE_SHIELD:
        case Artifact::STEALTH_SHIELD:
        case Artifact::SPIKED_SHIELD:
        case Artifact::SWORD_BREAKER:
            return Slot::SHIELD;
        case Artifact::DIVINE_BREASTPLATE:
        case Artifact::BREASTPLATE_ANDURAN:
        case Artifact::BATTLE_GARB:
            return Slot::BODY;
        case Artifact::CASTER_BRACELET:
        case Artifact::ARMORED_GAUNTLETS:
        case Artifact::ARM_MARTYR:
            return Slot::HANDS;
        case Artifact::NOMAD_BOOTS_MOBILITY:
        case Artifact::TRAVELER_BOOTS_MOBILITY:
            return Slot::FEET;
        case Artifact::MAGE_RING:
        case Artifact::SNAKE_RING:
        case Artifact::ELEMENTAL_RING:
        case Artifact::POWER_RING:
        case Artifact::EXT_CHAMPION_RING:
            return Slot::RING;
        case Artifact::MAGIC_BOOK:
            return Slot::BOOK;
        default:
            return Slot::MISC;
        }
    }

    bool slotAccepts( const size_t index, const int artifactId )
    {
        if ( index >= WORN_COUNT ) {
            // backpack: anything but the magic book
            return index < EXT_BAG_SIZE && artifactId != Artifact::MAGIC_BOOK;
        }
        switch ( slotOf( artifactId ) ) {
        case Slot::HEAD:
            return index == SLOT_HEAD;
        case Slot::NECK:
            return index == SLOT_NECK;
        case Slot::CLOAK:
            return index == SLOT_CLOAK;
        case Slot::WEAPON:
            return index == SLOT_WEAPON;
        case Slot::SHIELD:
            return index == SLOT_SHIELD;
        case Slot::BODY:
            return index == SLOT_BODY;
        case Slot::HANDS:
            return index == SLOT_HANDS;
        case Slot::FEET:
            return index == SLOT_FEET;
        case Slot::RING:
            return index == SLOT_RING_FIRST || index == SLOT_RING_FIRST + 1;
        case Slot::MISC:
            return index >= SLOT_MISC_FIRST && index < SLOT_MISC_FIRST + SLOT_MISC_COUNT;
        case Slot::BOOK:
            return index == SLOT_BOOK;
        }
        return false;
    }

    bool pushToExtBag( BagArtifacts & bag, const Artifact & art )
    {
        const int id = art.GetID();
        if ( id == Artifact::MAGIC_BOOK ) {
            if ( bag[SLOT_BOOK].isValid() ) {
                return false;
            }
            bag[SLOT_BOOK] = art;
            return true;
        }
        for ( size_t i = 0; i < WORN_COUNT; ++i ) {
            if ( !bag[i].isValid() && slotAccepts( i, id ) ) {
                bag[i] = art;
                return true;
            }
        }
        for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
            if ( !bag[i].isValid() ) {
                bag[i] = art;
                return true;
            }
        }
        return false;
    }

    void convertToExtBag( BagArtifacts & bag )
    {
        if ( isExtBag( bag ) ) {
            return;
        }
        std::vector<Artifact> items;
        for ( const Artifact & art : bag ) {
            if ( art.isValid() ) {
                items.push_back( art );
            }
        }
        bag.assign( EXT_BAG_SIZE, Artifact( Artifact::UNKNOWN ) );
        for ( const Artifact & art : items ) {
            pushToExtBag( bag, art );
        }
    }

    void autoEquip( BagArtifacts & bag )
    {
        if ( !isExtBag( bag ) ) {
            return;
        }
        std::vector<Artifact> items;
        for ( size_t i = 0; i < EXT_BAG_SIZE; ++i ) {
            if ( i != SLOT_BOOK && bag[i].isValid() ) {
                items.push_back( bag[i] );
                bag[i].Reset();
            }
        }
        // the computer joins every misc set it owns whole into its combined artifact first (docs/H2_EXTRAS_CONCEPT_v1.md 5)
        for ( size_t setIndex = 0; setIndex < artifactSets().size(); ++setIndex ) {
            const int combined = combinedArtifactOf( static_cast<int>( setIndex ) );
            if ( combined == Artifact::UNKNOWN ) {
                continue;
            }
            const std::vector<int> & pieces = artifactSets()[setIndex].pieces;
            const bool all = std::all_of( pieces.begin(), pieces.end(), [&items]( const int id ) {
                return std::any_of( items.begin(), items.end(), [id]( const Artifact & a ) { return a.GetID() == id; } );
            } );
            if ( !all ) {
                continue;
            }
            for ( const int id : pieces ) {
                items.erase( std::find_if( items.begin(), items.end(), [id]( const Artifact & a ) { return a.GetID() == id; } ) );
            }
            items.emplace_back( combined );
        }
        // most valuable first, so it takes the worn slot; cursed artifacts (negative value) end in the backpack
        std::stable_sort( items.begin(), items.end(), []( const Artifact & a, const Artifact & b ) { return a.getArtifactValue() > b.getArtifactValue(); } );
        for ( const Artifact & art : items ) {
            if ( art.getArtifactValue() < 0 ) {
                for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
                    if ( !bag[i].isValid() ) {
                        bag[i] = art;
                        break;
                    }
                }
                continue;
            }
            pushToExtBag( bag, art );
        }

        // Owner 2026-11-19 14:45 "컴터도 당연히 세트 써야지": try every backpack artifact in every worn slot that takes it and keep the swap when the
        // worn artifacts plus the set effects are worth more (a few rounds, sets need several swaps)
        const auto wornValue = [&bag]() { return bag.getArtifactValue() + setBonusValue( bag ); };
        double best = wornValue();

        // First whole sets: wear every owned piece of a set at once (one piece at a time may lose value before the tier is reached)
        for ( size_t setIndex = 0; setIndex < artifactSets().size(); ++setIndex ) {
            const BagArtifacts before = bag;
            bool moved = false;
            for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
                if ( !bag[i].isValid() || setIndexOf( bag[i].GetID() ) != static_cast<int>( setIndex ) ) {
                    continue;
                }
                // the worn slot for it: empty first, else the least valuable artifact that is not a piece of this set
                size_t target = SLOT_BOOK;
                for ( size_t k = 0; k < SLOT_BOOK; ++k ) {
                    if ( !slotAccepts( k, bag[i].GetID() ) || ( bag[k].isValid() && setIndexOf( bag[k].GetID() ) == static_cast<int>( setIndex ) ) ) {
                        continue;
                    }
                    if ( target == SLOT_BOOK || !bag[k].isValid()
                         || ( bag[target].isValid() && bag[k].getArtifactValue() < bag[target].getArtifactValue() ) ) {
                        target = k;
                    }
                    if ( !bag[k].isValid() ) {
                        break;
                    }
                }
                if ( target != SLOT_BOOK ) {
                    std::swap( bag[i], bag[target] );
                    moved = true;
                }
            }
            if ( !moved ) {
                continue;
            }
            const double value = wornValue();
            if ( value > best + 0.001 ) {
                best = value;
            }
            else {
                bag = before;
            }
        }

        // then single swaps
        for ( int round = 0; round < 4; ++round ) {
            bool improved = false;
            for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
                if ( !bag[i].isValid() || bag[i].getArtifactValue() < 0 ) {
                    continue;
                }
                for ( size_t k = 0; k < SLOT_BOOK; ++k ) {
                    if ( !slotAccepts( k, bag[i].GetID() ) ) {
                        continue;
                    }
                    std::swap( bag[i], bag[k] );
                    const double value = wornValue();
                    if ( value > best + 0.001 ) {
                        best = value;
                        improved = true;
                        if ( !bag[i].isValid() ) {
                            break;
                        }
                    }
                    else {
                        std::swap( bag[i], bag[k] );
                    }
                }
            }
            if ( !improved ) {
                break;
            }
        }

        // the computer always joins a whole misc set into its combined artifact (owner 16:26 "합치기", docs/H2_EXTRAS_CONCEPT_v1.md 5) and fills the
        // freed worn slots from the backpack, most valuable first
        // (again after each refill: the freed slots may complete another set)
        for ( int pass = 0; pass < 4; ++pass ) {
            bool joined = false;
            for ( size_t setIndex = 0; setIndex < artifactSets().size(); ++setIndex ) {
                joined = combineSet( bag, static_cast<int>( setIndex ) ) || joined;
            }
            if ( !joined ) {
                break;
            }
            for ( bool moved = true; moved; ) {
                moved = false;
                size_t bestFrom = EXT_BAG_SIZE;
                size_t bestTo = EXT_BAG_SIZE;
                for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
                    if ( !bag[i].isValid() || bag[i].getArtifactValue() <= 0 ) {
                        continue;
                    }
                    for ( size_t k = 0; k < SLOT_BOOK; ++k ) {
                        if ( !bag[k].isValid() && slotAccepts( k, bag[i].GetID() )
                             && ( bestFrom == EXT_BAG_SIZE || bag[i].getArtifactValue() > bag[bestFrom].getArtifactValue() ) ) {
                            bestFrom = i;
                            bestTo = k;
                        }
                    }
                }
                if ( bestFrom != EXT_BAG_SIZE ) {
                    std::swap( bag[bestFrom], bag[bestTo] );
                    moved = true;
                }
            }
            // a set of the backpack whose pieces now fit the freed slots
            for ( size_t setIndex = 0; setIndex < artifactSets().size(); ++setIndex ) {
                if ( combinedArtifactOf( static_cast<int>( setIndex ) ) == Artifact::UNKNOWN ) {
                    continue;
                }
                // only when the bag has every piece
                bool all = true;
                for ( const int id : artifactSets()[setIndex].pieces ) {
                    all = all && std::any_of( bag.begin(), bag.end(), [id]( const Artifact & a ) { return a.GetID() == id; } );
                }
                if ( !all ) {
                    continue;
                }
                for ( size_t i = WORN_COUNT; i < EXT_BAG_SIZE; ++i ) {
                    if ( !bag[i].isValid() || setIndexOf( bag[i].GetID() ) != static_cast<int>( setIndex ) ) {
                        continue;
                    }
                    for ( size_t k = 0; k < SLOT_BOOK; ++k ) {
                        if ( slotAccepts( k, bag[i].GetID() )
                             && ( !bag[k].isValid() || ( setIndexOf( bag[k].GetID() ) != static_cast<int>( setIndex ) && setOfCombinedArtifact( bag[k].GetID() ) < 0 ) ) ) {
                            std::swap( bag[i], bag[k] );
                            break;
                        }
                    }
                }
            }
        }
    }

    void moveAllArtifacts( BagArtifacts & from, BagArtifacts & to )
    {
        for ( size_t i = 0; i < from.size(); ++i ) {
            if ( i != SLOT_BOOK && from[i].isValid() && pushToExtBag( to, from[i] ) ) {
                from[i].Reset();
            }
        }
    }

    void swapAllArtifacts( BagArtifacts & first, BagArtifacts & second )
    {
        std::vector<Artifact> fromFirst;
        std::vector<Artifact> fromSecond;
        for ( size_t i = 0; i < first.size(); ++i ) {
            if ( i != SLOT_BOOK && first[i].isValid() ) {
                fromFirst.push_back( first[i] );
                first[i].Reset();
            }
        }
        for ( size_t i = 0; i < second.size(); ++i ) {
            if ( i != SLOT_BOOK && second[i].isValid() ) {
                fromSecond.push_back( second[i] );
                second[i].Reset();
            }
        }
        // both bags have 75 places for non-book artifacts, so everything fits
        for ( const Artifact & art : fromFirst ) {
            pushToExtBag( second, art );
        }
        for ( const Artifact & art : fromSecond ) {
            pushToExtBag( first, art );
        }
    }

    bool isNewArtifact( const int artifactId )
    {
        return artifactId >= Artifact::EXT_GIANT_BELT && artifactId <= Artifact::EXT_SHAMAN_SKULL;
    }

    uint32_t newArtifactMapSprite( const int artifactId )
    {
        // OBJNARTI: the engine uses 0-205 (HoMM2 and the expansion) and 206-207 (its Magic Book); the new items take 208 and up,
        // shadow first, then the item (the same order as the original sprites)
        return 208 + static_cast<uint32_t>( artifactId - Artifact::EXT_GIANT_BELT ) * 2 + 1;
    }

    int newArtifactFromMapSprite( const uint32_t index )
    {
        if ( !isOn() || index < 209 || index > 208 + NEW_ARTIFACT_COUNT * 2 || index % 2 == 0 ) {
            return Artifact::UNKNOWN;
        }
        return Artifact::EXT_GIANT_BELT + static_cast<int>( index - 209 ) / 2;
    }

    bool wears( const HeroBase * hero, const int artifactId )
    {
        if ( hero == nullptr ) {
            return false;
        }
        const BagArtifacts & bag = hero->GetBagArtifacts();
        const size_t count = std::min( activeCount( bag ), bag.size() );
        for ( size_t i = 0; i < count; ++i ) {
            if ( bag[i].GetID() == artifactId ) {
                return true;
            }
        }
        return false;
    }

    Funds upgradeCost( const Funds & cost, const HeroBase * hero )
    {
        if ( !wears( hero, Artifact::EXT_SMITH_HAMMER ) ) {
            return cost;
        }
        // Smith's Hammer: 25% off, rounded down (docs/H2_ITEM_CONCEPT_v5.md 3)
        Funds result = cost;
        for ( int32_t * value : { &result.wood, &result.mercury, &result.ore, &result.sulfur, &result.crystal, &result.gems, &result.gold } ) {
            *value = *value * 3 / 4;
        }
        return result;
    }
}
