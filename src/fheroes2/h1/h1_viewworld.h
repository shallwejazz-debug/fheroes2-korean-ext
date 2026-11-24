// Heroes I "view world" (homm1-remaster project): FUN_0045f09e, view-00..05.bin. See docs/analysis note v77.
#pragma once

namespace H1
{
    // mode: 0x13 mines, 0x14 resources, 0x15 artifacts, 0x16 towns, 0x17 heroes, 0x18 world (view-<mode - 0x13>.bin).
    // allMarkers: param_2 (markers also under the fog), revealAll: param_3 (whole map).
    // The adventure screen's View World button calls (0x18, false, false); the view spells call (mode, true, mode == 0x18).
    // Returns false (nothing shown) when the current map has no HoMM1 tile data.
    bool showViewWorld( int mode, bool allMarkers, bool revealAll );
}
