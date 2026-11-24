/***************************************************************************
 *   H2-EXT: artifact sets (h2_ext_sets.h)                                  *
 *   [제안] docs/H2_ITEM_CONCEPT_v5.md 6 — numbers are proposals; sets that *
 *   need Price of Loyalty artifacts wait for that AGG (owner 09:11).        *
 ***************************************************************************/

#include "h2_ext_sets.h"
#include "settings.h" // H2-EXT battle only
#include "game.h" // H2-EXT battle only

#include <algorithm>

#include "artifact.h"
#include "h2_ext_items.h"
#include "heroes_base.h"
#include "h2_ext.h"
#include "translations.h"
#include "ui_text.h"

namespace
{
    using fheroes2::ArtifactBonus;
    using T = fheroes2::ArtifactBonusType;
    using H2Ext::SetSpecial;

    uint32_t bits( const SetSpecial s )
    {
        return static_cast<uint32_t>( s );
    }

    std::vector<H2Ext::SetDefinition> makeSets()
    {
        return {
            { 1,
              gettext_noop( "Knight's Arms" ),
              { Artifact::DRAGON_SWORD, Artifact::STEALTH_SHIELD, Artifact::DEFENDER_HELM, Artifact::DIVINE_BREASTPLATE, Artifact::ARMORED_GAUNTLETS },
              { { 2, { ArtifactBonus( T::ATTACK_SKILL, 1 ), ArtifactBonus( T::DEFENCE_SKILL, 1 ) }, 0, gettext_noop( "2: Attack and Defense +1" ) },
                { 3, { ArtifactBonus( T::ATTACK_SKILL, 1 ), ArtifactBonus( T::DEFENCE_SKILL, 1 ) }, 0, gettext_noop( "3: Attack and Defense +2" ) },
                { 5, {}, bits( SetSpecial::BATTLE_BLOODLUST_STONESKIN ), gettext_noop( "5: battle start, allies get Bloodlust and Stoneskin" ) } } },
            { 2,
              gettext_noop( "Spiked Pair" ),
              { Artifact::SPIKED_HELM, Artifact::SPIKED_SHIELD },
              { { 2, { ArtifactBonus( T::ATTACK_SKILL, 1 ), ArtifactBonus( T::DEFENCE_SKILL, 1 ) }, bits( SetSpecial::THORNS ),
                  gettext_noop( "2: Attack and Defense +1, melee attackers take back 10% of their damage" ) } } },
            { 3,
              gettext_noop( "Mage's Attire" ),
              { Artifact::WIZARD_HAT, Artifact::ARCANE_NECKLACE, Artifact::CASTER_BRACELET, Artifact::MAGE_RING, Artifact::WITCHES_BROACH },
              { { 2, { ArtifactBonus( T::SPELL_POWER_SKILL, 1 ) }, 0, gettext_noop( "2: Spell Power +1" ) },
                { 3, { ArtifactBonus( T::SPELL_POWER_SKILL, 1 ) }, 0, gettext_noop( "3: Spell Power +2" ) },
                { 5, {}, bits( SetSpecial::DAILY_FULL_SPELL_POINTS ), gettext_noop( "5: spell points refilled every day" ) } } },
            { 4,
              gettext_noop( "Elemental Ward" ),
              { Artifact::FIRE_CLOAK, Artifact::LIGHTNING_HELM, Artifact::BROACH_SHIELDING },
              { { 2, { ArtifactBonus( T::ELEMENTAL_SPELL_DAMAGE_REDUCTION_PERCENT, 25 ) }, 0, gettext_noop( "2: elemental spell damage -25% more" ) },
                { 3,
                  { ArtifactBonus( T::FIRE_SPELL_DAMAGE_REDUCTION_PERCENT, 100 ), ArtifactBonus( T::COLD_SPELL_DAMAGE_REDUCTION_PERCENT, 100 ),
                    ArtifactBonus( T::LIGHTNING_SPELL_DAMAGE_REDUCTION_PERCENT, 100 ) },
                  0, gettext_noop( "3: no damage from fire, cold and lightning spells" ) } } },
            { 5,
              gettext_noop( "Elemental Power" ),
              { Artifact::EVERHOT_LAVA_ROCK, Artifact::EVERCOLD_ICICLE, Artifact::LIGHTNING_ROD },
              { { 3,
                  { ArtifactBonus( T::FIRE_SPELL_EXTRA_EFFECTIVENESS_PERCENT, 25 ), ArtifactBonus( T::COLD_SPELL_EXTRA_EFFECTIVENESS_PERCENT, 25 ),
                    ArtifactBonus( T::LIGHTNING_SPELL_EXTRA_EFFECTIVENESS_PERCENT, 25 ) },
                  bits( SetSpecial::ELEMENTAL_SPELL_COST_HALF ), gettext_noop( "3: fire, cold and lightning spells +25% damage, half cost" ) } } },
            { 6,
              gettext_noop( "Four Medals" ),
              { Artifact::MEDAL_VALOR, Artifact::MEDAL_COURAGE, Artifact::MEDAL_HONOR, Artifact::MEDAL_DISTINCTION },
              { { 2, { ArtifactBonus( T::MORALE, 1 ) }, 0, gettext_noop( "2: Morale +1" ) },
                { 4, { ArtifactBonus( T::MAXIMUM_MORALE ) }, bits( SetSpecial::ENEMY_MORALE_MINUS_1 ), gettext_noop( "4: maximum morale, enemy morale -1" ) } } },
            { 7,
              gettext_noop( "Four Charms" ),
              { Artifact::RABBIT_FOOT, Artifact::GOLDEN_HORSESHOE, Artifact::GAMBLER_LUCKY_COIN, Artifact::FOUR_LEAF_CLOVER },
              { { 2, { ArtifactBonus( T::LUCK, 1 ) }, 0, gettext_noop( "2: Luck +1" ) },
                { 4, { ArtifactBonus( T::MAXIMUM_LUCK ) }, bits( SetSpecial::ENEMY_LUCK_MINUS_1 ), gettext_noop( "4: maximum luck, enemy luck -1" ) } } },
            { 8,
              gettext_noop( "Scrolls of Knowledge" ),
              { Artifact::MINOR_SCROLL, Artifact::MAJOR_SCROLL, Artifact::SUPERIOR_SCROLL, Artifact::FOREMOST_SCROLL },
              { { 2, { ArtifactBonus( T::KNOWLEDGE_SKILL, 1 ) }, 0, gettext_noop( "2: Knowledge +1" ) },
                { 4, { ArtifactBonus( T::KNOWLEDGE_SKILL, 1 ) }, bits( SetSpecial::EXPERIENCE_25 ), gettext_noop( "4: Knowledge +2, battle experience +25%" ) } } },
            { 9,
              gettext_noop( "Pair of Pearls" ),
              { Artifact::WHITE_PEARL, Artifact::BLACK_PEARL },
              { { 2, { ArtifactBonus( T::SPELL_POWER_SKILL, 1 ), ArtifactBonus( T::KNOWLEDGE_SKILL, 1 ), ArtifactBonus( T::SPELL_POINTS_DAILY_GENERATION, 3 ) }, 0,
                  gettext_noop( "2: Spell Power and Knowledge +1, spell points +3 a day" ) } } },
            { 10,
              gettext_noop( "Rare Riches" ),
              { Artifact::ENDLESS_VIAL_MERCURY, Artifact::ENDLESS_POUCH_SULFUR, Artifact::ENDLESS_POUCH_CRYSTAL, Artifact::ENDLESS_POUCH_GEMS },
              { { 2, { ArtifactBonus( T::GOLD_INCOME, 250 ) }, 0, gettext_noop( "2: gold +250 a day" ) },
                { 4,
                  { ArtifactBonus( T::MERCURY_INCOME, 2 ), ArtifactBonus( T::SULFUR_INCOME, 2 ), ArtifactBonus( T::CRYSTAL_INCOME, 2 ),
                    ArtifactBonus( T::GEMS_INCOME, 2 ) },
                  0, gettext_noop( "4: mercury, sulfur, crystal and gems +2 more a day" ) } } },
            { 11,
              gettext_noop( "Endless Gold" ),
              { Artifact::ENDLESS_SACK_GOLD, Artifact::ENDLESS_BAG_GOLD, Artifact::ENDLESS_PURSE_GOLD },
              { { 2, { ArtifactBonus( T::GOLD_INCOME, 250 ) }, 0, gettext_noop( "2: gold +250 a day" ) },
                { 3, {}, bits( SetSpecial::CASTLE_GROWTH_20 ), gettext_noop( "3: creature growth of your towns +20%" ) } } },
            { 12,
              gettext_noop( "Pathfinder" ),
              { Artifact::TRAVELER_BOOTS_MOBILITY, Artifact::TRUE_COMPASS_MOBILITY, Artifact::SAILORS_ASTROLABE_MOBILITY },
              { { 2, { ArtifactBonus( T::LAND_MOBILITY, 100 ) }, 0, gettext_noop( "2: land movement +100" ) },
                { 3, {}, bits( SetSpecial::BOARDING_FREE ), gettext_noop( "3: no movement lost boarding or leaving a boat" ) } } },
            { 13,
              gettext_noop( "Archer's Tools" ),
              { Artifact::GOLDEN_BOW, Artifact::AMMO_CART, Artifact::TELESCOPE },
              { { 3, { ArtifactBonus( T::AREA_REVEAL_DISTANCE, 1 ) }, bits( SetSpecial::RANGED_20 ), gettext_noop( "3: shooting troops +20% damage, view +1" ) } } },
            { 14,
              gettext_noop( "Time Magic" ),
              { Artifact::ENCHANTED_HOURGLASS, Artifact::GOLD_WATCH },
              { { 2, { ArtifactBonus( T::EVERY_COMBAT_SPELL_DURATION, 5 ) }, bits( SetSpecial::DISPEL_RESIST_50 ),
                  gettext_noop( "2: combat spells last 5 turns more" ) } } },
            { 15,
              gettext_noop( "Elixir of Life" ),
              { Artifact::PENDANT_LIFE, Artifact::ANKH, Artifact::SNAKE_RING },
              { { 2, { ArtifactBonus( T::RESURRECT_SPELL_EXTRA_EFFECTIVENESS_PERCENT, 50 ) }, 0, gettext_noop( "2: resurrect spells +50%" ) },
                { 3, {}, bits( SetSpecial::LIVING_HP_15 ), gettext_noop( "3: living troops +15% hit points, heal a little each turn" ) } } },
            { 16,
              gettext_noop( "Cursed Things" ),
              { Artifact::HIDEOUS_MASK, Artifact::EVIL_EYE, Artifact::FIZBIN_MISFORTUNE },
              { { 3, {}, bits( SetSpecial::BATTLE_SLOW_CURSE ), gettext_noop( "3: battle start, enemies get Slow and Curse" ) } } },
            // expansion items (HEROES2X.AGG, owner 2026-11-19 10:46)
            { 17,
              gettext_noop( "Hand of Death" ),
              { Artifact::ARM_MARTYR, Artifact::SKULLCAP, Artifact::SPADE_NECROMANCY },
              { { 3, {}, bits( SetSpecial::NECROMANCY_ZOMBIES ) | bits( SetSpecial::NO_MARTYR_PENALTY ),
                  gettext_noop( "3: necromancy raises Zombies, no morale penalty from the Arm" ) } } },
            // the expansion's own set: the engine joins the three into the Battle Garb (original rule)
            { 18,
              gettext_noop( "Anduran" ),
              { Artifact::SWORD_ANDURAN, Artifact::BREASTPLATE_ANDURAN, Artifact::HELMET_ANDURAN },
              { { 3, {}, 0, gettext_noop( "3: they join into the Battle Garb" ) } } },
            // owner 2026-11-21 00:02 ("바.걸만 넣자"): one spell only, 3 pieces on the neck, cloak and feet
            { 19,
              gettext_noop( "Wind Step" ),
              { Artifact::EXT_WIND_CLOAK, Artifact::NOMAD_BOOTS_MOBILITY, Artifact::KINETIC_PENDANT },
              { { 3, {}, bits( SetSpecial::BATTLE_HASTE ), gettext_noop( "3: battle start, allies get Haste" ) } } },
        };
    }
}

namespace H2Ext
{
    const std::vector<SetDefinition> & artifactSets()
    {
        static const std::vector<SetDefinition> sets = makeSets();
        return sets;
    }

    int setIndexOf( const int artifactId )
    {
        const std::vector<SetDefinition> & sets = artifactSets();
        for ( size_t i = 0; i < sets.size(); ++i ) {
            if ( std::find( sets[i].pieces.begin(), sets[i].pieces.end(), artifactId ) != sets[i].pieces.end() ) {
                return static_cast<int>( i );
            }
        }
        // a combined artifact belongs to its set (badge, set text)
        return setOfCombinedArtifact( artifactId );
    }

    bool setsApply( const BagArtifacts & bag )
    {
        // An extension bag, or any bag in the Battle Only mode (its 14 slots all count as worn; owner 2026-11-20 16:10 set presets)
        return isExtBag( bag ) || ( isOn() && Settings::Get().IsGameType( Game::TYPE_BATTLEONLY ) );
    }

    int wornPieces( const BagArtifacts & bag, const int setIndex )
    {
        if ( !setsApply( bag ) || setIndex < 0 ) {
            return 0;
        }
        const size_t wornCount = std::min( activeCount( bag ), bag.size() );
        const std::vector<int> & pieces = artifactSets()[static_cast<size_t>( setIndex )].pieces;
        // a worn combined artifact is the whole set (docs/H2_EXTRAS_CONCEPT_v1.md 5)
        const int combined = combinedArtifactOf( setIndex );
        if ( combined != Artifact::UNKNOWN ) {
            for ( size_t i = 0; i < wornCount; ++i ) {
                if ( bag[i].GetID() == combined ) {
                    return static_cast<int>( pieces.size() );
                }
            }
        }
        int count = 0;
        for ( const int id : pieces ) {
            for ( size_t i = 0; i < wornCount; ++i ) {
                if ( bag[i].GetID() == id ) {
                    ++count;
                    break;
                }
            }
        }
        return count;
    }

    bool isSetComplete( const BagArtifacts & bag, const int setIndex )
    {
        return setIndex >= 0 && wornPieces( bag, setIndex ) >= static_cast<int>( artifactSets()[static_cast<size_t>( setIndex )].pieces.size() );
    }

    std::vector<ActiveSetBonus> activeSetBonuses( const BagArtifacts & bag )
    {
        std::vector<ActiveSetBonus> result;
        if ( !setsApply( bag ) ) {
            return result;
        }
        const std::vector<SetDefinition> & sets = artifactSets();
        for ( size_t s = 0; s < sets.size(); ++s ) {
            const int worn = wornPieces( bag, static_cast<int>( s ) );
            for ( const SetTier & tier : sets[s].tiers ) {
                if ( worn >= tier.pieces ) {
                    for ( const fheroes2::ArtifactBonus & b : tier.bonuses ) {
                        result.push_back( { b, static_cast<int>( s ) } );
                    }
                }
            }
        }
        return result;
    }

    bool hasSetSpecial( const BagArtifacts & bag, const SetSpecial special )
    {
        if ( !setsApply( bag ) ) {
            return false;
        }
        const std::vector<SetDefinition> & sets = artifactSets();
        for ( size_t s = 0; s < sets.size(); ++s ) {
            const int worn = wornPieces( bag, static_cast<int>( s ) );
            for ( const SetTier & tier : sets[s].tiers ) {
                if ( worn >= tier.pieces && ( tier.specials & static_cast<uint32_t>( special ) ) != 0 ) {
                    return true;
                }
            }
        }
        return false;
    }

    bool heroHasSetSpecial( const HeroBase * hero, const SetSpecial special )
    {
        return hero != nullptr && hasSetSpecial( hero->GetBagArtifacts(), special );
    }

    uint32_t battleExperience( const HeroBase * hero, const uint32_t experience )
    {
        return heroHasSetSpecial( hero, SetSpecial::EXPERIENCE_25 ) ? experience + experience / 4 : experience;
    }

    std::string setDescription( const int artifactId )
    {
        const int setIndex = setIndexOf( artifactId );
        if ( setIndex < 0 || !isOn() ) {
            return {};
        }
        const SetDefinition & set = artifactSets()[static_cast<size_t>( setIndex )];
        std::string text = std::string( _( "Set" ) ) + " " + std::to_string( set.number ) + ": " + _( set.name ) + "\n";
        for ( size_t i = 0; i < set.pieces.size(); ++i ) {
            text += Artifact( set.pieces[i] ).GetName();
            text += ( i + 1 < set.pieces.size() ) ? ", " : "\n";
        }
        for ( const SetTier & tier : set.tiers ) {
            text += _( tier.text );
            text += "\n";
        }
        return text;
    }

    void appendSetDescription( fheroes2::MultiFontText & text, const int artifactId, const BagArtifacts * bag, const bool withHeading /* = true */ )
    {
        const int setIndex = setIndexOf( artifactId );
        if ( setIndex < 0 || !isOn() ) {
            return;
        }
        const fheroes2::FontType white = fheroes2::FontType::normalWhite();
        const fheroes2::FontType yellow = fheroes2::FontType::normalYellow();
        const fheroes2::FontType grey{ fheroes2::FontSize::NORMAL, fheroes2::FontColor::GRAY };

        const SetDefinition & set = artifactSets()[static_cast<size_t>( setIndex )];
        if ( withHeading ) {
            text.add( fheroes2::Text( std::string( "\n\n" ) + _( "Set" ) + " " + std::to_string( set.number ) + ": " + _( set.name ) + "\n", yellow ) );
        }

        const bool extBag = bag != nullptr && setsApply( *bag );
        const size_t worn = extBag ? std::min( activeCount( *bag ), bag->size() ) : 0;
        for ( size_t i = 0; i < set.pieces.size(); ++i ) {
            bool isWorn = false;
            for ( size_t k = 0; k < worn; ++k ) {
                isWorn = isWorn || ( *bag )[k].GetID() == set.pieces[i];
            }
            std::string piece = Artifact( set.pieces[i] ).GetName();
            piece += ( i + 1 < set.pieces.size() ) ? ", " : "\n";
            text.add( fheroes2::Text( std::move( piece ), ( !extBag || isWorn ) ? white : grey ) );
        }
        const int wornCount = extBag ? wornPieces( *bag, setIndex ) : 0;
        for ( size_t t = 0; t < set.tiers.size(); ++t ) {
            const SetTier & tier = set.tiers[t];
            std::string line = _( tier.text );
            if ( t + 1 < set.tiers.size() ) {
                line += "\n";
            }
            text.add( fheroes2::Text( std::move( line ), !extBag ? white : ( wornCount >= tier.pieces ? yellow : grey ) ) );
        }
    }

    double setBonusValue( const BagArtifacts & bag )
    {
        double value = 0;
        for ( const ActiveSetBonus & b : activeSetBonuses( bag ) ) {
            value += Artifact::getBonusValue( b.bonus );
        }
        for ( uint32_t bit = 0; bit < 32; ++bit ) {
            if ( hasSetSpecial( bag, static_cast<SetSpecial>( 1U << bit ) ) ) {
                value += 2;
            }
        }
        return value;
    }

    int combinedArtifactOf( const int setIndex )
    {
        if ( setIndex < 0 || static_cast<size_t>( setIndex ) >= artifactSets().size() ) {
            return Artifact::UNKNOWN;
        }
        switch ( artifactSets()[static_cast<size_t>( setIndex )].number ) {
        case 6:
            return Artifact::EXT_HERO_MEDAL;
        case 7:
            return Artifact::EXT_LUCKY_CHARM;
        case 8:
            return Artifact::EXT_TOME_KNOWLEDGE;
        default:
            return Artifact::UNKNOWN;
        }
    }

    int setOfCombinedArtifact( const int artifactId )
    {
        if ( artifactId == Artifact::UNKNOWN ) {
            return -1;
        }
        for ( size_t i = 0; i < artifactSets().size(); ++i ) {
            if ( combinedArtifactOf( static_cast<int>( i ) ) == artifactId ) {
                return static_cast<int>( i );
            }
        }
        return -1;
    }

    bool canCombineSet( const BagArtifacts & bag, const int setIndex )
    {
        if ( combinedArtifactOf( setIndex ) == Artifact::UNKNOWN || !isExtBag( bag ) ) {
            return false;
        }
        // every piece worn (not the combined artifact itself)
        for ( const int id : artifactSets()[static_cast<size_t>( setIndex )].pieces ) {
            bool worn = false;
            for ( size_t i = 0; i < WORN_COUNT; ++i ) {
                worn = worn || bag[i].GetID() == id;
            }
            if ( !worn ) {
                return false;
            }
        }
        return true;
    }

    bool combineSet( BagArtifacts & bag, const int setIndex )
    {
        if ( !canCombineSet( bag, setIndex ) ) {
            return false;
        }
        size_t first = WORN_COUNT;
        for ( const int id : artifactSets()[static_cast<size_t>( setIndex )].pieces ) {
            for ( size_t i = 0; i < WORN_COUNT; ++i ) {
                if ( bag[i].GetID() == id ) {
                    bag[i].Reset();
                    first = std::min( first, i );
                    break;
                }
            }
        }
        bag[first] = Artifact( combinedArtifactOf( setIndex ) );
        return true;
    }

    bool splitCombined( BagArtifacts & bag, const size_t index )
    {
        if ( !isExtBag( bag ) || index >= bag.size() ) {
            return false;
        }
        const int setIndex = setOfCombinedArtifact( bag[index].GetID() );
        if ( setIndex < 0 ) {
            return false;
        }
        const std::vector<int> & pieces = artifactSets()[static_cast<size_t>( setIndex )].pieces;
        // room: the combined artifact's own place + free worn misc slots + free backpack places
        size_t free = 1;
        for ( size_t i = 0; i < bag.size(); ++i ) {
            free += ( i != SLOT_BOOK && !bag[i].isValid() && ( i >= WORN_COUNT || slotOf( pieces.front() ) == Slot::MISC ) ) ? 1 : 0;
        }
        if ( free < pieces.size() ) {
            return false;
        }
        bag[index].Reset();
        for ( const int id : pieces ) {
            pushToExtBag( bag, Artifact( id ) );
        }
        return true;
    }
}

