// Heroes I week and month rules (homm1-remaster project, note v168):
// FUN_0041a1b9 (new week), FUN_0041aa04 (new month), FUN_00413b0e (neutral castle troops) and the week message of FUN_0041ea28.
#pragma once

#include <string>

namespace H1::Week
{
    // New game: no week or month has been proclaimed yet.
    void reset();

    // The four values the original saves (DAT_004a9e90, DAT_004a7ba8, DAT_004a7bb0, DAT_004a7bf0) as one side-file line "week a b c d".
    std::string savedState();
    void loadSavedState( const std::string & line );

    // Called once at the start of every week from week 2 (engine World::NewWeek). newMonth: this week also starts a month (the original
    // week-of-month counter was 4 before the call, so no creature week). Runs FUN_0041a1b9's castle part and FUN_00413b0e, then
    // FUN_0041aa04 when newMonth.
    void newWeek( bool newMonth );

    // FUN_0041ea28: the "Astrologers proclaim ..." box on a human player's turn on the first day of a week (not the game's first day),
    // with music 0x33 (month) or 0x32 (week).
    void showWeekMessage();
}
