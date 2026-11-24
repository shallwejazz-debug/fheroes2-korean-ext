/***************************************************************************
 *   H2-EXT: pictures of the new items, see h2_ext_artimages.h              *
 ***************************************************************************/

#include "h2_ext_artimages.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "artifact.h"
#include "h2_ext_items.h"
#include "h2_ext_monsters.h"
#include "image.h"

namespace
{
    struct NewItemPicture
    {
        int artifactId;
        int baseArtifactId; // the similar HoMM2 item whose picture is recoloured [제안]
        double hue;
        double saturation;
        double value;
        double tint;
        uint8_t tintR;
        uint8_t tintG;
        uint8_t tintB;
    };

    // [제안] picture choices (owner sees them in game)
    const std::array<NewItemPicture, H2Ext::NEW_ARTIFACT_COUNT + 3> newItemPictures = { {
        { Artifact::EXT_GIANT_BELT, Artifact::CASTER_BRACELET, 190, 0.35, 0.95, 0, 0, 0, 0 }, // steel band
        { Artifact::EXT_SAGE_SPECTACLES, Artifact::TELESCOPE, 0, 0.15, 1.1, 0, 0, 0, 0 }, // silver lens
        { Artifact::EXT_WIND_CLOAK, Artifact::ICE_CLOAK, -90, 0.7, 1.15, 0, 0, 0, 0 }, // green cloak
        { Artifact::EXT_CHAMPION_RING, Artifact::MAGE_RING, 0, 1, 1.05, 0.7, 230, 170, 40 }, // golden ring
        { Artifact::EXT_MERCHANT_SCALES, Artifact::GOLD_WATCH, 0, 0.2, 1.1, 0, 0, 0, 0 }, // silver
        { Artifact::EXT_SMITH_HAMMER, Artifact::THUNDER_MACE, 0, 0.6, 0.8, 0.6, 200, 90, 30 }, // forged iron
        { Artifact::EXT_HUNTER_QUIVER, Artifact::GOLDEN_BOW, 80, 0.8, 0.9, 0, 0, 0, 0 }, // green bow
        { Artifact::EXT_SHAMAN_SKULL, Artifact::SPIKED_HELM, 100, 1.2, 1.1, 0, 0, 0, 0 }, // green skull
        // combined artifacts (docs/H2_EXTRAS_CONCEPT_v1.md 5)
        { Artifact::EXT_HERO_MEDAL, Artifact::MEDAL_DISTINCTION, 0, 1, 1.1, 0.7, 240, 190, 60 }, // gold medal
        { Artifact::EXT_LUCKY_CHARM, Artifact::FOUR_LEAF_CLOVER, 0, 1, 1.1, 0.7, 240, 200, 70 }, // golden clover
        { Artifact::EXT_TOME_KNOWLEDGE, Artifact::FOREMOST_SCROLL, 260, 1.2, 1.0, 0, 0, 0, 0 }, // purple scroll
    } };

    bool isOpaque( const fheroes2::Sprite & sprite, const int32_t pos )
    {
        return sprite.singleLayer() || sprite.transform()[pos] == 0;
    }

    // The item picture with its own colours changed and the coloured background kept: the background is every area of the colours found on the
    // picture's border that touches the border, or is large (the hole of a ring).
    fheroes2::Sprite recolourIcon( const fheroes2::Sprite & base, const std::vector<uint8_t> & palette )
    {
        fheroes2::Sprite result = base;
        const int32_t w = result.width();
        const int32_t h = result.height();
        if ( w <= 2 || h <= 2 ) {
            return result;
        }
        uint8_t * image = result.image();

        std::array<bool, 256> borderColour{};
        for ( int32_t x = 0; x < w; ++x ) {
            borderColour[image[x]] = true;
            borderColour[image[( h - 1 ) * w + x]] = true;
        }
        for ( int32_t y = 0; y < h; ++y ) {
            borderColour[image[y * w]] = true;
            borderColour[image[y * w + w - 1]] = true;
        }

        std::vector<uint8_t> state( static_cast<size_t>( w * h ), 0 ); // 0 not visited, 1 background, 2 item
        std::vector<int32_t> component;
        std::vector<int32_t> stack;
        for ( int32_t start = 0; start < w * h; ++start ) {
            if ( state[start] != 0 || !borderColour[image[start]] ) {
                continue;
            }
            component.clear();
            stack.assign( 1, start );
            bool touchesBorder = false;
            state[start] = 2;
            while ( !stack.empty() ) {
                const int32_t pos = stack.back();
                stack.pop_back();
                component.push_back( pos );
                const int32_t x = pos % w;
                const int32_t y = pos / w;
                touchesBorder = touchesBorder || x == 0 || y == 0 || x == w - 1 || y == h - 1;
                const int32_t next[4] = { x > 0 ? pos - 1 : -1, x < w - 1 ? pos + 1 : -1, y > 0 ? pos - w : -1, y < h - 1 ? pos + w : -1 };
                for ( const int32_t n : next ) {
                    if ( n >= 0 && state[n] == 0 && borderColour[image[n]] ) {
                        state[n] = 2;
                        stack.push_back( n );
                    }
                }
            }
            if ( touchesBorder || component.size() >= 25 ) {
                for ( const int32_t pos : component ) {
                    state[pos] = 1;
                }
            }
        }

        for ( int32_t pos = 0; pos < w * h; ++pos ) {
            if ( state[pos] != 1 && isOpaque( result, pos ) ) {
                image[pos] = palette[image[pos]];
            }
        }
        return result;
    }

    // A map sprite has a transparent background: every drawn pixel is the item.
    fheroes2::Sprite recolourMapSprite( const fheroes2::Sprite & base, const std::vector<uint8_t> & palette )
    {
        fheroes2::Sprite result = base;
        uint8_t * image = result.image();
        const int32_t size = result.width() * result.height();
        for ( int32_t pos = 0; pos < size; ++pos ) {
            if ( isOpaque( result, pos ) ) {
                image[pos] = palette[image[pos]];
            }
        }
        return result;
    }
}

namespace H2Ext
{
    void addNewArtifactImages( const ArtifactImageKind kind, std::vector<fheroes2::Sprite> & sprites )
    {
        // Only for the HoMM2 pictures: all the base items are in HoMM2 (ARTIFACT 83, ARTFX 82, OBJNARTI 172 sprites)
        const size_t needed = kind == ArtifactImageKind::LARGE ? 83 : kind == ArtifactImageKind::SMALL ? 82 : 172;
        if ( sprites.size() < needed ) {
            return;
        }

        const size_t newSize = kind == ArtifactImageKind::LARGE   ? static_cast<size_t>( Artifact::EXT_TOME_KNOWLEDGE ) + 1
                               : kind == ArtifactImageKind::SMALL ? static_cast<size_t>( Artifact::EXT_TOME_KNOWLEDGE )
                                                                  : static_cast<size_t>( newArtifactMapSprite( Artifact::EXT_TOME_KNOWLEDGE ) ) + 1;
        if ( sprites.size() < newSize ) {
            sprites.resize( newSize );
        }

        for ( const NewItemPicture & item : newItemPictures ) {
            const std::vector<uint8_t> palette = makeRecolorPalette( item.hue, item.saturation, item.value, item.tint, item.tintR, item.tintG, item.tintB );
            switch ( kind ) {
            case ArtifactImageKind::LARGE:
                sprites[item.artifactId] = recolourIcon( sprites[item.baseArtifactId], palette );
                break;
            case ArtifactImageKind::SMALL:
                sprites[item.artifactId - 1] = recolourIcon( sprites[item.baseArtifactId - 1], palette );
                break;
            case ArtifactImageKind::MAP: {
                const uint32_t body = newArtifactMapSprite( item.artifactId );
                const size_t baseBody = static_cast<size_t>( item.baseArtifactId - 1 ) * 2 + 1;
                sprites[body - 1] = sprites[baseBody - 1];
                sprites[body] = recolourMapSprite( sprites[baseBody], palette );
                break;
            }
            }
        }
    }
}
