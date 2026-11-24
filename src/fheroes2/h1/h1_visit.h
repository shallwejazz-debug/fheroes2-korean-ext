// Heroes I map object visits (homm1-remaster project): FUN_00401000 and its helpers, notes v112-v113.
#pragma once

#include "mp2.h"
#include <cstdint>
#include <string>

class Heroes;

namespace H1
{
    // Event text table 0x493410 (76 entries, FUN_004035e2 numbers 0..0x4b).
    const char * eventText( int id );

    enum class VisitResult
    {
        NotHandled, // the engine's own action runs
        Handled,
        HandledRemove, // handled; the object fades out and is taken off the map (FUN_00401000 local_20) with a pickup sound
        HandledRemoveKillfade // the same with killfade (FUN_004045b0 with 0; also monsters joined or beaten: FUN_00405804 sets local_20)
    };

    // Hero +0xae bits for the bonuses "until the next battle" (note v198): 2 buoy, 8 oasis (morale +1), 0x100 statue (morale +2),
    // 0x20 graveyard, 0x40 shipwreck (morale -1), 0x10 faerie ring, 4 fountain (luck +1). Hero +0x35 / +0x36 are only ever changed
    // with these bits, and FUN_00448cb6 (after every battle, both heroes) takes them all back, so the sums follow from the bits.
    bool hasBonusBit( const Heroes & hero, int bit );
    void setBonusBit( const Heroes & hero, int bit );
    void clearBonusBits( const Heroes & hero ); // FUN_00448cb6
    int bonusMorale( const Heroes & hero ); // hero +0x35
    int bonusLuck( const Heroes & hero ); // hero +0x36
    std::string bonusSavedState();
    void loadBonusSavedState( const std::string & line );
    void resetBonusBits();

    // FUN_00401000 for the object kinds ported so far.
    VisitResult visitObject( Heroes & hero, int32_t tileIndex );

    // FUN_004046c3 (computer player): the same rules without windows, for the kinds without a battle (note v131).
    VisitResult visitObjectComputer( Heroes & hero, int32_t tileIndex );

    // Stone liths (0x29) and whirlpools (0x2c), FUN_00401000 / FUN_004046c3 (note v158): the other tiles of the same kind with
    // |dx| + |dy| above 1 (liths) or 3 (whirlpool), counted row by row, one of them at random (1..n); `from` when there is none.
    int32_t teleportTarget( int32_t from );

    // The object a HoMM1 tile stands for when the engine has no action object there (note v195): the converter could not place a
    // HoMM2 object on every HoMM1 active tile (whirlpools have 3 + 3 active tiles but one HoMM2 object; some oases), yet the
    // original visits by the tile bytes (FUN_004046c3). Kinds 0x2c whirlpool, 0x29 stone liths, 0x1c oasis; otherwise engineType.
    MP2::MapObjectType effectiveObjectType( int32_t tileIndex, MP2::MapObjectType engineType );

    // FUN_00404479 (whirlpool): only for a human player's hero, 1 in 3: the troop with the smallest worth x count is halved (an army of
    // one troop keeps at least 1). No message.
    void whirlpoolLoss( Heroes & hero );

    // FUN_00415a33 (new game): the contents byte (tile byte 9) of the ported object kinds, kept with the engine tile (saved with the game).
    void initObjectValues();

    // Tile byte 9 as the game has it now (the value kept with the engine tile), -1 when this port does not keep it for the tile.
    int currentByte9( int32_t tileIndex );

    // FUN_0041a1b9 (new week, from week 2): water wheels 2 (= 1000 gold), windmills a resource 1..5 again.
    void newWeekObjectValues();

    // FUN_0041aa04 (a creature month): every land tile without an object gets, 1 in 361, a wandering monster of the creature (tile byte 8
    // 0x9a, byte 1 0xc, byte 2 creature, byte 9 = FUN_004192e7's count, never joining). Note v168.
    void spawnMonthMonsters( int creature );

    // FUN_00459928: the map quick info text of a tile ("\n\n" + name), "Uncharted territory" under the fog.
    std::string tileQuickInfoText( int32_t tileIndex );
}
