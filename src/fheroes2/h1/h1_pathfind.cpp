// Heroes I path search (homm1-remaster project). See h1_pathfind.h and docs/analysis notes v140, v141.
#include "h1_pathfind.h"

#include <algorithm>

#include "color.h"
#include "h1_map_render.h"
#include "h1_visit.h"
#include "h1_ai.h"
#include "h1_heroes.h"
#include "heroes.h"
#include "castle.h"
#include "route.h"
#include "direction.h"
#include "kingdom.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "players.h"
#include "world.h"

namespace
{
    // Table 0x491660: x, y step of directions 0 up, 1 up-right, 2 right, 3 down-right, 4 down, 5 down-left, 6 left, 7 up-left.
    const int dirX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int dirY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    // FUN_0044e770: straight step of each tile group (0 water, 1 grass, 2 snow, 3 swamp, 4 lava, 5 desert, 6 dirt); diagonal = x 1.5.
    int straightOf( const int group )
    {
        switch ( group ) {
        case 2:
        case 3:
            return 6;
        case 5:
            return 8;
        case 0:
        case 1:
        case 4:
        case 6:
            return 4;
        default:
            return 0;
        }
    }

    // FUN_0044ef10 / FUN_0046e820: objects (byte 8 & 0x7f) a path may enter from above although the tile has an upper picture.
    bool enterableFromAbove( const int object )
    {
        switch ( object ) {
        case 0x1a:
        case 0x1d:
        case 0x06:
        case 0x08:
        case 0x0b:
        case 0x30:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x09:
        case 0x1b:
        case 0x24:
        case 0x2c:
        case 0x2b:
            return true;
        default:
            return false;
        }
    }

    bool hasUpperPicture( const std::array<uint8_t, 10> & t )
    {
        return t[2] != 0xff && ( t[6] & 0x80 ) == 0;
    }
}

namespace H1
{
    namespace Path
    {
        const std::array<uint8_t, 10> & World::tile( const int x, const int y ) const
        {
            if ( x < 0 || y < 0 || x >= mapSize || y >= mapSize ) {
                return tiles[0];
            }
            return tiles[x * mapSize + y];
        }

        World buildWorld( const int colourIndex )
        {
            World w;
            w.tiles = gameTileBytes();
            w.tiles.resize( mapSize * mapSize );
            w.seen.assign( mapSize * mapSize, 0 );
            const PlayerColor color = Color::IndexToColor( colourIndex );
            for ( int32_t index = 0; index < mapSize * mapSize && index < static_cast<int32_t>( world.getSize() ); ++index ) {
                const Maps::Tile & tile = world.getTile( index );
                std::array<uint8_t, 10> & t = w.tiles[index];
                if ( const Heroes * hero = tile.getHero(); hero != nullptr ) {
                    t[8] = 0xbd;
                    t[9] = static_cast<uint8_t>( H1::heroNumberOfId( hero->GetID() ) );
                    t[6] = static_cast<uint8_t>( t[6] & 0xbf );
                }
                else if ( tile.getMainObjectType( false ) == MP2::OBJ_BOAT ) {
                    t[8] = 0xbe;
                }
                // Byte 9 as the game has it now (contents of chests, dwellings ... kept with the engine tile, h1_visit).
                if ( const int b9 = currentByte9( index ); b9 >= 0 && t[8] != 0xbd ) {
                    t[9] = static_cast<uint8_t>( b9 );
                }
                if ( !tile.isFog( color ) ) {
                    w.seen[index] = 1;
                }
            }
            // The engine keeps tiles row by row (index = y * 72 + x); the original column by column (x * 72 + y).
            std::vector<std::array<uint8_t, 10>> byColumn( mapSize * mapSize );
            std::vector<uint8_t> seenByColumn( mapSize * mapSize, 0 );
            for ( int x = 0; x < mapSize; ++x ) {
                for ( int y = 0; y < mapSize; ++y ) {
                    byColumn[x * mapSize + y] = w.tiles[y * mapSize + x];
                    seenByColumn[x * mapSize + y] = w.seen[y * mapSize + x];
                }
            }
            w.tiles = std::move( byColumn );
            w.seen = std::move( seenByColumn );
            w.humanPlayer = world.GetKingdom( color ).isControlHuman();
            w.currentPlayer = colourIndex;
            w.heroOwner = []( const int hero ) {
                const Heroes * h = H1::heroByNumber( hero );
                return ( h != nullptr && h->GetColor() != PlayerColor::NONE ) ? Color::GetIndex( h->GetColor() ) : -1;
            };
            fillMonsterArea( w );
            return w;
        }

        int groupOf( const std::array<uint8_t, 10> & tile )
        {
            // Table 0x4a7d68: 0x8c entries, entry n = n / 20 (FUN_00424f99).
            return tile[0] < 0x8c ? tile[0] / 20 : 0;
        }

        int stepCost( const int group, const bool diagonal, const int left, const int heroClass )
        {
            const int g = ( heroClass == 1 ) ? 1 : group;
            const int straight = straightOf( g );
            const int diag = straight + ( straight >> 1 );
            if ( !diagonal ) {
                return straight;
            }
            if ( diag <= left ) {
                return diag;
            }
            return ( left < straight ) ? diag : straight;
        }

        bool findGuard( const World & w, const int x, const int y, int & gx, int & gy, const int skipX, const int skipY )
        {
            int x0 = x - 1;
            int y0 = y - 1;
            int x1 = x + 2;
            int y1 = y + 2;
            if ( x < 1 || y < 1 || x > 0x46 || y > 0x46 ) {
                if ( x == 0x47 ) {
                    x1 = 0x48;
                }
                if ( y == 0x47 ) {
                    y1 = 0x48;
                }
                x0 = ( x == 0 ) ? 0 : x - 1;
                y0 = ( y == 0 ) ? 0 : y - 1;
            }
            for ( int cx = x0; cx < x1; ++cx ) {
                for ( int cy = y0; cy < y1; ++cy ) {
                    if ( cx < 0 || cy < 0 || cx >= mapSize || cy >= mapSize ) {
                        continue;
                    }
                    if ( w.tiles[cx * mapSize + cy][8] != 0x9a ) {
                        continue;
                    }
                    // A monster on a row above the tile does not guard it when the tile has an upper picture.
                    if ( !( y <= cy || !hasUpperPicture( w.tile( x, y ) ) ) ) {
                        continue;
                    }
                    if ( cx == skipX && cy == skipY ) {
                        continue;
                    }
                    gx = cx;
                    gy = cy;
                    return true;
                }
            }
            return false;
        }

        void fillMonsterArea( World & w )
        {
            w.monsterArea.assign( mapSize * mapSize, 0 );
            for ( int x = 0; x < mapSize; ++x ) {
                for ( int y = 0; y < mapSize; ++y ) {
                    int gx = 0;
                    int gy = 0;
                    if ( findGuard( w, x, y, gx, gy, -1, -1 ) ) {
                        w.monsterArea[y + x * mapSize] |= 0x80;
                    }
                }
            }
        }

        void Search::reset()
        {
            // FUN_0044e6f0.
            std::fill( grid.begin(), grid.end(), Cell() );
            queue.clear();
        }

        void Search::add( const int x, const int y, const int dir, const int cost, const int maxCost, const bool stop, const bool guarded, const int gx,
                          const int gy, const int brk, const int bx, const int by )
        {
            // FUN_0044ec60.
            const uint16_t cost16 = static_cast<uint16_t>( cost );
            if ( !( ( cost16 <= static_cast<uint16_t>( maxCost ) || static_cast<uint16_t>( maxCost ) == 0 ) && x >= 0 && x < mapSize && y >= 0 && y < mapSize
                    && queue.size() < 0x400 ) ) {
                return;
            }
            Cell & g = grid[x * mapSize + y];
            const bool reached = ( g.flags & 1 ) != 0;
            const bool wasGuarded = ( g.flags & 4 ) != 0;
            if ( reached && !( ( wasGuarded || !guarded ) && ( cost16 < ( g.dirCost >> 4 ) || ( wasGuarded && !guarded ) ) ) ) {
                return;
            }
            // Binary search in the queue (highest cost first).
            size_t lo = 0;
            size_t hi = queue.size();
            while ( lo < hi ) {
                const size_t mid = ( lo + hi ) >> 1;
                if ( cost16 < ( queue[mid].dirCost >> 4 ) ) {
                    lo = mid + 1;
                }
                else {
                    hi = mid;
                }
            }
            Cell c;
            // The original keeps bits 0..2 of the record it overwrites and sets them again just below; bits 3.. are written here.
            if ( movementLeft < static_cast<int>( cost16 ) && brk == 0 ) {
                c.flags = 8;
                c.breakX = static_cast<int8_t>( x - dirX[dir & 7] );
                c.breakY = static_cast<int8_t>( y - dirY[dir & 7] );
            }
            else {
                c.flags = static_cast<uint8_t>( brk << 3 );
                c.breakX = static_cast<int8_t>( bx );
                c.breakY = static_cast<int8_t>( by );
            }
            c.x = static_cast<int8_t>( x );
            c.y = static_cast<int8_t>( y );
            c.dirCost = static_cast<uint16_t>( ( dir & 0xf ) | ( cost16 << 4 ) );
            c.flags = static_cast<uint8_t>( ( c.flags & ~6 ) | ( stop ? 2 : 0 ) | ( guarded ? 4 : 0 ) | 1 );
            c.guardX = static_cast<int8_t>( gx );
            c.guardY = static_cast<int8_t>( gy );
            queue.insert( queue.begin() + static_cast<std::ptrdiff_t>( lo ), c );
            g = c;
        }

        void Search::neighbours( const World & w, const int x, const int y, int8_t ground[8], uint8_t stop[8], const bool objectsStop, const bool boat ) const
        {
            // FUN_0044ef10.
            const std::array<uint8_t, 10> & here = w.tile( x, y );
            for ( int d = 0; d < 8; ++d ) {
                stop[d] = 0;
                ground[d] = -1;
                const int nx = x + dirX[d];
                const int ny = y + dirY[d];
                if ( nx < -6 || nx > 0x47 || ny < -6 || ny > 0x47 ) {
                    continue;
                }
                const std::array<uint8_t, 10> & t = w.tile( nx, ny );
                if ( ( t[7] & 0x80 ) != 0 ) {
                    continue;
                }
                if ( w.humanPlayer ) {
                    // Outside the map the original reads other memory here; such tiles are never added (FUN_0044ec60), so they are skipped.
                    if ( nx < 0 || ny < 0 || nx >= mapSize || ny >= mapSize || w.seen[nx * mapSize + ny] == 0 ) {
                        continue;
                    }
                }
                if ( ( t[8] & 0x80 ) != 0 ) {
                    if ( !objectsStop ) {
                        if ( objectX != nx || objectY != ny ) {
                            continue;
                        }
                    }
                    else {
                        stop[d] = 1;
                    }
                }
                const int group = groupOf( t );
                bool ok = false;
                if ( group != 0 ) {
                    ok = !boat || t[8] == 0x1f;
                }
                else if ( !boat ) {
                    ok = ( t[8] == 0xbd || t[8] == 0xbe || t[8] == 0xa3 );
                }
                else {
                    ok = !( t[8] == 0xa3 || t[8] == 0xbe );
                }
                if ( !ok ) {
                    continue;
                }
                const unsigned bit = 1u << d;
                if ( ( bit & 0x83 ) == 0 ) {
                    if ( ( bit & 0x38 ) != 0 && hasUpperPicture( t ) && ( ( t[8] & 0x80 ) == 0 || !enterableFromAbove( t[8] & 0x7f ) ) ) {
                        continue;
                    }
                }
                else if ( hasUpperPicture( here ) ) {
                    continue;
                }
                ground[d] = static_cast<int8_t>( group );
            }
        }

        void Search::run( const World & w, const int startX, const int startY, const int startDir, const int maxCost, const bool boat, const bool monsters,
                          const int movement, const int heroClass, const int targetX, const int targetY, const bool goOn, const bool monsterTiles )
        {
            if ( !goOn ) {
                finished = false;
                movementLeft = movement;
                reset();
                objectX = -1;
                objectY = -1;
                lastCost = 0;
            }
            bool hasTarget = false;
            int best = 0; // DAT_004cb1f0
            if ( targetX >= 0 ) {
                if ( targetX >= mapSize || targetY < 0 || targetY >= mapSize || w.seen[targetX * mapSize + targetY] == 0 ) {
                    return;
                }
                const std::array<uint8_t, 10> & t = w.tile( targetX, targetY );
                if ( ( t[7] & 0x80 ) != 0 ) {
                    return;
                }
                if ( groupOf( t ) == 0 ) {
                    if ( !boat ) {
                        if ( t[8] != 0xbd && t[8] != 0xbe && t[8] != 0xa3 ) {
                            return;
                        }
                    }
                    else if ( t[8] == 0xa3 || t[8] == 0xbe ) {
                        return;
                    }
                }
                hasTarget = true;
                best = 9999;
            }
            if ( hasTarget && goOn ) {
                const Cell & t = grid[targetX * mapSize + targetY];
                if ( t.reached() && t.cost() <= lastCost + 4 ) {
                    return;
                }
            }
            if ( !goOn ) {
                add( startX, startY, startDir, 0, maxCost, false, false, 0, 0, 0, 0, 0 );
            }

            while ( !queue.empty() ) {
                const Cell node = queue.back();
                queue.pop_back();
                const int cost = node.cost();
                const int x = node.x;
                const int y = node.y;
                if ( hasTarget && best < 9999 && best <= cost + 4 ) {
                    lastCost = cost;
                    queue.push_back( node );
                    return;
                }
                if ( !( cost <= static_cast<int16_t>( maxCost ) || static_cast<int16_t>( maxCost ) < 1 ) ) {
                    continue;
                }
                bool guarded = ( node.flags & 4 ) != 0;
                int gx = guarded ? node.guardX : 0;
                int gy = guarded ? node.guardY : 0;
                bool expand = false;
                if ( ( node.flags & 2 ) == 0 ) {
                    expand = true;
                }
                else {
                    const int object = w.tile( x, y )[8] & 0x7f;
                    if ( object == 0x1a || object == 0x29 || object == 0x3d || object == 0x3e ) {
                        if ( monsters && !guarded ) {
                            guarded = true;
                            gx = x;
                            gy = y;
                            if ( object != 0x3d || !w.heroOwner || w.heroOwner( w.tile( x, y )[9] ) != w.currentPlayer ) {
                                expand = true;
                            }
                        }
                    }
                    else if ( monsters && object != 0x1d && object != 0x28 && object != 6 && object != 8 && object != 0xb && object != 0x30 && object != 2
                              && object != 3 && object != 4 && object != 9 && object != 0x1b && object != 0x24 && object != 0x2b ) {
                        expand = true;
                    }
                }
                if ( !expand ) {
                    continue;
                }
                if ( !boat ) {
                    if ( ( w.monsterArea[y + x * mapSize] & 0x80 ) != 0 && ( startX != x || startY != y ) ) {
                        if ( !monsters ) {
                            continue;
                        }
                        // The original tests the record's own guarded bit here (DAT_004cb1cc & 4), not the flag the object test above
                        // may have just set (note v182: found by running the original code).
                        if ( ( node.flags & 4 ) == 0 ) {
                            if ( findGuard( w, x, y, gx, gy, -1, -1 ) ) {
                                guarded = true;
                            }
                        }
                        else if ( findGuard( w, x, y, gx, gy, node.guardX, node.guardY ) ) {
                            continue;
                        }
                    }
                }
                else if ( w.tile( x, y )[8] == 0x1f ) {
                    continue;
                }
                int8_t ground[8];
                uint8_t stop[8];
                neighbours( w, x, y, ground, stop, true, boat );
                const int group = groupOf( w.tile( x, y ) );
                const int costStraight = cost + stepCost( group, false, movementLeft - cost, heroClass );
                const int costDiagonal = cost + stepCost( group, true, movementLeft - cost, heroClass );
                for ( int d = 0; d < 8; ++d ) {
                    if ( ground[d] == -1 ) {
                        continue;
                    }
                    const int nx = x + dirX[d];
                    const int ny = y + dirY[d];
                    if ( monsters && nx >= 0 && ny >= 0 && nx < mapSize && ny < mapSize && ( w.monsterArea[ny + nx * mapSize] & 0x80 ) != 0 ) {
                        const Cell & n = grid[nx * mapSize + ny];
                        // FUN_0041cac9 writes the found monster into the current guard position (same globals).
                        if ( n.reached() && ( n.flags & 4 ) != 0 && n.cost() < cost + 0xc && findGuard( w, nx, ny, gx, gy, -1, -1 ) && n.guardX == gx
                             && n.guardY == gy ) {
                            continue;
                        }
                    }
                    add( nx, ny, d, ( d & 1 ) ? costDiagonal : costStraight, maxCost, stop[d] != 0, guarded, gx, gy, node.flags >> 3, node.breakX, node.breakY );
                    if ( hasTarget && nx == targetX && ny == targetY && ( node.flags & 4 ) == 0 ) {
                        const int c = cost + stepCost( ground[d], ( d & 1 ) != 0, movementLeft - cost, heroClass );
                        if ( c < best ) {
                            best = c;
                        }
                    }
                }
            }

            if ( monsterTiles ) {
                // Every monster tile gets a cost from a reached neighbour (attacking it), the step from that neighbour's ground.
                for ( int mx = 0; mx < mapSize; ++mx ) {
                    for ( int my = 0; my < mapSize; ++my ) {
                        if ( ( w.tiles[mx * mapSize + my][8] & 0x7f ) != 0x1a ) {
                            continue;
                        }
                        for ( int d = 0; d < 8; ++d ) {
                            const int nx = mx + dirX[d];
                            const int ny = my + dirY[d];
                            // Outside the map the original reads other memory [unconfirmed]; skipped.
                            if ( nx < 0 || ny < 0 || nx >= mapSize || ny >= mapSize ) {
                                continue;
                            }
                            const std::array<uint8_t, 10> & t = w.tile( nx, ny );
                            bool allowed = true;
                            if ( ( ( 1u << d ) & 0x38 ) != 0 && t[2] != 0xff && ( t[6] & 0x80 ) == 0 ) {
                                allowed = false;
                            }
                            const Cell & n = grid[nx * mapSize + ny];
                            if ( allowed && n.reached() && ( t[8] & 0x80 ) == 0 ) {
                                const int c0 = n.cost();
                                const int c = c0 + stepCost( groupOf( t ), ( d & 1 ) != 0, movementLeft - c0, heroClass );
                                add( mx, my, ( d + 4 ) & 7, c, maxCost, true, false, -1, -1, 0, -1, -1 );
                            }
                        }
                    }
                }
            }
            finished = true;
        }
    }
}

namespace
{
    // The world for the human player's routes, built again after a change or when the hero moved (its own tile is 0xbd).
    struct HumanCache
    {
        bool valid = false;
        int colour = -1;
        int heroId = -1;
        int32_t heroIndex = -1;
        H1::Path::World world;
    };

    HumanCache humanCache;

    const H1::Path::World & worldFor( const int colour, const int heroId, const int32_t heroIndex )
    {
        if ( !humanCache.valid || humanCache.colour != colour || humanCache.heroId != heroId || humanCache.heroIndex != heroIndex ) {
            humanCache.world = H1::Path::buildWorld( colour );
            humanCache.valid = true;
            humanCache.colour = colour;
            humanCache.heroId = heroId;
            humanCache.heroIndex = heroIndex;
        }
        return humanCache.world;
    }

    const H1::Path::World & humanWorld( const Heroes & hero )
    {
        return worldFor( Color::GetIndex( hero.GetColor() ), hero.GetID(), hero.GetIndex() );
    }

    const int engineDirection[8] = { Direction::TOP, Direction::TOP_RIGHT, Direction::RIGHT, Direction::BOTTOM_RIGHT,
                                     Direction::BOTTOM, Direction::BOTTOM_LEFT, Direction::LEFT, Direction::TOP_LEFT };

    int originalDirection( const int engineDir )
    {
        for ( int d = 0; d < 8; ++d ) {
            if ( engineDirection[d] == engineDir ) {
                return d;
            }
        }
        return 2;
    }

    // FUN_00463a07 then FUN_0046e730( hero x, y, target x, y, 999 ): the directions from the target back to the hero (last step first).
    bool searchHuman( const Heroes & hero, const int32_t targetIndex, H1::Path::Search & search, std::vector<int> & directions, int & total )
    {
        directions.clear();
        total = 0;
        if ( targetIndex < 0 || targetIndex >= 72 * 72 || hero.GetIndex() < 0 ) {
            return false;
        }
        const H1::Path::World & w = humanWorld( hero );
        const H1::Ai::HeroView view = H1::Ai::viewOf( hero );
        const int tx = targetIndex % 72;
        const int ty = targetIndex / 72;
        if ( tx == view.x && ty == view.y ) {
            return false;
        }
        search.run( w, view.x, view.y, originalDirection( hero.GetDirection() ), 999, view.onBoat, false, view.moveLeft, view.heroClass, tx, ty, false,
                    true );
        return H1::Path::traceBack( search, view.x, view.y, tx, ty, 999, directions, total ) > 0;
    }
}

namespace H1::Path
{
    int traceBack( const Search & search, const int startX, const int startY, const int targetX, const int targetY, const int maxCost,
                   std::vector<int> & directions, int & firstCost )
    {
        directions.clear();
        firstCost = 0;
        int x = targetX;
        int y = targetY;
        for ( int guard = 0; guard < mapSize * mapSize; ++guard ) {
            if ( x == startX && y == startY ) {
                return static_cast<int>( directions.size() );
            }
            // The original reads outside the grid here; such a walk is treated as no route.
            if ( x < 0 || y < 0 || x >= mapSize || y >= mapSize ) {
                break;
            }
            const Cell & c = search.at( x, y );
            // FUN_0046e730 gives up only when both coordinates differ from the record.
            if ( c.x != x && c.y != y ) {
                break;
            }
            if ( c.cost() <= maxCost ) {
                if ( directions.empty() ) {
                    firstCost = c.cost();
                }
                directions.push_back( c.direction() & 7 );
                if ( directions.size() > 0xff ) {
                    break;
                }
            }
            const int back = ( c.direction() + 4 ) & 7;
            x += dirX[back];
            y += dirY[back];
        }
        directions.clear();
        firstCost = 0;
        return 0;
    }

    int routeMarkValue( const int thisDirection, const int nextDirection, const bool last, const int leftAfter )
    {
        // Table 0x492410 (64 bytes, [next direction x 8 + this direction]).
        static const uint8_t marks[64] = { 1,  6,  6,  6,  1,  38, 38, 38, 12, 2,  7,  7,  7,  2,  12, 12, 9,  9,  3,  8,  8,  8,
                                           3,  9,  10, 10, 10, 4,  13, 13, 13, 4,  5,  11, 11, 11, 5,  43, 43, 43, 42, 36, 45, 45,
                                           45, 36, 42, 42, 41, 41, 35, 40, 40, 40, 35, 41, 44, 44, 44, 34, 39, 39, 39, 34 };
        int value = last ? 0xe : marks[( nextDirection & 7 ) * 8 + ( thisDirection & 7 )];
        if ( leftAfter >= 0 ) {
            value += 0xe;
        }
        return value;
    }

    std::vector<RouteMark> routeMarks( const World & w, const int startX, const int startY, const int moveLeft, const int heroClass,
                                       const std::vector<int> & directions )
    {
        std::vector<RouteMark> marks;
        int left = moveLeft;
        int x = startX;
        int y = startY;
        for ( size_t i = directions.size(); i-- > 0; ) {
            const int d = directions[i] & 7;
            const int cost = stepCost( groupOf( w.tile( x, y ) ), ( d & 1 ) != 0, left, heroClass );
            left -= cost;
            x += dirX[d];
            y += dirY[d];
            RouteMark mark;
            mark.x = x;
            mark.y = y;
            mark.direction = d;
            mark.cost = cost;
            mark.value = routeMarkValue( d, ( i > 0 ) ? directions[i - 1] : 0, i == 0, left );
            marks.push_back( mark );
        }
        return marks;
    }

    std::list<Route::Step> humanRoute( const Heroes & hero, const int32_t targetIndex )
    {
        std::list<Route::Step> steps;
        Search search;
        std::vector<int> directions;
        int total = 0;
        if ( !searchHuman( hero, targetIndex, search, directions, total ) ) {
            return steps;
        }
        const World & w = humanWorld( hero );
        const H1::Ai::HeroView view = H1::Ai::viewOf( hero );
        // FUN_004634e7: the cost of each step from the tile it leaves, with the movement left counted down.
        int32_t from = view.y * 72 + view.x;
        for ( const RouteMark & mark : routeMarks( w, view.x, view.y, view.moveLeft, view.heroClass, directions ) ) {
            const int32_t to = mark.y * 72 + mark.x;
            steps.emplace_back( to, from, engineDirection[mark.direction], static_cast<uint32_t>( mark.cost ) * H1::movementScale );
            from = to;
        }
        return steps;
    }

    uint32_t humanDistance( const Heroes & hero, const int32_t targetIndex )
    {
        Search search;
        std::vector<int> directions;
        int total = 0;
        if ( !searchHuman( hero, targetIndex, search, directions, total ) ) {
            return 0;
        }
        return static_cast<uint32_t>( std::max( 1, total ) ) * H1::movementScale;
    }

    int humanCursor( const Heroes * hero, const int colourIndex, const int32_t tileIndex )
    {
        if ( tileIndex < 0 || tileIndex >= mapSize * mapSize ) {
            return 0;
        }
        const World & w = ( hero != nullptr ) ? humanWorld( *hero ) : worldFor( colourIndex, -1, -1 );
        const int x = tileIndex % mapSize;
        const int y = tileIndex / mapSize;
        const std::array<uint8_t, 10> & t = w.tile( x, y );
        const int kind = t[8] & 0x7f;
        auto castleOurs = [colourIndex]( const int cx, const int cy ) {
            const Castle * castle = world.getCastle( { cx, cy } );
            return castle != nullptr && castle->GetColor() != PlayerColor::NONE && Color::GetIndex( castle->GetColor() ) == colourIndex;
        };
        auto heroOurs = [&w, colourIndex]( const int number ) { return w.heroOwner && w.heroOwner( number ) == colourIndex; };

        if ( hero == nullptr ) {
            if ( kind == 0x28 && castleOurs( x, y ) ) {
                return 3;
            }
            if ( kind == 0x3d && heroOurs( t[9] ) ) {
                return 2;
            }
            return 0;
        }
        const H1::Ai::HeroView view = H1::Ai::viewOf( *hero );
        if ( x == view.x && y == view.y ) {
            return 2;
        }
        const bool boat = view.onBoat;
        if ( ( t[7] & 0x80 ) != 0 ) {
            // Not passable: an own castle's tile with castle tiles above and below -> castle.
            if ( kind == 0x28 && castleOurs( x, y ) && y > 0 && y < 0x47 ) {
                const std::array<uint8_t, 10> & up = w.tile( x, y - 1 );
                const std::array<uint8_t, 10> & down = w.tile( x, y + 1 );
                if ( ( ( up[8] & 0x7f ) == 0x28 || ( up[7] & 0x7f ) == 0x28 ) && ( ( down[8] & 0x7f ) == 0x28 || ( down[7] & 0x7f ) == 0x28 ) ) {
                    return 3;
                }
            }
            return 0;
        }
        const bool land = t[0] >= 0x14;
        if ( !( ( boat || land || t[8] == 0xbd || t[8] == 0xbe || t[8] == 0xa3 ) && ( !boat || !land || t[8] == 0x1f ) ) ) {
            return 0;
        }
        Search search;
        search.run( w, view.x, view.y, originalDirection( hero->GetDirection() ), 999, boat, false, view.moveLeft, view.heroClass, x, y, false, true );
        const Cell & c = search.at( x, y );
        if ( !c.reached() ) {
            return 0;
        }
        int day = 0;
        if ( view.moveLeft < c.cost() ) {
            day = std::min( 3, ( c.cost() - view.moveLeft ) / std::max( 1, view.maxMove ) + 1 );
        }
        const int base = day * 6;
        const bool guarded = ( w.monsterArea[y + x * mapSize] & 0x80 ) != 0;
        switch ( kind ) {
        case 0x1a:
            return base + 5;
        case 0x1f:
            return boat ? base + 7 : ( guarded ? base + 5 : base + 4 );
        case 0x3d:
            return heroOurs( t[9] ) ? base + 8 : base + 5;
        case 0x3e:
            return boat ? base : base + 6;
        case 0x28:
            if ( ( t[8] & 0x80 ) != 0 && !castleOurs( x, y ) ) {
                const Castle * castle = world.getCastle( { x, y } );
                if ( castle != nullptr && castle->GetArmy().isValid() ) {
                    return base + 5;
                }
            }
            [[fallthrough]];
        default:
            break;
        }
        if ( !( !guarded || boat || kind == 4 || kind == 6 || kind == 8 || kind == 0xb || kind == 0x1d || kind == 0x30 ) ) {
            return base + 5;
        }
        if ( ( t[8] & 0x80 ) == 0 ) {
            return boat ? base + 6 : base + 4;
        }
        if ( boat ) {
            return ( kind == 3 || kind == 0x2c ) ? 0x1c + day : base + 6;
        }
        static const int visit[] = { 1, 2, 4, 5, 6, 7, 8, 9, 10, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                     0x18, 0x19, 0x1b, 0x1c, 0x1d, 0x20, 0x21, 0x22, 0x23, 0x24, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30 };
        if ( std::find( std::begin( visit ), std::end( visit ), kind ) != std::end( visit ) ) {
            return base + 9;
        }
        return guarded ? base + 5 : base + 4;
    }

    void invalidateHumanWorld()
    {
        humanCache.valid = false;
    }
}
