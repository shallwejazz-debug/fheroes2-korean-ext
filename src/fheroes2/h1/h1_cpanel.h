// Heroes I game options window (homm1-remaster project): DOS cpanel.bin (owner decision 2026-11-15: DOS window as it is).
// Windows code FUN_00460236 / FUN_00460557 / FUN_004609b0 with the DOS items (Auto Save in place of Sound Quality). See note v83.
#pragma once

namespace H1
{
    enum class GameOptionsResult
    {
        NONE,
        NEW_GAME,
        LOAD_GAME,
        SAVE_GAME,
        QUIT,
        SCENARIO_INFO
    };

    // Opens the window at (0xa0, 10). New game / load / quit are already confirmed ("Are you sure ..." Yes) when returned.
    GameOptionsResult runGameOptions();

    // Auto Save (DOS item 11): save to "AUTOSAVE" at the end of each turn. On at start [temporary: the DOS start value is not checked].
    bool isAutoSaveOn();

    // Combat speed (added item, not in the original window; owner decision 2026-11-15): 1, 2 or 4 = animations 1x / 2x / 4x faster.
    // Stored as the engine battle speed 1 / 2 / 3 (other values = 1x).
    int combatSpeedMultiplier();

    // Show Path (item 12): off = one click on the map starts the hero moving at once.
    bool isShowPathOn();
}
