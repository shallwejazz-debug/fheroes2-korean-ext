// Heroes I hero rules (homm1-remaster project): new game set-up FUN_0041bdea, level-up FUN_00448e23 with its own random numbers
// (FUN_0041c9e6 / FUN_0041c90b / FUN_0041c95d / FUN_0041c87a). See docs/analysis note v85.
#pragma once

#include <cstdint>
#include <string>

class Heroes;

namespace H1
{
    // The game's own random numbers (state DAT_00490cec): FUN_0041c9e6 seed, FUN_0041c90b( a, b ) -> a .. b (note v85).
    struct GameRandom
    {
        uint32_t state = 0;

        void seed( const uint32_t value )
        {
            state = value;
        }

        int get( int a, int b );
    };

    // Class number of the hero record (+0x1c): 0 Knight, 1 Barbarian, 2 Sorceress, 3 Warlock (from the engine race), -1 otherwise.
    // H1-INTENDED(D16): 4 Necromancer (extension games, docs/EXTENSION_NECRO_v1.md).
    int heroClass( const Heroes & hero );

    // H1-INTENDED(D16): hero numbers. 0..35 = the HoMM1 heroes (engine LORDKILBURN..WRATHMONT, classes of 9), 36..44 = the 5th race's
    // heroes (engine ZOM..CELIA, the HoMM2 necromancers). Tile byte 9 of a hero tile and the per-hero tables use these numbers.
    constexpr int heroCount = 45;
    constexpr int firstNecromancerHero = 36;
    int heroNumber( const Heroes & hero ); // -1 for other engine heroes
    int heroNumberOfId( int engineHeroId );
    Heroes * heroByNumber( int number );
    // Class for pictures that exist for the 4 HoMM1 classes only (battle tents, crests): the necromancer uses the warlock's for now.
    inline int pictureClass( const int cls )
    {
        return cls == 4 ? 3 : cls;
    }

    // FUN_00448e23 for one new level: 0 attack, 1 defense, 2 spell power, 3 knowledge. seed = hero +0x3d.
    int levelUpStat( int heroClass, uint16_t seed, int newLevel );
    // H1-INTENDED(D16) extension mode: the second skill offered at a level-up (the first is levelUpStat). Another roll of the class
    // table; when it gives the same skill, the next most likely one of the class row.
    int levelUpSecondChoice( int heroClass, uint16_t seed, int newLevel );

    // New game for every hero (FUN_0041bdea): experience 40..90, level seed 1..16000, empty spellbook; HoMM1 heroes have no
    // secondary skills, so the engine ones are removed.
    void setUpHeroesForNewGame();

    // FUN_0041d657 (note v157): the map's hero setup records put their heroes on the map for their seats, after setUpHeroesForNewGame.
    void placeRecordHeroes();

    // FUN_0041beec( hero, recruited ): the hero's army is made new from the class table (creature, min, max): Knight peasants 30..50 /
    // archers 3..5, Barbarian goblins 15..25 / orcs 3..5, Sorceress sprites 10..20 / dwarves 2..4, Warlock centaurs 6..10 /
    // gargoyles 2..4. The first troop always; the second when a 0..99 roll is below 50 (recruited: 80) or a second roll is not below 25
    // (recruited: 65). Count = random(min x 10 .. max x 10 + 9) / 10; recruited heroes roll from the middle of that range.
    // Called for all heroes at new game and when a hero goes back to the pool (param 0), and when a hero is recruited (param 1).
    void rollHeroArmy( Heroes & hero, bool recruited );

    // Movement (note v139). Heroes I points are kept in the engine x 25 (grass step 4 -> 100, so every Heroes I value stays a whole
    // number and the engine's 100-based ground penalties and diagonal x 1.5 fit).
    constexpr uint32_t movementScale = 25;

    // FUN_0044732d: land = 40 / 50 / 60 by the slowest creature speed (1 Slow / 2 Medium / 3 Fast or more; no army = 60), + 24 Nomad Boots
    // (29), + 12 Traveler's Boots (30). Sea = 60, 80 for the owner of the lighthouse, + 40 Sailor's Astrolabe (36), then x class factor
    // (table 0x491E50: 1, 1, 2 Sorceress, 1, cut to a whole number). Both + 20 True Compass (35). A computer player above Average + 3.
    // Returned in engine points (x movementScale).
    uint32_t maxMovePoints( const Heroes & hero, bool onWater );

    // FUN_0044e7d0 straight step from a tile of the given engine ground (FUN_0044e770: water, grass, lava, dirt 4; snow, swamp 6;
    // desert 8), engine points. A Barbarian (class 1) always pays the grass value.
    uint32_t straightStepCost( int ground, bool barbarian );

    // Text of the level-up box: "%s has gained" + " a level.\n" / " %d levels.\n" + "\n%s +%d" per raised skill.
    std::string levelUpText( const std::string & name, int levels, const int raised[4] );
}
