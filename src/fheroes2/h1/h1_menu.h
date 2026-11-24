// Heroes I main menu (homm1-remaster project, note v51).
#pragma once

namespace fheroes2
{
    enum class GameMode : int;
}

namespace H1
{
    // HoMM1 main menu: heroes.bmp + the stpmain.bin dialog at (400, 35) (decompiled lines ~13855-13890).
    fheroes2::GameMode runMainMenu();
}
