/***************************************************************************
 *   H2-EXT: equipment window (docs/H2_ITEM_CONCEPT_v5.md 5-1)              *
 ***************************************************************************/

#pragma once

#include <cstdint>

class BagArtifacts;
class Heroes;

namespace fheroes2
{
    class Image;
}

namespace H2Ext
{
    // The hero's equipment window: worn slots around the portrait, misc slots, set effects, 15x4 backpack.
    // Read-only: right click information only. Returns true when anything was moved.
    bool showEquipmentWindow( Heroes & hero, bool readonly );

    // All sets on one screen: pieces owned / worn, tiers on (docs/H2_EXTRAS_CONCEPT_v1.md 4)
    void showSetCatalog( const Heroes & hero );

    // Two meeting heroes trade artifacts: a click moves an artifact to the other hero (its body slot if free, else the backpack).
    void showTradeWindow( Heroes & left, Heroes & right );

    // Set badge (docs/H2_ITEM_CONCEPT_v5.md 2-1): a small number plate, green while the set is incomplete, gold when complete.
    // (right, bottom) is the corner of the artifact picture it sits in.
    void drawSetBadge( fheroes2::Image & dst, int32_t right, int32_t bottom, int number, bool complete );

    // The badge of `artifactId` if it belongs to a set (worn pieces of `bag` decide the colour). Nothing for other artifacts.
    void drawArtifactSetBadge( fheroes2::Image & dst, int32_t right, int32_t bottom, int artifactId, const BagArtifacts & bag );
}
