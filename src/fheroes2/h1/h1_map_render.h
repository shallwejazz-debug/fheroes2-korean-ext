// HoMM1 map objects drawn with the original HoMM1 images (homm1-remaster project, note v45).
#pragma once

#include <array>
#include <functional>
#include <cstdint>
#include <vector>

namespace fheroes2
{
    class Image;
    class Sprite;
}

namespace Interface
{
    class GameArea;
}

namespace H1
{
    // Drawing stages of the HoMM1 map drawing function (note v45 / v39 section 1):
    // FLAT = lower layer drawn flat with the ground (tile byte 6 bit 0x80), LOWER = other lower-layer objects + lower flag,
    // MONSTERS = wandering monsters (minimon.icn), UPPER = upper layer + upper flag.
    enum class MapStage
    {
        FLAT,
        LOWER,
        MONSTERS,
        UPPER
    };

    // True if the current map is a converted HoMM1 map with original tile data (then fheroes2's own object images are not drawn).
    bool isHoMM1Map();

    // Radar (small map) colour of a visible tile, FUN_004593aa (note v59). Returns -1 if the current map has no HoMM1 tile data.
    int radarColour( int32_t tileIndex );

    // The 72 x 72 tiles' bytes as HoMM1 holds them during play (note v77): after new game the random castles' frames are moved
    // to the chosen race (FUN_0041ace5), random artifacts become 0xb0 with the artifact as frame and random resources 0x9d with
    // frame 0x3d + resource (FUN_0041c1cc); objects already taken (no game object left on a placed tile) get byte 8 = 0 and
    // byte 2 = 0xff [temporary: what the original writes there is not checked]. Empty if the map has no HoMM1 tile data.
    std::vector<std::array<uint8_t, 10>> gameTileBytes();

    // FUN_00456943 digs a hole by writing tile bytes 1 = 7, 2 = 1 (obj32-07 frame 1) and byte 6 |= 0x80 (flat) when byte 2 was empty.
    // The engine keeps the hole as a ground object part; this reads it back (note v154). t = the tile's bytes, changed in place.
    void applyDugHole( int32_t tileIndex, uint8_t * t );

    // DOS FUN_00062b6e -> FUN_0008a184( area 0xb4, 0xac, 0x78 x 0x78, block 1, speed 0x40 ) (note v174): the 120 x 120 centre of the
    // adventure view changes from what is shown now to what draw() puts on the screen, block by block in the order of a shift register
    // (taps table 0xe1d28 for 6 + 6 bits), each block copied to its four mirrored places; a pause of one 64 Hz timer tick every 255 steps.
    // The register never gives 0, so the four corner blocks keep the old picture until the next redraw (as in the original). The Windows
    // version cross-fades with ccycle*.bin tables of its own palette instead; the owner chose the DOS way (2026-11-16).
    // Block 2 is the original's choice for slow machines (CPU below 486 class), not used here.
    void dissolveCentre( const std::function<void()> & draw );

    // The same for any square area of the 640 x 480 screen (boarding / leaving a boat: 0xc0, 0xc0, 0x60, block 1, speed 0x40 in DOS
    // FUN_000549d2 / FUN_0005f410; the Windows version cross-fades the same area, FUN_0043ab9c / FUN_00401000 case 0x1f).
    void dissolveArea( int32_t x, int32_t y, int32_t size, const std::function<void()> & draw );

    // The screen flag DAT_004aa01c for a hero's step (FUN_0043aa66, note v177): on a human player's turn always on; on a computer player's
    // turn on only with 'Show Enemy Moves' on (DAT_004a9b84 = 0; the port: AI move speed above 0) and the step's start or end tile seen
    // by the human player (fog bits of DAT_004af2f8; the port: any human colour). Walking sounds and the dissolve of FUN_004045b0 /
    // FUN_0008a184 need it.
    bool screenShowsStep( int32_t fromIndex, int32_t toIndex );

    // An active object of the map bytes that the game has taken off (picked up, beaten).
    bool objectGoneOnTile( int32_t tileIndex );

    // The kind (byte 8 & 0x7f) of the active object on this tile now (random artifact / resource / monster placeholders as the real kind),
    // -1 when the tile has no active object or it was taken (note v156).
    int activeObjectKind( int32_t tileIndex );

    // Frame of a map image set (tile byte 1 / 3 low 4 bits: 0..7 obj32-0N, 8 mtn32, 9 tree32, 10 town32, 11 rsrc32, 12 mons32,
    // 13 art32, 14 flag32), nullptr if absent.
    const fheroes2::Sprite * setFrame( uint32_t set, uint32_t frame );

    void drawMapStage( MapStage stage, fheroes2::Image & dst, const Interface::GameArea & area, int32_t minX, int32_t minY, int32_t maxX, int32_t maxY,
                       bool renderFog );
}
