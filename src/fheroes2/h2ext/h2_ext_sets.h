/***************************************************************************
 *   H2-EXT: artifact sets (docs/H2_ITEM_CONCEPT_v5.md 6)                   *
 *   A set's pieces count only when worn (extension bag, h2_ext_items.h).   *
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "artifact_info.h"

class BagArtifacts;
class HeroBase;

namespace fheroes2
{
    class MultiFontText;
}

namespace H2Ext
{
    // Set effects that are new rules (◆), checked where they act
    enum class SetSpecial : uint32_t
    {
        NONE = 0,
        BATTLE_BLOODLUST_STONESKIN = 1 << 0, // 1 knight's arms: allies get Bloodlust and Stoneskin when a battle starts
        THORNS = 1 << 1, // 2 spikes: a melee attacker on our troops takes 10% of the damage it dealt
        DAILY_FULL_SPELL_POINTS = 1 << 2, // 3 mage's attire
        ENEMY_MORALE_MINUS_1 = 1 << 3, // 6 medals
        ENEMY_LUCK_MINUS_1 = 1 << 4, // 7 luck
        ELEMENTAL_SPELL_COST_HALF = 1 << 5, // 5 elemental power: fire, cold and lightning spells cost half
        EXPERIENCE_25 = 1 << 6, // 8 scrolls: battle experience +25%
        CASTLE_GROWTH_20 = 1 << 7, // 11 endless gold: creature growth of the kingdom's towns +20%
        BOARDING_FREE = 1 << 8, // 12 pathfinder: boarding and leaving a boat costs no movement
        RANGED_20 = 1 << 9, // 13 archer's tools: shooting troops +20% damage
        DISPEL_RESIST_50 = 1 << 10, // 14 time magic
        LIVING_HP_15 = 1 << 11, // 15 elixir of life
        BATTLE_SLOW_CURSE = 1 << 12, // 16 cursed things: enemies get Slow and Curse when a battle starts
        NECROMANCY_ZOMBIES = 1 << 13, // 17 hand of death: necromancy raises Zombies instead of Skeletons
        NO_MARTYR_PENALTY = 1 << 14, // 17 hand of death: the Arm of the Martyr gives no undead morale penalty
        BATTLE_HASTE = 1 << 15, // 19 wind step: allies get Haste when a battle starts (owner 2026-11-21 00:02)
    };

    struct SetTier
    {
        int pieces; // worn pieces needed
        std::vector<fheroes2::ArtifactBonus> bonuses; // engine bonus types, added on top of the earlier tiers
        uint32_t specials; // SetSpecial bits
        const char * text; // shown in the set panel / information window (translated)
    };

    struct SetDefinition
    {
        int number; // the number on the badge
        const char * name;
        std::vector<int> pieces; // artifact ids
        std::vector<SetTier> tiers;
    };

    const std::vector<SetDefinition> & artifactSets();

    // The set an artifact belongs to (index in artifactSets()), -1 when none
    int setIndexOf( int artifactId );

    // Sets count for this bag: an extension bag, or any bag in the Battle Only mode
    bool setsApply( const BagArtifacts & bag );

    // Worn pieces of set `setIndex` in the bag (0 for a non-extension bag)
    int wornPieces( const BagArtifacts & bag, int setIndex );

    // Every set has all its pieces worn
    bool isSetComplete( const BagArtifacts & bag, int setIndex );

    // Engine bonuses granted by the worn sets of this bag, and from which set (for the description)
    struct ActiveSetBonus
    {
        fheroes2::ArtifactBonus bonus;
        int setIndex;
    };
    std::vector<ActiveSetBonus> activeSetBonuses( const BagArtifacts & bag );

    bool hasSetSpecial( const BagArtifacts & bag, SetSpecial special );

    // Combined artifacts (docs/H2_EXTRAS_CONCEPT_v1.md 5): the one-slot artifact of a set (Artifact::UNKNOWN when the set has none)
    int combinedArtifactOf( int setIndex );
    // The set a combined artifact stands for, -1 when the artifact is not combined
    int setOfCombinedArtifact( int artifactId );
    // All pieces of the set are worn, so they can be joined
    bool canCombineSet( const BagArtifacts & bag, int setIndex );
    // Join the worn pieces into the combined artifact (in the place of the first piece). False when not possible.
    bool combineSet( BagArtifacts & bag, int setIndex );
    // Split the combined artifact at `index` back into its pieces (worn slots first, then backpack). False when there is no room.
    bool splitCombined( BagArtifacts & bag, size_t index );

    // Value of the worn set effects for the computer (the engine's artifact value of each bonus, each new rule 2) [제안]
    double setBonusValue( const BagArtifacts & bag );

    // The hero has the worn set effect (nullptr: no)
    bool heroHasSetSpecial( const HeroBase * hero, SetSpecial special );

    // Battle experience with the Scrolls of Knowledge set (+25%)
    uint32_t battleExperience( const HeroBase * hero, uint32_t experience );

    // Extension game: the set part of an artifact's description ("" when the artifact is not in a set)
    std::string setDescription( int artifactId );

    // Extension game: the same, coloured for the bag of the hero (docs/H2_ITEM_CONCEPT_v5.md 2-1): the set name yellow, worn pieces white and
    // the others grey, active tiers yellow and the others grey. Without a bag everything white. Nothing when the artifact is not in a set.
    // withHeading false: only the pieces and the tiers (the set catalog window shows the name as the window's header, owner 2026-11-23 14:50)
    void appendSetDescription( fheroes2::MultiFontText & text, int artifactId, const BagArtifacts * bag, bool withHeading = true );
}
