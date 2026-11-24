/***************************************************************************
 *   H2-EXT: creature banks of the extension game (docs/H2_EXTRAS_CONCEPT_v1.md 3, owner 2026-11-19 16:26 "무작위 지도에만").
 *   Barrow, Hunters' Camp and Dragon Nest. They are placed only on random maps made from the new game window while the
 *   extension game box is ticked. On the map they work like the Graveyard (MP2::OBJ_GRAVEYARD: fight the guards, take
 *   gold and an artifact) with their own pictures (recoloured HoMM2 pictures), guards, rewards and texts.
 *   They are told apart from the Graveyard by their picture set (icn type OBJ_ICN_TYPE_UNUSED_1, unused by the original maps).
 *   In a game without the extension rules they act as a plain Graveyard.
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mp2.h"

class Army;

namespace fheroes2
{
    class Sprite;
}

namespace Maps
{
    class Tile;
    struct ObjectInfo;
}

namespace H2Ext::Banks
{
    enum Kind : int
    {
        NONE = -1,
        BARROW = 0,
        HUNTERS_CAMP = 1,
        DRAGON_NEST = 2,
        KIND_COUNT = 3
    };

    // The picture set of the banks on the map.
    constexpr MP2::ObjectIcnType bankIcnType = MP2::OBJ_ICN_TYPE_UNUSED_1;

    // The bank on this tile (the main tile of the object), or NONE.
    int kindOfTile( const Maps::Tile & tile );

    // Appends the three banks to the ADVENTURE_MISCELLANEOUS object group (map_object_info.cpp) and remembers their numbers.
    void addObjectInfos( std::vector<Maps::ObjectInfo> & objects );
    // The object number of a bank in ADVENTURE_MISCELLANEOUS (-1 before addObjectInfos).
    int32_t objectIndex( int kind );

    // The pictures (ICN::H2EXT_MAP_OBJECTS).
    void makeSprites( std::vector<fheroes2::Sprite> & sprites );

    // Guards of a bank (Army constructor for a tile). False when the tile is not a bank of an extension game.
    bool setGuards( Army & army, const Maps::Tile & tile );

    // Reward of a bank (setInitialObjectInfo). False when the tile is not a bank of an extension game.
    bool setReward( Maps::Tile & tile );

    // Names and messages, translated. Empty name when not a bank of an extension game.
    std::string name( const Maps::Tile & tile );

    struct Texts
    {
        std::string ask; // before the fight
        std::string empty; // already plundered
        std::string win; // after the fight
    };
    Texts texts( const Maps::Tile & tile );
}
