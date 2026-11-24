// H2-EXT: see h2_ext_castle.h.
#include "h2_ext_castle.h"

#include "castle.h"
#include "h2_ext.h"
#include "race.h"

namespace H2Ext
{
    bool isAddedUpgrade( const int race, const uint32_t building )
    {
        if ( !isOn() ) {
            return false;
        }
        // the upgrade dwellings the original race lacks (castle.cpp _postLoad lists them)
        switch ( race ) {
        case Race::BARB:
            return building == DWELLING_UPGRADE3 || building == DWELLING_UPGRADE6;
        case Race::SORC:
            return building == DWELLING_UPGRADE5 || building == DWELLING_UPGRADE6;
        case Race::WRLK:
            return building == DWELLING_UPGRADE2 || building == DWELLING_UPGRADE3 || building == DWELLING_UPGRADE5;
        case Race::WZRD:
            return building == DWELLING_UPGRADE2 || building == DWELLING_UPGRADE4;
        case Race::NECR:
            return building == DWELLING_UPGRADE6;
        default:
            return false;
        }
    }

    bool isRemovedUpgrade( const int race, const uint32_t building )
    {
        return isOn() && race == Race::WRLK && building == DWELLING_UPGRADE7;
    }

    uint32_t baseDwelling( const uint32_t upgrade )
    {
        switch ( upgrade ) {
        case DWELLING_UPGRADE2:
            return DWELLING_MONSTER2;
        case DWELLING_UPGRADE3:
            return DWELLING_MONSTER3;
        case DWELLING_UPGRADE4:
            return DWELLING_MONSTER4;
        case DWELLING_UPGRADE5:
            return DWELLING_MONSTER5;
        case DWELLING_UPGRADE6:
        case DWELLING_UPGRADE7:
            return DWELLING_MONSTER6;
        default:
            return upgrade;
        }
    }

    uint32_t upgradeDwelling( const uint32_t dwelling )
    {
        switch ( dwelling ) {
        case DWELLING_MONSTER2:
            return DWELLING_UPGRADE2;
        case DWELLING_MONSTER3:
            return DWELLING_UPGRADE3;
        case DWELLING_MONSTER4:
            return DWELLING_UPGRADE4;
        case DWELLING_MONSTER5:
            return DWELLING_UPGRADE5;
        case DWELLING_MONSTER6:
            return DWELLING_UPGRADE6;
        default:
            return dwelling;
        }
    }

    Funds addedUpgradeCost( const int race, const uint32_t building )
    {
        // [제안] about the other races' upgrades of the same tier (buildinginfo.cpp). Funds( ore, wood, mercury, sulfur, crystal, gems, gold )
        switch ( race ) {
        case Race::BARB:
            return building == DWELLING_UPGRADE3 ? Funds( 0, 5, 0, 0, 0, 0, 1500 ) : Funds( 0, 0, 0, 0, 10, 0, 5000 );
        case Race::SORC:
            return building == DWELLING_UPGRADE5 ? Funds( 0, 0, 0, 0, 0, 5, 3000 ) : Funds( 0, 0, 10, 0, 0, 0, 5000 );
        case Race::WRLK:
            switch ( building ) {
            case DWELLING_UPGRADE2:
                return Funds( 5, 0, 0, 0, 0, 0, 1500 );
            case DWELLING_UPGRADE3:
                return Funds( 0, 0, 0, 0, 0, 5, 2000 );
            default:
                return Funds( 0, 0, 0, 5, 0, 0, 3000 );
            }
        case Race::WZRD:
            return building == DWELLING_UPGRADE2 ? Funds( 0, 5, 0, 0, 0, 0, 1500 ) : Funds( 5, 0, 0, 0, 0, 0, 2500 );
        case Race::NECR:
            return Funds( 0, 0, 10, 0, 0, 0, 5000 );
        default:
            return {};
        }
    }
}
