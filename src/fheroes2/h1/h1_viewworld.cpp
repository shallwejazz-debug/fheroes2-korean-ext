// Heroes I "view world" (homm1-remaster project). See h1_viewworld.h.
#include "h1_viewworld.h"

#include <array>
#include <cstdio>
#include <vector>

#include "castle.h"
#include "color.h"
#include "cursor.h"
#include "game_interface.h"
#include "h1_gfx.h"
#include "h1_map_render.h"
#include "h1_ui.h"
#include "heroes.h"
#include "image.h"
#include "maps_tiles.h"
#include "screen.h"
#include "settings.h"
#include "world.h"

namespace
{
    // The game area: 0x1c0 x 0x1c0 at (0x10, 0x10), filled with colour 0 first (FUN_004793b2). Tile (x, y) is drawn at screen
    // (0x18 + 6x, 0x18 + 6y), so inside the area at (8 + 6x, 8 + 6y).
    const int32_t areaX = 0x10;
    const int32_t areaY = 0x10;

    // FUN_0047a360: not mirrored, the frame goes at (x + frame x, y + frame y). Mirrored, x is the right edge: the frame's right
    // column is at x - frame x. (Ground tiles are mirrored with x + 5, so the 6 pixels wide tile still covers x .. x + 5.)
    void put( fheroes2::Image & area, const std::vector<fheroes2::Sprite> & icn, const int frame, const int32_t x, const int32_t y, const bool mirror )
    {
        if ( frame < 0 || static_cast<size_t>( frame ) >= icn.size() ) {
            return;
        }
        const fheroes2::Sprite & s = icn[frame];
        const int32_t left = mirror ? ( x - s.x() - s.width() + 1 ) : ( x + s.x() );
        fheroes2::Blit( s, area, left - areaX, y + s.y() - areaY, mirror );
    }

    // Player colour number (0 blue, 1 green, 2 red, 3 yellow) of an owner, -1 for none (castle +0x?? / mine owner table 0x1443d
    // hold the player number; the colour is that player's +0x21d byte).
    int colourIndex( const PlayerColor color )
    {
        const int index = Color::GetIndex( color );
        return ( color == PlayerColor::NONE || index < 0 || index > 3 ) ? -1 : index;
    }
}

namespace H1
{
    bool showViewWorld( const int mode, const bool allMarkers, const bool revealAll )
    {
        const std::vector<std::array<uint8_t, 10>> tiles = gameTileBytes();
        if ( tiles.empty() || mode < 0x13 || mode > 0x18 ) {
            return false;
        }

        // FUN_0045f09e first sets the mouse to advmice.mse frame 0 (FUN_00473410), the plain pointer (note v173). The screen is drawn
        // straight (FUN_00476c60 copies the area), no fade.
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );

        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        const fheroes2::Image saved = display;

        const std::vector<fheroes2::Sprite> & ground = getIcn( "ground6.icn" );
        const std::vector<fheroes2::Sprite> & flags = getIcn( "flag6.icn" );
        const std::vector<fheroes2::Sprite> & spheres = getIcn( "spheres.icn" );
        const std::vector<fheroes2::Sprite> & letters = getIcn( "letters.icn" );
        // Object sets drawn small: 8 mtn6, 9 tree6, 10 town6 (others are not loaded). Sets 8 and 9 always (0x300); towns too in the
        // towns and world modes (0x700).
        const std::vector<fheroes2::Sprite> * sets[16] = {};
        sets[8] = &getIcn( "mtn6.icn" );
        sets[9] = &getIcn( "tree6.icn" );
        sets[10] = &getIcn( "town6.icn" );
        const uint32_t setMask = ( mode == 0x16 || mode == 0x18 ) ? 0x700 : 0x300;

        const PlayerColor player = Settings::Get().CurrentColor();
        const Heroes * focus = Interface::GetFocusHeroes();

        fheroes2::Image area( 0x1c0, 0x1c0 );
        area._disableTransformLayer();
        fheroes2::Fill( area, 0, 0, area.width(), area.height(), 0 );

        auto visible = [&]( const int32_t index ) { return !world.getTile( index ).isFog( player ); };

        for ( int32_t ty = 0; ty < 72; ++ty ) {
            const int32_t py = ty * 6 + 0x18;
            // 1. Ground and lower layer, right to left.
            for ( int32_t tx = 71; tx >= 0; --tx ) {
                const int32_t index = ty * 72 + tx;
                const std::array<uint8_t, 10> & t = tiles[index];
                // Towns mode shows castle tiles even under the fog (byte 8 & 0x7f == 0x28).
                if ( !visible( index ) && !revealAll && !( mode == 0x16 && ( t[8] & 0x7f ) == 0x28 ) ) {
                    continue;
                }
                const int32_t px = tx * 6 + 0x18;
                int frame = t[0] >> 2;
                if ( t[6] & 1 ) {
                    frame += 0x1f;
                }
                const bool mirror = ( t[6] & 2 ) != 0;
                put( area, ground, frame, mirror ? px + 5 : px, py, mirror );
                const uint32_t set = t[1] & 0xf;
                if ( t[2] != 0xff && ( setMask & ( 1u << set ) ) != 0 && sets[set] != nullptr ) {
                    put( area, *sets[set], t[2], px, py, false );
                }
            }

            // 2. Markers on action tiles (byte 8 bit 0x80), right to left.
            for ( int32_t tx = 71; tx >= 0; --tx ) {
                const int32_t index = ty * 72 + tx;
                const std::array<uint8_t, 10> & t = tiles[index];
                const int32_t px = tx * 6 + 0x18;
                if ( ( allMarkers || visible( index ) ) && ( t[8] & 0x80 ) != 0 ) {
                    const int type = t[8] & 0x7f;
                    const Heroes * hero = world.GetHeroes( { tx, ty } );
                    // The original writes heroes into the map (byte 8 = 0x3d, the tile's own bytes kept in the hero, +0x23 / +0x24);
                    // the engine keeps heroes apart, so the tile bytes here are already the ones under the hero.
                    auto mine = [&]() {
                        if ( type != 0x01 && type != 0x19 && type != 0x20 ) {
                            return;
                        }
                        const int owner = colourIndex( world.ColorCapturedObject( index ) );
                        put( area, spheres, owner < 0 ? 4 : owner, px, py, false );
                        put( area, letters, getMineKind( index ), px, py, false );
                    };
                    auto castleFlags = [&]( const int leftX ) {
                        if ( t[8] != 0xa8 ) {
                            return;
                        }
                        const Castle * castle = world.getCastleEntrance( { tx, ty } );
                        const int owner = colourIndex( castle != nullptr ? castle->GetColor() : PlayerColor::NONE );
                        if ( owner >= 0 ) {
                            put( area, flags, owner, leftX, py, true );
                            put( area, flags, owner, px + 3, py, false );
                        }
                    };
                    auto heroFlag = [&]() {
                        if ( hero != nullptr ) {
                            const int owner = colourIndex( hero->GetColor() );
                            if ( owner >= 0 ) {
                                put( area, flags, owner, px, py, false );
                            }
                        }
                    };
                    switch ( mode ) {
                    case 0x13:
                        mine();
                        break;
                    case 0x14:
                        if ( t[8] == 0x9d ) {
                            put( area, spheres, 4, px - 3, py, false );
                            put( area, letters, t[2] - 0x3d, px - 3, py, false );
                        }
                        break;
                    case 0x15:
                        if ( t[8] == 0xb0 ) {
                            put( area, flags, 6, px, py, false );
                        }
                        break;
                    case 0x16:
                        castleFlags( px - 4 );
                        break;
                    case 0x17:
                        heroFlag();
                        break;
                    default:
                        if ( t[8] == 0xb0 ) {
                            put( area, flags, 6, px, py, false );
                        }
                        castleFlags( px - 4 );
                        mine();
                        heroFlag();
                        break;
                    }
                }
                // The selected hero: flag6 frame 5 on its tile, whatever the fog.
                if ( focus != nullptr && focus->GetCenter().x == tx && focus->GetCenter().y == ty ) {
                    put( area, flags, 5, px, py, false );
                }
            }

            // 3. Upper layer, right to left (towns mode: castle tiles whose byte 8 is exactly 0x28 also under the fog).
            for ( int32_t tx = 71; tx >= 0; --tx ) {
                const int32_t index = ty * 72 + tx;
                const std::array<uint8_t, 10> & t = tiles[index];
                if ( !visible( index ) && !revealAll && !( t[8] == 0x28 && mode == 0x16 ) ) {
                    continue;
                }
                const uint32_t set = t[3] & 0xf;
                if ( t[4] != 0xff && ( setMask & ( 1u << set ) ) != 0 && sets[set] != nullptr ) {
                    put( area, *sets[set], t[4], tx * 6 + 0x18, py, false );
                }
            }
        }

        fheroes2::Copy( area, 0, 0, display, ox + areaX, oy + areaY, area.width(), area.height() );
        display.render();

        // view-<mode - 0x13>.bin at (0x1e0, 0x10) over the radar; its Exit button closes (handler FUN_00424203).
        char name[16];
        std::snprintf( name, sizeof( name ), "view-%02d.bin", mode - 0x13 );
        UI::Layout panel = UI::loadLayout( name );
        UI::runLayoutDialog( panel, 0x1e0, 0x10, false );

        fheroes2::Copy( saved, display );
        display.render();
        return true;
    }
}
