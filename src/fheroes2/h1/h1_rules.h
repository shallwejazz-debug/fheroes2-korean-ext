// HoMM1 rules applied on top of fheroes2 data (homm1-remaster project).
// Every value here comes from the original HEROESW.EXE (Windows version, owner baseline 2026-11-14).
// Evidence: homm1-remaster/docs/analysis notes (v12 creature table, v15 damage, v16 spells, v20 specials, v42 port step 1).
#pragma once

#include <functional>

#include <cstdint>
#include <vector>

namespace fheroes2
{
    struct MonsterData;
}

class Heroes;

namespace H1
{
    // One row of the original creature table (HEROESW.EXE 0x491E78, 28 entries x 31 bytes).
    struct CreatureRow
    {
        int monsterId; // fheroes2 Monster id this HoMM1 creature maps to
        int gold; // +0x00
        int extraResource; // HoMM1 resource index (0 wood, 1 mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems), -1 none (FUN_00422c6a)
        int growth; // +0x07
        int hp; // +0x0c
        int speedLevel; // +0x0d : 1 Slow, 2 Medium, 3 Fast, 4 Blazing (cells on the HoMM1 battlefield)
        int attack; // +0x0f
        int defense; // +0x10
        int damageMin; // +0x11
        int damageMax; // +0x12
        int shots; // +0x14
        int flags; // +0x1b : 1 (two-cell, name unconfirmed), 2 (flying-like, name unconfirmed), 4 shooter, 8 hits the cell behind
    };

    const std::vector<CreatureRow> & getCreatureTable();

    // H1-INTENDED(D16) extension mode: HoMM2 neutral creatures after the 28 HoMM1 ones (owner 2026-11-17 21:45 / 23:18, note v233):
    // 28 medusa, 29 skeleton, 30 power lich, 31 halfling, 32 earth / 33 air / 34 fire / 35 water elemental. HoMM2 numbers, speed as the
    // HoMM1 level. Pictures from tools/extension/make_ext_creatures.py (h1/ext/). Only extension games have them.
    constexpr int firstExtensionCreature = 28;
    constexpr int extensionCreatureCount = 12;
    // 28, or 36 in an extension game.
    int creatureCount();
    // Row of any creature 0..35 (the HoMM1 table for 0..27); out of range -> row 0.
    const CreatureRow & creatureRow( int creature );
    // Battle picture file base ("peasant", ... "medusa"), names (one / more), the computer's worth of one creature and the experience
    // for one killed (HoMM1 tables 0x491e78 for 0..27; for 28..35 worth estimated from the HoMM1 numbers [adapted], experience = hp as
    // HoMM1 has it).
    const char * creatureFile( int creature );
    const char * creatureNameOne( int creature );
    const char * creatureNameMany( int creature );
    int creatureWorth( int creature );
    int creatureExperience( int creature );
    // Small picture (mons32.icn = creature; H1-INTENDED(D16) 28..35: h1/ext/mons32x.icn = creature - 28) and adventure map picture
    // (minimon.icn = creature x 7 + phase; 28..35: h1/ext/minimonx.icn = (creature - 28) x 7 + phase).
    const char * smallCreatureIcn( int creature );
    int smallCreatureFrame( int creature );
    const char * mapCreatureIcn( int creature );
    int mapCreatureFrame( int creature, int phase );

    // Called once at the end of fheroes2 populateMonsterData().
    void applyCreatureRules( std::vector<fheroes2::MonsterData> & monsterData );

    // Random castle / town race at game start, HoMM1 FUN_0041ace5 (note v46):
    // slots 0..3 of the castle list get a random race 0..3 not used by another of those slots; other slots any race 0..3.
    // Returns the fheroes2 race (Knight, Sorceress, Barbarian, Warlock).
    void resetRandomCastleRaces();
    // colourIndex: engine colour index of the castle owner (-1 = none); used only for the extension race choice below.
    int chooseRandomCastleRace( int castleSlot, int colourIndex = -1 );
    // H1-INTENDED(D16) extension mode: start race chosen by a person (h1Race 0 knight, 1 sorceress, 2 barbarian, 3 warlock; -1 = random as
    // the original). Set before the map loads, cleared after; a start slot castle of that colour takes the race; the other slots draw as the original and may get it too.
    void setForcedStartRace( int colourIndex, int h1Race );
    void clearForcedStartRaces();
    // Start castles of a new standard game (FUN_0041fb75 / FUN_004147d7, note v138). Set by the new game before the map is loaded, used once
    // by the map loading. seatColours: engine colour index of seat 0 (the human), 1, 2 ...; slotColour: for maps without castle setup owners,
    // the colour index that drew castle slot 0..3 (-1 = no seat: the castle becomes a neutral town).
    void setStartCastlePlan( const std::vector<int> & seatColours, const int ( &slotColour )[4] );
    void clearStartCastlePlan();
    bool hasStartCastlePlan();
    int startCastlePlanColour( int castleSlot ); // engine colour index, -1 = none
    // Maps with castle setup owners: owner n (seat n; n >= seat count = the last seat) -> engine colour index of that seat.
    int startPlanSeatColour( int owner );
    // HoMM1 race number (0 knight, 1 sorceress, 2 barbarian, 3 warlock) <-> fheroes2 race; -1 if not a HoMM1 race.
    int engineRaceFromH1( int h1Race );
    int h1RaceFromEngine( int engineRace );

    // HoMM1 artifact names in HoMM1 numbering (0..37); matching fheroes2 artifacts by name.
    const char * artifactName( int h1Artifact );
    // H1-INTENDED(D16) extension items 38..63 (docs/EXT_ITEMS_v1.md): hero screen text, find text, and whether a hero carries an item.
    constexpr int firstExtensionArtifact = 38;
    constexpr int extensionArtifactCount = 26;
    const char * extensionArtifactDescription( int h1Artifact );
    const char * extensionArtifactFindText( int h1Artifact );
    bool heroHasArtifact( const Heroes * hero, int h1Artifact );
    int engineArtifactFromH1( int h1Artifact ); // fheroes2 artifact id, or -1
    int h1ArtifactFromEngine( int engineArtifact ); // HoMM1 number, or -1

    // Random artifact at game start, HoMM1 FUN_0041bd71: a number 4..36 not yet given (table +0x14461);
    // random tries (up to 10,000), else the first free one; -1 if none (note v49). Reset with resetRandomCastleRaces().
    int chooseRandomArtifact();

    // Random resource at game start, HoMM1 FUN_0041c1cc: frame 0x3d..0x43 at random = resource 0..6 (note v49).
    // Returns the fheroes2 resource flag.
    int chooseRandomResource();

    // Random monster at game start, HoMM1 FUN_0041c1cc (note v181): map byte 0xbf (any), 0xc3 (weak), 0xc4 (medium), 0xc5 (strong),
    // 0xc6 (very strong). Creature 0..0x1b at random until its worth (creature table +0x02) is above the low and below the high limit
    // (0xbf 80..2000, 0xc3 0..400, 0xc4 80..1000, 0xc5 500..2500, 0xc6 2000..100000). Returns the HoMM1 creature, -1 for other bytes.
    int chooseRandomMonster( int tileByte );

    // Combat damage, HoMM1 FUN_0040a9ba (note v15, port note v43).
    // Multiplier table at 0x492288: 41 floats for attack - defense = -20 .. +20.
    float attackDefenseMultiplier( int difference );

    // One blow's damage, FUN_0040a9ba as the machine code does it (note v184): the base is summed as a 4-byte float, each step is
    // stored back as a 4-byte float, the end is (float + 0.5) cut to a whole number (__ftol). Steps: per creature min (damage state 1),
    // max (state 3) or random min..max; x table[attack - (defense + extra defense)]; luck +: x2, -: /2; a shooter in melee /2; damage
    // state 2 /2; genie (creature 0x1b): 1 in 5 (random 1..5 = 2) at least half the target's creatures (count + 1) / 2 x hit points
    // (halfTroops); 1 .. 32,000. extraDefense: 4 when shooting over the castle wall (v19), else 0. rnd( low, high ): FUN_0041c90b.
    struct StrikeInput
    {
        int count = 0; // +0x0f
        int damageState = 0; // +0x13
        int damageMin = 0; // +0x1b
        int damageMax = 0; // +0x1c
        int attack = 0; // +0x19
        int luck = 0; // +0x15
        int creature = 0; // +0x06
        bool shooterInMelee = false; // +0x25 & 4 and not shooting
        int targetDefense = 0; // target +0x1a
        int extraDefense = 0;
        int targetCount = 0; // target +0x0f
        int targetHp = 0; // target +0x16
    };
    int strikeDamage( const StrikeInput & in, const std::function<int( int, int )> & rnd, bool & halfTroops );

    // Steps 2-6 and 8 of the same function on an already summed base damage, for fheroes2's own battle code (battle_troop.cpp);
    // the same 4-byte float steps as strikeDamage. luck: +1 x2, -1 /2, 0 none.
    uint32_t finishDamage( double baseDamage, int attack, int defense, int extraDefense, int luck, bool shooterInMelee );

    // Heroes I hero names, table 0x493a08 of HEROESW.EXE (pairs: full name, short name; the full name is returned). index 0..35:
    // 0..8 knights, 9..17 barbarians, 18..26 sorceresses, 27..35 warlocks (note v57).
    const char * heroName( int index );
}
