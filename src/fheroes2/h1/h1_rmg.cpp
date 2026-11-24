// H1-INTENDED(D16) extension mode: random HoMM1 maps (owner 2026-11-18 11:17 "무작위 지도", 12:0x "1편용 생성기 새로", 12:06 "일단 만들어봐").
// The game version of the prototype tools/randommap/h1_rmg_v1.py (notes v244, v245). All pieces come from the original maps through
// h1/ext/RMGDATA.BIN (tools/randommap/export_data.py): ground edge rule, single objects cut round their entrance, mountain / tree templates.
// The extension objects are not placed here: the game puts them on every map when an extension game starts (placeExtensionObjects).

#include "h1_rmg.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <random>
#include <set>
#include <vector>

#include "h1_map_convert.h"

namespace
{
    constexpr int W = 72;

    struct StampTile
    {
        int dx, dy, group;
        std::array<uint8_t, 10> b;
    };

    struct Stamp
    {
        int kind = 0;
        int record = -1;
        std::vector<StampTile> tiles;
        std::vector<std::array<int, 3>> sounds; // dx, dy, sound
    };

    struct Part
    {
        int dx, dy, layer, set, frame, kind, bits;
    };

    struct Template
    {
        int set = 0;
        bool blocking = false;
        int groups = 0;
        std::vector<Part> parts;
    };

    struct Data
    {
        std::vector<uint8_t> head; // 0x554
        std::vector<uint8_t> tail38;
        uint8_t shape[256]{};
        uint8_t flip[256]{};
        uint16_t variant[7][5][4]{};
        std::vector<Stamp> lists[5]; // castle, town, mine, treasure, site
        std::vector<Template> templates;
    };

    enum List
    {
        CASTLE = 0,
        TOWN,
        MINE,
        TREASURE,
        SITE
    };

    bool loadData( Data & d, std::string & err )
    {
        std::ifstream f( "h1/ext/RMGDATA.BIN", std::ios::binary );
        if ( !f ) {
            err = "h1/ext/RMGDATA.BIN not found";
            return false;
        }
        const std::vector<uint8_t> v( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
        size_t p = 0;
        bool bad = false;
        auto need = [&]( const size_t n ) {
            if ( p + n > v.size() ) {
                bad = true;
            }
            return !bad;
        };
        auto u8 = [&]() { return need( 1 ) ? v[p++] : 0; };
        auto i8 = [&]() { return static_cast<int>( static_cast<int8_t>( u8() ) ); };
        auto u16 = [&]() {
            const int lo = u8();
            return lo | ( u8() << 8 );
        };
        if ( !need( 8 ) || std::memcmp( v.data(), "H1RMG1", 6 ) != 0 ) {
            err = "RMGDATA.BIN: wrong file";
            return false;
        }
        p = 8;
        if ( !need( 0x554 + 38 + 512 ) ) {
            err = "RMGDATA.BIN: too short";
            return false;
        }
        d.head.assign( v.begin() + static_cast<std::ptrdiff_t>( p ), v.begin() + static_cast<std::ptrdiff_t>( p + 0x554 ) );
        p += 0x554;
        d.tail38.assign( v.begin() + static_cast<std::ptrdiff_t>( p ), v.begin() + static_cast<std::ptrdiff_t>( p + 38 ) );
        p += 38;
        std::memcpy( d.shape, &v[p], 256 );
        p += 256;
        std::memcpy( d.flip, &v[p], 256 );
        p += 256;
        for ( auto & g : d.variant ) {
            for ( auto & s : g ) {
                for ( uint16_t & w : s ) {
                    w = static_cast<uint16_t>( u16() );
                }
            }
        }
        for ( auto & list : d.lists ) {
            const int n = u16();
            for ( int i = 0; i < n && !bad; ++i ) {
                Stamp s;
                s.kind = u8();
                const int rec = u8();
                s.record = ( rec == 0xff ) ? -1 : rec;
                const int nt = u8();
                const int ns = u8();
                for ( int k = 0; k < nt && !bad; ++k ) {
                    StampTile t;
                    t.dx = i8();
                    t.dy = i8();
                    t.group = u8();
                    for ( uint8_t & b : t.b ) {
                        b = static_cast<uint8_t>( u8() );
                    }
                    s.tiles.push_back( t );
                }
                for ( int k = 0; k < ns && !bad; ++k ) {
                    const int dx = i8();
                    const int dy = i8();
                    s.sounds.push_back( { dx, dy, u8() } );
                }
                list.push_back( std::move( s ) );
            }
        }
        const int nt = u16();
        for ( int i = 0; i < nt && !bad; ++i ) {
            Template t;
            t.set = u8();
            t.blocking = u8() != 0;
            t.groups = u8();
            const int np = u8();
            for ( int k = 0; k < np && !bad; ++k ) {
                Part q;
                q.dx = i8();
                q.dy = i8();
                q.layer = u8();
                q.set = u8();
                q.frame = u8();
                q.kind = u8();
                q.bits = u8();
                t.parts.push_back( q );
            }
            d.templates.push_back( std::move( t ) );
        }
        if ( bad ) {
            err = "RMGDATA.BIN: cut short";
            return false;
        }
        return true;
    }

    using Tile = std::array<uint8_t, 10>;

    Tile emptyTile()
    {
        return { 0, 0, 0xff, 0, 0xff, 0, 0, 0, 0, 0 };
    }

    // Random monster marks (byte 8, note v39 table): weak .. very strong; picture until the game picks the creature.
    const int monsterMark[5] = { 0, 0xc3, 0xc4, 0xc5, 0xc6 };
    const int monsterFrame[5] = { 0, 0, 9, 20, 23 };
    const int offs[8][2] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } };

    class Maker
    {
    public:
        Maker( const Data & data, const uint32_t seed )
            : d( data )
            , rnd( seed )
            , seedValue( seed )
        {
            for ( auto & row : T ) {
                row.fill( emptyTile() );
            }
            for ( auto & row : zone ) {
                row.fill( -1 );
            }
            for ( auto & row : inst ) {
                for ( auto & c : row ) {
                    c = { -1, -1 };
                }
            }
        }

        bool make( const std::string & path, std::string & err )
        {
            land();
            towns();
            if ( std::count_if( castles.begin(), castles.end(), []( const CastleEntry & c ) { return c.player; } ) < 4 ) {
                err = "no room for the castles";
                return false;
            }
            objects();
            borders();
            for ( const auto & g : gaps ) {
                monster( g.first, g.second, 2 );
            }
            scatter( 0.12 );
            const CastleEntry & p0 = *std::find_if( castles.begin(), castles.end(), []( const CastleEntry & c ) { return c.player; } );
            openPaths( p0.x, p0.y + 1 );
            return write( path, err );
        }

    private:
        const Data & d;
        std::mt19937 rnd;
        uint32_t seedValue;
        std::array<std::array<int, W>, W> group{};
        std::array<std::array<int, W>, W> zone{};
        std::array<std::array<Tile, W>, W> T{};
        std::array<std::array<bool, W>, W> action{};
        std::array<std::array<bool, W>, W> keep{};
        std::array<std::array<std::array<int, 2>, W>, W> inst{}; // template instance per layer
        std::vector<std::vector<std::array<int, 3>>> instances; // x, y, layer
        std::vector<std::array<int, 3>> soundList;
        struct Seed
        {
            double x, y;
            int type; // 0 player, 1 neutral, 2 centre
        };
        std::vector<Seed> seeds;
        std::vector<int> zoneGroup;
        struct CastleEntry
        {
            bool player;
            int x, y;
        };
        std::vector<CastleEntry> castles;
        std::vector<std::array<int, 3>> mines;
        std::vector<std::pair<int, int>> gaps;

        int ri( const int a, const int b )
        {
            return std::uniform_int_distribution<int>( a, b )( rnd );
        }
        double rf( const double a, const double b )
        {
            return std::uniform_real_distribution<double>( a, b )( rnd );
        }
        static bool inside( const int x, const int y, const int margin = 1 )
        {
            return x >= margin && y >= margin && x < W - margin && y < W - margin;
        }
        bool isFree( const int x, const int y ) const
        {
            const Tile & t = T[y][x];
            return t[2] == 0xff && t[4] == 0xff && t[8] == 0;
        }

        // ---- ground edge rule (terrain.py)
        int mask( const int x, const int y ) const
        {
            const int g = group[y][x];
            int m = 0;
            for ( int k = 0; k < 8; ++k ) {
                const int nx = std::clamp( x + offs[k][0], 0, W - 1 );
                const int ny = std::clamp( y + offs[k][1], 0, W - 1 );
                if ( group[ny][nx] != g ) {
                    m |= 1 << k;
                }
            }
            return m;
        }

        void smooth()
        {
            for ( int round = 0; round < 12; ++round ) {
                int changed = 0;
                for ( int y = 0; y < W; ++y ) {
                    for ( int x = 0; x < W; ++x ) {
                        if ( group[y][x] == 6 || d.shape[mask( x, y )] != 0xff ) {
                            continue;
                        }
                        int count[7] = {};
                        for ( const auto & o : offs ) {
                            ++count[group[std::clamp( y + o[1], 0, W - 1 )][std::clamp( x + o[0], 0, W - 1 )]];
                        }
                        group[y][x] = static_cast<int>( std::max_element( count, count + 7 ) - count );
                        ++changed;
                    }
                }
                if ( changed == 0 ) {
                    return;
                }
            }
        }

        // ---- 1. land and zones
        void land()
        {
            const double cx = W / 2.0;
            const double cy = W / 2.0;
            const double rad = W * 0.41;
            std::array<std::array<double, 3>, 4> bumps{};
            for ( auto & b : bumps ) {
                b = { rf( 0, 6.283 ), rf( 0.04, 0.09 ), static_cast<double>( ri( 2, 5 ) ) };
            }
            for ( int y = 0; y < W; ++y ) {
                for ( int x = 0; x < W; ++x ) {
                    const double a = std::atan2( y - cy, x - cx );
                    double lim = 1;
                    for ( const auto & b : bumps ) {
                        lim += b[1] * std::sin( b[2] * a + b[0] );
                    }
                    group[y][x] = ( std::hypot( x - cx, y - cy ) < rad * lim && inside( x, y, 3 ) ) ? 1 : 0;
                }
            }
            const double a0 = rf( 0, 6.283 );
            for ( int k = 0; k < 4; ++k ) {
                const double a = a0 + k * 1.5708;
                seeds.push_back( { cx + std::cos( a ) * rad * 0.62, cy + std::sin( a ) * rad * 0.62, 0 } );
            }
            for ( int k = 0; k < 4; ++k ) {
                const double a = a0 + ( k + 0.5 ) * 1.5708;
                seeds.push_back( { cx + std::cos( a ) * rad * 0.70, cy + std::sin( a ) * rad * 0.70, 1 } );
            }
            seeds.push_back( { cx, cy, 2 } );
            // Borders bend with smooth waves (tile noise would make saw-tooth edges).
            const double p1 = rf( 0, 6.3 ), f1 = rf( 0.15, 0.3 ), p2 = rf( 0, 6.3 ), f2 = rf( 0.15, 0.3 );
            const double p3 = rf( 0, 6.3 ), p4 = rf( 0, 6.3 );
            for ( int y = 0; y < W; ++y ) {
                for ( int x = 0; x < W; ++x ) {
                    if ( group[y][x] == 0 ) {
                        continue;
                    }
                    const double jx = x + 3.0 * std::sin( y * f1 + p1 ) + 1.5 * std::sin( y * f1 * 2.7 + p3 );
                    const double jy = y + 3.0 * std::sin( x * f2 + p2 ) + 1.5 * std::sin( x * f2 * 2.3 + p4 );
                    int best = 0;
                    double bd = 1e9;
                    for ( size_t i = 0; i < seeds.size(); ++i ) {
                        const double dd = ( seeds[i].x - jx ) * ( seeds[i].x - jx ) + ( seeds[i].y - jy ) * ( seeds[i].y - jy );
                        if ( dd < bd ) {
                            bd = dd;
                            best = static_cast<int>( i );
                        }
                    }
                    zone[y][x] = best;
                }
            }
            static const int playerGroups[7] = { 1, 1, 6, 6, 2, 3, 5 }; // grass, dirt more often
            for ( const Seed & s : seeds ) {
                zoneGroup.push_back( s.type == 0 ? playerGroups[ri( 0, 6 )] : ri( 1, 6 ) );
            }
            for ( int y = 0; y < W; ++y ) {
                for ( int x = 0; x < W; ++x ) {
                    if ( zone[y][x] >= 0 ) {
                        group[y][x] = zoneGroup[static_cast<size_t>( zone[y][x] )];
                    }
                }
            }
            smooth();
        }

        // ---- stamps (single objects)
        bool fits( const Stamp & s, const int x, const int y, const int z ) const
        {
            for ( const StampTile & t : s.tiles ) {
                const int tx = x + t.dx, ty = y + t.dy;
                if ( !inside( tx, ty, 2 ) || !isFree( tx, ty ) || keep[ty][tx] || group[ty][tx] != t.group || zone[ty][tx] != z ) {
                    return false;
                }
            }
            for ( const StampTile & t : s.tiles ) {
                for ( int ey = -1; ey <= 1; ++ey ) {
                    for ( int ex = -1; ex <= 1; ++ex ) {
                        if ( action[y + t.dy + ey][x + t.dx + ex] ) {
                            return false;
                        }
                    }
                }
            }
            return group[y + 1][x] != 0 && isFree( x, y + 1 );
        }

        void put( const Stamp & s, const int x, const int y )
        {
            for ( const StampTile & t : s.tiles ) {
                T[y + t.dy][x + t.dx] = t.b;
                action[y + t.dy][x + t.dx] = true;
            }
            for ( const auto & snd : s.sounds ) {
                soundList.push_back( { x + snd[0], y + snd[1], snd[2] } );
            }
            keep[y + 1][x] = true;
        }

        std::vector<const Stamp *> matching( const List list, const int z, const int record = -2 ) const
        {
            std::vector<const Stamp *> out;
            const int g = zoneGroup[static_cast<size_t>( z )];
            for ( const Stamp & s : d.lists[list] ) {
                if ( record != -2 && s.record != record ) {
                    continue;
                }
                if ( std::all_of( s.tiles.begin(), s.tiles.end(), [g]( const StampTile & t ) { return t.group == g; } ) ) {
                    out.push_back( &s );
                }
            }
            return out;
        }

        // Returns the entrance or {-1, -1}.
        std::pair<int, int> place( const std::vector<const Stamp *> & pool, const int z, const Stamp ** chosen, const int nearX = -1, const int nearY = -1,
                                   const int spread = 6, const int tries = 500 )
        {
            if ( pool.empty() ) {
                return { -1, -1 };
            }
            std::vector<std::pair<int, int>> zt;
            for ( int y = 0; y < W; ++y ) {
                for ( int x = 0; x < W; ++x ) {
                    if ( zone[y][x] == z ) {
                        zt.emplace_back( x, y );
                    }
                }
            }
            if ( zt.empty() ) {
                return { -1, -1 };
            }
            for ( int i = 0; i < tries; ++i ) {
                const Stamp & s = *pool[static_cast<size_t>( ri( 0, static_cast<int>( pool.size() ) - 1 ) )];
                int x, y;
                if ( nearX >= 0 ) {
                    x = nearX + ri( -spread, spread );
                    y = nearY + ri( -spread, spread );
                }
                else {
                    const auto & p = zt[static_cast<size_t>( ri( 0, static_cast<int>( zt.size() ) - 1 ) )];
                    x = p.first;
                    y = p.second;
                }
                if ( inside( x, y, 2 ) && fits( s, x, y, z ) ) {
                    put( s, x, y );
                    if ( chosen ) {
                        *chosen = &s;
                    }
                    return { x, y };
                }
            }
            return { -1, -1 };
        }

        bool monster( const int x, const int y, const int level )
        {
            if ( !inside( x, y, 2 ) || group[y][x] == 0 || !isFree( x, y ) ) {
                return false;
            }
            Tile t = emptyTile();
            t[1] = 0x0c;
            t[2] = static_cast<uint8_t>( monsterFrame[level] );
            t[8] = static_cast<uint8_t>( monsterMark[level] );
            T[y][x] = t;
            action[y][x] = true;
            return true;
        }

        void guard( const int ex, const int ey, const int level )
        {
            static const int spots[5][2] = { { 0, 2 }, { -1, 2 }, { 1, 2 }, { -1, 1 }, { 1, 1 } };
            for ( const auto & s : spots ) {
                if ( monster( ex + s[0], ey + s[1], level ) ) {
                    return;
                }
            }
        }

        // ---- 2. castles, mines, sites
        void towns()
        {
            for ( size_t i = 0; i < seeds.size(); ++i ) {
                const Seed & s = seeds[i];
                if ( s.type == 2 || ( s.type == 1 && ri( 0, 1 ) == 0 ) ) {
                    continue;
                }
                const List list = ( s.type == 0 ) ? CASTLE : TOWN;
                std::vector<const Stamp *> full, pool;
                for ( const Stamp & st : d.lists[list] ) {
                    if ( st.tiles.size() == 12 ) {
                        full.push_back( &st );
                    }
                }
                const int g = zoneGroup[i];
                for ( const Stamp * st : full ) {
                    if ( std::all_of( st->tiles.begin(), st->tiles.end(), [g]( const StampTile & t ) { return t.group == g; } ) ) {
                        pool.push_back( st );
                    }
                }
                if ( pool.empty() ) {
                    pool = full;
                    // castle pictures of another ground: put them on this zone's ground (only the castle tiles change)
                }
                const auto e = placeAnyGround( pool, static_cast<int>( i ), static_cast<int>( s.x ), static_cast<int>( s.y ) );
                if ( e.first < 0 ) {
                    continue;
                }
                for ( int x = e.first - 3; x < e.first + 3; ++x ) {
                    for ( int y = e.second + 1; y < e.second + 3; ++y ) {
                        if ( inside( x, y ) ) {
                            keep[y][x] = true; // the road out of the gate
                        }
                    }
                }
                castles.push_back( { s.type == 0, e.first, e.second } );
            }
        }

        // Castles: the zone's ground under all tiles is enough (castle pictures do not change with the ground).
        std::pair<int, int> placeAnyGround( const std::vector<const Stamp *> & pool, const int z, const int nx, const int ny )
        {
            for ( int i = 0; i < 2000 && !pool.empty(); ++i ) {
                const Stamp & s = *pool[static_cast<size_t>( ri( 0, static_cast<int>( pool.size() ) - 1 ) )];
                const int spread = 4 + i / 200;
                const int x = nx + ri( -spread, spread ), y = ny + ri( -spread, spread );
                if ( !inside( x, y, 3 ) ) {
                    continue;
                }
                bool ok = group[y + 1][x] != 0 && isFree( x, y + 1 );
                for ( const StampTile & t : s.tiles ) {
                    const int tx = x + t.dx, ty = y + t.dy;
                    if ( !inside( tx, ty, 2 ) || !isFree( tx, ty ) || keep[ty][tx] || zone[ty][tx] != z || group[ty][tx] == 0 ) {
                        ok = false;
                        break;
                    }
                }
                if ( ok ) {
                    put( s, x, y );
                    return { x, y };
                }
            }
            return { -1, -1 };
        }

        void objects()
        {
            for ( size_t i = 0; i < seeds.size(); ++i ) {
                const int z = static_cast<int>( i );
                const int type = seeds[i].type;
                const int nx = type == 0 ? static_cast<int>( seeds[i].x ) : -1;
                const int ny = type == 0 ? static_cast<int>( seeds[i].y ) : -1;
                const int mineCount = type == 0 ? 3 : 2;
                for ( int k = 0; k < mineCount; ++k ) {
                    std::vector<const Stamp *> pool;
                    if ( type == 0 && k < 2 ) {
                        pool = matching( MINE, z, k == 0 ? 0 : 2 ); // wood and ore near the castle (list kinds 0 / 2)
                    }
                    if ( pool.empty() ) {
                        pool = matching( MINE, z );
                    }
                    const Stamp * chosen = nullptr;
                    const auto e = place( pool, z, &chosen, nx, ny, 9 );
                    if ( e.first >= 0 && chosen ) {
                        mines.push_back( { e.first, e.second, chosen->record } );
                        if ( type != 0 ) {
                            guard( e.first, e.second, 2 );
                        }
                    }
                }
                const int treasures = type == 0 ? 5 : 7;
                for ( int k = 0; k < treasures; ++k ) {
                    const auto e = place( matching( TREASURE, z ), z, nullptr );
                    if ( e.first >= 0 && ( type != 0 || ri( 0, 9 ) < 3 ) ) {
                        guard( e.first, e.second, type == 0 ? 1 : ri( 2, 3 ) );
                    }
                }
                const int sites = type == 0 ? 2 : ( type == 1 ? 3 : 4 );
                for ( int k = 0; k < sites; ++k ) {
                    const auto e = place( matching( SITE, z ), z, nullptr );
                    if ( e.first >= 0 && ri( 0, 9 ) < 6 ) {
                        guard( e.first, e.second, type == 2 ? ri( 3, 4 ) : 2 );
                    }
                }
            }
        }

        // ---- 3. mountains and trees
        bool putTemplate( const Template & t, const int x, const int y, const bool gapOk = false )
        {
            for ( const Part & q : t.parts ) {
                const int tx = x + q.dx, ty = y + q.dy;
                if ( !inside( tx, ty ) || !( t.groups & ( 1 << group[ty][tx] ) ) || action[ty][tx] || ( keep[ty][tx] && !gapOk )
                     || T[ty][tx][static_cast<size_t>( 2 + 2 * q.layer )] != 0xff ) {
                    return false;
                }
            }
            const int id = static_cast<int>( instances.size() );
            instances.emplace_back();
            for ( const Part & q : t.parts ) {
                Tile & tt = T[y + q.dy][x + q.dx];
                tt[static_cast<size_t>( 1 + 2 * q.layer )] = static_cast<uint8_t>( ( tt[static_cast<size_t>( 1 + 2 * q.layer )] & 0xf0 ) | q.set );
                tt[static_cast<size_t>( 2 + 2 * q.layer )] = static_cast<uint8_t>( q.frame );
                tt[6] = static_cast<uint8_t>( tt[6] | ( q.bits & 0x8c ) );
                if ( q.bits & 0x01 ) {
                    tt[7] = static_cast<uint8_t>( tt[7] | 0x80 );
                }
                if ( q.kind && tt[8] == 0 ) {
                    tt[8] = static_cast<uint8_t>( q.kind );
                }
                inst[y + q.dy][x + q.dx][static_cast<size_t>( q.layer )] = id;
                instances.back().push_back( { x + q.dx, y + q.dy, q.layer } );
            }
            return true;
        }

        void removeInstance( const int id )
        {
            for ( const auto & c : instances[static_cast<size_t>( id )] ) {
                Tile & t = T[c[1]][c[0]];
                t[static_cast<size_t>( 1 + 2 * c[2] )] &= 0xf0;
                t[static_cast<size_t>( 2 + 2 * c[2] )] = 0xff;
                inst[c[1]][c[0]][static_cast<size_t>( c[2] )] = -1;
                if ( t[2] == 0xff && t[4] == 0xff ) {
                    t = emptyTile();
                }
            }
            instances[static_cast<size_t>( id )].clear();
        }

        std::vector<const Template *> obstaclePool( const int z, const bool blocking ) const
        {
            const int g = zoneGroup[static_cast<size_t>( z )];
            std::vector<const Template *> out, any;
            for ( const Template & t : d.templates ) {
                if ( ( t.groups & ( 1 << g ) ) && t.blocking == blocking ) {
                    any.push_back( &t );
                    if ( t.set == 8 || t.set == 9 ) {
                        out.push_back( &t );
                    }
                }
            }
            return out.empty() ? any : out;
        }

        void borders()
        {
            std::vector<std::vector<std::pair<int, int>>> pairs( seeds.size() * seeds.size() );
            for ( int y = 1; y < W - 1; ++y ) {
                for ( int x = 1; x < W - 1; ++x ) {
                    const int a = zone[y][x];
                    if ( a < 0 ) {
                        continue;
                    }
                    for ( const auto & o : { std::pair<int, int>{ 1, 0 }, std::pair<int, int>{ 0, 1 } } ) {
                        const int b = zone[y + o.second][x + o.first];
                        if ( b >= 0 && b != a ) {
                            pairs[static_cast<size_t>( std::min( a, b ) ) * seeds.size() + static_cast<size_t>( std::max( a, b ) )].emplace_back( x, y );
                        }
                    }
                }
            }
            for ( auto & tiles : pairs ) {
                if ( tiles.empty() ) {
                    continue;
                }
                std::sort( tiles.begin(), tiles.end() );
                const int n = static_cast<int>( tiles.size() );
                const int gapCount = n < 30 ? 1 : 2;
                for ( int k = 0; k < gapCount; ++k ) {
                    const auto & g = tiles[static_cast<size_t>( n * ( 2 * k + 1 ) / ( 2 * gapCount ) )];
                    for ( int ey = -1; ey <= 1; ++ey ) {
                        for ( int ex = -1; ex <= 1; ++ex ) {
                            if ( inside( g.first + ex, g.second + ey ) ) {
                                keep[g.second + ey][g.first + ex] = true;
                            }
                        }
                    }
                    gaps.push_back( g );
                }
                // A thick wall: every border tile and its neighbours get tries with blocking mountains / trees.
                for ( const auto & c : tiles ) {
                    for ( int k = 0; k < 6; ++k ) {
                        const int x = c.first + ri( -1, 1 ), y = c.second + ri( -1, 1 );
                        if ( !inside( x, y ) || zone[y][x] < 0 ) {
                            continue;
                        }
                        const auto pool = obstaclePool( zone[y][x], true );
                        if ( pool.empty() ) {
                            break;
                        }
                        const Template & t = *pool[static_cast<size_t>( ri( 0, static_cast<int>( pool.size() ) - 1 ) )];
                        const Part & q = t.parts[static_cast<size_t>( ri( 0, static_cast<int>( t.parts.size() ) - 1 ) )];
                        putTemplate( t, x - q.dx, y - q.dy );
                    }
                }
            }
        }

        void scatter( const double share )
        {
            std::vector<std::pair<int, int>> landTiles;
            for ( int y = 0; y < W; ++y ) {
                for ( int x = 0; x < W; ++x ) {
                    if ( group[y][x] ) {
                        landTiles.emplace_back( x, y );
                    }
                }
            }
            const int target = static_cast<int>( static_cast<double>( landTiles.size() ) * share );
            int n = 0;
            for ( int i = 0; i < 8000 && n < target; ++i ) {
                const auto & p = landTiles[static_cast<size_t>( ri( 0, static_cast<int>( landTiles.size() ) - 1 ) )];
                const int z = zone[p.second][p.first];
                if ( z < 0 ) {
                    continue;
                }
                const auto pool = obstaclePool( z, ri( 0, 9 ) < 7 );
                if ( pool.empty() ) {
                    continue;
                }
                const Template & t = *pool[static_cast<size_t>( ri( 0, static_cast<int>( pool.size() ) - 1 ) )];
                if ( putTemplate( t, p.first, p.second ) ) {
                    n += static_cast<int>( t.parts.size() );
                }
            }
        }

        // ---- 4. every entrance reachable on foot from player 1's castle front: clear whole mountains / trees on the way
        bool walkable( const int x, const int y ) const
        {
            if ( group[y][x] == 0 ) {
                return false;
            }
            const Tile & t = T[y][x];
            if ( t[8] & 0x80 ) {
                return true;
            }
            return !( t[7] & 0x80 ) && !action[y][x];
        }

        std::vector<uint8_t> reach( const int sx, const int sy ) const
        {
            std::vector<uint8_t> seen( W * W, 0 );
            std::deque<std::pair<int, int>> q;
            seen[sy * W + sx] = 1;
            q.emplace_back( sx, sy );
            while ( !q.empty() ) {
                const auto [x, y] = q.front();
                q.pop_front();
                if ( ( T[y][x][8] & 0x80 ) && !( x == sx && y == sy ) ) {
                    continue; // visiting does not lead on
                }
                for ( const auto & o : offs ) {
                    const int nx = x + o[0], ny = y + o[1];
                    if ( inside( nx, ny ) && !seen[ny * W + nx] && ( walkable( nx, ny ) || T[ny][nx][1] == 0x0c ) ) {
                        seen[ny * W + nx] = 1;
                        // a guard monster is fought, then passed
                        q.emplace_back( nx, ny );
                    }
                }
            }
            return seen;
        }

        void openPaths( const int sx, const int sy )
        {
            for ( int round = 0; round < 40; ++round ) {
                const auto seen = reach( sx, sy );
                std::vector<std::pair<int, int>> lost;
                for ( int y = 0; y < W; ++y ) {
                    for ( int x = 0; x < W; ++x ) {
                        if ( ( T[y][x][8] & 0x80 ) && !seen[y * W + x] ) {
                            lost.emplace_back( x, y );
                        }
                    }
                }
                if ( lost.empty() ) {
                    return;
                }
                const auto [ex, ey] = lost.front();
                const int fx = ex, fy = ey + 1;
                int bx = sx, by = sy, bd = 1 << 30;
                for ( int y = 0; y < W; ++y ) {
                    for ( int x = 0; x < W; ++x ) {
                        const int dd = ( x - fx ) * ( x - fx ) + ( y - fy ) * ( y - fy );
                        if ( seen[y * W + x] && dd < bd && !( T[y][x][8] & 0x80 ) ) {
                            bd = dd;
                            bx = x;
                            by = y;
                        }
                    }
                }
                const int steps = std::max( { std::abs( bx - fx ), std::abs( by - fy ), 1 } );
                for ( int k = 0; k <= steps; ++k ) {
                    const int x = static_cast<int>( std::lround( fx + ( bx - fx ) * static_cast<double>( k ) / steps ) );
                    const int y = static_cast<int>( std::lround( fy + ( by - fy ) * static_cast<double>( k ) / steps ) );
                    for ( int layer = 0; layer < 2; ++layer ) {
                        const int id = inst[y][x][static_cast<size_t>( layer )];
                        if ( id >= 0 ) {
                            removeInstance( id );
                        }
                    }
                    if ( !action[y][x] ) {
                        T[y][x][7] = static_cast<uint8_t>( T[y][x][7] & 0x7f );
                    }
                }
            }
        }

        // ---- 5. the .MAP file (h1_map_convert.cpp readH1 layout, note v244)
        bool write( const std::string & path, std::string & err )
        {
            std::vector<uint8_t> out( d.head );
            const std::string name = "Random " + std::to_string( seedValue % 100000 );
            std::fill( out.begin() + 4, out.begin() + 4 + 150, 0 );
            std::copy( name.begin(), name.end(), out.begin() + 4 );
            static const char desc[] = "A random map made from pieces of the original maps.";
            std::fill( out.begin() + 0x9a, out.begin() + 0x9a + 121, 0 );
            std::copy( desc, desc + sizeof( desc ) - 1, out.begin() + 0x9a );
            auto u16 = [&out]( const int v ) {
                out.push_back( static_cast<uint8_t>( v & 0xff ) );
                out.push_back( static_cast<uint8_t>( v >> 8 ) );
            };
            u16( 0x1112 );
            u16( W );
            u16( W );
            std::uniform_int_distribution<int> any( 0, 1 << 20 );
            for ( int x = 0; x < W; ++x ) {
                for ( int y = 0; y < W; ++y ) {
                    Tile t = T[y][x];
                    const int g = group[y][x];
                    int shape = 0, flip = 0;
                    if ( g != 6 ) {
                        const int m = mask( x, y );
                        if ( d.shape[m] != 0xff ) {
                            shape = d.shape[m];
                            flip = d.flip[m];
                        }
                    }
                    const uint16_t * w = d.variant[g][shape];
                    const int total = w[0] + w[1] + w[2] + w[3];
                    int v = 0;
                    if ( total > 0 ) {
                        int r = any( rnd ) % total;
                        while ( v < 3 && r >= w[v] ) {
                            r -= w[v];
                            ++v;
                        }
                    }
                    t[0] = static_cast<uint8_t>( g * 20 + shape * 4 + v );
                    t[6] = static_cast<uint8_t>( ( t[6] & 0xfc ) | flip );
                    // Shore mark: free land touching water (note v39: the editor marks all of them 0x1f).
                    if ( g != 0 && t[8] == 0 && t[2] == 0xff && t[4] == 0xff && inside( x, y ) ) {
                        bool water = false;
                        for ( const auto & o : offs ) {
                            water = water || group[y + o[1]][x + o[0]] == 0;
                        }
                        if ( water ) {
                            t[8] = 0x1f;
                        }
                    }
                    out.insert( out.end(), t.begin(), t.end() );
                }
            }
            // Castle list: players first (slots 0..3 = players 0..3 when there are no setup records), 0x84 = random race castle, 0x04 =
            // random race town (h1_map_convert h1RaceIndex); empty slots x = 0xff.
            std::vector<CastleEntry> ordered;
            for ( const auto & c : castles ) {
                if ( c.player ) {
                    ordered.push_back( c );
                }
            }
            for ( const auto & c : castles ) {
                if ( !c.player ) {
                    ordered.push_back( c );
                }
            }
            for ( size_t i = 0; i < 36; ++i ) {
                if ( i < ordered.size() ) {
                    out.insert( out.end(), { static_cast<uint8_t>( ordered[i].x ), static_cast<uint8_t>( ordered[i].y ), static_cast<uint8_t>( ordered[i].player ? 0x84 : 0x04 ) } );
                }
                else {
                    out.insert( out.end(), { 0xff, 0xff, 0 } );
                }
            }
            for ( size_t i = 0; i < 36; ++i ) {
                if ( i < mines.size() ) {
                    out.insert( out.end(), { static_cast<uint8_t>( mines[i][0] ), static_cast<uint8_t>( mines[i][1] ), static_cast<uint8_t>( mines[i][2] ) } );
                }
                else {
                    out.insert( out.end(), { 0xff, 0xff, 0xff } );
                }
            }
            out.insert( out.end(), d.tail38.begin(), d.tail38.end() );
            std::vector<uint8_t> sounds( W * W, 0xff );
            for ( const auto & s : soundList ) {
                if ( inside( s[0], s[1], 0 ) ) {
                    sounds[static_cast<size_t>( s[0] * W + s[1] )] = static_cast<uint8_t>( s[2] );
                }
            }
            out.insert( out.end(), sounds.begin(), sounds.end() );
            out.insert( out.end(), { 1, 0, 0, 0 } ); // record count 1: only the unused record 0 (no signs, no castle setups)
            std::ofstream f( path, std::ios::binary );
            if ( !f ) {
                err = "cannot write " + path;
                return false;
            }
            f.write( reinterpret_cast<const char *>( out.data() ), static_cast<std::streamsize>( out.size() ) );
            return static_cast<bool>( f );
        }
    };
}

namespace H1
{
    bool makeRandomMap( const std::string & mapPath, const uint32_t seed, std::string & err )
    {
        static Data data;
        static bool loaded = false;
        if ( !loaded ) {
            if ( !loadData( data, err ) ) {
                return false;
            }
            loaded = true;
        }
        // A few seeds in turn when the castles do not fit.
        for ( uint32_t k = 0; k < 5; ++k ) {
            Maker maker( data, seed + k * 7919 );
            if ( maker.make( mapPath, err ) ) {
                return true;
            }
        }
        return false;
    }

    bool prepareRandomMap( const uint32_t seed )
    {
        std::string err;
        const std::string mapPath = std::string( "h1/" ) + randomMapBase + ".MAP";
        if ( !makeRandomMap( mapPath, seed, err ) ) {
            std::fprintf( stderr, "random map: %s\n", err.c_str() );
            return false;
        }
        const std::string out = std::string( "maps/" ) + randomMapBase + ".fh2m";
        return convertMap( mapPath.c_str(), out.c_str(), "h1/RNDM1234.log" ) == 0;
    }
}
