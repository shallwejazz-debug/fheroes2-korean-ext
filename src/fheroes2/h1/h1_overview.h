// Heroes I kingdom overview (homm1-remaster project): FUN_004186f2, overwind.bin, gold per day FUN_00419ae2. See docs/analysis note v74.
#pragma once

class Kingdom;
struct Funds;

namespace H1
{
    void runKingdomOverview( const Kingdom & kingdom );

    // FUN_00419ae2 (human player part): Dragon City 1000, gold mine 1000 each, castle 1000 / town 250 each, and per copy of the
    // gold artifacts 26 / 27 / 28 on the player's heroes 1000 / 750 / 500.
    int goldPerDay( const Kingdom & kingdom );

    // Computer player level (player +0x21e: 1 Dumb, 2 Average, 3 Smart, 4 Genius) from the new game screen (note v139).
    int computerLevel( const Kingdom & kingdom );

    // FUN_00419d62 + FUN_00419ae2: what a player gets at the start of a day (note v79). Mines: wood and ore 2, mercury, sulfur, crystal
    // and gems 1 per mine (mine-list entries 2..35; entry 0 = Dragon City pays gold only); gold = goldPerDay, for a computer player x0.75
    // (level 1), x1.29 (3), x1.45 (4), cut to a whole number. Computer players above Average also get 1 wood + 1 ore, Genius one more
    // resource by the day of the week that just ended (1 wood .. 6 gems).
    Funds dailyIncome( const Kingdom & kingdom );
}
