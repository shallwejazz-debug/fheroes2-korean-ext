/***************************************************************************
 *   H2-EXT: equipment slots for the HoMM2 extension mode                   *
 *   docs/H2_ITEM_CONCEPT_v5.md (owner 2026-11-19 08:30 "3편식 부위 착용",  *
 *   09:01 equipment window, 09:11 "5번 빼고 있는 것만 먼저 해")           *
 ***************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

class Artifact;
class BagArtifacts;
class HeroBase;

#include "resource.h"

namespace H2Ext
{
    // In an extension game a hero's artifact bag has a fixed layout:
    //   0 head, 1 neck, 2 cloak, 3 weapon, 4 shield, 5 body, 6 hands, 7 feet, 8-9 rings, 10-14 misc, 15 magic book  = worn (effects count)
    //   16 .. 75 backpack (no effect)
    // An original game keeps the engine's 14 slots. The bag size tells which layout a bag has.
    enum class Slot : uint8_t
    {
        HEAD,
        NECK,
        CLOAK,
        WEAPON,
        SHIELD,
        BODY,
        HANDS,
        FEET,
        RING,
        MISC,
        BOOK
    };

    constexpr size_t SLOT_HEAD = 0;
    constexpr size_t SLOT_NECK = 1;
    constexpr size_t SLOT_CLOAK = 2;
    constexpr size_t SLOT_WEAPON = 3;
    constexpr size_t SLOT_SHIELD = 4;
    constexpr size_t SLOT_BODY = 5;
    constexpr size_t SLOT_HANDS = 6;
    constexpr size_t SLOT_FEET = 7;
    constexpr size_t SLOT_RING_FIRST = 8;
    constexpr size_t SLOT_MISC_FIRST = 10;
    constexpr size_t SLOT_MISC_COUNT = 5;
    constexpr size_t SLOT_BOOK = 15;
    constexpr size_t WORN_COUNT = 16;
    constexpr size_t BACKPACK_COUNT = 60;
    constexpr size_t EXT_BAG_SIZE = WORN_COUNT + BACKPACK_COUNT;

    // Bag size for a new hero bag: the extension layout in an extension game, otherwise the engine's 14.
    size_t newBagSize();

    bool isExtBag( const BagArtifacts & bag );

    // Number of bag entries whose artifacts take effect (worn part of an extension bag, the whole bag otherwise).
    size_t activeCount( const BagArtifacts & bag );

    // The body part of an artifact (docs/H2_ITEM_CONCEPT_v5.md 5-2).
    Slot slotOf( int artifactId );

    // True when bag index `index` of an extension bag accepts the artifact.
    bool slotAccepts( size_t index, int artifactId );

    // Extension bag: put the artifact in its free worn slot, else in the first free backpack place. False when there is no room.
    bool pushToExtBag( BagArtifacts & bag, const Artifact & art );

    // Turn a 14-slot bag into the extension layout (new extension game, or an extension save made before the layout).
    void convertToExtBag( BagArtifacts & bag );

    // Extension bags: move every artifact (not the magic book) from one hero to the other, as far as there is room.
    void moveAllArtifacts( BagArtifacts & from, BagArtifacts & to );

    // Extension bags: the two heroes exchange all their artifacts (not the magic books), as far as there is room.
    void swapAllArtifacts( BagArtifacts & first, BagArtifacts & second );

    // Extension bag: wear the most valuable artifacts, the rest go to the backpack (computer players; replaces the engine's sort).
    void autoEquip( BagArtifacts & bag );

    // New items of the extension mode (Artifact::EXT_GIANT_BELT .. EXT_SHAMAN_SKULL, docs/H2_ITEM_CONCEPT_v5.md 3)
    constexpr int NEW_ARTIFACT_COUNT = 8;
    bool isNewArtifact( int artifactId );

    // Adventure map sprite (OBJNARTI) of a new item; the shadow is the index before it.
    uint32_t newArtifactMapSprite( int artifactId );

    // The new item of an OBJNARTI sprite index (extension game only), Artifact::UNKNOWN otherwise.
    int newArtifactFromMapSprite( uint32_t index );

    // The hero wears the artifact (worn part of the bag; the whole bag for an original 14-slot bag).
    bool wears( const HeroBase * hero, int artifactId );

    // Upgrade cost for the troops of `hero` (Smith's Hammer: -25%).
    Funds upgradeCost( const Funds & cost, const HeroBase * hero );
}
