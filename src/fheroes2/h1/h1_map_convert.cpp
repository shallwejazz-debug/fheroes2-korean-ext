// HoMM1 .MAP -> fheroes2 .fh2m converter (homm1-remaster project, prototype A).
// Original HoMM1 data layout: docs/analysis notes v11, v39, v41 of homm1-remaster.
// Object mapping from HoMM1 to HoMM2 equivalents is a design choice of this converter (see port/fheroes2/README.md).

#include "h1_map_convert.h"
#include "h1_rules.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "artifact.h"
#include "color.h"
#include "ground.h"
#include "map_format_helper.h"
#include "map_format_info.h"
#include "map_object_info.h"
#include "map_random_generator_helper.h"
#include "maps.h"
#include "maps_fileinfo.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "settings.h"
#include "ui_map_object.h"
#include "world.h"
#include "world_object_uid.h"

namespace
{
    using Maps::ObjectGroup;

    constexpr int W = 72;

    struct H1Tile
    {
        uint8_t b[10];
    };

    struct H1Map
    {
        std::string name;
        uint8_t headerDifficulty = 0;
        uint8_t headerSize = 0;
        std::string headerDescription; // header offset 0x9a
        std::vector<H1Tile> tiles; // index = y * 72 + x (converted from file's x-major order)
        struct Castle
        {
            int x, y, type;
        };
        std::vector<Castle> castles; // 36 slots, x < 0 means empty
        struct Mine
        {
            int x, y, kind;
        };
        std::vector<Mine> mines;
        std::vector<std::vector<uint8_t>> records; // records[0] unused
        std::vector<uint8_t> loopSounds; // index = y * 72 + x: loop%04d.82m number of the tile, 0xff none (world +0x145d6, note v109)
    };

    bool readH1( const char * path, H1Map & m, std::string & err )
    {
        std::ifstream f( path, std::ios::binary );
        if ( !f ) {
            err = "cannot open";
            return false;
        }
        std::vector<uint8_t> d( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
        size_t p = 0;
        auto u16 = [&]() {
            const uint16_t v = static_cast<uint16_t>( d[p] | ( d[p + 1] << 8 ) );
            p += 2;
            return v;
        };
        auto u32 = [&]() {
            const uint32_t v = d[p] | ( d[p + 1] << 8 ) | ( d[p + 2] << 16 ) | ( static_cast<uint32_t>( d[p + 3] ) << 24 );
            p += 4;
            return v;
        };
        if ( d.size() < 0x55A + 0xCA80 ) {
            err = "file too small";
            return false;
        }
        uint16_t v = u16();
        if ( v == 1000 ) {
            // Header bytes 2 and 3: map difficulty and size, as the scenario list reads them (FUN_00467e1c, note v53).
            m.headerDifficulty = d[2];
            m.headerSize = d[3];
            // Offset 0x9a: description (the scenario list copies it, FUN_00467e1c / FUN_0046a097, note v54).
            m.headerDescription = std::string( reinterpret_cast<const char *>( &d[0x9a] ), strnlen( reinterpret_cast<const char *>( &d[0x9a] ), 0x554 - 0x9a ) );
            m.name = std::string( reinterpret_cast<const char *>( &d[4] ) );
            p += 0x552;
            v = u16();
        }
        const uint16_t version = v;
        u16();
        u16(); // width, height (ignored by the game)
        m.tiles.resize( W * W );
        for ( int x = 0; x < W; ++x ) {
            for ( int y = 0; y < W; ++y ) {
                std::memcpy( m.tiles[y * W + x].b, &d[p], 10 );
                p += 10;
            }
        }
        for ( int i = 0; i < 36; ++i ) {
            const int8_t x = static_cast<int8_t>( d[p] );
            m.castles.push_back( { x, static_cast<int8_t>( d[p + 1] ), d[p + 2] } );
            p += 3;
        }
        for ( int i = 0; i < 36; ++i ) {
            const int8_t x = static_cast<int8_t>( d[p] );
            m.mines.push_back( { x, static_cast<int8_t>( d[p + 1] ), d[p + 2] } );
            p += 3;
        }
        // 0x25 bytes, 1 byte, then the 72 x 72 loop sound table (FUN_00416696 reads it to world +0x145d6, x-major like the tiles).
        p += 37 + 1;
        m.loopSounds.assign( W * W, 0xff );
        if ( p + W * W <= d.size() ) {
            for ( int x = 0; x < W; ++x ) {
                for ( int y = 0; y < W; ++y ) {
                    m.loopSounds[y * W + x] = d[p + static_cast<size_t>( x ) * W + y];
                }
            }
        }
        p += W * W;
        m.records.emplace_back();
        if ( version >= 0x458 && p + 4 <= d.size() ) {
            const uint32_t n = u32();
            for ( uint32_t i = 1; i < n && p + 4 <= d.size(); ++i ) {
                const uint32_t sz = u32();
                if ( p + sz > d.size() ) {
                    break;
                }
                m.records.emplace_back( d.begin() + static_cast<std::ptrdiff_t>( p ), d.begin() + static_cast<std::ptrdiff_t>( p + sz ) );
                p += sz;
            }
        }
        return true;
    }

    // HoMM1 terrain group (tile index / 20, notes v13/v32) -> fheroes2 ground.
    int h1Ground( uint8_t tileIndex )
    {
        switch ( tileIndex / 20 ) {
        case 0:
            return Maps::Ground::WATER;
        case 1:
            return Maps::Ground::GRASS;
        case 2:
            return Maps::Ground::SNOW;
        case 3:
            return Maps::Ground::SWAMP;
        case 4:
            return Maps::Ground::LAVA;
        case 5:
            return Maps::Ground::DESERT;
        default:
            return Maps::Ground::DIRT;
        }
    }

    // HoMM1 creature id (notes v12) -> fheroes2 monster id.
    int h1Monster( int c )
    {
        static const int table[28] = { Monster::PEASANT,  Monster::ARCHER,   Monster::PIKEMAN,  Monster::SWORDSMAN, Monster::CAVALRY,      Monster::PALADIN,
                                       Monster::GOBLIN,   Monster::ORC,      Monster::WOLF,     Monster::OGRE,      Monster::TROLL,        Monster::CYCLOPS,
                                       Monster::SPRITE,   Monster::DWARF,    Monster::ELF,      Monster::DRUID,     Monster::UNICORN,      Monster::PHOENIX,
                                       Monster::CENTAUR,  Monster::GARGOYLE, Monster::GRIFFIN,  Monster::MINOTAUR,  Monster::HYDRA,        Monster::GREEN_DRAGON,
                                       Monster::ROGUE,    Monster::NOMAD,    Monster::GHOST,    Monster::GENIE };
        return ( c >= 0 && c < 28 ) ? table[c] : Monster::UNKNOWN;
    }

    // HoMM1 faction (notes v35: 0 knight, 1 sorceress, 2 barbarian, 3 warlock) -> fheroes2 race index.
    int h1RaceIndex( int faction )
    {
        static const int t[4] = { 0, 2, 1, 3 };
        return ( faction >= 0 && faction < 4 ) ? t[faction] : -1;
    }


    int findIndexByMetadata( ObjectGroup group, uint32_t md0 )
    {
        const auto & objs = Maps::getObjectsByGroup( group );
        for ( size_t i = 0; i < objs.size(); ++i ) {
            if ( !objs[i].empty() && objs[i].metadata[0] == md0 ) {
                return static_cast<int>( i );
            }
        }
        return -1;
    }

    int findIndexByType( ObjectGroup group, MP2::MapObjectType type, uint32_t md0 = 0xFFFFFFFF )
    {
        const auto & objs = Maps::getObjectsByGroup( group );
        for ( size_t i = 0; i < objs.size(); ++i ) {
            if ( !objs[i].empty() && objs[i].objectType == type && ( md0 == 0xFFFFFFFF || objs[i].metadata[0] == md0 ) ) {
                return static_cast<int>( i );
            }
        }
        return -1;
    }

    int artifactObjectIndex( int h1Art )
    {
        if ( h1Art < 0 || h1Art >= 38 ) {
            return -1;
        }
        for ( int id = 1; id < Artifact::ARTIFACT_COUNT; ++id ) {
            if ( std::strcmp( Artifact( id ).GetName(), H1::artifactName( h1Art ) ) == 0 ) {
                return findIndexByMetadata( ObjectGroup::ADVENTURE_ARTIFACTS, static_cast<uint32_t>( id ) );
            }
        }
        return -1;
    }

    // Which grounds an object ICN belongs to (for landscape filling).
    bool icnFitsGround( MP2::ObjectIcnType icn, int ground )
    {
        using namespace MP2;
        switch ( icn ) {
        case OBJ_ICN_TYPE_MTNSNOW:
        case OBJ_ICN_TYPE_TRESNOW:
        case OBJ_ICN_TYPE_OBJNSNOW:
            return ground == Maps::Ground::SNOW;
        case OBJ_ICN_TYPE_MTNSWMP:
        case OBJ_ICN_TYPE_TREJNGL:
        case OBJ_ICN_TYPE_OBJNSWMP:
            return ground == Maps::Ground::SWAMP;
        case OBJ_ICN_TYPE_MTNLAVA:
        case OBJ_ICN_TYPE_TREEVIL:
        case OBJ_ICN_TYPE_OBJNLAVA:
        case OBJ_ICN_TYPE_OBJNLAV2:
        case OBJ_ICN_TYPE_OBJNLAV3:
            return ground == Maps::Ground::LAVA;
        case OBJ_ICN_TYPE_MTNDSRT:
        case OBJ_ICN_TYPE_OBJNDSRT:
            return ground == Maps::Ground::DESERT;
        case OBJ_ICN_TYPE_MTNDIRT:
        case OBJ_ICN_TYPE_TREFALL:
        case OBJ_ICN_TYPE_OBJNDIRT:
            return ground == Maps::Ground::DIRT;
        case OBJ_ICN_TYPE_MTNGRAS:
        case OBJ_ICN_TYPE_TREDECI:
        case OBJ_ICN_TYPE_TREFIR:
        case OBJ_ICN_TYPE_OBJNGRAS:
        case OBJ_ICN_TYPE_OBJNGRA2:
            return ground == Maps::Ground::GRASS;
        case OBJ_ICN_TYPE_MTNMULT:
        case OBJ_ICN_TYPE_OBJNMULT:
        case OBJ_ICN_TYPE_OBJNMUL2:
            return ground != Maps::Ground::WATER;
        case OBJ_ICN_TYPE_OBJNWATR:
        case OBJ_ICN_TYPE_OBJNWAT2:
            return ground == Maps::Ground::WATER;
        default:
            return false;
        }
    }

    struct Placement
    {
        ObjectGroup group;
        int index;
        int monsterCount = 0;
    };

    // Action object mapping: HoMM1 object type (tile byte 8 & 0x7f, active) -> HoMM2 object.
    bool mapActionObject( const H1Map & m, int tileId, const H1Tile & t, Placement & out, std::string & note )
    {
        const int type = t.b[8] & 0x7f;
        auto byType = [&]( ObjectGroup g, MP2::MapObjectType mt ) {
            out.group = g;
            out.index = findIndexByType( g, mt );
            return out.index >= 0;
        };
        // 0xbe in a map file is a random resource (FUN_0041c1cc turns every 0xbe into resource 0x9d at new game; all 1980 of them in the
        // 46 maps lie on land with rsrc32 frame 0x54), not a boat (kind 0x3e, which only the game writes when a hero leaves a boat).
        // Until v191 the kind switch below caught it as a boat (note v192).
        if ( t.b[8] == 0xbe ) {
            return byType( ObjectGroup::ADVENTURE_TREASURES, MP2::OBJ_RANDOM_RESOURCE );
        }
        switch ( type ) {
        case 0x01:
            return byType( ObjectGroup::ADVENTURE_MINES, MP2::OBJ_ALCHEMIST_LAB );
        case 0x02:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_SIGN );
        case 0x03:
            return byType( ObjectGroup::ADVENTURE_WATER, MP2::OBJ_BUOY );
        case 0x04:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_SKELETON );
        case 0x05:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_DAEMON_CAVE );
        case 0x06:
            return byType( ObjectGroup::ADVENTURE_TREASURES, MP2::OBJ_TREASURE_CHEST );
        case 0x07:
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_FAERIE_RING );
        case 0x08:
            return byType( ObjectGroup::ADVENTURE_TREASURES, MP2::OBJ_CAMPFIRE );
        case 0x09:
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_FOUNTAIN );
        case 0x0a:
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_GAZEBO );
        case 0x0b:
            return byType( ObjectGroup::ADVENTURE_TREASURES, MP2::OBJ_GENIE_LAMP );
        case 0x0c:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_GRAVEYARD );
        case 0x0d:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_GOBLIN_HUT );
        case 0x0e:
        case 0x11:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_PEASANT_HUT );
        case 0x0f:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_ARCHER_HOUSE );
        case 0x10:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_DWARF_COTTAGE );
        case 0x16:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_DRAGON_CITY );
        case 0x17:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_LIGHTHOUSE );
        case 0x18:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_WATER_WHEEL );
        case 0x19: {
            // Mine: resource from the mine list (record index = byte 9 before game start is not set; find by position).
            int kind = -1;
            const int x = tileId % W, y = tileId / W;
            // The record at this very tile first (FUN_0041208d matches x and y exactly, note v199); a mine next to another one (W95D1234
            // (57, 8) gold beside (58, 9) ore) took the neighbour's kind before.
            for ( const auto & mi : m.mines ) {
                if ( mi.x == x && mi.y == y ) {
                    kind = mi.kind;
                }
            }
            if ( kind < 0 ) {
                for ( const auto & mi : m.mines ) {
                    if ( mi.x >= 0 && std::abs( mi.x - x ) <= 1 && std::abs( mi.y - y ) <= 1 ) {
                        kind = mi.kind;
                    }
                }
            }
            static const uint32_t resBit[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
            out.group = ObjectGroup::ADVENTURE_MINES;
            out.index = ( kind >= 2 && kind <= 6 ) ? findIndexByType( out.group, MP2::OBJ_MINE, resBit[kind] ) : -1;
            if ( out.index < 0 ) {
                note = "mine kind " + std::to_string( kind );
            }
            return out.index >= 0;
        }
        case 0x1a: {
            out.group = ObjectGroup::MONSTERS;
            const int mon = h1Monster( t.b[2] );
            out.index = mon - 1;
            out.monsterCount = t.b[9] & 0x7f;
            return mon != Monster::UNKNOWN;
        }
        case 0x1b:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_OBELISK );
        case 0x1c:
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_OASIS );
        case 0x1d: {
            // Resource kind = sprite - 0x3d (FUN_00415a33): 0 wood, 1 mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems, 6 gold - rsrc32.icn
            // frames 0x40 / 0x41 are the sulfur / crystal pictures and letters.icn reads W M O S C J G (note v77 corrects v34).
            static const int res[7] = { 0, 1, 2, 3, 4, 5, 6 };
            const int s = t.b[2] - 0x3d;
            out.group = ObjectGroup::ADVENTURE_TREASURES;
            out.index = ( s >= 0 && s < 7 ) ? res[s] : -1;
            return out.index >= 0;
        }
        case 0x20:
            return byType( ObjectGroup::ADVENTURE_MINES, MP2::OBJ_SAWMILL );
        case 0x22:
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_SHRINE_FIRST_CIRCLE );
        case 0x23:
            return byType( ObjectGroup::ADVENTURE_WATER, MP2::OBJ_SHIPWRECK );
        case 0x24:
            note = "Statue -> Temple (approximation)";
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_TEMPLE );
        case 0x27:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_DESERT_TENT );
        case 0x29:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_STONE_LITHS );
        case 0x2a:
            return byType( ObjectGroup::ADVENTURE_DWELLINGS, MP2::OBJ_WAGON_CAMP );
        case 0x2b:
            note = "Well -> Magic Well (approximation)";
            return byType( ObjectGroup::ADVENTURE_POWER_UPS, MP2::OBJ_MAGIC_WELL );
        case 0x2c:
            return byType( ObjectGroup::ADVENTURE_WATER, MP2::OBJ_WHIRLPOOL );
        case 0x2d:
            return byType( ObjectGroup::ADVENTURE_MISCELLANEOUS, MP2::OBJ_WINDMILL );
        case 0x30:
            out.group = ObjectGroup::ADVENTURE_ARTIFACTS;
            out.index = artifactObjectIndex( t.b[2] );
            return out.index >= 0;
        case 0x3e:
            return byType( ObjectGroup::ADVENTURE_WATER, MP2::OBJ_BOAT );
        default:
            break;
        }
        // Random placeholders (notes v39 3): full byte values.
        switch ( t.b[8] ) {
        case 0xbd:
            return byType( ObjectGroup::ADVENTURE_ARTIFACTS, MP2::OBJ_RANDOM_ARTIFACT );
        case 0xbe:
            return byType( ObjectGroup::ADVENTURE_TREASURES, MP2::OBJ_RANDOM_RESOURCE );
        case 0xbf:
            return byType( ObjectGroup::MONSTERS, MP2::OBJ_RANDOM_MONSTER );
        case 0xc2:
            note = "random mine -> ore mine (approximation)";
            out.group = ObjectGroup::ADVENTURE_MINES;
            out.index = findIndexByType( out.group, MP2::OBJ_MINE, Resource::ORE );
            return out.index >= 0;
        case 0xc3:
            return byType( ObjectGroup::MONSTERS, MP2::OBJ_RANDOM_MONSTER_WEAK );
        case 0xc4:
            return byType( ObjectGroup::MONSTERS, MP2::OBJ_RANDOM_MONSTER_MEDIUM );
        case 0xc5:
            return byType( ObjectGroup::MONSTERS, MP2::OBJ_RANDOM_MONSTER_STRONG );
        case 0xc6:
            return byType( ObjectGroup::MONSTERS, MP2::OBJ_RANDOM_MONSTER_VERY_STRONG );
        default:
            break;
        }
        note = "unmapped type " + std::to_string( type );
        return false;
    }

    // Ground footprint of an object relative to the main tile: blocking parts and all parts.
    void footprint( const Maps::ObjectInfo & info, std::vector<fheroes2::Point> & blocking, std::vector<fheroes2::Point> & all )
    {
        for ( const auto & part : info.groundLevelParts ) {
            all.push_back( part.tileOffset );
            if ( part.layerType == Maps::OBJECT_LAYER || part.layerType == Maps::BACKGROUND_LAYER ) {
                blocking.push_back( part.tileOffset );
            }
        }
    }
}

namespace H1
{
    int convertMap( const char * inPath, const char * outPath, const char * logPath )
    {
        FILE * log = std::fopen( logPath, "w" );
        if ( log == nullptr ) {
            return 3;
        }
        H1Map m;
        std::string err;
        if ( !readH1( inPath, m, err ) ) {
            std::fprintf( log, "read failed: %s\n", err.c_str() );
            std::fclose( log );
            return 4;
        }

        Maps::Map_Format::MapFormat map;
        world.generateUninitializedMap( W );
        map.width = W;
        map.tiles.resize( W * W );
        for ( int32_t i = 0; i < W * W; ++i ) {
            world.getTile( i ).setIndex( i );
            Maps::setTerrainOnTile( map, i, Maps::Ground::WATER );
        }
        Maps::resetObjectUID();
        Settings::Get().getCurrentMapInfo().version = GameVersion::RESURRECTION;

        // 1. Terrain.
        std::vector<int> ground( W * W );
        for ( int i = 0; i < W * W; ++i ) {
            ground[i] = h1Ground( m.tiles[i].b[0] );
            Maps::setTerrainOnTile( map, i, ground[i] );
        }
        for ( int i = 0; i < W * W; ++i ) {
            const int x = i % W, y = i / W;
            bool border = false;
            for ( int dy = -1; dy <= 1 && !border; ++dy ) {
                for ( int dx = -1; dx <= 1; ++dx ) {
                    const int nx = x + dx, ny = y + dy;
                    if ( nx >= 0 && ny >= 0 && nx < W && ny < W && ground[ny * W + nx] != ground[i] ) {
                        border = true;
                        break;
                    }
                }
            }
            if ( border ) {
                Maps::setTerrainWithTransition( map, i, i, ground[i] );
            }
        }

        std::vector<char> used( W * W, 0 );
        auto markUsed = [&]( int tileId, const Maps::ObjectInfo & info ) {
            for ( const auto & part : info.groundLevelParts ) {
                const int x = tileId % W + part.tileOffset.x, y = tileId / W + part.tileOffset.y;
                if ( x >= 0 && y >= 0 && x < W && y < W ) {
                    used[y * W + x] = 1;
                }
            }
        };
        // Tiles whose game object was placed (written to .h1t so the HoMM1 drawing hides only objects the game removed later).
        std::vector<uint8_t> placedMask( W * W, 0 );
        // markPlaced: false for the landscape stand-ins of step 4 (until v194 they marked their tile too, so a HoMM1 object whose own
        // placement had failed on that tile was taken for "removed by the game" and not drawn; note v195).
        auto put = [&]( int tileId, ObjectGroup g, int idx, const bool markPlaced = true ) {
            if ( Maps::Random_Generator::putObjectOnMap( map, world.getTile( tileId ), g, idx ) ) {
                markUsed( tileId, Maps::getObjectInfo( g, idx ) );
                if ( markPlaced ) {
                    placedMask[tileId] = 1;
                }
                return true;
            }
            return false;
        };

        int placed = 0, failed = 0;

        bool hasHeroRecords = false;
        bool campaignAutoSeat0 = false;
        {
            const std::string in = inPath;
            const size_t slash = in.find_last_of( "/\\" );
            std::string name = ( slash == std::string::npos ) ? in : in.substr( slash + 1 );
            for ( char & ch : name ) {
                ch = static_cast<char>( std::toupper( static_cast<unsigned char>( ch ) ) );
            }
            if ( name.size() == 9 && name.compare( 0, 4, "CAMP" ) == 0 && name.compare( 5, 4, ".CMP" ) == 0 ) {
                const int scenario = name[4] - '1'; // CAMP%d.CMP with scenario + 1
                campaignAutoSeat0 = scenario > 3 && scenario < 8;
            }
        }
        for ( const H1Tile & t : m.tiles ) {
            if ( t.b[8] == 0xc7 ) {
                hasHeroRecords = true;
            }
        }

        // 2. Castles and towns (castle list, notes v41 1-3). Owners: castle setup record if any, else slots 0..3 -> players 0..3 (converter choice).
        std::vector<int> castleColorIndex( 36, -1 );
        bool recordOwners = false;
        for ( int i = 0; i < 36; ++i ) {
            const auto & c = m.castles[i];
            if ( c.x < 0 ) {
                continue;
            }
            const H1Tile & et = m.tiles[c.y * W + c.x];
            const uint8_t rec = et.b[9];
            if ( rec > 0 && rec < m.records.size() && m.records[rec].size() >= 2 && m.records[rec][0] != 0 ) {
                const int8_t owner = static_cast<int8_t>( m.records[rec][1] );
                if ( owner >= 0 ) {
                    castleColorIndex[i] = owner;
                    recordOwners = true;
                }
            }
        }
        if ( !recordOwners ) {
            for ( int i = 0; i < 4; ++i ) {
                if ( m.castles[i].x >= 0 ) {
                    castleColorIndex[i] = i;
                }
            }
        }
        for ( int i = 0; i < 36; ++i ) {
            const auto & c = m.castles[i];
            if ( c.x < 0 ) {
                continue;
            }
            const int tileId = c.y * W + c.x;
            const bool isCastle = ( c.type & 0x80 ) != 0;
            const int race = h1RaceIndex( c.type & 0x7f );
            const int townIndex = ( race < 0 ) ? ( isCastle ? 12 : 13 ) : race * 2 + ( isCastle ? 0 : 1 );
            const int basementId = fheroes2::getTownBasementId( ground[tileId] );
            Maps::Tile & tile = world.getTile( tileId );
            if ( !Maps::Random_Generator::putObjectOnMap( map, tile, ObjectGroup::LANDSCAPE_TOWN_BASEMENTS, basementId ) ) {
                std::fprintf( log, "castle %d basement failed at %d,%d\n", i, c.x, c.y );
                ++failed;
                continue;
            }
            const uint32_t objectId = Maps::getLastObjectUID() - 1;
            Maps::setLastObjectUID( objectId );
            if ( !put( tileId, ObjectGroup::KINGDOM_TOWNS, townIndex ) ) {
                std::fprintf( log, "castle %d failed at %d,%d\n", i, c.x, c.y );
                ++failed;
                continue;
            }
            const PlayerColor color = ( castleColorIndex[i] >= 0 ) ? Color::IndexToColor( castleColorIndex[i] ) : PlayerColor::NONE;
            if ( color == PlayerColor::NONE ) {
                Maps::setDefaultCastleDefenderArmy( map.castleMetadata[Maps::getLastObjectUID()] );
            }
            // The engine's castle flags (the HoMM1 drawing has its own) go only on tiles without a HoMM1 object of their own, and do not
            // mark the tile placed: in W95D1234 the left flag took (61,12), whose resource then could not be placed, and the mask made
            // the drawing treat it as taken (note v195).
            if ( ( m.tiles[tileId - 1].b[8] & 0x80 ) == 0 ) {
                Maps::setLastObjectUID( objectId );
                put( tileId - 1, ObjectGroup::LANDSCAPE_FLAGS, Color::GetIndex( color ) * 2, false );
            }
            if ( ( m.tiles[tileId + 1].b[8] & 0x80 ) == 0 ) {
                Maps::setLastObjectUID( objectId );
                put( tileId + 1, ObjectGroup::LANDSCAPE_FLAGS, Color::GetIndex( color ) * 2 + 1, false );
            }
            // A map with hero setup records (tile byte 8 = 0xc7) gives no automatic starting heroes (game +0x16e79, note v157), except seat 0
            // on campaign scenarios 4..7 (CAMP5..8.CMP, 8638 of the decompile; note v164).
            if ( color != PlayerColor::NONE && ( !hasHeroRecords || ( campaignAutoSeat0 && castleColorIndex[i] == 0 ) ) ) {
                // Starting hero below the castle (notes v41 3-4: HoMM1 puts the starting hero into the castle).
                const int heroSprite = Color::GetIndex( color ) * 7 + ( race >= 0 ? race : 6 );
                put( tileId + W, ObjectGroup::KINGDOM_HEROES, heroSprite, false );
            }
            ++placed;
            std::fprintf( log, "castle %d at %d,%d type 0x%02x -> town index %d color %d\n", i, c.x, c.y, c.type, townIndex, castleColorIndex[i] );
        }

        // 3. Action objects (active tiles).
        std::map<std::string, int> notes;
        for ( int i = 0; i < W * W; ++i ) {
            const H1Tile & t = m.tiles[i];
            if ( ( t.b[8] & 0x80 ) == 0 || ( t.b[8] & 0x7f ) == 0x28 || t.b[8] == 0xc0 || t.b[8] == 0xc1 || t.b[8] == 0xc7 ) {
                continue;
            }
            Placement pl{ ObjectGroup::NONE, -1 };
            std::string note;
            if ( !mapActionObject( m, i, t, pl, note ) ) {
                std::fprintf( log, "skip %d,%d byte8=0x%02x byte2=%d: %s\n", i % W, i / W, t.b[8], t.b[2], note.c_str() );
                ++failed;
                continue;
            }
            if ( !note.empty() ) {
                notes[note]++;
            }
            if ( !put( i, pl.group, pl.index ) ) {
                std::fprintf( log, "place failed %d,%d byte8=0x%02x group %d index %d\n", i % W, i / W, t.b[8], static_cast<int>( pl.group ), pl.index );
                ++failed;
                continue;
            }
            if ( pl.group == ObjectGroup::MONSTERS && pl.monsterCount > 0 ) {
                map.monsterMetadata[Maps::getLastObjectUID()].count = pl.monsterCount;
            }
            ++placed;
        }

        // 4. Landscape: impassable tiles (byte 7 bit 0x80, notes v39) not covered by placed objects -> HoMM2 landscape objects.
        std::vector<char> need( W * W, 0 );
        for ( int i = 0; i < W * W; ++i ) {
            if ( ( m.tiles[i].b[7] & 0x80 ) && !used[i] ) {
                need[i] = 1;
            }
        }
        struct Cand
        {
            ObjectGroup g;
            int idx;
            std::vector<fheroes2::Point> block, all;
        };
        std::vector<Cand> cands;
        for ( ObjectGroup g : { ObjectGroup::LANDSCAPE_MOUNTAINS, ObjectGroup::LANDSCAPE_TREES, ObjectGroup::LANDSCAPE_ROCKS, ObjectGroup::LANDSCAPE_MISCELLANEOUS } ) {
            const auto & objs = Maps::getObjectsByGroup( g );
            for ( size_t k = 0; k < objs.size(); ++k ) {
                if ( objs[k].empty() ) {
                    continue;
                }
                Cand c{ g, static_cast<int>( k ), {}, {} };
                footprint( objs[k], c.block, c.all );
                if ( !c.block.empty() ) {
                    cands.push_back( std::move( c ) );
                }
            }
        }
        std::stable_sort( cands.begin(), cands.end(), []( const Cand & a, const Cand & b ) { return a.block.size() > b.block.size(); } );
        int landscape = 0;
        for ( int pass = 0; pass < 2; ++pass ) {
            for ( int i = 0; i < W * W; ++i ) {
                if ( !need[i] ) {
                    continue;
                }
                const int h1type = m.tiles[i].b[8] & 0x7f;
                const bool mountain = h1type >= 0x34 && h1type <= 0x37;
                const bool tree = h1type >= 0x38 && h1type <= 0x3c;
                for ( const Cand & c : cands ) {
                    if ( pass == 0 && ( ( mountain && c.g != ObjectGroup::LANDSCAPE_MOUNTAINS ) || ( tree && c.g != ObjectGroup::LANDSCAPE_TREES ) ) ) {
                        continue;
                    }
                    const auto & info = Maps::getObjectInfo( c.g, c.idx );
                    if ( !icnFitsGround( info.groundLevelParts.front().icnType, ground[i] ) ) {
                        continue;
                    }
                    // Try every blocking part of the candidate as the one covering tile i.
                    bool done = false;
                    for ( const auto & anchorPart : c.block ) {
                        const int mx = i % W - anchorPart.x, my = i / W - anchorPart.y;
                        bool ok = true;
                        for ( const auto & bp : c.block ) {
                            const int x = mx + bp.x, y = my + bp.y;
                            if ( x < 0 || y < 0 || x >= W || y >= W || !need[y * W + x] ) {
                                ok = false;
                                break;
                            }
                        }
                        for ( const auto & ap : c.all ) {
                            const int x = mx + ap.x, y = my + ap.y;
                            if ( x < 0 || y < 0 || x >= W || y >= W || used[y * W + x] ) {
                                ok = false;
                                break;
                            }
                        }
                        if ( !ok ) {
                            continue;
                        }
                        const int mainTile = my * W + mx;
                        if ( put( mainTile, c.g, c.idx, false ) ) {
                            for ( const auto & bp : c.block ) {
                                need[( my + bp.y ) * W + mx + bp.x] = 0;
                            }
                            ++landscape;
                            done = true;
                            break;
                        }
                    }
                    if ( done ) {
                        break;
                    }
                }
            }
        }
        int unfilled = 0;
        for ( int i = 0; i < W * W; ++i ) {
            unfilled += need[i];
        }
        // A later object can take the place of one put before it (for example a mine whose upper part lies over a resource): such a
        // tile has no game object in the end, so it is not marked placed (the HoMM1 drawing keeps it; note v192).
        int overwritten = 0;
        for ( int i = 0; i < W * W; ++i ) {
            if ( !placedMask[i] || ( m.tiles[i].b[8] & 0x80 ) == 0 ) {
                continue;
            }
            const Maps::Tile & tile = world.getTile( i );
            auto isAction = []( const Maps::ObjectPart & part ) {
                return part.icnType != MP2::OBJ_ICN_TYPE_UNKNOWN && Maps::getObjectTypeByIcn( part.icnType, part.icnIndex ) >= MP2::OBJ_ACTION_OBJECT_TYPE;
            };
            bool action = isAction( tile.getMainObjectPart() );
            for ( const Maps::ObjectPart & part : tile.getGroundObjectParts() ) {
                action = action || isAction( part );
            }
            for ( const Maps::ObjectPart & part : tile.getTopObjectParts() ) {
                action = action || isAction( part );
            }
            if ( !action ) {
                std::fprintf( log, "overwritten %d,%d byte8=0x%02x byte2=%d\n", i % W, i / W, m.tiles[i].b[8], m.tiles[i].b[2] );
                placedMask[i] = 0;
                ++overwritten;
            }
        }

        world.updatePassabilities();
        Maps::updateMapPlayers( map );
        map.name = m.name.empty() ? std::string( "HoMM1 map" ) : m.name;
        map.description = "Heroes of Might and Magic I map (converted by homm1-remaster).";

        for ( const auto & [n, cnt] : notes ) {
            std::fprintf( log, "note: %s x%d\n", n.c_str(), cnt );
        }
        std::fprintf( log, "placed %d, failed %d, landscape objects %d, unfilled blocked tiles %d, overwritten %d\n", placed, failed, landscape, unfilled,
                      overwritten );
        const bool saved = Maps::Map_Format::saveMap( outPath, map );
        // HoMM1 graphics layer: keep the original 10 tile bytes (row-major) next to the converted map ("<out>.h1t").
        {
            FILE * side = std::fopen( ( std::string( outPath ) + ".h1t" ).c_str(), "wb" );
            if ( side != nullptr ) {
                for ( const H1Tile & t : m.tiles ) {
                    std::fwrite( t.b, 1, 10, side );
                }
                // Castle list of the original map (36 slots: x, y, type; x = -1 empty), needed for the random castle rule (note v46).
                for ( int i = 0; i < 36; ++i ) {
                    const auto & c = m.castles[i];
                    const uint8_t rec[3] = { static_cast<uint8_t>( c.x < 0 ? 0xff : c.x ), static_cast<uint8_t>( c.x < 0 ? 0xff : c.y ),
                                             static_cast<uint8_t>( c.type ) };
                    std::fwrite( rec, 1, 3, side );
                }
                // Placed-object mask, one byte per tile (row-major).
                std::fwrite( placedMask.data(), 1, placedMask.size(), side );
                // Original header bytes 2 and 3 (map difficulty, map size) for the difficulty rating (FUN_0041f2df, note v53).
                const uint8_t header[2] = { m.headerDifficulty, m.headerSize };
                std::fwrite( header, 1, 2, side );
                // Then the original description, ended by a zero byte (note v54).
                std::fwrite( m.headerDescription.c_str(), 1, m.headerDescription.size() + 1, side );
                // Then the original mine list (36 slots: x, y, type; 0xff = empty), for the radar's mine owner colours (note v59).
                for ( int i = 0; i < 36; ++i ) {
                    const auto & mi = m.mines[i];
                    const uint8_t rec[3] = { static_cast<uint8_t>( mi.x < 0 ? 0xff : mi.x ), static_cast<uint8_t>( mi.x < 0 ? 0xff : mi.y ),
                                             static_cast<uint8_t>( mi.kind ) };
                    std::fwrite( rec, 1, 3, side );
                }
                // Then "HERO", the number of hero setup records and each one: x, y and the record's first 0x19 bytes (FUN_0041d657: owner,
                // 5 creatures, 5 counts, hero number, 4 artifacts, experience), note v157.
                {
                    std::vector<uint8_t> heroes;
                    int count = 0;
                    for ( int y = 0; y < W; ++y ) {
                        for ( int x = 0; x < W; ++x ) {
                            const H1Tile & t = m.tiles[y * W + x];
                            if ( t.b[8] != 0xc7 ) {
                                continue;
                            }
                            const size_t rec = t.b[9];
                            uint8_t bytes[0x19] = {};
                            if ( rec > 0 && rec < m.records.size() ) {
                                std::copy_n( m.records[rec].begin(), std::min<size_t>( 0x19, m.records[rec].size() ), bytes );
                            }
                            heroes.push_back( static_cast<uint8_t>( x ) );
                            heroes.push_back( static_cast<uint8_t>( y ) );
                            heroes.insert( heroes.end(), bytes, bytes + 0x19 );
                            ++count;
                            std::fprintf( log, "hero record at %d,%d: owner %d hero %d\n", x, y, static_cast<int8_t>( bytes[0] ), bytes[0x10] );
                        }
                    }
                    std::fwrite( "HERO", 1, 4, side );
                    const uint8_t n = static_cast<uint8_t>( count );
                    std::fwrite( &n, 1, 1, side );
                    if ( !heroes.empty() ) {
                        std::fwrite( heroes.data(), 1, heroes.size(), side );
                    }
                }
                // Then "LOOP" and the loop sound table (row-major), note v109.
                std::fwrite( "LOOP", 1, 4, side );
                std::fwrite( m.loopSounds.data(), 1, m.loopSounds.size(), side );
                std::fclose( side );
            }
        }
        std::fprintf( log, "saved: %d\n", saved ? 1 : 0 );
        std::fclose( log );
        return saved ? 0 : 5;
    }
}
