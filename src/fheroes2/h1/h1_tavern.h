// Heroes I tavern heroes (homm1-remaster project, note v169): the two heroes each player can recruit (player +0x22a / +0x22b) and the
// hero state bytes (game +0x1431d: 0xff free, '@' in a tavern, 0..3 the owning player). Port: '@' = the engine's RECRUIT mode, the two
// places = the kingdom's recruits.
#pragma once

class Heroes;
class Kingdom;

namespace H1::Tavern
{
    // FUN_00411ef0: a free hero of the class (0 Knight, 1 Barbarian, 2 Sorceress, 3 Warlock = hero numbers class x 9 .. + 8) at random;
    // else any free hero; else the last hero in a tavern. Returns the engine hero or nullptr.
    Heroes * pickHero( int heroClass );

    // New game (FUN_004147d7 per seat, before FUN_0041d657): place 0 = a class 0..3 at random (campaign: table 0x492a78 by colour),
    // place 1 = (that class + 1..3) & 3, both '@'. Called before the map's hero records are placed.
    void newGame();

    // FUN_0041d99f (end of FUN_0041d657): a tavern hero that a player now owns is replaced by FUN_00411ef0( 0 ), '@'.
    void replaceOwned();

    // FUN_0041a1b9: every week each player's two places get new heroes, the class going round (+ 1..3) & 3 from player to player and
    // place to place; the old hero is freed when it was '@'; the new ones are not marked.
    void newWeek();

    // After a hero was recruited from place `place` (0 / 1). Human (FUN_00445be4): (hired class + 1..3) & 3; computer (FUN_0042fb6e):
    // class 0..3 at random. The new hero is '@'.
    void afterHire( Kingdom & kingdom, int place, const Heroes & hired );

    // FUN_004486a2 after a retreat or surrender (DAT_004a7f58): the hero goes to place 0 or 1 (random) of its player's tavern, '@'; the
    // hero there before is freed when it was '@'.
    void heroRetreated( Kingdom & kingdom, Heroes & hero );
}
