#pragma once

#include "localevent.h"

namespace H1
{
    // H1-INTENDED(D17): cheat codes typed on the adventure map (owner 2026-11-18 16:42, for a blog review). Not in the original HoMM1.
    // Returns true when the key was a digit and has been taken by the cheat input.
    bool cheatKey( fheroes2::Key key );
}
