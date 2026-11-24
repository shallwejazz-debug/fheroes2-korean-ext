// Heroes I path search (homm1-remaster project): FUN_0046e820 with FUN_0044ec60 (add to the queue), FUN_0044ef10 (neighbours) and
// FUN_0041cac9 (guarding monster). Used by the Heroes I computer player and, since note v155, for human heroes' routes.
// See docs/analysis notes v140, v141, v155.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <list>
#include <vector>

class Heroes;

namespace Route
{
    class Step;
}

namespace H1
{
    namespace Path
    {
        constexpr int mapSize = 72;

        // One 9-byte record of the result grid / queue (note v140 section 1).
        struct Cell
        {
            int8_t x = 0;
            int8_t y = 0;
            uint16_t dirCost = 0; // low 4 bits direction, cost << 4
            uint8_t flags = 0; // bit 0 reached, bit 1 object tile (stop), bit 2 guarded, bits 3.. "turn break" mark
            int8_t guardX = 0;
            int8_t guardY = 0;
            int8_t breakX = 0;
            int8_t breakY = 0;

            int cost() const
            {
                return dirCost >> 4;
            }

            int direction() const
            {
                return dirCost & 0xf;
            }

            bool reached() const
            {
                return ( flags & 1 ) != 0;
            }
        };

        // What the search reads from the game (the original reads the game state directly).
        struct World
        {
            // Original tile bytes (10 per tile, index x * 72 + y), current state: heroes 0xbd, boats 0xbe (see buildTiles).
            std::vector<std::array<uint8_t, 10>> tiles;
            // Table 0x4a8708: bit 0x80 = a guarding monster is next to this tile (FUN_0041ce17), index y + x * 72.
            std::vector<uint8_t> monsterArea;
            // Table +0x15a16 bit of the current player (1 << player): the tile was seen. Index x * 72 + y.
            std::vector<uint8_t> seen;
            // DAT_004a9b60 of the current player: non-zero for a human player.
            bool humanPlayer = false;
            // Owner (player number) of hero n (hero record +1), for the 0x3d rule; and the current player.
            std::function<int( int hero )> heroOwner;
            int currentPlayer = 0;

            const std::array<uint8_t, 10> & tile( int x, int y ) const; // FUN_0045932d: outside the map = tile 0, 0
        };

        // The search's view of the current game for the player of this colour (engine colour index): original tile bytes of the map
        // (h1_map_render gameTileBytes: objects taken or beaten are gone) with every hero tile as 0xbd (byte 9 = hero number, byte 6 bit 0x40
        // cleared, as FUN_004147d7 / the move code set it) and every empty boat as 0xbe; seen = no fog for that colour; monsterArea filled.
        World buildWorld( int colourIndex );

        // Tile group (table 0x4a7d68 = tile number / 20).
        int groupOf( const std::array<uint8_t, 10> & tile );

        // FUN_0044e7d0: one step from a tile of this group, direction diagonal or not, left = movement left, heroClass = hero +0x1c.
        int stepCost( int group, bool diagonal, int left, int heroClass );

        // FUN_0041cac9.
        bool findGuard( const World & w, int x, int y, int & gx, int & gy, int skipX, int skipY );

        // FUN_0041ce17: fills monsterArea from the tiles.
        void fillMonsterArea( World & w );

        struct Search
        {
            std::vector<Cell> grid = std::vector<Cell>( mapSize * mapSize ); // index x * 72 + y
            std::vector<Cell> queue; // sorted from the highest cost to the lowest (taken from the end)
            int objectX = -1; // structure +0xc / +0x10: the one object tile FUN_0044ef10 may enter when "objects stop" is off
            int objectY = -1;
            bool finished = false; // DAT_004cb1f4
            int lastCost = 0; // DAT_004cb1c0
            int movementLeft = 0; // DAT_004c2cd0

            const Cell & at( int x, int y ) const
            {
                return grid[x * mapSize + y];
            }

            // FUN_0046e820( start x, y, direction, max cost (0 = no limit), boat, monsters, movement left, hero class, target x, y,
            // go on, monster tiles ).
            void run( const World & w, int startX, int startY, int startDir, int maxCost, bool boat, bool monsters, int movement, int heroClass,
                      int targetX, int targetY, bool goOn, bool monsterTiles );

        private:
            void reset();
            void add( int x, int y, int dir, int cost, int maxCost, bool stop, bool guarded, int gx, int gy, int brk, int bx, int by );
            void neighbours( const World & w, int x, int y, int8_t ground[8], uint8_t stop[8], bool objectsStop, bool boat ) const;
        };

        // FUN_0046e730( start x, y, target x, y, maxCost ): from the target back to the start along the search result, the direction of
        // every cell whose cost is maxCost or less (buffer order: the last step first). Returns the count; 0 when the walk leaves the
        // recorded cells or passes 0xff steps. firstCost: the cost of the first cell taken (the route's total).
        int traceBack( const Search & search, int startX, int startY, int targetX, int targetY, int maxCost, std::vector<int> & directions,
                       int & firstCost );

        // FUN_004634e7: the route marks. For each step in walking order (directions in traceBack's order): the tile reached, the step's
        // direction, its cost (FUN_0044e7d0 from the tile it leaves, the movement left counted down) and the value written for that tile.
        struct RouteMark
        {
            int x = 0;
            int y = 0;
            int direction = 0;
            int cost = 0;
            int value = 0;
        };
        std::vector<RouteMark> routeMarks( const World & w, int startX, int startY, int moveLeft, int heroClass, const std::vector<int> & directions );

        // The value of one mark: table 0x492410[next direction x 8 + this direction], the last step 0xe; + 0xe while the movement left after
        // the step is 0 or more. route.icn frame = (value & 0x1f) - 1, bit 0x20 = mirrored.
        int routeMarkValue( int thisDirection, int nextDirection, bool last, int leftAfter );

        // A human hero's route (FUN_00463a07 search: start direction, max cost 999, boat, monsters not checked, movement left, class,
        // target, monster tiles; then FUN_0046e730 with 999). Steps in walking order with the engine penalty (FUN_0044e7d0 x 25, the
        // movement left going down step by step as FUN_004634e7 counts it). Empty when the target cannot be reached.
        std::list<Route::Step> humanRoute( const Heroes & hero, int32_t targetIndex );

        // The route's total cost x 25 (0 = not reachable), for the engine's cursor.
        uint32_t humanDistance( const Heroes & hero, int32_t targetIndex );

        // FUN_00456dde (note v161): the advmice.icn frame for the mouse over this tile, for the human player of colourIndex with this
        // hero selected (nullptr = none). 0 pointer, 2 hero, 3 castle, day x 6 + 4 move / 5 fight / 6 boat / 7 anchor / 8 meet / 9 visit,
        // 0x1c + day boat visit.
        int humanCursor( const Heroes * hero, int colourIndex, int32_t tileIndex );

        // The game changed (objects, heroes): the next human route builds the world again.
        void invalidateHumanWorld();
    }
}
