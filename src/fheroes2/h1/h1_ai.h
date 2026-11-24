// Heroes I computer player on the adventure map (homm1-remaster project), rebuilt from the original functions (notes v142, v143 ...).
// Coordinates are the original ones (x, y on the 72 x 72 map); the path costs come from the original path search (h1_pathfind).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "h1_pathfind.h"

class Heroes;
class Kingdom;
class Army;
class Castle;

namespace H1::Ai
{
    // Globals of the original that the destination functions share (names = original addresses).
    struct State
    {
        Path::World world; // the game as the original reads it, for the player on turn
        Path::Search grid; // the shared search grid DAT_004a7bcc as FUN_0042aaf9 left it (FUN_0046e730 builds the path on it)
        int player = 0; // engine colour index of the player on turn (DAT_004ac968)
        int level = 2; // player +0x12
        int dayNumber = 1; // 0x4c0b9c (v33 1)
        std::array<int, 6> digConf{}; // player +0x52 per engine colour
        std::array<int, 6> digPosX{ -1, -1, -1, -1, -1, -1 }; // player +0x53
        std::array<int, 6> digPosY{ -1, -1, -1, -1, -1, -1 }; // player +0x54
        int digConfidence = 0; // player +0x52 of the player on turn
        bool teleportGuard = true; // 0x49f7a0
        // DAT_004c0b94: the tile the last object value (not taken from the cache) was worked out for. It is one global: the nested object
        // values of a teleport's surroundings move it, and the teleport loop and the end of FUN_004307f8 read it afterwards (note v199).
        int currentX = 0;
        int currentY = 0;
        bool danger2Allowed = true; // 0x4c2050, also one global (set at the start of each value worked out)
        int digX = -1; // player +0x53 / +0x54: the tile the computer means to dig (v34 2-14)
        int digY = -1;

        // FUN_0042817b caches: 0x4aa0e8 surroundings value, 0x4aca78 safety, 0x4af300 object value (mode 0), 0x4c0bb0 hero win chance.
        std::vector<int16_t> surround = std::vector<int16_t>( 72 * 72, -32001 );
        std::vector<int16_t> safety = std::vector<int16_t>( 72 * 72, -32001 );
        std::vector<int16_t> objectCache = std::vector<int16_t>( 72 * 72, -32001 );
        std::array<int16_t, 45> heroWin{}; // H1-INTENDED(D16): 45 hero numbers (necromancers 36..44)

        // Flags FUN_004307f8 sets while FUN_0042aaf9 scores tiles (v36 4).
        int capturedCastle = -1; // DAT_004aa0d8: castle slot taken by the hero now moving (-1 none)
        bool heroHired = false; // 0x4c0ba0: a hero was hired this turn (one per turn)
        std::array<int, 36> turnsOwned = initTurns(); // castle +0x35 by castle slot: days since the castle changed owner (10 at new game)
        std::array<int, 36> lastOwner = initOwners(); // castle owner seen last (to notice a change of owner)
        int lastDay = 0;

        // Player +0xd5 .. +0xed (7 ints, wood, mercury, ore, sulfur, crystal, gems, gold) per engine colour: what the last day change
        // (FUN_00419d62) gave the player = resources after it minus before; 0 until the first day change (note v199).
        std::array<std::array<int, 7>, 6> dayGain{};

        static std::array<int, 36> initTurns()
        {
            std::array<int, 36> a{};
            a.fill( 10 );
            return a;
        }

        static std::array<int, 36> initOwners()
        {
            std::array<int, 36> a{};
            a.fill( -2 );
            return a;
        }

        int boatCastle = -1; // DAT_004aa0e0
        int8_t boatCastleDistance = 0; // DAT_004aa030
        bool shipyardCandidate = false; // DAT_004b1b80
        bool hasShipyard = false; // DAT_004c204c
        bool boatValued = false; // DAT_004c205c
        std::array<int, 6> shipyardGoal{ -1, -1, -1, -1, -1, -1 }; // 0x4c2040 per player
        std::array<int, 6> boatGoal{ -1, -1, -1, -1, -1, -1 }; // 0x4aca68 per player
        std::array<int, 6> goalDay{}; // 0x4b1b84 per player

        // Hero record +0x20 / +0x21: the destination each hero chose last (kept between turns). Start value [unconfirmed]: -1.
        std::array<int8_t, 45> destX = initDest(); // H1-INTENDED(D16): 45 (saved games keep 0..35 only)
        std::array<int8_t, 45> destY = initDest();

        int heroDestX( const int hero ) const
        {
            return ( hero >= 0 && hero < 45 ) ? destX[hero] : -1;
        }

        int heroDestY( const int hero ) const
        {
            return ( hero >= 0 && hero < 45 ) ? destY[hero] : -1;
        }

        static std::array<int8_t, 45> initDest()
        {
            std::array<int8_t, 45> a{};
            a.fill( -1 );
            return a;
        }

        // FUN_00429db9 turn values (note v33 2..4).
        std::array<double, 7> resourceValue{ 250, 250, 200, 250, 250, 250, 1 }; // 0x4b3010 (floats)
        float ad = 0.4f; // player +0xad
        float b1 = 0.3f; // +0xb1
        float b5 = 0.3f; // +0xb5
        float f9 = 0.3f; // +0xf9
        float averageArtifact = 0; // +0xfd
        float share = 0.25f; // +0x101
        int ultimate = 0; // +0xf1
        float x = 1.0f; // 0x49f4d8 (human opponents)
        float y = 1.0f; // 0x49f4dc (computer opponents)
        int maxHeroes = 3; // 0x4c2048
        std::array<float, 45> heroB2{}; // hero +0xb2 (H1-INTENDED(D16): 45)
        std::array<float, 45> heroFactor{}; // 0x4aa038 (H1-INTENDED(D16): 45)
        std::vector<int8_t> enemyNear = std::vector<int8_t>( 72 * 72, 7 ); // 0x4b1b90, index x * 72 + y

        // FUN_00428c79 / FUN_0042905e (note v142 2, 3).
        float danger1 = 1.0f; // 0x4aa0cc
        bool danger1Set = false; // 0x4c0b98
        float danger2 = 1.0f; // 0x4aca74
        bool danger2Set = false; // 0x4c203c
    };

    State & state();

    // A new game starts from the map's records: every value above back to its start (the original's hero, castle and player records
    // come from the new map). The saved part (note v153): the fields of the original's saved records - player +0x52..+0x54 (dig guess),
    // hero +0x20 / +0x21 (destination), castle +0x35 (days since the owner changed, with the helpers that count it).
    void resetState();
    std::string savedState();
    void loadSavedState( const std::string & line );

    // Player +0xd5 (note v199): kept at each day change (the income the engine adds then), read by the computer's values.
    void recordDayGain( int colourIndex, const int gain[7] );
    const std::array<int, 7> & dayGain( int colourIndex );

    // FUN_0042817b( all, x, y ): all = clear every cache; else the surroundings within |dx| + |dy| < 10 of (x, y), the object value of
    // (x, y) and the win chance of heroes near it (the original compares both distances with the hero's x).
    void clearCaches( bool all, int x, int y );

    // Original hero data the functions read (+0x1e/+0x1f position, +0x20/+0x21 destination, +0x25/+0x27 movement, +0x1c class,
    // +0xae bit 0x80 on a boat, +0x00 hero number, +0x01 owner).
    struct HeroView
    {
        const Heroes * hero = nullptr;
        int number = -1;
        int owner = -1; // engine colour index
        int x = 0;
        int y = 0;
        int destX = -1;
        int destY = -1;
        int maxMove = 0; // Heroes I points
        int moveLeft = 0;
        int heroClass = 0;
        bool onBoat = false;
        int direction = 2;
    };

    HeroView viewOf( const Heroes & hero );

    // FUN_004307f8( hero, x, y, mode, &K ): value of the object / hero / castle at (x, y) for this hero (note v34). Mode 0 is cached.
    int objectValue( const HeroView & hero, int x, int y, int mode, int & k );

    // FUN_0042d407( hero, x, y, wide, &safety ) (note v32 2, v143).
    int surroundings( const HeroView & hero, int x, int y, bool wide, int & safetyOut );

    // FUN_0042cecb (note v32 1, v143). grid = the hero's path search.
    int destinationScore( const HeroView & hero, const Path::Search & grid, int x, int y, bool guarded, int guardX, int guardY, bool turnBreak, int breakX,
                          int breakY, int mode );

    // FUN_00429db9 for the player of this engine colour (note v33 4).
    void prepareTurn( int colourIndex );

    // FUN_0042e009 for an engine army (corrections, castle part of the castle at the entrance x, y or none).
    int strength( const ::Army & army, const Heroes * hero, bool corrections, bool castle, int castleX, int castleY );

    struct Forecast
    {
        float p = 1.0f;
        int l1 = 0;
        int l2 = 0;
        int l3 = 0;
        int l4 = 0;
        int g = 0;
    };

    // FUN_0042ba83 (building score) as the machine code does it (note v189): everything it reads.
    struct BuildingInput
    {
        int number = 0; // building 0..12
        int race = 0; // castle +3 (0 knight, 1 sorceress, 2 barbarian, 3 warlock)
        int bits = 0; // castle +0x16
        int guildByte = 0; // castle +0x18
        int garrison[5] = { -1, -1, -1, -1, -1 }; // castle +6
        int garrisonCounts[5] = { 0, 0, 0, 0, 0 }; // castle +0xb
        int dwellingCounts[6] = { 0, 0, 0, 0, 0, 0 }; // castle +0x1a
        float ad = 0; // player +0xad
        float b1 = 0; // player +0xb1
        int have[7] = {}; // player +0x7d
        int income[7] = {}; // player +0xd5
        int cost[7] = {}; // FUN_00422bb1
        // The player's castles for FUN_004115d3: race, buildings, guild byte.
        int castles = 0;
        int castleRace[36] = {};
        int castleBits[36] = {};
        int castleGuild[36] = {};
    };
    float buildingScoreOriginal( const BuildingInput & in );

    // FUN_0042c164 (creature purchase score) as the machine code does it (note v191): everything it reads.
    struct CreatureInput
    {
        int creature = 0; // second argument
        int count = 0; // fourth argument
        float f9 = 0; // player DAT_004aca70 + 0xf9
        float b1 = 0; // castle owner's player + 0xb1 (game + owner x 0x105 + 0x2bd)
        bool hero = false; // castle +0x15 != -1
        int heroClass = 0; // hero +0x1c
        int heroArmy[5] = { -1, -1, -1, -1, -1 }; // hero +0x57
        int garrison[5] = { -1, -1, -1, -1, -1 }; // castle +6
        int have[7] = {}; // player +0x7d
        int income[7] = {}; // player +0xd5
    };
    struct CreatureScore
    {
        int value = 0; // third argument
        float score = 0; // fifth argument
    };
    CreatureScore creatureScoreOriginal( const CreatureInput & in );

    // FUN_0042cd5b: a resource bundle's worth, as the machine code sums it (amounts 1, 5, 3, 2, 6, 4, 0 times the 4-byte float values
    // 0x4b3010, then cut to a whole number), note v189.
    int bundleValueOriginal( const int amounts[7] );

    // FUN_0042b42b as the machine code does it (note v188): everything it reads, with the strengths FUN_0042e009 returned.
    struct ForecastInput
    {
        int a = 0; // FUN_0042e009( my army, my hero, 1, 0, 0 )
        int b = 0; // FUN_0042e009( enemy army, enemy hero, 1, castle, castle number )
        bool extra = false; // fifth argument (a second enemy army)
        int bExtra = 0; // FUN_0042e009( extra, 0, 1, 0, 0 )
        int va = 0; // the same three with the third argument 0
        int vb = 0;
        int vbExtra = 0;
        bool castle = false;
        int owner = -1; // eighth argument: the enemy's player, -1 none
        bool ownerHuman = false; // DAT_004a9b60[owner]
        int level = 0; // DAT_004aca70 + 0x12
        float b1 = 0; // + 0xb1
        float f9 = 0; // + 0xf9
        float x = 2.0f; // 0x49f4d8
        float y = 0.8f; // 0x49f4dc
        bool myHero = false;
        int myArtifacts = 0; // sum of table 0x491960 over my hero's artifacts (+0xa0, 14, below 0x25)
        float b2 = 0; // my hero + 0xb2
        int experience = 0; // FUN_0041c7ee( enemy army, enemy hero )
        bool enemyHero = false;
        int enemyArtifacts = 0;
        bool enemyHeroHuman = false; // DAT_004a9b60[enemy hero + 1]
    };
    Forecast forecastOriginal( const ForecastInput & in );

    // FUN_0042b42b (note v33 5): enemyOwner = engine colour index of the enemy, -1 for monsters without an owner.
    Forecast forecast( const HeroView & me, const ::Army & enemy, const Heroes * enemyHero, const ::Army * extra, bool castle, int castleX, int castleY,
                       int enemyOwner );

    // FUN_00428c79 and FUN_0042905e for the hero about to move (note v142 2, 3).
    void dangerFactors( const HeroView & hero );

    // FUN_00428ad0: a direction 0..7 to a neighbouring object worth the detour, -1 none (note v148).
    int sidePickup( const HeroView & hero );

    // FUN_0041173d for the player of this engine colour (not for seat 0): after an obelisk visit and after any digging (note v148).
    void digGuess( int colourIndex );

    // FUN_00456943 for a computer hero at its tile; returns true when the ultimate artifact was found.
    bool computerDig( Heroes & hero );

    // FUN_00456943 for any hero at its tile, with the human player's messages and sounds (note v154).
    bool dig( Heroes & hero );

    // FUN_0042eca6 with param_3 = 0 (FUN_004285db): a computer hero in an own castle buys the spell book (500, else waits: movement 0),
    // learns the guild spells and moves creatures between its army and the garrison (note v148).
    void ownCastleActions( Heroes & hero, Castle & castle );

    // Computer visit decisions (FUN_004046c3, note v149): hire at an object (returns the count hired and adds them), chest gold or experience.
    // addCount >= 0: FUN_0040599e adds that many (the whole group) once FUN_0042e76d says yes.
    int computerHire( Heroes & hero, int creature, int available, bool free, int addCount = -1 );
    bool computerTakesGold( const Heroes & hero, int gold, int experience );

    // FUN_0042e947 for a computer hero against an army without owner or hero (FUN_00430052, FUN_004301c9; note v38 1-2, v150): no battle screen.
    // The losses are applied to both armies; a hero whose loss ratio reaches 0.99 is dismissed (FUN_004043fc). Returns true when the hero won.
    // castle = the garrison's castle (FUN_004305d7 passes param_5 = 1): its defence counts in the forecast, and a win takes the castle.
    // enemyHero = the computer hero that owns `enemy` (case 0x3d): it can win experience, lose artifacts and be beaten.
    bool quickCombat( Heroes & hero, ::Army & enemy, Castle * castle = nullptr, Heroes * enemyHero = nullptr );

    // FUN_0042f88d for a computer hero: the worth of fighting an army without owner for this reward (0 = not worth it).
    int fightWorth( const Heroes & hero, const ::Army & enemy, int reward );

    // FUN_0042f954: the computer buys the artifact (HoMM1 number) for this price.
    bool computerBuysArtifact( const Heroes & hero, int h1Artifact, int price );

    // True when the Heroes I computer player moves the computer heroes (always, unless H1_ENGINE_AI is set for a test).
    bool enabled();

    // FUN_0042868b "CheckBuy" (note v35) for the player on turn [AC6].
    void checkBuy();

    // FUN_0042aaf9 (note v36, v143): writes the destination into destX / destY (-1 when none) and returns the best score.
    // range = search limit R (v142 1-6).
    int chooseDestination( HeroView & hero, int range );
}
