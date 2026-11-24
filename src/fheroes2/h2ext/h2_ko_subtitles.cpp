// H2-KOREAN: see h2_ko_subtitles.h.
#include "h2_ko_subtitles.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <utility>

#include "h1_mode.h"
#include "image.h"
#include "math_base.h"
#include "screen.h"
#include "translations.h"
#include "ui_text.h"

namespace
{
    struct Line
    {
        uint32_t startMs;
        uint32_t endMs;
        std::string text;
    };

    const std::map<std::string, std::vector<Line>> & allLines()
    {
        static std::map<std::string, std::vector<Line>> lines;
        static bool loaded = false;
        if ( loaded ) {
            return lines;
        }
        loaded = true;
        std::ifstream in( "h2/KOREAN_SUBS.TXT", std::ios::binary );
        std::string row;
        while ( std::getline( in, row ) ) {
            if ( !row.empty() && row.back() == '\r' ) {
                row.pop_back();
            }
            if ( row.empty() || row[0] == '#' ) {
                continue;
            }
            const size_t a = row.find( '\t' );
            const size_t b = a == std::string::npos ? a : row.find( '\t', a + 1 );
            const size_t c = b == std::string::npos ? b : row.find( '\t', b + 1 );
            if ( c == std::string::npos ) {
                continue;
            }
            lines[row.substr( 0, a )].push_back(
                { static_cast<uint32_t>( std::stoul( row.substr( a + 1, b - a - 1 ) ) ), static_cast<uint32_t>( std::stoul( row.substr( b + 1, c - b - 1 ) ) ),
                  row.substr( c + 1 ) } );
        }
        return lines;
    }

    uint8_t extremeColour( const std::vector<uint8_t> & palette, const bool brightest )
    {
        uint8_t best = 0;
        int bestValue = brightest ? -1 : 1 << 20;
        // colours 0-9 and 246-255 are left alone: they can be cycling or system colours
        for ( int i = 10; i < 246 && static_cast<size_t>( i * 3 + 2 ) < palette.size(); ++i ) {
            const int r = palette[i * 3];
            const int g = palette[i * 3 + 1];
            const int b = palette[i * 3 + 2];
            // white: bright and grey (not a bright yellow or blue); black: dark
            const int value = brightest ? ( r + g + b ) - 2 * ( std::max( { r, g, b } ) - std::min( { r, g, b } ) ) : ( r + g + b );
            if ( brightest ? value > bestValue : value < bestValue ) {
                bestValue = value;
                best = static_cast<uint8_t>( i );
            }
        }
        return best;
    }
}

namespace H2Korean
{
    VideoSubtitles::VideoSubtitles( const std::vector<Video::VideoInfo> & infos )
    {
        if ( H1::isH1Mode() || !Translation::isKorean() ) {
            return;
        }
        const auto & lines = allLines();
        for ( const Video::VideoInfo & info : infos ) {
            const auto it = lines.find( info.fileName );
            if ( it == lines.end() ) {
                continue;
            }
            constexpr int32_t maxWidth = 600;
            for ( const Line & line : it->second ) {
                const fheroes2::Text text( line.text, fheroes2::FontType::normalWhite() );
                const int32_t textWidth = text.width( maxWidth );
                const int32_t textHeight = text.height( textWidth );

                // the text with a one-pixel outline, as the engine's own subtitles (Video::Subtitle)
                const uint8_t background = 36;
                fheroes2::Image image( textWidth + 2, textHeight + 2 );
                image.fill( background );
                text.draw( 1, 2, textWidth, image );
                fheroes2::ReplaceColorIdByTransformId( image, background, 1 );
                fheroes2::Blit( fheroes2::CreateContour( image, background ), image );

                Item item{ line.startMs, line.endMs, 0, 0, image.width(), image.height(), {} };
                item.mask.resize( static_cast<size_t>( image.width() ) * image.height(), 0 );
                for ( size_t i = 0; i < item.mask.size(); ++i ) {
                    if ( image.transform()[i] == 0 ) {
                        item.mask[i] = image.image()[i] == background ? 2 : 1;
                    }
                }
                item.x = ( fheroes2::Display::DEFAULT_WIDTH - item.width ) / 2;
                item.y = fheroes2::Display::DEFAULT_HEIGHT - 10 - item.height;
                _items.push_back( std::move( item ) );
            }
        }
    }

    void VideoSubtitles::render( fheroes2::Image & output, const fheroes2::Rect & frameRoi, const uint32_t timeMs, const std::vector<uint8_t> & palette ) const
    {
        if ( _items.empty() || palette.size() < 256 * 3 ) {
            return;
        }
        const uint8_t white = extremeColour( palette, true );
        const uint8_t black = extremeColour( palette, false );
        const int32_t outWidth = output.width();
        uint8_t * out = output.image();

        for ( const Item & item : _items ) {
            if ( timeMs < item.startMs || timeMs >= item.endMs ) {
                continue;
            }
            const int32_t left = frameRoi.x + item.x;
            const int32_t top = frameRoi.y + item.y;
            // half-dark band behind the text (every other pixel black)
            for ( int32_t y = top - 3; y < top + item.height + 3; ++y ) {
                for ( int32_t x = left - 8; x < left + item.width + 8; ++x ) {
                    if ( x >= 0 && y >= 0 && x < outWidth && y < output.height() && ( ( x + y ) & 1 ) == 0 ) {
                        out[y * outWidth + x] = black;
                    }
                }
            }
            for ( int32_t y = 0; y < item.height; ++y ) {
                for ( int32_t x = 0; x < item.width; ++x ) {
                    const uint8_t m = item.mask[static_cast<size_t>( y ) * item.width + x];
                    const int32_t ox = left + x;
                    const int32_t oy = top + y;
                    if ( m == 0 || ox < 0 || oy < 0 || ox >= outWidth || oy >= output.height() ) {
                        continue;
                    }
                    out[oy * outWidth + ox] = m == 1 ? white : black;
                }
            }
        }
    }
}
