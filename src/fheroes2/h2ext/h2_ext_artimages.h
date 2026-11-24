/***************************************************************************
 *   H2-EXT: pictures of the new items (docs/H2_ITEM_CONCEPT_v5.md 3        *
 *   "그림은 비슷한 아이템의 색을 바꿈"). Made at load time from the         *
 *   owner's HEROES2.AGG pictures of similar items, recoloured.             *
 ***************************************************************************/

#pragma once

#include <vector>

namespace fheroes2
{
    class Sprite;
}

namespace H2Ext
{
    enum class ArtifactImageKind
    {
        LARGE, // ARTIFACT.ICN, index = artifact id
        SMALL, // ARTFX.ICN, index = artifact id - 1
        MAP // OBJNARTI.ICN, shadow and item (newArtifactMapSprite)
    };

    // Append the pictures of the new items to a loaded ICN (nothing when the ICN does not have the HoMM2 pictures).
    void addNewArtifactImages( ArtifactImageKind kind, std::vector<fheroes2::Sprite> & sprites );
}
