// Heroes I right-click quick info windows (homm1-remaster project): FUN_00459928 (map tile, qwikinfo.bin), FUN_0045c027 (hero,
// qhero0/1.bin), FUN_0045ce3a (town, qtown1.bin); note v121.
#pragma once

#include <cstdint>

class Castle;
class Heroes;

namespace H1
{
    // Right click on a map tile (the engine's game area click): castle, hero or tile window at the original place next to the tile.
    // Returns false when Heroes I graphics are not in use (the engine window runs).
    bool showMapQuickInfo( int32_t tileIndex );

    // Right click in the hero list (slot 0..3) or the castle list.
    bool showHeroListQuickInfo( const Heroes & hero, int slot );
    bool showCastleListQuickInfo( const Castle & castle );
}
