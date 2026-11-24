// Heroes I creature window (homm1-remaster project): armywin.bin, FUN_00417c59 / FUN_0041853a, and the morale / luck rules it shows
// (FUN_00467754, FUN_004678f9, FUN_0041ca0a). See docs/analysis note v60.
#pragma once

class Army;
class Castle;
class Heroes;

namespace H1
{
    // HoMM1 creature number (0..27, table 0x491e78) of a troop's monster, -1 if it has none.
    int creatureFromMonster( int monsterId );

    // FUN_004678f9( army, -1 ): 0 when the five slots change creature fewer than 2 times (one kind only), else by the number of
    // creature groups (creature / 6) present: 1 -> +1, 2 -> 0, 3 -> -1, 4 -> -2, 5 -> -3.
    int armyMorale( const Army & army );

    // FUN_00467754( army, hero, 0 ): army morale, +1 for a Knight hero, + visited-object bonus (hero +0x35), +1 per Medal 8..11,
    // -2 for the Fizbin 12; limited to -3..3. hero may be null.
    // castle: +1 when its tavern is built (castle +0x16 & 4, the third argument of FUN_00467754).
    int heroMorale( const Heroes * hero, const Army & army, const Castle * castle = nullptr );

    // FUN_0041ca0a( hero ): +1 per luck artifact 31..34, + visited-object bonus (hero +0x36); limited to -3..3. 0 with no hero.
    int heroLuck( const Heroes * hero );

    // FUN_00417c59 at (0x77, 0x14). popup: right-click view (no Exit button, closes when the mouse button is let go).
    // hideDismiss: param_6. Returns true when the player dismissed the creature; then every slot of army holding that creature has
    // already been emptied (as the original does after the window closes).
    // windowY: 0x14 (hero screen, castle) or 0x20 (recruit window, FUN_00466f8e).
    // A battle unit shown by FUN_00410e9f (note v192): window at (x, y), picture facing (unit +0x0a, param_7), shots left (unit +0x1e,
    // "Shots left: "), defense + 3 with spell 9 on the unit (+0x51).
    struct BattleUnitView
    {
        int x = 0x77;
        int y = 0x14;
        int facing = 0;
        int shotsLeft = 0;
        bool spell9 = false;
    };

    bool showCreatureWindow( int monsterId, int count, const Heroes * hero, Army * army, bool hideDismiss, bool popup,
                             const Castle * castle = nullptr, int windowY = 0x14, const BattleUnitView * battle = nullptr );
}
