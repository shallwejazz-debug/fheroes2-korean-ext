/***************************************************************************
 *   H2-CHEATS: the original Heroes II cheat codes on the adventure map (owner 2026-11-20 08:49 "원본 치트는 그대로 넣어 줘").
 *   [확정: HEROES2W.EXE 0x4914e6~0x491726] The game keeps the typed number keys as code = (code * 10 + digit) mod 10,000,000 and checks
 *     code mod 100,000    = 32167   -> 5 Black Dragons (monster 0x25) join the selected hero
 *     code mod 1,000      = 911     -> the scenario is won
 *     code mod 10,000     = 1313    -> the scenario is lost
 *     code mod 10,000,000 = 8675309 -> the whole map is revealed for all six player colours (fog function 0x43e300 at 30,30 radius 180, players 0..5)
 *   The first three are skipped while [0x4f645c] is not 0 [미확인: what that flag is]. The original also sets a "cheated" flag
 *   (player data +0x6094); the engine has no such flag, so it is not kept.
 *   Active only in the Heroes II mode of the Korean patch; the number keys still reach the engine hotkeys when no code is completed.
 ***************************************************************************/

#pragma once

#include <cstdint>

#include "localevent.h"

namespace H2Cheats
{
    // A number key on the adventure map of a human player. Returns true when a code was completed (the key is then taken).
    bool key( const fheroes2::Key key );

    // 911 / 1313: the result to give at the next game-over check, then cleared. 0 when none.
    uint32_t takeForcedResult();
}
