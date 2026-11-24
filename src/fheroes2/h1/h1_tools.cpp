// HoMM1 port tools (homm1-remaster project).
// --h1dump <out.txt> : dump fheroes2 object catalog (group, index, MP2 type, ICN type, parts).
#include "h1_tools.h"
#include "h1_map_convert.h"
#include "h1_rmg.h"
#include "h1_rules.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "map_object_info.h"
#include "monster_info.h"
#include "mp2.h"

namespace
{
    int dumpCatalog( const char * outPath )
    {
        FILE * f = std::fopen( outPath, "w" );
        if ( f == nullptr ) {
            return 1;
        }
        for ( uint8_t g = 0; g < static_cast<uint8_t>( Maps::ObjectGroup::GROUP_COUNT ); ++g ) {
            const Maps::ObjectGroup group = static_cast<Maps::ObjectGroup>( g );
            if ( group == Maps::ObjectGroup::NONE ) {
                continue;
            }
            const auto & objects = Maps::getObjectsByGroup( group );
            for ( size_t i = 0; i < objects.size(); ++i ) {
                const Maps::ObjectInfo & info = objects[i];
                if ( info.empty() ) {
                    continue;
                }
                const auto & main = info.groundLevelParts.front();
                int minX = 0, maxX = 0, minY = 0, maxY = 0;
                for ( const auto & p : info.groundLevelParts ) {
                    minX = std::min( minX, p.tileOffset.x );
                    maxX = std::max( maxX, p.tileOffset.x );
                    minY = std::min( minY, p.tileOffset.y );
                    maxY = std::max( maxY, p.tileOffset.y );
                }
                std::fprintf( f, "%d\t%zu\t%d\t%s\t%d\t%u\t%zu\t%zu\t%d,%d,%d,%d\t%u,%u\n", g, i, static_cast<int>( info.objectType ),
                              MP2::StringObject( info.objectType ), static_cast<int>( main.icnType ), main.icnIndex, info.groundLevelParts.size(),
                              info.topLevelParts.size(), minX, maxX, minY, maxY, info.metadata[0], info.metadata[1] );
            }
        }
        std::fclose( f );
        return 0;
    }

    // --h1creatures <out.tsv> : dump the creature data the engine actually uses after H1::applyCreatureRules.
    int dumpCreatures( const char * outPath )
    {
        FILE * f = std::fopen( outPath, "w" );
        if ( f == nullptr ) {
            return 1;
        }
        std::fprintf( f, "h1id\tname\tattack\tdefense\tdmin\tdmax\thp\tspeed(fh2)\tshots\tgrowth\tgold\twood,mercury,ore,sulfur,crystal,gems\tabilities(type:percent:value)\n" );
        int h1id = 0;
        for ( const H1::CreatureRow & row : H1::getCreatureTable() ) {
            const fheroes2::MonsterData & d = fheroes2::getMonsterData( row.monsterId );
            const auto & b = d.battleStats;
            const auto & g = d.generalStats;
            std::string abilities;
            for ( const auto & a : b.abilities ) {
                abilities += std::to_string( static_cast<int>( a.type ) ) + ":" + std::to_string( a.percentage ) + ":" + std::to_string( a.value ) + " ";
            }
            std::fprintf( f, "%d\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u,%u,%u,%u,%u,%u\t%s\n", h1id++, g.untranslatedName, b.attack, b.defense, b.damageMin,
                          b.damageMax, b.hp, b.speed, b.shots, g.baseGrowth, g.cost.gold, g.cost.wood, g.cost.mercury, g.cost.ore, g.cost.sulfur, g.cost.crystal,
                          g.cost.gems, abilities.c_str() );
        }
        std::fclose( f );
        return 0;
    }
}

namespace H1
{
    bool isToolCommand( int argc, char ** argv )
    {
        // Tool commands only; plain "--h1" starts the game in Heroes I mode (h1_mode.cpp).
        return argc >= 2
               && ( std::strcmp( argv[1], "--h1dump" ) == 0 || std::strcmp( argv[1], "--h1convert" ) == 0 || std::strcmp( argv[1], "--h1creatures" ) == 0
                   || std::strcmp( argv[1], "--h1random" ) == 0 );
    }

    int runTool( int argc, char ** argv )
    {
        if ( std::strcmp( argv[1], "--h1dump" ) == 0 && argc >= 3 ) {
            return dumpCatalog( argv[2] );
        }
        if ( std::strcmp( argv[1], "--h1creatures" ) == 0 && argc >= 3 ) {
            return dumpCreatures( argv[2] );
        }
        if ( std::strcmp( argv[1], "--h1convert" ) == 0 && argc >= 5 ) {
            return convertMap( argv[2], argv[3], argv[4] );
        }
        if ( std::strcmp( argv[1], "--h1random" ) == 0 && argc >= 4 ) {
            // H1-INTENDED(D16): random map maker test (note v245): --h1random SEED OUT.MAP
            std::string err;
            if ( !makeRandomMap( argv[3], static_cast<uint32_t>( std::strtoul( argv[2], nullptr, 10 ) ), err ) ) {
                std::fprintf( stderr, "%s\n", err.c_str() );
                return 1;
            }
            return 0;
        }
        return 2;
    }
}
