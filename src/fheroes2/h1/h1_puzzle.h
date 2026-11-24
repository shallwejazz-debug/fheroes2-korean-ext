// Heroes I puzzle map (homm1-remaster project): FUN_0045e89c, puzzle.icn, viewpuzl.bin. See docs/analysis note v76.
#pragma once

#include <bitset>

namespace fheroes2
{
    class Image;
}

namespace H1
{
    // map: the 448 x 448 picture of the land around the ultimate artifact; opened[i]: piece i is taken away.
    void showPuzzle( const fheroes2::Image & map, const std::bitset<48> & opened );
}
