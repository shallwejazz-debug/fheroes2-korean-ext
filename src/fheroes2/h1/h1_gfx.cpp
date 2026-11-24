// HoMM1 graphics layer (homm1-remaster project). See h1_gfx.h and docs/analysis note v44.
#include "h1_gfx.h"
#include "rand.h"

#include "screen.h"
#include "h1_ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>

#include "image.h"
#include "image_tool.h"
#include "pal.h"
#include "maps_fileinfo.h"
#include "settings.h"
#include "h1_mode.h"
#include "h1_rmg.h"

namespace
{
    // HoMM1 AGG: uint16 count, count x 14-byte entries (uint16 hash, uint32 offset, uint32 size, uint32 size),
    // names at the end, 15 bytes each (homm1-agg-analysis.md).
    struct H1Agg
    {
        bool loaded = false;
        bool present = false;
        std::map<std::string, std::vector<uint8_t>> files;
    };

    std::string lower( std::string s )
    {
        std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
        return s;
    }

    H1Agg & agg()
    {
        static H1Agg instance;
        if ( instance.loaded ) {
            return instance;
        }
        instance.loaded = true;

        // H1-INTENDED(D7): graphics from the DOS HEROES.AGG (256 colours), not the Windows AGG (docs/INTENDED_DIFFERENCES.md).
        const char * env = std::getenv( "H1_AGG" );
        const std::string path = ( env != nullptr && *env != '\0' ) ? env : "h1/HEROES.AGG";
        std::ifstream f( path, std::ios::binary );
        if ( !f ) {
            return instance;
        }
        const std::vector<uint8_t> d( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
        if ( d.size() < 2 ) {
            return instance;
        }
        const size_t count = d[0] | ( d[1] << 8 );
        if ( d.size() < 2 + count * 14 + count * 15 ) {
            return instance;
        }
        const size_t namesOffset = d.size() - count * 15;
        auto le32 = [&d]( size_t o ) { return static_cast<uint32_t>( d[o] | ( d[o + 1] << 8 ) | ( d[o + 2] << 16 ) | ( d[o + 3] << 24 ) ); };
        for ( size_t i = 0; i < count; ++i ) {
            const size_t e = 2 + i * 14;
            const uint32_t offset = le32( e + 2 );
            const uint32_t size = le32( e + 6 );
            std::string name;
            for ( size_t k = 0; k < 13 && d[namesOffset + i * 15 + k] != 0; ++k ) {
                name += static_cast<char>( d[namesOffset + i * 15 + k] );
            }
            if ( static_cast<size_t>( offset ) + size <= d.size() ) {
                instance.files[lower( name )] = std::vector<uint8_t>( d.begin() + offset, d.begin() + offset + size );
            }
        }
        instance.present = !instance.files.empty();
        return instance;
    }

    std::array<uint8_t, 256> h2ToH1{};
    bool remapReady = false;

    // Terrain cache for the current map.
    std::string terrainForMap;
    std::string terrainCopyFile; // H1-INTENDED(D16): a save's own tile bytes (random map), empty = the map's .h1t
    std::vector<uint8_t> terrainVariantBytes;
    std::vector<uint8_t> terrainBytes; // 72 x 72 x 10, row-major (index = y * 72 + x)
    std::map<int32_t, std::array<uint8_t, 10>> terrainPatches; // tile bytes the game changed (note v168)
    std::array<std::vector<fheroes2::Image>, 4> ground32; // HoMM1 ground32.til, 4 flip variants

    void loadGround32()
    {
        if ( !ground32[0].empty() ) {
            return;
        }
        const std::vector<uint8_t> & til = H1::getH1AggFile( "ground32.til" );
        if ( til.size() < 6 ) {
            return;
        }
        const size_t count = til[0] | ( til[1] << 8 );
        const int32_t width = til[2] | ( til[3] << 8 );
        const int32_t height = til[4] | ( til[5] << 8 );
        if ( 6 + count * width * height != til.size() ) {
            return;
        }
        fheroes2::decodeTILImages( til.data() + 6, count, width, height, ground32[0] );
        for ( uint32_t shape = 1; shape < 4; ++shape ) {
            ground32[shape].resize( count );
            for ( size_t i = 0; i < count; ++i ) {
                fheroes2::Image & image = ground32[shape][i];
                image._disableTransformLayer();
                image.resize( width, height );
                // Same meaning as the HoMM1 tile byte 6: bit 1 vertical flip, bit 2 horizontal flip (note v39).
                fheroes2::Flip( ground32[0][i], 0, 0, image, 0, 0, width, height, ( shape & 2 ) != 0, ( shape & 1 ) != 0 );
            }
        }
    }
}

namespace H1
{
    bool hasH1Data()
    {
        return agg().present;
    }

    bool isGraphicsMode()
    {
        return isH1Mode() && agg().present;
    }

    bool setMouseCursor( const std::string & mseName, const int frame )
    {
        const std::vector<uint8_t> & mse = getH1AggFile( mseName );
        if ( mse.size() < 15 || frame < 0 || frame >= mse[0] ) {
            return false;
        }
        std::string icn;
        for ( size_t i = 2; i < 15 && mse[i] != 0; ++i ) {
            icn.push_back( static_cast<char>( mse[i] ) );
        }
        const std::vector<fheroes2::Sprite> & sprites = getIcn( icn );
        const size_t at = 15 + static_cast<size_t>( frame ) * 8;
        if ( static_cast<size_t>( frame ) >= sprites.size() || at + 8 > mse.size() ) {
            return false;
        }
        const int hotX = mse[at + 4] | ( mse[at + 5] << 8 );
        const int hotY = mse[at + 6] | ( mse[at + 7] << 8 );
        // The hotspot is in the frame's cell (width x height of the .mse); the ICN picture sits in that cell at its own offset.
        const fheroes2::Sprite & spr = sprites[static_cast<size_t>( frame )];
        fheroes2::cursor().update( spr, hotX - spr.x(), hotY - spr.y() ); // the engine takes the hotspot inside the picture
        return true;
    }

    const std::vector<uint8_t> & getH1AggFile( const std::string & name )
    {
        static const std::vector<uint8_t> empty;
        // H1-INTENDED(D11): with the Korean files (h1/KOREAN.TXT, h1/HANGUL.BIN) a file in h1/ko/ (button pictures with Korean labels,
        // tools/korean/make_ko_buttons.py) is used instead of the one of HEROES.AGG.
        static const bool korean = std::ifstream( "h1/KOREAN.TXT" ).good() && std::ifstream( "h1/HANGUL.BIN" ).good();
        if ( korean ) {
            static std::map<std::string, std::vector<uint8_t>> replaced;
            const std::string key = lower( name );
            auto r = replaced.find( key );
            if ( r == replaced.end() ) {
                std::string upper = key;
                std::transform( upper.begin(), upper.end(), upper.begin(), []( const unsigned char c ) { return static_cast<char>( std::toupper( c ) ); } );
                std::ifstream f( "h1/ko/" + upper, std::ios::binary );
                std::vector<uint8_t> data;
                if ( f ) {
                    data.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
                }
                r = replaced.emplace( key, std::move( data ) ).first;
            }
            if ( !r->second.empty() ) {
                return r->second;
            }
        }
        {
            // H1-INTENDED(D16): extension mode pictures (new frames appended to original files, tools/extension/) in h1/ext/.
            static std::map<std::string, std::vector<uint8_t>> extFiles;
            const std::string key = lower( name );
            auto r = extFiles.find( key );
            if ( r == extFiles.end() ) {
                std::string upper = key;
                std::transform( upper.begin(), upper.end(), upper.begin(), []( const unsigned char c ) { return static_cast<char>( std::toupper( c ) ); } );
                std::ifstream f( "h1/ext/" + upper, std::ios::binary );
                std::vector<uint8_t> data;
                if ( f ) {
                    data.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
                }
                r = extFiles.emplace( key, std::move( data ) ).first;
            }
            if ( !r->second.empty() ) {
                return r->second;
            }
        }
        const H1Agg & a = agg();
        const auto it = a.files.find( lower( name ) );
        return it == a.files.end() ? empty : it->second;
    }

    const std::vector<uint8_t> & getPalette()
    {
        return getH1AggFile( "kb.pal" );
    }

    void buildRemapFromHoMM2Palette( const std::vector<uint8_t> & h2Palette )
    {
        const std::vector<uint8_t> & h1 = getPalette();
        if ( h2Palette.size() < 768 || h1.size() < 768 ) {
            return;
        }
        for ( int i = 0; i < 256; ++i ) {
            int best = 0;
            int bestDistance = 1 << 30;
            // Targets 224..255 are excluded: HoMM1 changes these colours while running (palette cycling, agg note).
            for ( int j = 0; j < 224; ++j ) {
                const int dr = h2Palette[i * 3] - h1[j * 3];
                const int dg = h2Palette[i * 3 + 1] - h1[j * 3 + 1];
                const int db = h2Palette[i * 3 + 2] - h1[j * 3 + 2];
                const int distance = dr * dr + dg * dg + db * db;
                if ( distance < bestDistance ) {
                    bestDistance = distance;
                    best = j;
                }
            }
            h2ToH1[i] = static_cast<uint8_t>( best );
        }
        // Inverse: each HoMM1 colour -> nearest HoMM2 colour (for the engine's colour tables).
        std::vector<uint8_t> h1ToH2( 256 );
        for ( int j = 0; j < 256; ++j ) {
            int best = 0;
            int bestDistance = 1 << 30;
            for ( int i = 0; i < 256; ++i ) {
                const int dr = h2Palette[i * 3] - h1[j * 3];
                const int dg = h2Palette[i * 3 + 1] - h1[j * 3 + 1];
                const int db = h2Palette[i * 3 + 2] - h1[j * 3 + 2];
                const int distance = dr * dr + dg * dg + db * db;
                if ( distance < bestDistance ) {
                    bestDistance = distance;
                    best = i;
                }
            }
            h1ToH2[j] = static_cast<uint8_t>( best );
        }
        PAL::remapTablesToNewPalette( std::vector<uint8_t>( h2ToH1.begin(), h2ToH1.end() ), h1ToH2 );
        fheroes2::useColorTablesForCurrentPalette( std::vector<uint8_t>( h2ToH1.begin(), h2ToH1.end() ), h1ToH2 );
        remapReady = true;
    }

    const std::vector<uint8_t> & darkenTable()
    {
        // DOS HEROES.EXE file offset 0x126068 (matches the Windows table 0x4a3e2c through the DOS -> Windows colour table 0x492b18 in 205 of
        // 256 entries; the rest are the Windows reserved colours), note v108.
        static const std::vector<uint8_t> table{ 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
          21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 31, 31, 31, 31, 31,
          37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
          53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 63, 63, 63, 63, 63,
          69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84,
          85, 86, 87, 87, 87, 87, 87, 87, 93, 94, 95, 96, 97, 98, 99, 100,
          101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 111, 111, 111, 111, 111,
          117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
          133, 134, 135, 135, 135, 135, 135, 135, 141, 142, 143, 144, 145, 146, 147, 148,
          149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 159, 159, 159, 159, 159,
          165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
          181, 182, 183, 183, 183, 183, 183, 183, 189, 190, 191, 192, 193, 194, 195, 196,
          197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 207, 207, 207, 207, 207,
          213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 223, 223, 223, 223, 223,
          198, 201, 202, 205, 123, 127, 131, 135, 237, 238, 239, 239, 239, 239, 239, 239,
          81, 82, 83, 84, 85, 73, 76, 71, 79, 83, 87, 254, 254, 254, 254, 255 };
        return table;
    }

    void remapHoMM2Image( fheroes2::Image & image )
    {
        if ( !remapReady || image.empty() ) {
            return;
        }
        uint8_t * data = image.image();
        const size_t size = static_cast<size_t>( image.width() ) * image.height();
        for ( size_t i = 0; i < size; ++i ) {
            data[i] = h2ToH1[data[i]];
        }
    }

    const uint8_t * getTileBytes( const int32_t tileIndex )
    {
        if ( !isGraphicsMode() ) {
            return nullptr;
        }
        const std::string & mapFile = Settings::Get().getCurrentMapInfo().filename;
        if ( mapFile != terrainForMap ) {
            terrainForMap = mapFile;
            terrainBytes.clear();
            std::ifstream f( terrainCopyFile.empty() ? mapFile + ".h1t" : terrainCopyFile, std::ios::binary );
            if ( f ) {
                terrainBytes.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
            }
            // Sizes by version: tiles only; + castle list; + placed mask; + header bytes and description (v53/v54, longer).
            // (v53-v58 accepted only the first three sizes, so the HoMM1 map drawing was off with the newer files - fixed in v59.)
            if ( terrainBytes.size() != 72 * 72 * 10 && terrainBytes.size() != 72 * 72 * 10 + 36 * 3 && terrainBytes.size() < 72 * 72 * 10 + 36 * 3 + 72 * 72 ) {
                terrainBytes.clear();
            }
            // The original runs FUN_00415a33 on the map bytes at new game (note v192). The flags depend only on the bytes, so they are
            // put on every time the map bytes are read; tiles the game changed afterwards (patches) are written over them.
            if ( terrainBytes.size() >= 72 * 72 * 10 && terrainVariantBytes.size() == 72 * 72 ) {
                for ( size_t i = 0; i < 72 * 72; ++i ) {
                    uint8_t & b = terrainBytes[i * 10];
                    if ( b % 20 > 3 ) {
                        b = static_cast<uint8_t>( ( b & 0xfc ) + ( terrainVariantBytes[i] & 3 ) );
                    }
                }
            }
            if ( terrainBytes.size() >= 72 * 72 * 10 ) {
                applyNewGameTileFlags( terrainBytes.data(), true );
            }
            for ( const auto & [index, bytes] : terrainPatches ) {
                if ( static_cast<size_t>( index ) * 10 + 10 <= terrainBytes.size() ) {
                    std::copy( bytes.begin(), bytes.end(), terrainBytes.begin() + static_cast<std::ptrdiff_t>( index ) * 10 );
                }
            }
        }
        if ( terrainBytes.empty() || tileIndex < 0 || tileIndex >= 72 * 72 ) {
            return nullptr;
        }
        return &terrainBytes[static_cast<size_t>( tileIndex ) * 10];
    }

    void mountainEdgeFlags( uint8_t * tiles, const int32_t x, const int32_t y )
    {
        // FUN_0041584f: a tile with no lower picture (byte 2 = 0xff) but an upper one (byte 4), mountain kind 0x35 / 0x39 or 0x37 / 0x3b.
        // FUN_004157e0 groups 0x34..0x37 -> 0x34, 0x38..0x3c -> 0x38. Its right neighbour (x + 1) of the same group: 0x35 / 0x39 blocked,
        // 0x37 / 0x3b move the upper picture down; of another group the other way round. Nothing at the last column.
        mountainEdgeFlagsOne( tiles + ( y * 72 + x ) * 10, ( x + 1 < 72 ) ? tiles[( y * 72 + x + 1 ) * 10 + 8] : -1 );
    }

    void mountainEdgeFlagsOne( uint8_t * t, const int rightKind )
    {
        auto group = []( const int kind ) { return ( kind >= 0x34 && kind <= 0x37 ) ? 0x34 : ( ( kind >= 0x38 && kind <= 0x3c ) ? 0x38 : kind ); };
        if ( t[2] != 0xff || t[4] == 0xff ) {
            return;
        }
        const int kind = t[8];
        const bool blockIfSame = ( kind == 0x35 || kind == 0x39 );
        if ( !blockIfSame && kind != 0x37 && kind != 0x3b ) {
            return;
        }
        if ( rightKind < 0 ) {
            return;
        }
        const bool same = group( kind ) == group( rightKind );
        if ( same == blockIfSame ) {
            t[7] = static_cast<uint8_t>( t[7] | 0x80 );
        }
        else {
            t[2] = t[4];
            t[1] = t[3];
            t[3] = 0;
            t[4] = 0xff;
        }
    }

    void takeOffObject( uint8_t * t, const int32_t tileIndex )
    {
        // FUN_00402f1a: byte 8 = 0, byte 2 = 0xff, byte 6 bit 4 off; a kind kept in byte 7 (low 7 bits 1..0x7e: the object under the
        // taken one) comes back as byte 8, byte 7 loses it, byte 9 = byte 9 of a tile around with that kind (the last one found, columns
        // x - 1..x + 1, in each rows y - 1..y + 1), and FUN_0041584f runs on the tile.
        t[8] = 0;
        t[2] = 0xff;
        if ( t[6] & 4 ) {
            t[6] = static_cast<uint8_t>( t[6] - 4 );
        }
        const int under = t[7] & 0x7f;
        if ( under == 0 || under >= 0x7f ) {
            return;
        }
        t[8] = static_cast<uint8_t>( under );
        t[7] = static_cast<uint8_t>( t[7] - under );
        const int32_t x = tileIndex % 72;
        const int32_t y = tileIndex / 72;
        for ( int32_t cx = x - 1; cx <= x + 1; ++cx ) {
            for ( int32_t cy = y - 1; cy <= y + 1; ++cy ) {
                if ( cx < 0 || cx >= 72 || cy < 0 || cy >= 72 ) {
                    continue;
                }
                const uint8_t * n = getTileBytes( cy * 72 + cx );
                if ( n != nullptr && n[8] == t[8] ) {
                    t[9] = n[9];
                }
            }
        }
        const uint8_t * right = ( x + 1 < 72 ) ? getTileBytes( tileIndex + 1 ) : nullptr;
        mountainEdgeFlagsOne( t, right != nullptr ? right[8] : -1 );
    }

    void applyNewGameTileFlags( uint8_t * tiles, const bool keepLazyRandom )
    {
        if ( keepLazyRandom ) {
            // FUN_0041c1cc runs before FUN_00415a33: for random castles (0xc0 / 0xc1) FUN_0041ace5 turns, on the 4 x 3 tiles x-2..x+1,
            // y-2..y, byte 8 with kind 0 or above 0x30 into 0x28 (byte 9 = castle number: kept with the drawing bytes, h1_map_render)
            // and gives the others byte 7 |= 0x28; then the entrance's byte 8 |= 0x80 (so 0xc1 -> 0xa8). Its race choice and pictures
            // are the engine's. Until v194 the port did this after the flags below, which then saw the old kinds (note v195).
            // FUN_0041c1cc( 1 ) does the 0xc1 castles first, then FUN_0041c1cc( 0 ) goes through the map (rows y, columns x) and turns
            // each random tile into its object as it comes: a random artifact, resource, monster or mine met before a 0xc0 castle has
            // a kind of 0x30 or less by then (byte 7 |= 0x28 there), one met after it is still 0x3d..0x46 (it becomes 0x28). The port
            // leaves the random tiles to the engine, so their kinds at that moment are worked out here.
            std::vector<uint8_t> done( 72 * 72, 0 );
            auto isRandom = []( const uint8_t b ) { return b == 0xbd || b == 0xbe || b == 0xbf || ( b >= 0xc2 && b <= 0xc6 ); };
            auto footprint = [tiles, &done]( const int32_t x, const int32_t y ) {
                for ( int32_t dy = -2; dy <= 0; ++dy ) {
                    for ( int32_t dx = -2; dx <= 1; ++dx ) {
                        if ( x + dx < 0 || y + dy < 0 || x + dx >= 72 || y + dy >= 72 ) {
                            continue;
                        }
                        const int32_t k = ( y + dy ) * 72 + x + dx;
                        uint8_t * t = tiles + k * 10;
                        const int kind = done[k] ? 0x30 : ( t[8] & 0x7f );
                        if ( kind == 0 || kind > 0x30 ) {
                            t[8] = 0x28;
                        }
                        else {
                            t[7] = static_cast<uint8_t>( t[7] | 0x28 );
                        }
                    }
                }
                tiles[( y * 72 + x ) * 10 + 8] |= 0x80;
            };
            for ( int32_t y = 0; y < 72; ++y ) {
                for ( int32_t x = 0; x < 72; ++x ) {
                    if ( tiles[( y * 72 + x ) * 10 + 8] == 0xc1 ) {
                        footprint( x, y );
                    }
                }
            }
            for ( int32_t y = 0; y < 72; ++y ) {
                for ( int32_t x = 0; x < 72; ++x ) {
                    const uint8_t b = tiles[( y * 72 + x ) * 10 + 8];
                    if ( b == 0xc0 ) {
                        footprint( x, y );
                    }
                    else if ( isRandom( b ) ) {
                        done[y * 72 + x] = 1;
                    }
                }
            }
        }
        // FUN_00415a33. Its first loop (rows y, columns x) writes contents bytes (byte 9, kept with the engine tiles: initObjectValues)
        // and changes byte 8 in three cases: 0x2c becomes 0xac; 0xa3 without kind 0x23 on its left (x - 1) and 0xaa without kind 0x2a
        // on both sides (or in the first / last column) lose bit 0x80.
        auto kindAt = [tiles]( const int32_t x, const int32_t y ) { return tiles[( y * 72 + x ) * 10 + 8] & 0x7f; };
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                uint8_t * t = tiles + ( y * 72 + x ) * 10;
                if ( t[8] == 0x2c ) {
                    t[8] = 0xac;
                }
                else if ( t[8] == 0xa3 ) {
                    if ( !( x > 0 && x < 71 && kindAt( x - 1, y ) == 0x23 ) ) {
                        t[8] = 0x23;
                    }
                }
                else if ( t[8] == 0xaa ) {
                    if ( x < 1 || x > 70 || kindAt( x - 1, y ) != 0x2a || kindAt( x + 1, y ) != 0x2a ) {
                        t[8] = 0x2a;
                    }
                }
            }
        }
        // Second loop, rows y then columns x.
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                uint8_t * t = tiles + ( y * 72 + x ) * 10;
                if ( t[2] != 0xff && t[4] != 0xff ) {
                    const int lower = t[1] & 0x0f;
                    const int upper = t[3] & 0x0f;
                    if ( ( lower == 8 || lower == 9 ) && ( upper == 8 || upper == 9 ) ) {
                        t[7] = static_cast<uint8_t>( t[7] | 0x80 );
                    }
                }
                mountainEdgeFlags( tiles, x, y );
                if ( ( x == 0 || y == 0 || x == 71 || y == 71 ) && t[8] > 0x33 && t[8] < 0x3d ) {
                    t[7] = static_cast<uint8_t>( t[7] | 0x80 );
                }
            }
        }
        // Third loop: kind 0x32 gets byte 6 bit 0x80; active kinds other than these lose bit 0x80.
        static const uint8_t visited[] = { 1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   0xb,  0xc,  0xd,  0xe,  0xf,  0x10,
                                           0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x20, 0x21,
                                           0x22, 0x23, 0x24, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2f, 0x30, 0x31, 0x3d, 0x3e, 0x3f };
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                uint8_t * t = tiles + ( y * 72 + x ) * 10;
                if ( t[8] == 0x32 ) {
                    t[6] = static_cast<uint8_t>( t[6] | 0x80 );
                }
                if ( ( t[8] & 0x80 ) == 0 ) {
                    continue;
                }
                if ( keepLazyRandom && t[8] >= 0xc0 && t[8] <= 0xc6 ) {
                    continue;
                }
                const uint8_t kind = t[8] & 0x7f;
                if ( std::find( std::begin( visited ), std::end( visited ), kind ) == std::end( visited ) ) {
                    t[8] = kind;
                }
            }
        }
    }

    void patchTileBytes( const int32_t tileIndex, const std::array<uint8_t, 10> & bytes )
    {
        if ( tileIndex < 0 || tileIndex >= 72 * 72 ) {
            return;
        }
        terrainPatches[tileIndex] = bytes;
        if ( getTileBytes( 0 ) != nullptr ) {
            std::copy( bytes.begin(), bytes.end(), terrainBytes.begin() + static_cast<std::ptrdiff_t>( tileIndex ) * 10 );
        }
    }

    const std::map<int32_t, std::array<uint8_t, 10>> & tilePatches()
    {
        return terrainPatches;
    }

    void saveTileBytesCopy( const std::string & path )
    {
        const std::string & mapFile = Settings::Get().getCurrentMapInfo().filename;
        const size_t slash = mapFile.find_last_of( "/\\" );
        const std::string base = slash == std::string::npos ? mapFile : mapFile.substr( slash + 1 );
        if ( base.compare( 0, std::char_traits<char>::length( randomMapBase ), randomMapBase ) != 0 ) {
            std::remove( path.c_str() );
            return;
        }
        std::vector<char> bytes;
        {
            std::ifstream in( terrainCopyFile.empty() ? mapFile + ".h1t" : terrainCopyFile, std::ios::binary );
            bytes.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
        }
        if ( bytes.empty() ) {
            return;
        }
        std::ofstream out( path, std::ios::binary | std::ios::trunc );
        out.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
        out.close();
        terrainCopyFile = path; // the copy is now the source (the map file may be made again)
    }

    void useTileBytesCopy( const std::string & path )
    {
        std::ifstream in( path, std::ios::binary );
        terrainCopyFile = in ? path : std::string();
        terrainForMap.clear();
        terrainBytes.clear();
    }

    void resetTilePatches()
    {
        terrainCopyFile.clear();
        terrainPatches.clear();
        terrainForMap.clear();
        terrainBytes.clear();
        terrainVariantBytes.clear();
    }

    void newGameTerrainVariants()
    {
        std::vector<uint8_t> variants( 72 * 72 );
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                // FUN_0041d268 draws FUN_00476010( 0, 3 ) only for tiles with n % 20 > 3; the others take none.
                const uint8_t * t = getTileBytes( y * 72 + x );
                if ( t != nullptr && t[0] % 20 > 3 ) {
                    variants[y * 72 + x] = static_cast<uint8_t>( Rand::Get( 0, 3 ) );
                }
            }
        }
        setTerrainVariants( std::move( variants ) );
    }

    const std::vector<uint8_t> & terrainVariants()
    {
        return terrainVariantBytes;
    }

    void setTerrainVariants( std::vector<uint8_t> variants )
    {
        terrainVariantBytes = std::move( variants );
        terrainForMap.clear();
        terrainBytes.clear();
    }

    bool wasPlaced( const int32_t tileIndex )
    {
        if ( terrainPatches.count( tileIndex ) > 0 ) {
            return true;
        }
        if ( getTileBytes( 0 ) == nullptr || terrainBytes.size() < 72 * 72 * 10 + 36 * 3 + 72 * 72 || tileIndex < 0 || tileIndex >= 72 * 72 ) {
            // Older .h1t without the mask: treat every tile as placed.
            return true;
        }
        return terrainBytes[72 * 72 * 10 + 36 * 3 + static_cast<size_t>( tileIndex )] != 0;
    }

    int getCastleSlot( const int32_t tileIndex, int * type )
    {
        if ( getTileBytes( 0 ) == nullptr || terrainBytes.size() < 72 * 72 * 10 + 36 * 3 ) {
            return -1;
        }
        const uint8_t * list = &terrainBytes[72 * 72 * 10];
        for ( int i = 0; i < 36; ++i ) {
            if ( list[i * 3] != 0xff && list[i * 3 + 1] * 72 + list[i * 3] == tileIndex ) {
                if ( type != nullptr ) {
                    *type = list[i * 3 + 2];
                }
                return i;
            }
        }
        return -1;
    }

    int getMineKind( const int32_t tileIndex )
    {
        if ( getTileBytes( 0 ) == nullptr ) {
            return -1;
        }
        size_t offset = 72 * 72 * 10 + 36 * 3 + 72 * 72 + 2;
        while ( offset < terrainBytes.size() && terrainBytes[offset] != 0 ) {
            ++offset;
        }
        ++offset;
        if ( terrainBytes.size() < offset + 36 * 3 ) {
            return -1;
        }
        for ( int slot = 0; slot < 36; ++slot ) {
            const uint8_t * rec = &terrainBytes[offset + static_cast<size_t>( slot ) * 3];
            if ( rec[0] != 0xff && rec[1] * 72 + rec[0] == tileIndex ) {
                return rec[2];
            }
        }
        return -1;
    }

    int getLoopSound( const int32_t tileIndex )
    {
        if ( getTileBytes( 0 ) == nullptr || tileIndex < 0 || tileIndex >= 72 * 72 ) {
            return -1;
        }
        const size_t tableSize = 72 * 72;
        if ( terrainBytes.size() < tableSize + 4 ) {
            return -1;
        }
        const size_t start = terrainBytes.size() - tableSize;
        if ( std::memcmp( &terrainBytes[start - 4], "LOOP", 4 ) != 0 ) {
            return -1;
        }
        const uint8_t v = terrainBytes[start + static_cast<size_t>( tileIndex )];
        return v == 0xff ? -1 : v;
    }

    std::vector<HeroRecord> mapHeroRecords()
    {
        std::vector<HeroRecord> list;
        if ( getTileBytes( 0 ) == nullptr ) {
            return list;
        }
        // After the mine list (see getListPosition): "HERO", a count, then per record x, y and 0x19 bytes.
        size_t offset = 72 * 72 * 10 + 36 * 3 + 72 * 72 + 2;
        while ( offset < terrainBytes.size() && terrainBytes[offset] != 0 ) {
            ++offset;
        }
        offset += 1 + 36 * 3;
        if ( terrainBytes.size() < offset + 5 || std::memcmp( &terrainBytes[offset], "HERO", 4 ) != 0 ) {
            return list;
        }
        const int count = terrainBytes[offset + 4];
        offset += 5;
        for ( int i = 0; i < count && offset + 2 + 0x19 <= terrainBytes.size(); ++i ) {
            HeroRecord r;
            r.x = terrainBytes[offset];
            r.y = terrainBytes[offset + 1];
            std::copy_n( &terrainBytes[offset + 2], 0x19, r.bytes.begin() );
            list.push_back( r );
            offset += 2 + 0x19;
        }
        return list;
    }

    bool getListPosition( const bool mines, const int slot, int32_t & x, int32_t & y )
    {
        if ( getTileBytes( 0 ) == nullptr || slot < 0 || slot >= 36 ) {
            return false;
        }
        size_t offset = 72 * 72 * 10;
        if ( mines ) {
            // After the placed mask: 2 header bytes, the description up to its zero byte, then the mine list (v59 files).
            offset = 72 * 72 * 10 + 36 * 3 + 72 * 72 + 2;
            while ( offset < terrainBytes.size() && terrainBytes[offset] != 0 ) {
                ++offset;
            }
            ++offset;
        }
        if ( terrainBytes.size() < offset + 36 * 3 ) {
            return false;
        }
        const uint8_t * rec = &terrainBytes[offset + static_cast<size_t>( slot ) * 3];
        if ( rec[0] == 0xff ) {
            return false;
        }
        x = rec[0];
        y = rec[1];
        return true;
    }

    const std::vector<fheroes2::Image> & getTil( const std::string & name )
    {
        static std::map<std::string, std::vector<fheroes2::Image>> cache;
        auto it = cache.find( name );
        if ( it != cache.end() ) {
            return it->second;
        }
        std::vector<fheroes2::Image> & images = cache[name];
        const std::vector<uint8_t> & til = getH1AggFile( name );
        if ( til.size() >= 6 ) {
            const size_t count = til[0] | ( til[1] << 8 );
            const int32_t width = til[2] | ( til[3] << 8 );
            const int32_t height = til[4] | ( til[5] << 8 );
            if ( 6 + count * width * height <= til.size() ) {
                fheroes2::decodeTILImages( til.data() + 6, count, width, height, images );
            }
        }
        return images;
    }

    const fheroes2::Image * getTerrainImage( const int32_t tileIndex )
    {
        const uint8_t * t = getTileBytes( tileIndex );
        if ( t == nullptr ) {
            return nullptr;
        }
        loadGround32();
        const uint32_t index = t[0];
        const uint32_t shape = t[6] & 3;
        if ( index >= ground32[shape].size() ) {
            return nullptr;
        }
        return &ground32[shape][index];
    }

    const std::vector<fheroes2::Sprite> & getIcn( const std::string & name )
    {
        return getIcnFrames( name, false );
    }

    const std::vector<fheroes2::Sprite> & getIcnMono( const std::string & name )
    {
        return getIcnFrames( name, true );
    }

    const std::vector<fheroes2::Sprite> & getIcnFrames( const std::string & name, const bool forceMono )
    {
        static std::map<std::string, std::vector<fheroes2::Sprite>> cache;
        const std::string key = lower( name );
        const std::string cacheKey = forceMono ? key + "#mono" : key;
        auto it = cache.find( cacheKey );
        if ( it != cache.end() ) {
            return it->second;
        }
        std::vector<fheroes2::Sprite> & out = cache[cacheKey];
        const std::vector<uint8_t> & b = getH1AggFile( key );
        if ( b.size() < 6 ) {
            return out;
        }
        const size_t count = b[0] | ( b[1] << 8 );
        const uint32_t total = static_cast<uint32_t>( b[2] | ( b[3] << 8 ) | ( b[4] << 16 ) | ( b[5] << 24 ) );
        if ( b.size() < 6 + count * 12 ) {
            return out;
        }
        auto s16 = [&b]( size_t o ) { return static_cast<int16_t>( b[o] | ( b[o + 1] << 8 ) ); };
        auto u32 = [&b]( size_t o ) { return static_cast<uint32_t>( b[o] | ( b[o + 1] << 8 ) | ( b[o + 2] << 16 ) | ( b[o + 3] << 24 ) ); };
        out.resize( count );
        for ( size_t i = 0; i < count; ++i ) {
            const size_t h = 6 + i * 12;
            const int32_t x = s16( h );
            const int32_t y = s16( h + 2 );
            const int32_t w = static_cast<uint16_t>( s16( h + 4 ) );
            const int32_t hh = static_cast<uint16_t>( s16( h + 6 ) );
            const uint32_t off = u32( h + 8 );
            const uint32_t end = ( i + 1 < count ) ? u32( h + 12 + 8 ) : total;
            if ( w <= 0 || hh <= 0 || 6 + static_cast<size_t>( end ) > b.size() || off > end ) {
                continue;
            }
            const uint8_t * data = b.data() + 6 + off;
            const uint8_t * dataEnd = b.data() + 6 + end;

            // HoMM1 ICN pixel codes (agg note): 0x00 next row, 0x01-0x7F n pixels (n data bytes follow; a "mask" frame has no data bytes),
            // 0x80 end, 0x81-0xFF skip (n - 0x80). There is no type byte: the frame kind is the decoding that fits width and height.
            auto decode = [&]( const bool mono, fheroes2::Sprite * target ) {
                int32_t cx = 0;
                int32_t cy = 0;
                const uint8_t * p = data;
                while ( p < dataEnd ) {
                    const uint8_t c = *p++;
                    if ( c == 0 ) {
                        ++cy;
                        cx = 0;
                    }
                    else if ( c < 0x80 ) {
                        if ( !mono && p + c > dataEnd ) {
                            return false;
                        }
                        for ( uint32_t k = 0; k < c; ++k, ++cx ) {
                            if ( cx >= w || cy >= hh ) {
                                if ( target == nullptr ) {
                                    return false;
                                }
                                continue;
                            }
                            if ( target != nullptr ) {
                                const size_t pos = static_cast<size_t>( cy ) * w + cx;
                                target->image()[pos] = mono ? 0 : p[k];
                                target->transform()[pos] = 0;
                            }
                        }
                        if ( !mono ) {
                            p += c;
                        }
                    }
                    else if ( c == 0x80 ) {
                        return true;
                    }
                    else {
                        cx += c - 0x80;
                    }
                }
                return true;
            };
            // Text glyphs are always drawn by the mono routine (FUN_0047a5f0 -> FUN_0047d882), so fonts are read as mono (note v52).
            const bool mono = forceMono || ( !decode( false, nullptr ) && decode( true, nullptr ) );
            fheroes2::Sprite sprite( w, hh, x, y );
            sprite.reset(); // all transparent
            decode( mono, &sprite );
            out[i] = std::move( sprite );
        }
        return out;
    }

    void replaceWithH1Frames( const char * icnFileName, std::vector<fheroes2::Sprite> & frames )
    {
        if ( !isGraphicsMode() || icnFileName == nullptr ) {
            return;
        }
        // HoMM1 / HoMM2 frame counts compared file by file (note v48): equal for these, so HoMM2 kept the layout.
        // shadow32: HoMM1 56 frames, HoMM2 86 -> only the first 56 are replaced.
        static const char * const checked[] = { "kngt32.icn",   "barb32.icn",   "sorc32.icn",   "wrlk32.icn",   "boat32.icn",   "shadow32.icn",
                                                 "b-flag32.icn", "g-flag32.icn", "r-flag32.icn", "y-flag32.icn", "b-bflg32.icn", "g-bflg32.icn",
                                                 "r-bflg32.icn", "y-bflg32.icn", "stonback.icn", "scroll.icn",   "scroll2.icn",  "mobility.icn", "resource.icn", "hourglas.icn", "brcrest.icn" };
        const std::string name = lower( icnFileName );
        if ( name == "advbord.icn" && !frames.empty() ) {
            // Adventure screen border: HoMM1 bord.bmp (640 x 480, same frame layout as the HoMM2 border picture; note v56).
            const fheroes2::Image bord = H1::UI::readBmp( "bord.bmp" );
            if ( bord.width() == frames[0].width() && bord.height() == frames[0].height() ) {
                fheroes2::Sprite sprite( bord.width(), bord.height() );
                sprite.reset();
                fheroes2::Copy( bord, 0, 0, sprite, 0, 0, bord.width(), bord.height() );
                frames[0] = std::move( sprite );
            }
            return;
        }
        bool found = false;
        // Big hero portraits port0000..port0035.icn (101 x 93 in both games): hero slot n shows the Heroes I hero n (note v57).
        if ( name.size() == 12 && name.compare( 0, 4, "port" ) == 0 && name.compare( 8, 4, ".icn" ) == 0 && std::isdigit( static_cast<unsigned char>( name[4] ) ) ) {
            const int number = std::atoi( name.substr( 4, 4 ).c_str() );
            found = ( number >= 0 && number < 36 );
        }
        for ( const char * c : checked ) {
            if ( name == c ) {
                found = true;
            }
        }
        if ( !found ) {
            return;
        }
        const std::vector<fheroes2::Sprite> & h1 = getIcn( name );
        for ( size_t i = 0; i < frames.size() && i < h1.size(); ++i ) {
            frames[i] = h1[i];
        }
    }
}
