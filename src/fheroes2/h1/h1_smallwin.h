// Heroes I small windows (homm1-remaster project): scenario information (sceninfo.bin, FUN_0041f727) and the Dimension Door target
// (dimdoor.bin, FUN_00462adf / FUN_0046138c); note v123.
#pragma once

#include <cstdint>

namespace H1
{
    // Returns false when Heroes I graphics are not in use (the engine window runs).
    bool showScenarioInfo();

    // FUN_00462adf: picks a target tile in the shown map. Returns the tile, -1 when cancelled, or -2 when the chosen tile does not suit
    // (land / water: "Dimension Door failed!!!" was shown; the spell is used up all the same).
    int32_t dimensionDoorDestination( bool onBoat );
}
