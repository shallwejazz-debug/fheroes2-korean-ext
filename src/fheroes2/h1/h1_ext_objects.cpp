#include "h1_ext_objects.h"

#include <vector>

#include "game_assets.h"
#include "h1_ext_objects_internal.h"
#include "image.h"
#include "map_object_info.h"
#include "mp2.h"

namespace H1
{
    namespace
    {
        struct Part
        {
            int icnType;
            uint32_t frame;
        };

        std::vector<ExtObjectDef> definitions;
        std::vector<Part> parts;

        const Maps::ObjectInfo * findObject( const MP2::MapObjectType type )
        {
            static const Maps::ObjectGroup groups[3] = { Maps::ObjectGroup::ADVENTURE_DWELLINGS, Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS,
                                                         Maps::ObjectGroup::ADVENTURE_TREASURES };
            for ( const Maps::ObjectGroup group : groups ) {
                for ( const Maps::ObjectInfo & info : Maps::getObjectsByGroup( group ) ) {
                    if ( info.objectType == type ) {
                        return &info;
                    }
                }
            }
            return nullptr;
        }

        void build()
        {
            if ( !definitions.empty() ) {
                return;
            }
            // Fixed order: the part numbers are kept in the tile bytes of saved games.
            const struct
            {
                int kind;
                MP2::MapObjectType type;
                const char * name;
            } list[7] = { { EXT_KIND_RUINS, MP2::OBJ_RUINS, "Ruins" },
                          { EXT_KIND_EXCAVATION, MP2::OBJ_EXCAVATION, "Excavation" },
                          { EXT_KIND_CITY_OF_DEAD, MP2::OBJ_CITY_OF_DEAD, "City of the Dead" },
                          { EXT_KIND_HALFLING_HOLE, MP2::OBJ_HALFLING_HOLE, "Halfling Hole" },
                          { EXT_KIND_MARKET, MP2::OBJ_TRADING_POST, "Creature Market" },
                          { EXT_KIND_PYRAMID, MP2::OBJ_PYRAMID, "Pyramid" },
                          // Owner 11:3x: "Barrow Mounds" is a Price of Loyalty picture, not in the owner's HEROES2.AGG -> the HoMM2 graveyard.
                          { EXT_KIND_BARROW, MP2::OBJ_GRAVEYARD, "Barrow Mounds" } };
            for ( const auto & entry : list ) {
                ExtObjectDef def;
                def.kind = entry.kind;
                def.name = entry.name;
                def.info = findObject( entry.type );
                if ( def.info != nullptr ) {
                    for ( const Maps::LayeredObjectPartInfo & p : def.info->groundLevelParts ) {
                        def.groundPart.push_back( static_cast<int>( parts.size() ) );
                        parts.push_back( { static_cast<int>( p.icnType ), p.icnIndex } );
                    }
                    for ( const Maps::ObjectPartInfo & p : def.info->topLevelParts ) {
                        def.topPart.push_back( static_cast<int>( parts.size() ) );
                        parts.push_back( { static_cast<int>( p.icnType ), p.icnIndex } );
                    }
                }
                definitions.push_back( def );
            }
        }
    }

    const std::vector<ExtObjectDef> & extensionObjectDefinitions()
    {
        build();
        return definitions;
    }

    const fheroes2::Sprite * extensionObjectPart( const uint32_t index )
    {
        build();
        if ( index >= parts.size() ) {
            return nullptr;
        }
        const int icn = MP2::getIcnIdFromObjectIcnType( static_cast<MP2::ObjectIcnType>( parts[index].icnType ) );
        // HoMM2 pictures are put into the HoMM1 palette when they are read in HoMM1 graphics mode (game_assets.cpp).
        const fheroes2::Sprite & sprite = Assets::getImage( icn, parts[index].frame );
        return sprite.empty() ? nullptr : &sprite;
    }

    const char * extensionObjectName( const int kind )
    {
        for ( const ExtObjectDef & def : extensionObjectDefinitions() ) {
            if ( def.kind == kind ) {
                return def.name;
            }
        }
        return nullptr;
    }
}
