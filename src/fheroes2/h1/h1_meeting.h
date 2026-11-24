// Heroes I hero meeting window (homm1-remaster project): swapwin.bin, FUN_0044f990 / FUN_0044ffa7 and helpers, note v122.
#pragma once

class Heroes;

namespace H1
{
    // The moving hero on the left (+0x3c), the met hero on the right (+0x38). Returns false when Heroes I graphics are not in use.
    bool runMeetingWindow( Heroes & left, Heroes & right );
}
