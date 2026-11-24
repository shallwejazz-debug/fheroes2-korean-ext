#pragma once

// H1-INTENDED(D16) extension mode: HoMM2 adventure objects put on the original maps at new game (owner 2026-11-18 08:34, plan "가":
// some original monsters become neutral creatures, new dwellings, a creature market and guarded sites on free land; note v240).
//
// Drawing: the HoMM1 tile bytes name an image set (low 4 bits of bytes 1 / 3) and a frame (bytes 2 / 4). Set 15 is unused by HoMM1
// (note v39); here it means "part of an extension object": the frame is an index into a fixed table of HoMM2 object parts (the
// fheroes2 object definitions, map_object_info.cpp), drawn with the HoMM2 picture already in the HoMM1 palette.

#include <cstdint>

namespace fheroes2
{
    class Sprite;
}

namespace H1
{
    enum : int
    {
        EXT_KIND_RUINS = 0x50, // medusas (28)
        EXT_KIND_EXCAVATION = 0x51, // skeletons (29)
        EXT_KIND_CITY_OF_DEAD = 0x52, // power liches (30)
        EXT_KIND_HALFLING_HOLE = 0x53, // halflings (31)
        EXT_KIND_MARKET = 0x54, // creature market: neutral creatures of the week
        EXT_KIND_PYRAMID = 0x55, // guarded site: undead guards, then gold and an item
        EXT_KIND_BARROW = 0x56 // guarded site (note v243, HoMM2 graveyard picture): zombies and mummies, then gold and an item
    };

    constexpr uint32_t extensionObjectSet = 15;

    // Picture of extension object part `index` (tile byte 2 / 4 with set 15), nullptr if none.
    const fheroes2::Sprite * extensionObjectPart( uint32_t index );

    // New extension game, after the map is read: neutral creatures for a quarter of the original monsters, then the new objects.
    void placeExtensionObjects();

    // New week: dwellings grow by the creature's growth (+5 in its creature week); the market gets new creatures.
    void extensionObjectsNewWeek( int weekCreature );

    // Name for the quick info window, or nullptr if the kind is not an extension object.
    const char * extensionObjectName( int kind );
}
