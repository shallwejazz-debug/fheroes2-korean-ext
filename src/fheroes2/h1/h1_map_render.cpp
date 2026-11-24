// HoMM1 map objects drawn with the original HoMM1 images (homm1-remaster project).
// Based on the HoMM1 map drawing function (HEROESW.EXE, decompiled lines ~38000-38190; notes v39, v45).
#include "h1_map_render.h"

#include <array>
#include <chrono>
#include <thread>
#include <string>

#include "game.h"
#include "h1_gfx.h"
#include "h1_armywin.h"
#include "h1_rules.h"
#include "image.h"
#include "localevent.h"
#include "screen.h"
#include "interface_gamearea.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "monster.h"
#include "mp2.h"
#include "world.h"
#include "heroes.h"
#include "settings.h"
#include "kingdom.h"
#include "players.h"
#include "castle.h"
#include "color.h"
#include "artifact.h"
#include "map_object_info.h"
#include "h1_ext_objects.h"

#include <map>

namespace
{
    // Image set number in tile bytes 1 and 3 (low 4 bits) -> HoMM1 ICN file (note v39: read order of the game, set 15 unused).
    const std::array<const char *, 15> setFiles{ "obj32-00.icn", "obj32-01.icn", "obj32-02.icn", "obj32-03.icn", "obj32-04.icn",
                                                 "obj32-05.icn", "obj32-06.icn", "obj32-07.icn", "mtn32.icn",    "tree32.icn",
                                                 "town32.icn",   "rsrc32.icn",   "mons32.icn",   "art32.icn",    "flag32.icn" };

    const fheroes2::Sprite * frameOf( const uint32_t set, const uint32_t frame )
    {
        if ( set == H1::extensionObjectSet ) {
            return H1::extensionObjectPart( frame ); // H1-INTENDED(D16): HoMM2 objects of extension games (h1_ext_objects.h)
        }
        if ( set >= setFiles.size() ) {
            return nullptr;
        }
        const std::vector<fheroes2::Sprite> & frames = H1::getIcn( setFiles[set] );
        if ( frame >= frames.size() || frames[frame].empty() ) {
            return nullptr;
        }
        return &frames[frame];
    }

    void blit( fheroes2::Image & dst, const Interface::GameArea & area, const fheroes2::Sprite * sprite, const int32_t x, const int32_t y, const int32_t dy = 0 )
    {
        if ( sprite != nullptr ) {
            area.BlitOnTile( dst, *sprite, sprite->x(), sprite->y() + dy, { x, y }, false, 255 );
        }
    }

    // Animation step (+0x143): 0..5, advanced by one on each animation tick (note v45 section 2).
    // Monster phases (+0x147..+0x14a, chosen by x & 3): start 0, 1, 3, 5; on a tick that makes the step odd,
    // phases 1 and 3 (+0x148, +0x14a) advance, otherwise phases 0 and 2; all modulo 6.
    uint32_t ticks()
    {
        return Game::getAdventureMapAnimationIndex();
    }

    uint32_t animationStep()
    {
        return ticks() % 6;
    }

    uint32_t monsterPhase( const int32_t x )
    {
        const uint32_t n = ticks();
        const uint32_t oddTicks = ( n + 1 ) / 2;
        const uint32_t evenTicks = n / 2;
        switch ( x & 3 ) {
        case 0:
            return evenTicks % 6;
        case 1:
            return ( 1 + oddTicks ) % 6;
        case 2:
            return ( 3 + evenTicks ) % 6;
        default:
            return ( 5 + oddTicks ) % 6;
        }
    }

    // Rendering sync with the game logic (port rule, not an original rule): an active object tile (byte 8 bit 0x80)
    // whose fheroes2 object has gone (picked up, defeated) is not drawn any more.
    bool objectGone( const Maps::Tile & tile, const uint8_t * t )
    {
        // Objects the converter could not place (no game object from the start) stay drawn.
        // A land tile next to water gets the engine type "coast" once its object is taken off (Tile::updateObjectType): also gone (note v155).
        // An object taken off leaves the parts of neighbouring objects on the tile (for example the lower part of a windmill next to
        // it), so the engine's main type is then one of theirs: gone = no part on the tile belongs to an action object (note v192).
        if ( ( t[8] & 0x80 ) == 0 || !H1::wasPlaced( tile.GetIndex() ) ) {
            return false;
        }
        auto isAction = []( const Maps::ObjectPart & part ) {
            return part.icnType != MP2::OBJ_ICN_TYPE_UNKNOWN
                   && Maps::getObjectTypeByIcn( part.icnType, part.icnIndex ) >= MP2::OBJ_ACTION_OBJECT_TYPE;
        };
        if ( isAction( tile.getMainObjectPart() ) ) {
            return false;
        }
        for ( const Maps::ObjectPart & part : tile.getGroundObjectParts() ) {
            if ( isAction( part ) ) {
                return false;
            }
        }
        for ( const Maps::ObjectPart & part : tile.getTopObjectParts() ) {
            if ( isAction( part ) ) {
                return false;
            }
        }
        const MP2::MapObjectType type = tile.getMainObjectType( false );
        return type < MP2::OBJ_ACTION_OBJECT_TYPE;
    }

    // Random castles: at new game HoMM1 moves the frames of the 4 x 3 castle area from the "random" block of town32.icn to the
    // chosen race's block: frame - (4 - race) x 24 for byte 4 of row y-2 and byte 2 of rows y-1 and y, columns x-2..x+1
    // (FUN_0041ace5, note v46). The race is the one the game chose (fheroes2 castle at the entrance).
    struct FrameShift
    {
        int lower = 0;
        int upper = 0;
    };

    // Also (note v138): a start castle (slot 0..3) that no seat drew becomes a town at new game: frames - 0xc on the same bytes
    // (FUN_004147d7). Here: a slot whose map type is a castle (0x80) while the game castle is a town.
    std::map<int32_t, FrameShift> randomCastleShifts()
    {
        std::map<int32_t, FrameShift> shifts;
        const int32_t w = world.w();
        for ( int32_t index = 0; index < 72 * 72; ++index ) {
            int type = 0;
            const int slot = H1::getCastleSlot( index, &type );
            if ( slot < 0 ) {
                continue;
            }
            const int32_t x = index % w;
            const int32_t y = index / w;
            const Castle * castle = world.getCastleEntrance( { x, y } );
            if ( castle == nullptr ) {
                continue;
            }
            int delta = 0;
            if ( ( type & 0x7f ) == 4 ) {
                const int race = H1::h1RaceFromEngine( castle->GetRace() );
                if ( race < 0 ) {
                    continue;
                }
                // H1-INTENDED(D16): race 4 (necromancer) = frames 120..143 after the random ones (h1/ext/TOWN32.ICN, note v247)
                delta = ( race == 4 ) ? 24 : -( 4 - race ) * 24;
            }
            if ( slot < 4 && ( type & 0x80 ) != 0 && !castle->isCastle() ) {
                delta -= 0xc;
            }
            if ( delta == 0 ) {
                continue;
            }
            for ( int32_t cx = x - 2; cx <= x + 1; ++cx ) {
                if ( y - 2 >= 0 ) {
                    shifts[( y - 2 ) * w + cx].upper = delta;
                }
                shifts[( y - 1 ) * w + cx].lower = delta;
                shifts[y * w + cx].lower = delta;
            }
        }
        return shifts;
    }

    // Castle flags: when a player owns a castle, HoMM1 puts flag32 frame (player colour x 2) on the tile left of the entrance and
    // (colour x 2 + 1) on the tile right of it, as lower-layer flags (decompiled lines ~9370-9380, note v50).
    // Here: tile -> frame for owned castles (-1 = castle neighbour without owner: draw no flag).
    std::map<int32_t, int> castleFlags()
    {
        std::map<int32_t, int> flags;
        const int32_t w = world.w();
        for ( int32_t index = 0; index < 72 * 72; ++index ) {
            if ( H1::getCastleSlot( index, nullptr ) < 0 ) {
                continue;
            }
            const int32_t x = index % w;
            const int32_t y = index / w;
            const Castle * castle = world.getCastleEntrance( { x, y } );
            int colour = -1;
            if ( castle != nullptr && castle->GetColor() != PlayerColor::NONE ) {
                colour = Color::GetIndex( castle->GetColor() );
                if ( colour > 3 ) {
                    colour = -1;
                }
            }
            if ( x > 0 ) {
                flags[index - 1] = ( colour < 0 ) ? -1 : colour * 2;
            }
            if ( x + 1 < w ) {
                flags[index + 1] = ( colour < 0 ) ? -1 : colour * 2 + 1;
            }
        }
        return flags;
    }

    // Mine flags (FUN_00416df1, note v192): when a player takes an entry of the mine list, the original writes on one tile next to it
    // byte 6 |= 0x20, byte 3 |= 0xe0 (third picture from flag32.icn) and byte 5 = player colour + 0x10 (list type 0, sawmill),
    // 0x0c (type 1, alchemist), 0x14 (type 0x16), 0x18 (type 0x17), 8 (other types: mines). The tile: type 1 (x, y - 2),
    // 0x16 (x - 1, y - 3), 0x17 (x - 2, y), others (x, y - 1). Drawn here from the engine's owner of the listed tile.
    std::map<int32_t, int> mineFlags()
    {
        std::map<int32_t, int> flags;
        const int32_t w = world.w();
        for ( int slot = 0; slot < 36; ++slot ) {
            int32_t x = 0;
            int32_t y = 0;
            if ( !H1::getListPosition( true, slot, x, y ) ) {
                continue;
            }
            const PlayerColor owner = world.ColorCapturedObject( y * w + x );
            const int colour = Color::GetIndex( owner );
            if ( owner == PlayerColor::NONE || colour < 0 || colour > 3 ) {
                continue;
            }
            const int type = H1::getMineKind( y * w + x );
            int base = 8;
            int32_t fx = x;
            int32_t fy = y - 1;
            switch ( type ) {
            case 0:
                base = 0x10;
                break;
            case 1:
                base = 0x0c;
                fy = y - 2;
                break;
            case 0x16:
                base = 0x14;
                fx = x - 1;
                fy = y - 3;
                break;
            case 0x17:
                base = 0x18;
                fx = x - 2;
                fy = y;
                break;
            default:
                break;
            }
            if ( fx >= 0 && fy >= 0 && fx < 72 && fy < 72 ) {
                flags[fy * w + fx] = base + colour;
            }
        }
        return flags;
    }

    int h1CreatureOf( const Monster & monster )
    {
        return H1::creatureFromMonster( monster.GetID() ); // H1-INTENDED(D16): 36 creatures in extension games
    }
}

namespace H1
{
    int radarColour( const int32_t tileIndex )
    {
        const uint8_t * t = getTileBytes( tileIndex );
        if ( t == nullptr ) {
            return -1;
        }
        // Tables of HEROESW.EXE (DOS palette numbers): 0x491138 = colour per terrain group (tile number / 20, table DAT_004a7d68),
        // 0x491128 = colour per player colour (blue, green, red, yellow, then 4 = no owner).
        static const int terrainColour[16] = { 82, 99, 7, 180, 26, 123, 55, 0, 16, 48, 98, 160, 126, 74, 110, 179 };
        static const int playerColour[5] = { 79, 105, 200, 129, 10 };
        const int group = t[0] / 20;
        const int ground = ( group >= 0 && group < 16 ) ? terrainColour[group] : 0;
        auto colourOf = []( const PlayerColor color ) {
            const int index = Color::GetIndex( color );
            return ( color == PlayerColor::NONE || index < 0 || index > 3 ) ? playerColour[4] : playerColour[index];
        };

        // Heroes: the original marks hero tiles on the map (byte 8 = 0x3d) and colours them only for the player looking at the map.
        // The engine keeps heroes apart from the map bytes, so the engine hero on the tile is used.
        const int32_t w = world.w();
        const Heroes * hero = world.GetHeroes( { tileIndex % w, tileIndex / w } );
        if ( hero != nullptr ) {
            return ( hero->GetColor() == Settings::Get().CurrentColor() ) ? colourOf( hero->GetColor() ) : ground;
        }

        switch ( t[1] & 0xf ) {
        case 8:
        case 9:
            return ground + 3;
        case 10: {
            // Castle tiles: byte 9 = castle number -> owner. The game writes that number at new game (FUN_0041ace5): columns x-2..x+1,
            // rows y-2..y around each castle entrance, on tiles whose byte 8 (& 0x7f) is 0 or above 0x30. Other tiles keep the file's byte 9.
            const int32_t tx = tileIndex % w;
            const int32_t ty = tileIndex / w;
            int slot = t[9];
            for ( int s = 0; s < 36; ++s ) {
                int32_t cx = 0;
                int32_t cy = 0;
                if ( getListPosition( false, s, cx, cy ) && tx >= cx - 2 && tx <= cx + 1 && ty >= cy - 2 && ty <= cy
                     && ( ( t[8] & 0x7f ) == 0 || ( t[8] & 0x7f ) > 0x30 ) ) {
                    slot = s;
                }
            }
            int32_t x = 0;
            int32_t y = 0;
            if ( getListPosition( false, slot, x, y ) ) {
                const Castle * castle = world.getCastleEntrance( { x, y } );
                return colourOf( castle != nullptr ? castle->GetColor() : PlayerColor::NONE );
            }
            return playerColour[4];
        }
        case 11:
            switch ( t[8] ) {
            case 0x01:
            case 0x19:
            case 0x20:
            case 0x81:
            case 0x99:
            case 0xa0: {
                // Mines: byte 9 = mine number -> owner. Written at new game (FUN_00415a33) for each mine entrance (byte 8 0x81 / 0x99 /
                // 0xa0) on the 2 x 2 tiles x..x+1, y-1..y when their byte 9 is 0 or they are the same object.
                const int32_t tx = tileIndex % w;
                const int32_t ty = tileIndex / w;
                int slot = t[9];
                for ( int s = 0; s < 36; ++s ) {
                    int32_t mx = 0;
                    int32_t my = 0;
                    if ( getListPosition( true, s, mx, my ) && tx >= mx && tx <= mx + 1 && ty >= my - 1 && ty <= my ) {
                        const uint8_t * entrance = getTileBytes( my * w + mx );
                        if ( entrance != nullptr && ( t[9] == 0 || ( ( t[8] ^ entrance[8] ) & 0x7f ) == 0 ) ) {
                            slot = s;
                        }
                    }
                }
                int32_t x = 0;
                int32_t y = 0;
                if ( getListPosition( true, slot, x, y ) ) {
                    return colourOf( world.ColorCapturedObject( y * w + x ) );
                }
                return playerColour[4];
            }
            default:
                return ground;
            }
        default:
            return ground;
        }
    }

    bool isHoMM1Map()
    {
        return getTileBytes( 0 ) != nullptr;
    }

    const fheroes2::Sprite * setFrame( const uint32_t set, const uint32_t frame )
    {
        return frameOf( set, frame );
    }

    int activeObjectKind( const int32_t tileIndex )
    {
        const uint8_t * t = getTileBytes( tileIndex );
        if ( t == nullptr || ( t[8] & 0x80 ) == 0 ) {
            return -1;
        }
        if ( objectGone( world.getTile( tileIndex ), t ) ) {
            return -1;
        }
        if ( t[8] == 0xbd ) {
            return 0x30;
        }
        if ( t[8] == 0xbe ) {
            return 0x1d;
        }
        if ( t[8] == 0xbf || ( t[8] >= 0xc3 && t[8] <= 0xc6 ) ) {
            return 0x1a;
        }
        return t[8] & 0x7f;
    }

    void dissolveCentre( const std::function<void()> & draw )
    {
        dissolveArea( 0xb4, 0xac, 0x78, draw );
    }

    void dissolveArea( const int32_t x, const int32_t y, const int32_t size, const std::function<void()> & draw )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t areaX = ( display.width() - 640 ) / 2 + x;
        const int32_t areaY = ( display.height() - 480 ) / 2 + y;
        fheroes2::Image before( size, size );
        fheroes2::Copy( display, areaX, areaY, before, 0, 0, size, size );
        draw();
        fheroes2::Image after( size, size );
        fheroes2::Copy( display, areaX, areaY, after, 0, 0, size, size );
        fheroes2::Copy( before, 0, 0, display, areaX, areaY, size, size );

        const int32_t block = 1;
        const int32_t half = size / ( 2 * block ); // columns and rows per quarter (60 for the centre area)
        int bits = 0;
        for ( uint32_t v = static_cast<uint32_t>( half ); v != 0; v >>= 1 ) {
            ++bits;
        }
        static const uint32_t taps[20] = { 0x0, 0x0, 0x3, 0x6, 0xc, 0x14, 0x30, 0x60, 0xb8, 0x110, 0x240, 0x500, 0xca0, 0x1b00, 0x3500, 0x6000, 0xb400, 0x12000, 0x20400, 0x72000 };
        const uint32_t tap = taps[std::min( bits * 2, 19 )];
        const uint32_t columnMask = ( 1u << bits ) - 1;
        const int32_t mirrorX = ( half - 1 ) * 2 * block + block; // relative to the area
        const int32_t mirrorY = mirrorX;
        LocalEvent & le = LocalEvent::Get();
        uint32_t value = 1;
        uint32_t steps = 0;
        do {
            const int32_t column = static_cast<int32_t>( value & columnMask );
            const int32_t row = static_cast<int32_t>( value >> bits );
            if ( row < half && column < half ) {
                const int32_t x1 = column * block;
                const int32_t y1 = row * block;
                const int32_t x2 = mirrorX - x1;
                const int32_t y2 = mirrorY - y1;
                fheroes2::Copy( after, x1, y1, display, areaX + x1, areaY + y1, block, block );
                fheroes2::Copy( after, x2, y1, display, areaX + x2, areaY + y1, block, block );
                fheroes2::Copy( after, x1, y2, display, areaX + x1, areaY + y2, block, block );
                fheroes2::Copy( after, x2, y2, display, areaX + x2, areaY + y2, block, block );
            }
            value = ( value & 1 ) ? ( tap ^ ( value >> 1 ) ) : ( value >> 1 );
            ++steps;
            if ( ( steps & 0xff ) == 0xff ) {
                // FUN_00077ba9: wait for the 64 Hz timer counter (0x40 >> 6 = 1 tick).
                display.render( { areaX, areaY, size, size } );
                le.HandleEvents( false );
                std::this_thread::sleep_for( std::chrono::microseconds( 15625 ) );
            }
        } while ( value != 1 );
        display.render( { areaX, areaY, size, size } );
    }

    bool screenShowsStep( const int32_t fromIndex, const int32_t toIndex )
    {
        const Settings & conf = Settings::Get();
        if ( !world.GetKingdom( conf.CurrentColor() ).isControlAI() ) {
            return true;
        }
        if ( conf.AIMoveSpeed() == 0 ) {
            return false;
        }
        const PlayerColorsSet humans = conf.GetPlayers().GetColors( CONTROL_HUMAN, true );
        const auto seen = [humans]( const int32_t index ) { return Maps::isValidAbsIndex( index ) && !world.getTile( index ).isFog( humans ); };
        return seen( fromIndex ) || seen( toIndex );
    }

    bool objectGoneOnTile( const int32_t tileIndex )
    {
        const uint8_t * t = getTileBytes( tileIndex );
        return t != nullptr && objectGone( world.getTile( tileIndex ), t );
    }

    void applyDugHole( const int32_t tileIndex, uint8_t * t )
    {
        if ( t[2] != 0xff ) {
            return;
        }
        const Maps::Tile & tile = world.getTile( tileIndex );
        MP2::ObjectIcnType icnType = MP2::OBJ_ICN_TYPE_UNKNOWN;
        uint8_t imageIndex = 0;
        if ( !MP2::getDiggingHoleSprite( tile.GetGround(), icnType, imageIndex ) ) {
            return;
        }
        for ( const Maps::ObjectPart & part : tile.getGroundObjectParts() ) {
            if ( part.icnType == icnType && part.icnIndex == imageIndex ) {
                t[1] = 7;
                t[2] = 1;
                t[6] = static_cast<uint8_t>( t[6] | 0x80 );
                return;
            }
        }
    }

    std::vector<std::array<uint8_t, 10>> gameTileBytes()
    {
        std::vector<std::array<uint8_t, 10>> tiles;
        if ( getTileBytes( 0 ) == nullptr ) {
            return tiles;
        }
        const std::map<int32_t, FrameShift> shifts = randomCastleShifts();
        tiles.resize( 72 * 72 );
        for ( int32_t index = 0; index < 72 * 72; ++index ) {
            const uint8_t * src = getTileBytes( index );
            std::array<uint8_t, 10> & t = tiles[index];
            if ( src == nullptr ) {
                t.fill( 0 );
                continue;
            }
            std::copy( src, src + 10, t.begin() );
            if ( const auto it = shifts.find( index ); it != shifts.end() ) {
                if ( t[2] != 0xff ) {
                    t[2] = static_cast<uint8_t>( t[2] + it->second.lower );
                }
                if ( t[4] != 0xff ) {
                    t[4] = static_cast<uint8_t>( t[4] + it->second.upper );
                }
            }
            const Maps::Tile & tile = world.getTile( index );
            if ( objectGone( tile, t.data() ) ) {
                takeOffObject( t.data(), index );
                applyDugHole( index, t.data() );
                continue;
            }
            applyDugHole( index, t.data() );
            if ( t[8] == 0xbd && tile.getMainObjectType( false ) == MP2::OBJ_ARTIFACT ) {
                const int art = H1::h1ArtifactFromEngine( Maps::getArtifactFromTile( tile ).GetID() );
                if ( art >= 0 ) {
                    t[2] = static_cast<uint8_t>( art );
                }
                t[8] = 0xb0;
            }
            else if ( t[8] == 0xbe && tile.getMainObjectType( false ) == MP2::OBJ_RESOURCE ) {
                const uint32_t flag = tile.metadata()[0];
                for ( int r = 0; r < 7; ++r ) {
                    if ( flag == ( 1u << r ) ) {
                        t[2] = static_cast<uint8_t>( 0x3d + r );
                    }
                }
                t[8] = 0x9d;
            }
        }
        // Random castles (castle-list type & 0x7f == 4): FUN_0041ace5 changed their 4 x 3 tiles at new game (H1::applyNewGameTileFlags
        // does that part, note v195) and gave the tiles it turned into 0x28 byte 9 = castle number.
        for ( int slot = 0; slot < 36; ++slot ) {
            int32_t cx = 0;
            int32_t cy = 0;
            int type = 0;
            if ( !getListPosition( false, slot, cx, cy ) || H1::getCastleSlot( cy * 72 + cx, &type ) != slot || ( type & 0x7f ) != 4 ) {
                continue;
            }
            for ( int32_t dy = -2; dy <= 0; ++dy ) {
                for ( int32_t dx = -2; dx <= 1; ++dx ) {
                    const int32_t x = cx + dx;
                    const int32_t y = cy + dy;
                    if ( x >= 0 && y >= 0 && x < 72 && y < 72 && ( tiles[y * 72 + x][8] & 0x7f ) == 0x28 ) {
                        tiles[y * 72 + x][9] = static_cast<uint8_t>( slot );
                    }
                }
            }
        }
        return tiles;
    }

    void drawMapStage( const MapStage stage, fheroes2::Image & dst, const Interface::GameArea & area, const int32_t minX, const int32_t minY, const int32_t maxX,
                       const int32_t maxY, const bool renderFog )
    {
        const int32_t worldWidth = world.w();
        const uint32_t step = animationStep();
        const std::map<int32_t, FrameShift> shifts = randomCastleShifts();
        const std::map<int32_t, int> flags = ( stage == MapStage::LOWER ) ? castleFlags() : std::map<int32_t, int>{};
        const std::map<int32_t, int> ownedMines = ( stage == MapStage::UPPER ) ? mineFlags() : std::map<int32_t, int>{};

        for ( int32_t y = minY; y < maxY; ++y ) {
            for ( int32_t x = minX; x < maxX; ++x ) {
                const int32_t index = y * worldWidth + x;
                const uint8_t * t = getTileBytes( index );
                if ( t == nullptr ) {
                    continue;
                }
                const Maps::Tile & tile = world.getTile( index );
                if ( renderFog && tile.getFogDirection() == DIRECTION_ALL ) {
                    continue;
                }

                if ( stage == MapStage::MONSTERS ) {
                    // Monsters: minimon.icn frame = creature x 7 + phase, drawn 5 pixels higher (decompiled line ~38079).
                    // The creature comes from the game state (after random monsters are resolved).
                    if ( tile.getMainObjectType( false ) == MP2::OBJ_MONSTER ) {
                        const int creature = h1CreatureOf( Maps::getMonsterFromTile( tile ) );
                        if ( creature >= 0 ) {
                            // H1-INTENDED(D16): extension creatures 28..35 from h1/ext/minimonx.icn.
                            const std::vector<fheroes2::Sprite> & mini = getIcn( H1::mapCreatureIcn( creature ) );
                            const size_t frame = static_cast<size_t>( H1::mapCreatureFrame( creature, monsterPhase( x ) ) );
                            if ( frame < mini.size() ) {
                                blit( dst, area, &mini[frame], x, y, -5 );
                            }
                        }
                    }
                    continue;
                }

                uint8_t holeBytes[10];
                if ( objectGone( tile, t ) ) {
                    // A dug hole on the tile of a taken object is still drawn.
                    std::copy( t, t + 10, holeBytes );
                    takeOffObject( holeBytes, index );
                    applyDugHole( index, holeBytes );
                    if ( holeBytes[2] == 0xff ) {
                        continue;
                    }
                    t = holeBytes;
                }
                else if ( t[2] == 0xff ) {
                    std::copy( t, t + 10, holeBytes );
                    applyDugHole( index, holeBytes );
                    t = holeBytes;
                }

                const uint32_t lowerSet = t[1] & 0x0f;
                const uint32_t upperSet = t[3] & 0x0f;
                uint8_t shifted[10];
                if ( const auto it = shifts.find( index ); it != shifts.end() ) {
                    std::copy( t, t + 10, shifted );
                    if ( shifted[2] != 0xff ) {
                        shifted[2] = static_cast<uint8_t>( shifted[2] + it->second.lower );
                    }
                    if ( shifted[4] != 0xff ) {
                        shifted[4] = static_cast<uint8_t>( shifted[4] + it->second.upper );
                    }
                    t = shifted;
                }
                // Random artifact / resource: HoMM1 writes the chosen frame into byte 2 at game start (FUN_0041c1cc):
                // art32 frame = artifact number; resource frame = 0x3d + resource (note v49). Taken from the game state here.
                if ( t[8] == 0xbd || t[8] == 0xbe ) {
                    int frame = -1;
                    if ( t[8] == 0xbd && tile.getMainObjectType( false ) == MP2::OBJ_ARTIFACT ) {
                        frame = H1::h1ArtifactFromEngine( Maps::getArtifactFromTile( tile ).GetID() );
                    }
                    else if ( t[8] == 0xbe && tile.getMainObjectType( false ) == MP2::OBJ_RESOURCE ) {
                        const uint32_t flag = tile.metadata()[0];
                        for ( int r = 0; r < 7; ++r ) {
                            if ( flag == ( 1u << r ) ) {
                                frame = 0x3d + r;
                            }
                        }
                    }
                    if ( frame >= 0 ) {
                        if ( t != shifted ) {
                            std::copy( t, t + 10, shifted );
                        }
                        shifted[2] = static_cast<uint8_t>( frame );
                        t = shifted;
                    }
                }
                switch ( stage ) {
                case MapStage::FLAT:
                    if ( ( t[6] & 0x80 ) != 0 && t[2] != 0xff ) {
                        blit( dst, area, frameOf( lowerSet, t[2] ), x, y );
                    }
                    break;
                case MapStage::LOWER:
                    if ( ( t[6] & 0x80 ) == 0 && t[2] != 0xff && lowerSet != 12 ) {
                        blit( dst, area, frameOf( lowerSet, t[2] ), x, y );
                        if ( t[6] & 0x04 ) {
                            blit( dst, area, frameOf( lowerSet, t[2] + step + 1 ), x, y );
                        }
                    }
                    if ( const auto flag = flags.find( index ); flag != flags.end() ) {
                        if ( flag->second >= 0 ) {
                            blit( dst, area, frameOf( 14, static_cast<uint32_t>( flag->second ) ), x, y );
                        }
                    }
                    else if ( t[6] & 0x10 ) {
                        blit( dst, area, frameOf( t[1] >> 4, t[5] ), x, y );
                    }
                    break;
                case MapStage::UPPER:
                    if ( t[4] != 0xff ) {
                        blit( dst, area, frameOf( upperSet, t[4] ), x, y );
                        if ( t[6] & 0x08 ) {
                            blit( dst, area, frameOf( upperSet, t[4] + step + 1 ), x, y );
                        }
                    }
                    if ( const auto mine = ownedMines.find( index ); mine != ownedMines.end() ) {
                        blit( dst, area, frameOf( 14, static_cast<uint32_t>( mine->second ) ), x, y );
                    }
                    else if ( t[6] & 0x20 ) {
                        blit( dst, area, frameOf( t[3] >> 4, t[5] ), x, y );
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
}
