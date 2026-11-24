// Heroes I obelisks, puzzle pieces and the ultimate artifact (homm1-remaster project). See docs/analysis note v78.
#pragma once

#include <bitset>

namespace fheroes2
{
    class Image;
}

namespace H1
{
    // Number of obelisks of the current HoMM1 map (map file byte after the mine list + 37 = count of 0x9b tiles in all 37 original
    // maps but W95B1234, which has none and stores 1). 0 when the map has no HoMM1 tile data.
    int obeliskCount();

    // FUN_00411ac3: a first visit to an obelisk opens 48 / obelisks pieces (at least 1). For each piece: walk from piece 0 by random
    // steps of 1..5 while the piece is open; also draw a random piece 0..47 up to 100 times until a closed one; open the walked
    // piece if it is below 48, else the drawn one (which may already be open).
    void openObeliskPieces( std::bitset<48> & opened );

    // End of FUN_00415a33 (new game): the ultimate artifact's place and kind. Returns false when the map has no HoMM1 tile data.
    bool placeUltimateArtifact();

    // FUN_0045e89c / FUN_0045ec76: the 448 x 448 map picture of the puzzle around the ultimate artifact.
    fheroes2::Image puzzleMapPicture();
}
