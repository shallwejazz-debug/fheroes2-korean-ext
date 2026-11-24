// H2-EXT: castle buildings of the extension mode (docs/H2_EXTENSION_DESIGN_v5.md 1-3, 1-4). No new building: the original upgrade
// dwellings are used, and an upgraded dwelling sells both branches (owner 2026-11-18 20:09).
#pragma once

#include <cstdint>

#include "resource.h"

namespace H2Ext
{
    // An upgrade dwelling the original race does not have but the extension adds (Barbarian 3, 6 / Sorceress 5, 6 / Warlock 2, 3, 5 /
    // Wizard 2, 4 / Necromancer 6).
    bool isAddedUpgrade( int race, uint32_t building );
    // The original Black Tower (second Warlock tier 6 upgrade) is not built in extension games: Green upgrades once to Red or Black.
    bool isRemovedUpgrade( int race, uint32_t building );
    // DWELLING_UPGRADEn -> DWELLING_MONSTERn.
    uint32_t baseDwelling( uint32_t upgrade );
    // DWELLING_MONSTERn -> DWELLING_UPGRADEn.
    uint32_t upgradeDwelling( uint32_t dwelling );
    // Cost of an added upgrade dwelling ([제안] values).
    Funds addedUpgradeCost( int race, uint32_t building );
}
