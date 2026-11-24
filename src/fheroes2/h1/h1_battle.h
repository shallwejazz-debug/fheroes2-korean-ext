// Heroes I battle (homm1-remaster project).
// Step P2: the battlefield drawn the HoMM1 way (FUN_0046a5f0, FUN_0046b12f, FUN_00438de7, FUN_00438fbb, FUN_00406e0c), note v90.
// Step P3: the battle played by the HoMM1 rules - turn order, walking / flying paths, melee, retaliation, shooting, creature specials,
// morale, the computer's choice of action, the end of the battle and experience. See docs/analysis notes v88-v91.
#pragma once

#include <cstdint>

class Army;
class Heroes;

namespace Battle
{
    struct Result;
}

namespace H1
{
    // Plays a battle at tileIndex on the HoMM1 battlefield and writes the outcome into the armies (troop counts) and into result
    // (win / loss / retreat flags and experience). Returns false when the map has no HoMM1 data (the engine battle runs then).
    // Not yet ported (marked [temporary] in the notes): hero spells, the computer's retreat decision, surrender, sieges, animations.
    bool runBattle( Army & attacker, Army & defender, int32_t tileIndex, Battle::Result & result );

    // Creature worth (table 0x491e7a) and the strength of an army under a hero (FUN_0042e009).
    int creatureWorthOf( int creature );

    // FUN_0042e009( army, hero, corrections, castle, castle number ) as the machine code does it (note v186). Everything the function
    // reads is in this record; morale and luck are the results of FUN_00467754 / FUN_0041ca0a for the hero.
    struct StrengthInput
    {
        int creatures[5] = { -1, -1, -1, -1, -1 }; // army +0..4 (-1 empty)
        int counts[5] = { 0, 0, 0, 0, 0 }; // army +5 (2 bytes each)
        bool corrections = false; // third argument
        bool castle = false; // fourth argument
        int castleBits = 0; // castle +0x16 (2 bytes) of the castle in the fifth argument
        bool hero = false; // second argument not null
        int attack = 0; // hero +0x30
        int defense = 0; // hero +0x31
        int power = 0; // hero +0x32
        int spells[29]; // hero +0x66 (-1 none)
        int casts[29]; // hero +0x83
        int morale = 0;
        int luck = 0;
        StrengthInput()
        {
            for ( int k = 0; k < 29; ++k ) {
                spells[k] = -1;
                casts[k] = 0;
            }
        }
    };
    int armyStrengthOriginal( const StrengthInput & in );

    // FUN_0042e009 in full (note v144): corrections off = creature value x count only; castle = the castle part from castle +0x16 bits.
    int armyStrengthFull( const int creatures[5], const int counts[5], const Heroes * hero, int moraleValue, int luckValue, bool corrections, bool castle,
                          int castleBits );
    int armyStrengthOf( const Army & army, const Heroes * hero );

    // Creature names, tables 0x493068 (one) and 0x4930d8 (more than one); "" for other numbers.
    const char * creatureNameOne( int creature );
    const char * creatureNameMany( int creature );
}
