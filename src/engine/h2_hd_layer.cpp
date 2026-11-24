// H2-KOREAN: HD background layer, see h2_hd_layer.h
#include "h2_hd_layer.h"

#include <array>
#include <utility>

namespace
{
    std::array<fheroes2::HDLayer::Layer, fheroes2::HDLayer::slotCount> layers;
    uint32_t layerGeneration = 1;
    bool enabled = true;
}

namespace fheroes2::HDLayer
{
    void set( const int slot, const Rect & rect, std::vector<uint8_t> low, std::shared_ptr<const std::vector<uint32_t>> big )
    {
        if ( slot < 0 || slot >= slotCount ) {
            return;
        }
        Layer & layer = layers[slot];
        if ( layer.rect == rect && layer.big == big ) {
            return;
        }
        layer.rect = rect;
        layer.low = std::move( low );
        layer.big = std::move( big );
        ++layerGeneration;
    }

    void clear( const int slot )
    {
        if ( slot < 0 || slot >= slotCount || layers[slot].big == nullptr ) {
            return;
        }
        layers[slot] = {};
        ++layerGeneration;
    }

    const Layer & get( const int slot )
    {
        return layers[slot];
    }

    uint32_t generation()
    {
        return layerGeneration;
    }

    bool isEnabled()
    {
        return enabled;
    }

    void setEnabled( const bool enable )
    {
        if ( enabled != enable ) {
            enabled = enable;
            ++layerGeneration;
        }
    }

    bool anyActive()
    {
        if ( !enabled ) {
            return false;
        }
        for ( const Layer & layer : layers ) {
            if ( layer.big != nullptr ) {
                return true;
            }
        }
        return false;
    }
}
