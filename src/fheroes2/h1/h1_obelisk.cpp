// Heroes I obelisks, puzzle pieces and the ultimate artifact (homm1-remaster project). See h1_obelisk.h.
#include "h1_obelisk.h"

#include <array>
#include <cstdlib>
#include <vector>

#include "artifact.h"
#include "artifact_ultimate.h"
#include "color.h"
#include "h1_gfx.h"
#include "h1_map_render.h"
#include "h1_rules.h"
#include "heroes.h"
#include "image.h"
#include "kingdom.h"
#include "players.h"
#include "rand.h"
#include "world.h"

namespace
{
    // FUN_00476010( a, b ): a random number a..b (both included).
    int random( const int from, const int to )
    {
        return static_cast<int>( Rand::Get( static_cast<uint32_t>( from ), static_cast<uint32_t>( to ) ) );
    }
}

namespace H1
{
    int obeliskCount()
    {
        if ( getTileBytes( 0 ) == nullptr ) {
            return 0;
        }
        int count = 0;
        for ( int32_t i = 0; i < 72 * 72; ++i ) {
            if ( getTileBytes( i )[8] == 0x9b ) {
                ++count;
            }
        }
        return count;
    }

    void openObeliskPieces( std::bitset<48> & opened )
    {
        const int count = obeliskCount();
        int pieces = 48 / ( count > 0 ? count : 1 );
        if ( pieces < 1 ) {
            pieces = 1;
        }
        for ( int n = 0; n < pieces; ++n ) {
            int walked = 0;
            while ( walked < 48 && opened.test( static_cast<size_t>( walked ) ) ) {
                walked += random( 1, 5 );
            }
            int drawn = 0;
            for ( int tries = 0; tries < 100; ++tries ) {
                drawn = random( 0, 0x2f );
                if ( !opened.test( static_cast<size_t>( drawn ) ) ) {
                    break;
                }
            }
            opened.set( static_cast<size_t>( walked < 48 ? walked : drawn ) );
        }
    }

    bool placeUltimateArtifact()
    {
        if ( getTileBytes( 0 ) == nullptr ) {
            return false;
        }
        // DAT_004a7bbc = number of human players. With one, the place must be farther (|dx| + |dy|) than a random distance
        // 3..70 (1..20 + 1..20 + 1..30) from player 0's first hero (+0x222).
        const PlayerColorsSet humans = Players::HumanColors();
        int humanCount = 0;
        PlayerColor first = PlayerColor::NONE;
        for ( const PlayerColor color : PlayerColorsVector( humans ) ) {
            if ( first == PlayerColor::NONE ) {
                first = color;
            }
            ++humanCount;
        }
        const Heroes * hero = nullptr;
        if ( first != PlayerColor::NONE ) {
            const VecHeroes & heroes = world.GetKingdom( first ).GetHeroes();
            if ( !heroes.empty() ) {
                hero = heroes.front();
            }
        }

        int x = random( 9, 0x3e );
        int y = random( 9, 0x3e );
        int distance = random( 1, 0x14 ) + random( 1, 0x14 ) + random( 1, 0x1e );
        // The original tries until it finds a place; the port stops after many tries (no original map comes near this).
        for ( int tries = 0; tries < 1000000; ++tries ) {
            const uint8_t * t = getTileBytes( y * 72 + x );
            // No lower and no upper object, ground tile above 0x13 (not water).
            if ( t[2] == 0xff && t[4] == 0xff && t[0] > 0x13 ) {
                bool accept = ( humanCount != 1 );
                if ( !accept ) {
                    const int hx = hero != nullptr ? hero->GetCenter().x : 0;
                    const int hy = hero != nullptr ? hero->GetCenter().y : 0;
                    accept = distance < std::abs( x - hx ) + std::abs( y - hy );
                }
                if ( accept ) {
                    // 0x16e58 = random 0..3: the ultimate artifact is HoMM1 artifact 0..3 (Book, Sword, Cloak, Wand).
                    const int engineArtifact = engineArtifactFromH1( random( 0, 3 ) );
                    world.setUltimateArtifactAt( y * 72 + x, Artifact( engineArtifact >= 0 ? engineArtifact : Artifact::UNKNOWN ) );
                    return true;
                }
            }
            distance = random( 1, 0x14 ) + random( 1, 0x14 ) + random( 1, 0x1e );
            x = random( 9, 0x3e );
            y = random( 9, 0x3e );
        }
        return true;
    }

    fheroes2::Image puzzleMapPicture()
    {
        fheroes2::Image picture( 0x1c0, 0x1c0 );
        picture._disableTransformLayer();
        fheroes2::Fill( picture, 0, 0, picture.width(), picture.height(), 0 );
        const std::vector<std::array<uint8_t, 10>> tiles = gameTileBytes();
        const int32_t ultimate = world.GetUltimateArtifact().getPosition();
        if ( tiles.empty() || ultimate < 0 ) {
            return picture;
        }
        const int ux = ultimate % 72;
        const int uy = ultimate / 72;

        // FUN_0045e89c: a = (x + y) % 3 - 1, b = (5y + 2x) % 3 - 1. When (x + y) % 3 == 1 the code that would push a further sees a = 0
        // and changes nothing; otherwise, when x + y is odd, b is pushed one further from 0 (1 -> 2, -1 -> -2).
        int a = ( ux + uy ) % 3 - 1;
        int b = ( 5 * uy + 2 * ux ) % 3 - 1;
        if ( ( ux + uy ) % 3 == 1 ) {
            if ( a > 0 ) {
                ++a;
            }
            else if ( a < 0 ) {
                --a;
            }
        }
        else if ( ( ux + uy ) % 2 == 1 ) {
            if ( b > 0 ) {
                ++b;
            }
            else if ( b < 0 ) {
                --b;
            }
        }
        const int startX = ux - 7 + a;
        const int startY = uy - 7 + b;

        const std::vector<fheroes2::Image> & stone = getTil( "ston.til" );
        const std::vector<fheroes2::Sprite> & route = getIcn( "route.icn" );

        // FUN_0045ec76: 15 x 15 tiles of 32 pixels from screen (0, 0); the border (FUN_00464aa8) then covers 16 pixels on every side,
        // so the picture (screen 0x10..0x1d0) starts 16 pixels into the first tile.
        auto put = [&picture]( const fheroes2::Image * image, const int32_t x, const int32_t y ) {
            if ( image != nullptr && !image->empty() ) {
                fheroes2::Blit( *image, picture, x - 0x10, y - 0x10 );
            }
        };
        auto putSprite = [&picture]( const fheroes2::Sprite * s, const int32_t x, const int32_t y ) {
            if ( s != nullptr && !s->empty() ) {
                fheroes2::Blit( *s, picture, x + s->x() - 0x10, y + s->y() - 0x10 );
            }
        };
        auto shown = []( const uint32_t set ) { return set == 4 || set == 8 || set == 9; };

        for ( int row = 0; row < 15; ++row ) {
            for ( int col = 0; col < 15; ++col ) {
                const int mx = startX + col;
                const int my = startY + row;
                const int32_t px = col << 5;
                const int32_t py = row << 5;
                // FUN_00458445( x, y, col, row, 1, 1, 0 ).
                if ( mx < 0 || my < 0 || mx > 0x47 || my > 0x47 ) {
                    // Outside the map: ston.til, edge frames 0x10..0x23, else ((y + 16) & 3) x 4 + ((x + 16) & 3).
                    int frame = -1;
                    if ( mx == -1 ) {
                        frame = ( my == -1 ) ? 0x10 : ( my == 0x48 ? 0x13 : ( my >= 0 && my < 0x48 ? ( my & 3 ) + 0x20 : -1 ) );
                    }
                    else if ( mx == 0x48 ) {
                        frame = ( my == -1 ) ? 0x11 : ( my == 0x48 ? 0x12 : ( my >= 0 && my < 0x48 ? ( my & 3 ) + 0x18 : -1 ) );
                    }
                    else if ( my == -1 && mx >= 0 && mx < 0x48 ) {
                        frame = ( mx & 3 ) + 0x14;
                    }
                    else if ( my == 0x48 && mx >= 0 && mx < 0x48 ) {
                        frame = ( mx & 3 ) + 0x1c;
                    }
                    if ( frame < 0 ) {
                        frame = ( std::abs( my + 0x10 ) & 3 ) * 4 + ( std::abs( mx + 0x10 ) & 3 );
                    }
                    if ( static_cast<size_t>( frame ) < stone.size() ) {
                        put( &stone[frame], px, py );
                    }
                    continue;
                }
                const int32_t index = my * 72 + mx;
                const std::array<uint8_t, 10> & t = tiles[index];
                // Ground (byte 0, flips from byte 6), then objects drawn flat with it (byte 6 bit 0x80) - all sets, except obj32-07
                // frame 1 (skipped when the sixth argument is set, as here).
                put( getTerrainImage( index ), px, py );
                const uint32_t lowerSet = t[1] & 0xf;
                if ( ( t[6] & 0x80 ) != 0 && t[2] != 0xff && !( lowerSet == 7 && t[2] == 1 ) ) {
                    putSprite( setFrame( lowerSet, t[2] ), px, py );
                }
                // Then sets 4 (obj32-04), 8 (mountains) and 9 (trees) only: lower layer when not flat, upper layer.
                if ( ( t[6] & 0x80 ) == 0 && t[2] != 0xff && shown( lowerSet ) ) {
                    putSprite( setFrame( lowerSet, t[2] ), px, py );
                }
                if ( t[4] != 0xff && shown( t[3] & 0xf ) ) {
                    putSprite( setFrame( t[3] & 0xf, t[4] ), px, py );
                }
                // The ultimate artifact's tile: route.icn frame 0xd (a cross), 2 pixels lower.
                if ( mx == ux && my == uy && route.size() > 0xd ) {
                    putSprite( &route[0xd], px, py + 2 );
                }
            }
        }
        return picture;
    }
}
