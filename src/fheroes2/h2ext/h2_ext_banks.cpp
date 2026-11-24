/***************************************************************************
 *   H2-EXT: creature banks of the extension game, see h2_ext_banks.h
 ***************************************************************************/

#include "h2_ext_banks.h"

#include <array>
#include <cstddef>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "game_assets.h"
#include "h2_ext.h"
#include "h2_ext_items.h"
#include "h2_ext_monsters.h"
#include "h2_ext_sets.h"
#include "icn.h"
#include "image.h"
#include "map_object_info.h"
#include "maps_tiles.h"
#include "monster.h"
#include "rand.h"
#include "translations.h"

namespace
{
    // Picture numbers in ICN::H2EXT_MAP_OBJECTS: each bank is a recoloured copy of a HoMM2 object [제안: pictures, owner sees them in game].
    struct BankPicture
    {
        int baseIcn; // the HoMM2 picture file
        uint32_t firstBase; // first and last picture of the HoMM2 object
        uint32_t lastBase;
        uint32_t firstOwn; // first picture in H2EXT_MAP_OBJECTS
        double hue;
        double saturation;
        double value;
        double tint;
        uint8_t tintR;
        uint8_t tintG;
        uint8_t tintB;
    };

    const std::array<BankPicture, H2Ext::Banks::KIND_COUNT> pictures = { {
        { ICN::OBJNMUL2, 202, 209, 0, 0, 0.5, 0.85, 0.45, 110, 150, 90 }, // Barrow: the Graveyard, mouldy green
        { ICN::OBJNDSRT, 69, 73, 8, 0, 1.0, 2.0, 0.8, 140, 100, 45 }, // Hunters' Camp: the Nomads' tent, leather brown
        { ICN::OBJNMUL2, 34, 55, 13, 0, 0.6, 0.75, 0.55, 150, 40, 30 } // Dragon Nest: the Dragon City, scorched red
    } };

    std::array<int32_t, H2Ext::Banks::KIND_COUNT> objectIndices = { -1, -1, -1 };

    uint32_t own( const int kind, const uint32_t baseIndex )
    {
        const BankPicture & p = pictures[static_cast<size_t>( kind )];
        return p.firstOwn + ( baseIndex - p.firstBase );
    }

    // The graveyard, nomads' tent and dragon city objects of map_object_info.cpp with the bank pictures and the Graveyard action.
    Maps::ObjectInfo makeObject( const int kind )
    {
        const MP2::MapObjectType action = MP2::OBJ_GRAVEYARD;
        const MP2::MapObjectType part = MP2::OBJ_NON_ACTION_GRAVEYARD;

        Maps::ObjectInfo object{ action };
        const auto ground = [&object, kind]( const uint32_t base, const int x, const int y, const MP2::MapObjectType type, const Maps::ObjectLayerType layer ) {
            object.groundLevelParts.emplace_back( H2Ext::Banks::bankIcnType, own( kind, base ), fheroes2::Point{ x, y }, type, layer );
        };
        const auto top = [&object, kind]( const uint32_t base, const int x, const int y, const MP2::MapObjectType type ) {
            object.topLevelParts.emplace_back( H2Ext::Banks::bankIcnType, own( kind, base ), fheroes2::Point{ x, y }, type );
        };

        switch ( kind ) {
        case H2Ext::Banks::BARROW: // Graveyard, generic terrain
            ground( 208, 0, 0, action, Maps::OBJECT_LAYER );
            ground( 209, 1, 0, part, Maps::OBJECT_LAYER );
            ground( 207, -1, 0, part, Maps::OBJECT_LAYER );
            ground( 206, -2, 0, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            top( 205, 1, -1, part );
            top( 204, 0, -1, part );
            top( 203, -1, -1, part );
            top( 202, -2, -1, MP2::OBJ_NONE );
            break;
        case H2Ext::Banks::HUNTERS_CAMP: // Desert Tent
            ground( 73, 0, 0, action, Maps::OBJECT_LAYER );
            ground( 72, -1, 0, part, Maps::OBJECT_LAYER );
            ground( 71, -2, 0, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            top( 70, 0, -1, part );
            top( 69, -1, -1, part );
            break;
        case H2Ext::Banks::DRAGON_NEST: // Dragon City
            ground( 54, 0, 0, action, Maps::OBJECT_LAYER );
            ground( 51, -3, 0, part, Maps::OBJECT_LAYER );
            ground( 52, -2, 0, part, Maps::OBJECT_LAYER );
            ground( 53, -1, 0, part, Maps::OBJECT_LAYER );
            ground( 55, 1, 0, part, Maps::OBJECT_LAYER );
            ground( 46, -1, -1, part, Maps::OBJECT_LAYER );
            ground( 49, -5, 0, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            ground( 50, -4, 0, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            ground( 42, -5, -1, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            ground( 43, -4, -1, MP2::OBJ_NONE, Maps::SHADOW_LAYER );
            top( 44, -3, -1, part );
            top( 45, -2, -1, part );
            top( 47, 0, -1, part );
            top( 48, 1, -1, part );
            top( 37, -3, -2, part );
            top( 38, -2, -2, part );
            top( 39, -1, -2, part );
            top( 40, 0, -2, part );
            top( 41, 1, -2, part );
            top( 34, -2, -3, MP2::OBJ_NONE );
            top( 35, -1, -3, part );
            top( 36, 0, -3, MP2::OBJ_NONE );
            break;
        default:
            break;
        }
        return object;
    }

    bool isExtBank( const Maps::Tile & tile, int & kind )
    {
        kind = H2Ext::Banks::kindOfTile( tile );
        return kind != H2Ext::Banks::NONE && H2Ext::isOn();
    }

    // A new item or a piece of a set (not a joined set), at random.
    int randomSetPieceOrNewItem()
    {
        std::vector<int> candidates;
        for ( int id = Artifact::UNKNOWN + 1; id <= Artifact::EXT_TOME_KNOWLEDGE; ++id ) {
            if ( H2Ext::setOfCombinedArtifact( id ) >= 0 ) {
                continue;
            }
            if ( H2Ext::isNewArtifact( id ) || H2Ext::setIndexOf( id ) >= 0 ) {
                const Artifact art( id );
                if ( art.isValid() && ( art.Level() & Artifact::ART_ULTIMATE ) == 0 ) {
                    candidates.push_back( id );
                }
            }
        }
        if ( candidates.empty() ) {
            return Artifact::Rand( Artifact::ART_LEVEL_ALL_NORMAL );
        }
        return Rand::Get( candidates );
    }
}

namespace H2Ext::Banks
{
    int kindOfTile( const Maps::Tile & tile )
    {
        if ( tile.getMainObjectType( false ) != MP2::OBJ_GRAVEYARD ) {
            return NONE;
        }
        const Maps::ObjectPart & part = tile.getMainObjectPart();
        if ( part.icnType != bankIcnType ) {
            return NONE;
        }
        for ( int kind = KIND_COUNT - 1; kind >= 0; --kind ) {
            if ( part.icnIndex >= pictures[static_cast<size_t>( kind )].firstOwn ) {
                return kind;
            }
        }
        return NONE;
    }

    void addObjectInfos( std::vector<Maps::ObjectInfo> & objects )
    {
        for ( int kind = 0; kind < KIND_COUNT; ++kind ) {
            objectIndices[static_cast<size_t>( kind )] = static_cast<int32_t>( objects.size() );
            objects.emplace_back( makeObject( kind ) );
        }
    }

    int32_t objectIndex( const int kind )
    {
        return ( kind >= 0 && kind < KIND_COUNT ) ? objectIndices[static_cast<size_t>( kind )] : -1;
    }

    void makeSprites( std::vector<fheroes2::Sprite> & sprites )
    {
        const BankPicture & last = pictures.back();
        sprites.resize( last.firstOwn + ( last.lastBase - last.firstBase ) + 1 );
        for ( const BankPicture & p : pictures ) {
            const std::vector<uint8_t> palette = H2Ext::makeRecolorPalette( p.hue, p.saturation, p.value, p.tint, p.tintR, p.tintG, p.tintB );
            for ( uint32_t i = p.firstBase; i <= p.lastBase; ++i ) {
                fheroes2::Sprite sprite = Assets::getImage( p.baseIcn, i );
                uint8_t * image = sprite.image();
                const uint8_t * transform = sprite.singleLayer() ? nullptr : sprite.transform();
                const int32_t size = sprite.width() * sprite.height();
                for ( int32_t pos = 0; pos < size; ++pos ) {
                    if ( transform == nullptr || transform[pos] == 0 ) {
                        image[pos] = palette[image[pos]];
                    }
                }
                sprites[p.firstOwn + ( i - p.firstBase )] = std::move( sprite );
            }
        }
    }

    bool setGuards( Army & army, const Maps::Tile & tile )
    {
        int kind = NONE;
        if ( !isExtBank( tile, kind ) ) {
            return false;
        }
        // [제안] guards (docs/H2_EXTRAS_CONCEPT_v1.md 3); compare: Graveyard = 5 x 20 Mutant Zombies, Dragon City = 5 dragons
        switch ( kind ) {
        case BARROW:
            army.GetTroop( 0 )->Set( Monster::EXT_PLAGUE_ZOMBIE, 15 );
            army.GetTroop( 1 )->Set( Monster::EXT_MUMMY_PRIEST, 4 );
            army.GetTroop( 2 )->Set( Monster::EXT_PLAGUE_ZOMBIE, 15 );
            army.GetTroop( 3 )->Set( Monster::EXT_MUMMY_PRIEST, 4 );
            army.GetTroop( 4 )->Set( Monster::EXT_PLAGUE_ZOMBIE, 15 );
            break;
        case HUNTERS_CAMP:
            army.GetTroop( 0 )->Set( Monster::EXT_CROSSBOWMAN, 12 );
            army.GetTroop( 1 )->Set( Monster::EXT_FOREST_SNIPER, 8 );
            army.GetTroop( 2 )->Set( Monster::EXT_FOREST_SNIPER, 8 );
            army.GetTroop( 3 )->Set( Monster::EXT_FOREST_SNIPER, 8 );
            army.GetTroop( 4 )->Set( Monster::EXT_CROSSBOWMAN, 12 );
            break;
        case DRAGON_NEST:
            army.GetTroop( 0 )->Set( Monster::EXT_WRAITH_DRAGON, 2 );
            army.GetTroop( 2 )->Set( Monster::EXT_PLAGUE_DRAGON, 2 );
            army.GetTroop( 4 )->Set( Monster::EXT_WRAITH_DRAGON, 2 );
            break;
        default:
            return false;
        }
        return true;
    }

    bool setReward( Maps::Tile & tile )
    {
        int kind = NONE;
        if ( !isExtBank( tile, kind ) ) {
            return false;
        }
        // [제안] rewards (docs/H2_EXTRAS_CONCEPT_v1.md 3): metadata [0] artifact, [1] gold, as the Graveyard
        switch ( kind ) {
        case BARROW:
            tile.metadata()[0] = static_cast<uint32_t>( randomSetPieceOrNewItem() );
            tile.metadata()[1] = 1500;
            break;
        case HUNTERS_CAMP:
            tile.metadata()[0] = static_cast<uint32_t>( Rand::Get( 0, 1 ) == 0 ? Artifact::EXT_HUNTER_QUIVER : Artifact::GOLDEN_BOW );
            tile.metadata()[1] = 1000;
            break;
        case DRAGON_NEST:
            tile.metadata()[0] = static_cast<uint32_t>( Artifact::Rand( Artifact::ART_LEVEL_MAJOR ) );
            tile.metadata()[1] = 5000;
            break;
        default:
            return false;
        }
        return true;
    }

    std::string name( const Maps::Tile & tile )
    {
        int kind = NONE;
        if ( !isExtBank( tile, kind ) ) {
            return {};
        }
        switch ( kind ) {
        case BARROW:
            return _( "Barrow" );
        case HUNTERS_CAMP:
            return _( "Hunters' Camp" );
        case DRAGON_NEST:
            return _( "Dragon Nest" );
        default:
            return {};
        }
    }

    Texts texts( const Maps::Tile & tile )
    {
        int kind = NONE;
        if ( !isExtBank( tile, kind ) ) {
            return {};
        }
        switch ( kind ) {
        case BARROW:
            return { _( "An old barrow mound rises from the ground. Its dead do not rest. Do you want to enter the barrow?" ),
                     _( "The barrow is empty. Nothing is left in it." ), _( "Upon defeating the restless dead you search the barrow and find something!" ) };
        case HUNTERS_CAMP:
            return { _( "A camp of hunters watches the road with their bows ready. Do you want to attack the camp?" ),
                     _( "The camp is abandoned. Nothing is left in it." ), _( "Upon defeating the hunters you search the camp and find something!" ) };
        case DRAGON_NEST:
            return { _( "You have found the nest of the dead dragons, full of the gold of their victims. Do you want to attack the nest?" ),
                     _( "The nest is empty. Nothing is left in it." ), _( "Upon defeating the dragons you search the nest and find a great treasure!" ) };
        default:
            return {};
        }
    }
}
