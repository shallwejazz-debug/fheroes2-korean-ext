// H2-KOREAN: HD backgrounds, see h2_ko_hd.h
#include "h2_ko_hd.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "h2_hd_layer.h"
#include "icn.h"
#include "image_palette.h"
#include "zzlib.h"

namespace
{
    int blend = 50; // percent of the AI picture; the rest is a smooth enlargement of the original

    struct Plane
    {
        int32_t width{ 0 };
        int32_t height{ 0 };
        std::vector<uint8_t> data;
    };

    uint32_t readU32( const std::vector<uint8_t> & in, size_t & pos )
    {
        if ( pos + 4 > in.size() ) {
            pos = in.size() + 1;
            return 0;
        }
        const uint32_t v = in[pos] | ( in[pos + 1] << 8 ) | ( in[pos + 2] << 16 ) | ( static_cast<uint32_t>( in[pos + 3] ) << 24 );
        pos += 4;
        return v;
    }

    bool readPlane( const std::vector<uint8_t> & in, size_t & pos, Plane & plane )
    {
        plane.width = static_cast<int32_t>( readU32( in, pos ) );
        plane.height = static_cast<int32_t>( readU32( in, pos ) );
        const uint32_t length = readU32( in, pos );
        if ( pos > in.size() || pos + length > in.size() || plane.width <= 0 || plane.height <= 0 ) {
            return false;
        }
        plane.data = Compression::unzipData( in.data() + pos, length, static_cast<size_t>( plane.width ) * plane.height );
        pos += length;
        if ( plane.data.size() != static_cast<size_t>( plane.width ) * plane.height ) {
            return false;
        }
        // undo the left-neighbour filter of pack_hd.py
        for ( int32_t y = 0; y < plane.height; ++y ) {
            uint8_t * row = plane.data.data() + static_cast<size_t>( y ) * plane.width;
            for ( int32_t x = 1; x < plane.width; ++x ) {
                row[x] = static_cast<uint8_t>( row[x] + row[x - 1] );
            }
        }
        return true;
    }

    // Reads h2/hd/<NAME>.h2hd: "H2HD", width, height (3x the original), then Y, Cb, Cr planes (Cb, Cr at half size)
    std::shared_ptr<std::vector<uint32_t>> readFile( const std::string & path, int32_t & width, int32_t & height )
    {
        std::ifstream file( path, std::ios::binary );
        if ( !file ) {
            return nullptr;
        }
        const std::vector<uint8_t> in( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
        if ( in.size() < 12 || std::memcmp( in.data(), "H2HD", 4 ) != 0 ) {
            return nullptr;
        }
        size_t pos = 4;
        width = static_cast<int32_t>( readU32( in, pos ) );
        height = static_cast<int32_t>( readU32( in, pos ) );
        Plane y;
        Plane cb;
        Plane cr;
        if ( !readPlane( in, pos, y ) || !readPlane( in, pos, cb ) || !readPlane( in, pos, cr ) || y.width != width || y.height != height ) {
            return nullptr;
        }
        auto out = std::make_shared<std::vector<uint32_t>>( static_cast<size_t>( width ) * height );
        const auto clamp = []( const double v ) { return static_cast<uint32_t>( std::clamp( v, 0.0, 255.0 ) + 0.5 ); };
        for ( int32_t py = 0; py < height; ++py ) {
            for ( int32_t px = 0; px < width; ++px ) {
                const double Y = y.data[static_cast<size_t>( py ) * width + px];
                const int32_t cx = std::min( px / 2, cb.width - 1 );
                const int32_t cy = std::min( py / 2, cb.height - 1 );
                const double Cb = cb.data[static_cast<size_t>( cy ) * cb.width + cx] - 128.0;
                const double Cr = cr.data[static_cast<size_t>( cy ) * cr.width + cx] - 128.0;
                const uint32_t r = clamp( Y + 1.402 * Cr );
                const uint32_t g = clamp( Y - 0.344136 * Cb - 0.714136 * Cr );
                const uint32_t b = clamp( Y + 1.772 * Cb );
                ( *out )[static_cast<size_t>( py ) * width + px] = ( r << 16 ) | ( g << 8 ) | b;
            }
        }
        return out;
    }

    // The AI picture mixed with a smooth (bilinear) enlargement of the original, 'blend' percent AI
    void mixWithOriginal( std::vector<uint32_t> & big, const int32_t bigWidth, const int32_t bigHeight, const fheroes2::Sprite & low )
    {
        const fheroes2::RGB * palette = fheroes2::getRGBGamePalette();
        const int32_t w = low.width();
        const int32_t h = low.height();
        const uint8_t * image = low.image();
        const int a = std::clamp( blend, 0, 100 );
        for ( int32_t y = 0; y < bigHeight; ++y ) {
            const double fy = std::clamp( ( y + 0.5 ) * h / bigHeight - 0.5, 0.0, h - 1.0 );
            const int32_t y0 = static_cast<int32_t>( fy );
            const int32_t y1 = std::min( y0 + 1, h - 1 );
            const double ty = fy - y0;
            for ( int32_t x = 0; x < bigWidth; ++x ) {
                const double fx = std::clamp( ( x + 0.5 ) * w / bigWidth - 0.5, 0.0, w - 1.0 );
                const int32_t x0 = static_cast<int32_t>( fx );
                const int32_t x1 = std::min( x0 + 1, w - 1 );
                const double tx = fx - x0;
                const fheroes2::RGB & c00 = palette[image[y0 * w + x0]];
                const fheroes2::RGB & c10 = palette[image[y0 * w + x1]];
                const fheroes2::RGB & c01 = palette[image[y1 * w + x0]];
                const fheroes2::RGB & c11 = palette[image[y1 * w + x1]];
                const auto lerp = [tx, ty]( const double v00, const double v10, const double v01, const double v11 ) {
                    return ( v00 * ( 1 - tx ) + v10 * tx ) * ( 1 - ty ) + ( v01 * ( 1 - tx ) + v11 * tx ) * ty;
                };
                const double sr = lerp( c00.r, c10.r, c01.r, c11.r );
                const double sg = lerp( c00.g, c10.g, c01.g, c11.g );
                const double sb = lerp( c00.b, c10.b, c01.b, c11.b );
                uint32_t & p = big[static_cast<size_t>( y ) * bigWidth + x];
                const auto mix = [a]( const double ai, const double smooth ) { return static_cast<uint32_t>( ( ai * a + smooth * ( 100 - a ) ) / 100.0 + 0.5 ); };
                p = ( mix( ( p >> 16 ) & 0xFF, sr ) << 16 ) | ( mix( ( p >> 8 ) & 0xFF, sg ) << 8 ) | mix( p & 0xFF, sb );
            }
        }
    }

    std::map<int, std::shared_ptr<const std::vector<uint32_t>>> cache; // by ICN; nullptr = no file
}

namespace H2Korean::Hd
{
    void showBackground( const Slot slot, const int icnId, const fheroes2::Sprite & low, const fheroes2::Point & screenPos )
    {
        if ( low.empty() ) {
            return;
        }
        auto it = cache.find( icnId );
        if ( it == cache.end() ) {
            std::string name = ICN::getIcnFileName( icnId );
            const size_t dot = name.find( '.' );
            if ( dot != std::string::npos ) {
                name.resize( dot );
            }
            int32_t width = 0;
            int32_t height = 0;
            std::shared_ptr<std::vector<uint32_t>> big = readFile( "h2/hd/" + name + "_000.h2hd", width, height );
            if ( big != nullptr
                 && ( width != low.width() * fheroes2::HDLayer::storedScale || height != low.height() * fheroes2::HDLayer::storedScale ) ) {
                big = nullptr;
            }
            if ( big != nullptr ) {
                mixWithOriginal( *big, width, height, low );
            }
            it = cache.emplace( icnId, big ).first;
        }
        if ( it->second == nullptr ) {
            hide( slot );
            return;
        }
        std::vector<uint8_t> indices( low.image(), low.image() + static_cast<size_t>( low.width() ) * low.height() );
        fheroes2::HDLayer::set( slot, { screenPos.x, screenPos.y, low.width(), low.height() }, std::move( indices ), it->second );
    }

    void hide( const Slot slot )
    {
        fheroes2::HDLayer::clear( slot );
    }

    bool toggle()
    {
        fheroes2::HDLayer::setEnabled( !fheroes2::HDLayer::isEnabled() );
        return fheroes2::HDLayer::isEnabled();
    }

    void applySetting( const bool enabled, const int blendPercent )
    {
        if ( blend != blendPercent ) {
            blend = blendPercent;
            cache.clear();
        }
        fheroes2::HDLayer::setEnabled( enabled );
    }
}
