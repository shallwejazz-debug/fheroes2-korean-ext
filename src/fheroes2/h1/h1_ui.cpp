// HoMM1 dialog layouts (.bin) and fonts (homm1-remaster project). See h1_ui.h.
#include "h1_ui.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstddef>

#include "h1_gfx.h"
#include "h1_spells.h"
#include "cursor.h"
#include "image.h"
#include "localevent.h"
#include "pal.h"
#include "screen.h"

namespace
{
    struct Font
    {
        int32_t lineHeight = 0; // +0x0e
        int32_t baseline = 0; // +0x10
        const std::vector<fheroes2::Sprite> * glyphs = nullptr;
    };

    const Font & getFont( const std::string & name )
    {
        static std::vector<std::pair<std::string, Font>> cache;
        for ( const auto & [n, f] : cache ) {
            if ( n == name ) {
                return f;
            }
        }
        Font font;
        const std::vector<uint8_t> & b = H1::getH1AggFile( name );
        if ( b.size() >= 4 + 13 ) {
            // FUN_0047a900: u16 line height, u16 baseline, 13-byte .icn name.
            font.lineHeight = b[0] | ( b[1] << 8 );
            font.baseline = b[2] | ( b[3] << 8 );
            std::string icn( reinterpret_cast<const char *>( b.data() + 4 ), 13 );
            icn = icn.substr( 0, icn.find( '\0' ) );
            font.glyphs = &H1::getIcnMono( icn );
        }
        cache.emplace_back( name, font );
        return cache.back().second;
    }

    // Glyph index of a character (FUN_0047aa30): char - 0x20, out of 0..0x5f -> 0x5f.
    int glyphIndex( const char c )
    {
        const int g = static_cast<signed char>( c ) - 0x20;
        return ( g < 0 || g > 0x5f ) ? 0x5f : g;
    }

    int32_t glyphWidth( const Font & font, const int g )
    {
        if ( font.glyphs == nullptr || g >= static_cast<int>( font.glyphs->size() ) ) {
            return 0;
        }
        return ( *font.glyphs )[g].width();
    }

    void drawMono( fheroes2::Image & dst, const fheroes2::Sprite & s, const int32_t x, const int32_t y, const uint8_t colour )
    {
        for ( int32_t sy = 0; sy < s.height(); ++sy ) {
            for ( int32_t sx = 0; sx < s.width(); ++sx ) {
                const size_t pos = static_cast<size_t>( sy ) * s.width() + sx;
                if ( !s.singleLayer() && s.transform()[pos] != 0 ) {
                    continue;
                }
                const int32_t dx = x + s.x() + sx;
                const int32_t dy = y + s.y() + sy;
                if ( dx < 0 || dy < 0 || dx >= dst.width() || dy >= dst.height() ) {
                    continue;
                }
                dst.image()[static_cast<size_t>( dy ) * dst.width() + dx] = colour;
                if ( !dst.singleLayer() ) {
                    dst.transform()[static_cast<size_t>( dy ) * dst.width() + dx] = 0;
                }
            }
        }
    }

    // FUN_0047aa30: one line; glyph 0 (space) is not drawn; advance = frame width + 1.
    void drawLine( fheroes2::Image & dst, const Font & font, const std::string & line, int32_t x, const int32_t y, const uint8_t colour )
    {
        for ( const char c : line ) {
            if ( c == '\0' ) {
                break;
            }
            const int g = glyphIndex( c );
            if ( g != 0 && font.glyphs != nullptr && g < static_cast<int>( font.glyphs->size() ) ) {
                drawMono( dst, ( *font.glyphs )[g], x, font.baseline + y, colour );
            }
            x += glyphWidth( font, g ) + 1;
        }
    }

    // H1-INTENDED(D11): Korean text (owner decision 2026-11-17). Not in the original: with h1/KOREAN.TXT (English <tab> Korean, UTF-8,
    // "\n" for a new line, %s / %d for values, %1..%9 to reorder them) and h1/HANGUL.BIN (tools/korean/make_hangul_bin.py: Hangul drawn
    // after the big font, 12 x 12 cells, and a 10 x 10 set for the small font) the texts of the Heroes I screens are shown in Korean.
    // Without the two files every text stays the original English and goes through the original drawing below unchanged.
    namespace Korean
    {
        struct Data
        {
            bool loaded = false;
            std::map<std::string, std::string> exact;
            std::vector<std::pair<std::vector<std::string>, std::string>> templates; // English pieces between %s / %d, Korean
            std::vector<std::string> kinds; // per template: the placeholder letters in order ("sd")
            std::vector<uint16_t> big; // 11172 x 12 rows
            std::vector<uint16_t> small; // 11172 x 10 rows
        };

        std::string unescape( const std::string & t )
        {
            std::string r;
            for ( size_t i = 0; i < t.size(); ++i ) {
                if ( t[i] == '\\' && i + 1 < t.size() && t[i + 1] == 'n' ) {
                    r += '\n';
                    ++i;
                }
                else {
                    r += t[i];
                }
            }
            return r;
        }

        const Data & data()
        {
            static Data d;
            if ( d.loaded ) {
                return d;
            }
            d.loaded = true;
            std::ifstream glyphs( "h1/HANGUL.BIN", std::ios::binary );
            std::ifstream table( "h1/KOREAN.TXT", std::ios::binary );
            if ( !glyphs || !table ) {
                return d;
            }
            char head[10] = {};
            glyphs.read( head, 10 );
            if ( std::string( head, 4 ) != "H1KO" ) {
                return d;
            }
            const size_t count = static_cast<uint8_t>( head[4] ) | ( static_cast<uint8_t>( head[5] ) << 8 );
            std::vector<char> raw( count * 22 * 2 );
            glyphs.read( raw.data(), static_cast<std::streamsize>( raw.size() ) );
            if ( glyphs.gcount() != static_cast<std::streamsize>( raw.size() ) ) {
                return d;
            }
            auto word = [&raw]( const size_t i ) { return static_cast<uint16_t>( static_cast<uint8_t>( raw[i * 2] ) | ( static_cast<uint8_t>( raw[i * 2 + 1] ) << 8 ) ); };
            d.big.resize( count * 12 );
            d.small.resize( count * 10 );
            for ( size_t i = 0; i < count * 12; ++i ) {
                d.big[i] = word( i );
            }
            for ( size_t i = 0; i < count * 10; ++i ) {
                d.small[i] = word( count * 12 + i );
            }
            std::string line;
            while ( std::getline( table, line ) ) {
                if ( !line.empty() && line.back() == '\r' ) {
                    line.pop_back();
                }
                const size_t tab = line.find( '\t' );
                if ( line.empty() || line[0] == '#' || tab == std::string::npos ) {
                    continue;
                }
                const std::string en = unescape( line.substr( 0, tab ) );
                const std::string ko = unescape( line.substr( tab + 1 ) );
                if ( en.find( "%s" ) == std::string::npos && en.find( "%d" ) == std::string::npos ) {
                    d.exact[en] = ko;
                    continue;
                }
                std::vector<std::string> pieces( 1 );
                std::string kinds;
                for ( size_t i = 0; i < en.size(); ++i ) {
                    if ( en[i] == '%' && i + 1 < en.size() && ( en[i + 1] == 's' || en[i + 1] == 'd' ) ) {
                        kinds += en[i + 1];
                        pieces.emplace_back();
                        ++i;
                    }
                    else {
                        pieces.back() += en[i];
                    }
                }
                d.templates.emplace_back( pieces, ko );
                d.kinds.push_back( kinds );
            }
            return d;
        }

        bool active()
        {
            return !data().big.empty();
        }

        // Match one template: literal pieces in order, a %d takes digits only, a %s takes the shortest text before the next piece.
        bool matchTemplate( const std::string & text, const std::vector<std::string> & pieces, const std::string & kinds, std::vector<std::string> & values )
        {
            if ( text.compare( 0, pieces[0].size(), pieces[0] ) != 0 ) {
                return false;
            }
            size_t pos = pieces[0].size();
            values.clear();
            for ( size_t k = 1; k < pieces.size(); ++k ) {
                const bool last = ( k + 1 == pieces.size() );
                size_t end = 0;
                if ( last ) {
                    if ( text.size() < pos + pieces[k].size() || text.compare( text.size() - pieces[k].size(), pieces[k].size(), pieces[k] ) != 0 ) {
                        return false;
                    }
                    end = text.size() - pieces[k].size();
                }
                else {
                    end = pieces[k].empty() ? std::string::npos : text.find( pieces[k], pos );
                    if ( end == std::string::npos ) {
                        return false;
                    }
                }
                if ( end < pos ) {
                    return false;
                }
                const std::string value = text.substr( pos, end - pos );
                // A value never runs over a line break: a short template ("Attack %s") must not take a whole block of lines.
                if ( value.empty() || value.find( '\n' ) != std::string::npos ) {
                    return false;
                }
                if ( kinds[k - 1] == 'd' && value.find_first_not_of( "-0123456789" ) != std::string::npos ) {
                    return false;
                }
                values.push_back( value );
                pos = end + pieces[k].size();
            }
            return pos == text.size();
        }

        // A value put into a template (a name for %s) is itself looked up in the table, so built sentences get Korean names.
        std::string translateValue( const std::string & value )
        {
            const Data & d = data();
            if ( const auto it = d.exact.find( value ); it != d.exact.end() ) {
                return it->second;
            }
            return value;
        }

        std::string translateWhole( const std::string & text );

        // Whole text first; a text of several lines without a whole match is translated line by line (stat blocks built line by
        // line, e.g. the army window "Attack Skill: 5\nDefense Skill: 3 ...").
        std::string translate( const std::string & text )
        {
            const Data & d = data();
            if ( d.big.empty() || text.empty() ) {
                return text;
            }
            std::string whole = translateWhole( text );
            if ( whole != text || text.find( '\n' ) == std::string::npos ) {
                return whole;
            }
            std::string out;
            size_t start = 0;
            while ( true ) {
                const size_t end = text.find( '\n', start );
                const std::string line = text.substr( start, end == std::string::npos ? std::string::npos : end - start );
                out += line.empty() ? line : translateWhole( line );
                if ( end == std::string::npos ) {
                    break;
                }
                out += '\n';
                start = end + 1;
            }
            return out;
        }

        // Korean particles after a value put into a template: "이(가)", "을(를)", "은(는)", "와(과)", "(으)로" become the one that fits
        // the sound before them (a final consonant or not; for "(으)로" a final ㄹ counts as none). Digits are read as Korean numbers
        // (0 영, 1 일, 3 삼, 6 육, 7 칠, 8 팔 end in a consonant; 2 이, 4 사, 5 오, 9 구 do not). Anything else keeps the double form.
        int finalConsonant( const std::string & s, const size_t end )
        {
            if ( end == 0 ) {
                return -1;
            }
            size_t start = end - 1;
            while ( start > 0 && ( static_cast<uint8_t>( s[start] ) & 0xc0 ) == 0x80 ) {
                --start;
            }
            const uint8_t c = static_cast<uint8_t>( s[start] );
            if ( c >= '0' && c <= '9' ) {
                static const int digit[10] = { 21, 8, 0, 16, 0, 0, 1, 8, 8, 0 };
                return digit[c - '0'];
            }
            if ( ( c & 0xf0 ) == 0xe0 && end - start == 3 ) {
                const uint32_t cp = ( ( c & 0x0fu ) << 12 ) | ( ( static_cast<uint8_t>( s[start + 1] ) & 0x3fu ) << 6 ) | ( static_cast<uint8_t>( s[start + 2] ) & 0x3fu );
                if ( cp >= 0xac00 && cp <= 0xd7a3 ) {
                    return static_cast<int>( ( cp - 0xac00 ) % 28 );
                }
            }
            return -1;
        }

        std::string fixParticles( std::string s )
        {
            struct Pair
            {
                const char * both;
                const char * withFinal;
                const char * withoutFinal;
                bool rieulAsNone;
            };
            static const Pair pairs[5] = { { "이(가)", "이", "가", false }, { "을(를)", "을", "를", false }, { "은(는)", "은", "는", false },
                                           { "와(과)", "과", "와", false }, { "(으)로", "으로", "로", true } };
            for ( const Pair & p : pairs ) {
                const std::string both = p.both;
                size_t at = 0;
                while ( ( at = s.find( both, at ) ) != std::string::npos ) {
                    const int f = finalConsonant( s, at );
                    if ( f < 0 ) {
                        at += both.size();
                        continue;
                    }
                    const bool hasFinal = f != 0 && !( p.rieulAsNone && f == 8 );
                    const std::string one = hasFinal ? p.withFinal : p.withoutFinal;
                    s.replace( at, both.size(), one );
                    at += one.size();
                }
            }
            return s;
        }

        std::string translateWhole( const std::string & text )
        {
            const Data & d = data();
            if ( const auto it = d.exact.find( text ); it != d.exact.end() ) {
                return it->second;
            }
            std::vector<std::string> values;
            for ( size_t t = 0; t < d.templates.size(); ++t ) {
                if ( !matchTemplate( text, d.templates[t].first, d.kinds[t], values ) ) {
                    continue;
                }
                const std::string & ko = d.templates[t].second;
                std::string out;
                size_t next = 0;
                for ( size_t i = 0; i < ko.size(); ++i ) {
                    if ( ko[i] == '%' && i + 1 < ko.size() && ( ko[i + 1] == 's' || ko[i + 1] == 'd' ) ) {
                        out += next < values.size() ? translateValue( values[next] ) : std::string();
                        ++next;
                        ++i;
                    }
                    else if ( ko[i] == '%' && i + 1 < ko.size() && ko[i + 1] >= '1' && ko[i + 1] <= '9' ) {
                        const size_t n = static_cast<size_t>( ko[i + 1] - '1' );
                        out += n < values.size() ? translateValue( values[n] ) : std::string();
                        ++i;
                    }
                    else {
                        out += ko[i];
                    }
                }
                return fixParticles( out );
            }
            return text;
        }

        bool hasNonAscii( const std::string & s )
        {
            return std::any_of( s.begin(), s.end(), []( const char c ) { return ( static_cast<uint8_t>( c ) & 0x80 ) != 0; } );
        }

        // UTF-8 to code points (bad bytes are kept as single characters).
        std::vector<uint32_t> decode( const std::string & s )
        {
            std::vector<uint32_t> out;
            for ( size_t i = 0; i < s.size(); ) {
                const uint8_t c = static_cast<uint8_t>( s[i] );
                if ( c < 0x80 ) {
                    out.push_back( c );
                    ++i;
                }
                else if ( ( c & 0xe0 ) == 0xc0 && i + 1 < s.size() ) {
                    out.push_back( ( ( c & 0x1fu ) << 6 ) | ( static_cast<uint8_t>( s[i + 1] ) & 0x3fu ) );
                    i += 2;
                }
                else if ( ( c & 0xf0 ) == 0xe0 && i + 2 < s.size() ) {
                    out.push_back( ( ( c & 0x0fu ) << 12 ) | ( ( static_cast<uint8_t>( s[i + 1] ) & 0x3fu ) << 6 ) | ( static_cast<uint8_t>( s[i + 2] ) & 0x3fu ) );
                    i += 3;
                }
                else {
                    out.push_back( '?' );
                    ++i;
                }
            }
            return out;
        }

        bool isSmall( const Font & font )
        {
            return font.lineHeight < 16;
        }

        int32_t advance( const Font & font, const uint32_t cp )
        {
            if ( cp >= 0xAC00 && cp <= 0xD7A3 ) {
                return isSmall( font ) ? 11 : 13;
            }
            return glyphWidth( font, glyphIndex( cp < 0x80 ? static_cast<char>( cp ) : '?' ) ) + 1;
        }

        void drawLine( fheroes2::Image & dst, const Font & font, const std::vector<uint32_t> & line, int32_t x, const int32_t y, const uint8_t colour )
        {
            const Data & d = data();
            for ( const uint32_t cp : line ) {
                if ( cp >= 0xAC00 && cp <= 0xD7A3 ) {
                    const bool small = isSmall( font );
                    const int rows = small ? 10 : 12;
                    const uint16_t * g = ( small ? d.small.data() : d.big.data() ) + static_cast<size_t>( cp - 0xAC00 ) * rows;
                    // The taller Hangul cell is centred on the English capitals (big font: rows 2..13 of the 16-pixel line, capitals
                    // 3..12; small font: rows 2..11 of the 12-pixel line, capitals 3..9), using the room of the descenders.
                    const int32_t top = y + font.baseline - ( rows - ( small ? 3 : 2 ) );
                    for ( int r = 0; r < rows; ++r ) {
                        for ( int c = 0; c < 12; ++c ) {
                            if ( ( g[r] >> c ) & 1 ) {
                                const int32_t dx = x + c;
                                const int32_t dy = top + r;
                                if ( dx >= 0 && dy >= 0 && dx < dst.width() && dy < dst.height() ) {
                                    dst.image()[static_cast<size_t>( dy ) * dst.width() + dx] = colour;
                                    if ( !dst.singleLayer() ) {
                                        dst.transform()[static_cast<size_t>( dy ) * dst.width() + dx] = 0;
                                    }
                                }
                            }
                        }
                    }
                }
                else if ( cp < 0x80 && cp != ' ' ) {
                    const int g = glyphIndex( static_cast<char>( cp ) );
                    if ( g != 0 && font.glyphs != nullptr && g < static_cast<int>( font.glyphs->size() ) ) {
                        drawMono( dst, ( *font.glyphs )[g], x, font.baseline + y, colour );
                    }
                }
                x += advance( font, cp );
            }
        }

        // The original line breaking (FUN_0047ab10) on code points: back to the last space when the width is passed.
        std::vector<std::pair<std::vector<uint32_t>, int32_t>> breakLines( const Font & font, const std::string & text, const int32_t w )
        {
            const std::vector<uint32_t> cps = decode( text );
            std::vector<std::pair<std::vector<uint32_t>, int32_t>> lines;
            size_t pos = 0;
            while ( pos < cps.size() ) {
                const size_t start = pos;
                int32_t width = 0;
                size_t lastSpace = std::string::npos;
                while ( pos < cps.size() && cps[pos] != '\n' ) {
                    const int32_t a = advance( font, cps[pos] );
                    if ( width + a > w && pos > start ) {
                        break;
                    }
                    if ( cps[pos] == ' ' ) {
                        lastSpace = pos;
                    }
                    width += a;
                    ++pos;
                }
                size_t end = pos;
                if ( pos < cps.size() && cps[pos] != '\n' && lastSpace != std::string::npos && lastSpace > start ) {
                    end = lastSpace;
                    pos = lastSpace + 1;
                }
                else if ( pos < cps.size() && cps[pos] == '\n' ) {
                    ++pos;
                }
                std::vector<uint32_t> line( cps.begin() + static_cast<std::ptrdiff_t>( start ), cps.begin() + static_cast<std::ptrdiff_t>( end ) );
                int32_t lw = 0;
                for ( const uint32_t cp : line ) {
                    lw += advance( font, cp );
                }
                lines.emplace_back( line, lw );
            }
            return lines;
        }
    }
}

namespace H1::UI
{
    int & editKey();

    Item * Layout::find( const int id )
    {
        // FUN_00477880 puts every new item at the head of the dialog's list and FUN_00477af0 walks from the head, so a message reaches
        // the LAST item of the file with that id (herowind.bin has "Spell Power" and the status bar text both as id 302; note v58).
        for ( auto it = items.rbegin(); it != items.rend(); ++it ) {
            if ( it->id == id ) {
                return &*it;
            }
        }
        return nullptr;
    }

    Layout loadLayout( const std::string & name )
    {
        Layout layout;
        const std::vector<uint8_t> & b = getH1AggFile( name );
        if ( b.size() < 6 ) {
            return layout;
        }
        size_t o = 0;
        auto u16 = [&]() {
            if ( o + 2 > b.size() ) {
                o = b.size();
                return 0;
            }
            const int v = b[o] | ( b[o + 1] << 8 );
            o += 2;
            return v;
        };
        // Positions are signed (magewind.bin places the guild picture at x = -250; its frames carry +315 offsets, note v68).
        auto s16 = [&]() { return static_cast<int>( static_cast<int16_t>( u16() ) ); };
        auto name13 = [&]() {
            if ( o + 13 > b.size() ) {
                o = b.size();
                return std::string();
            }
            std::string s( reinterpret_cast<const char *>( b.data() + o ), 13 );
            o += 13;
            return s.substr( 0, s.find( '\0' ) );
        };
        layout.width = u16();
        layout.height = u16();
        u16(); // flags
        while ( o + 2 <= b.size() ) {
            Item it;
            it.type = u16();
            if ( it.type == 0 ) {
                break;
            }
            if ( it.type == ITEM_IMAGE ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.id = u16();
                it.flags = u16();
                if ( it.flags == 0x800 ) {
                    it.name = name13();
                }
                else {
                    it.colour = u16() & 0xff;
                }
            }
            else if ( it.type == ITEM_BUTTON ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.name = name13();
                it.frame = u16();
                it.pressedFrame = u16();
                u16();
                it.hotkey = u16();
                it.id = u16();
                it.flags = u16();
            }
            else if ( it.type == ITEM_TEXT ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                const int len = u16();
                if ( o + len > b.size() ) {
                    break;
                }
                std::string s( reinterpret_cast<const char *>( b.data() + o ), len );
                o += len;
                it.text = s.substr( 0, s.find( '\0' ) );
                it.name = name13();
                it.colour = u16() & 0xff;
                it.align = u16() & 0xff;
                it.id = u16();
                u16();
                it.flags = 0x200;
            }
            else if ( it.type == ITEM_ICON ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.name = name13();
                it.frame = u16();
                it.mirror = ( u16() & 0xff ) != 0;
                it.id = u16();
                it.flags = u16();
                it.monoColour = u16() & 0xff;
            }
            else if ( it.type == ITEM_AREA ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.id = u16();
                it.flags = u16();
            }
            else if ( it.type == 0x100 || it.type == 0x201 || it.type == 0x202 ) {
                const int kind = it.type;
                it.type = ITEM_EDIT;
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.maxLength = u16();
                if ( o + it.maxLength > b.size() ) {
                    break;
                }
                std::string s( reinterpret_cast<const char *>( b.data() + o ), it.maxLength );
                o += it.maxLength;
                it.text = s.substr( 0, s.find( '\0' ) );
                it.name = name13();
                it.colour = u16() & 0xff;
                it.align = u16() & 0xff;
                name13(); // .icn (cursor) [use unconfirmed]
                if ( kind == 0x201 ) {
                    for ( int k = 0; k < 6; ++k ) {
                        u16();
                    }
                }
                u16();
                it.id = u16();
                u16();
                it.flags = 0x4000;
            }
            else if ( it.type == ITEM_STRIP ) {
                it.x = s16(), it.y = s16(), it.w = u16(), it.h = u16();
                it.id = u16();
                it.flags = u16();
            }
            else {
                break;
            }
            layout.items.push_back( std::move( it ) );
        }
        return layout;
    }

    fheroes2::Image readBmp( const std::string & name )
    {
        const std::vector<uint8_t> & b = getH1AggFile( name );
        fheroes2::Image image;
        if ( b.size() < 6 ) {
            return image;
        }
        const int32_t w = b[2] | ( b[3] << 8 );
        const int32_t h = b[4] | ( b[5] << 8 );
        if ( b.size() < 6 + static_cast<size_t>( w ) * h ) {
            return image;
        }
        image._disableTransformLayer();
        image.resize( w, h );
        std::copy( b.begin() + 6, b.begin() + 6 + static_cast<ptrdiff_t>( w ) * h, image.image() );
        return image;
    }

    void drawText( fheroes2::Image & dst, const std::string & text, const std::string & fontName, const int32_t x, const int32_t y, const int32_t w,
                   const int32_t h, const int colour, const int align )
    {
        const Font & font = getFont( fontName );
        if ( font.glyphs == nullptr ) {
            return;
        }
        // Note v54: the colour is a DOS palette index used as is (v52 wrongly applied the Windows palette table 0x492b18).
        const uint8_t pixel = static_cast<uint8_t>( colour );
        if ( Korean::active() ) {
            // H1-INTENDED(D11): Korean text, see the Korean namespace above.
            const std::string ko = Korean::translate( text );
            if ( Korean::hasNonAscii( ko ) ) {
                int32_t lineY = 0;
                for ( const auto & [line, lineWidth] : Korean::breakLines( font, ko, w ) ) {
                    if ( font.lineHeight + lineY > h ) {
                        break;
                    }
                    const int32_t offset = ( align == 1 ) ? ( w - lineWidth ) / 2 : ( align == 2 ? w - lineWidth : 0 );
                    Korean::drawLine( dst, font, line, x + offset, y + lineY, pixel );
                    lineY += font.lineHeight;
                }
                return;
            }
        }

        // FUN_0047ab10, line by line: take characters until the end, a '\n' or the width is passed; if passed, go back to the last
        // space. Stop when the next line would not fit in the box height.
        std::string s = text;
        const int32_t length = static_cast<int32_t>( s.size() );
        int32_t pos = 0;
        int32_t lineY = 0;
        while ( pos < length && s[pos] != '\0' && font.lineHeight + lineY <= h ) {
            const int32_t start = pos;
            int32_t lineWidth = 0;
            while ( pos < length && s[pos] != '\0' && s[pos] != '\n' && lineWidth <= w ) {
                lineWidth += glyphWidth( font, glyphIndex( s[pos] ) ) + 1;
                ++pos;
            }
            if ( w < lineWidth ) {
                while ( true ) {
                    --pos;
                    if ( pos < 0 || s[pos] == ' ' || pos < start ) {
                        break;
                    }
                    lineWidth -= glyphWidth( font, glyphIndex( s[pos] ) ) + 1;
                }
                if ( pos >= 0 && s[pos] == ' ' ) {
                    lineWidth -= glyphWidth( font, 0 ) + 1;
                }
            }
            // The original cuts the string at pos and draws from start. When no space was found pos is start - 1 and the original
            // draws the rest of the string (open case, not reached by the texts used so far).
            std::string line = ( pos >= start ) ? s.substr( start, pos - start ) : s.substr( start );
            int32_t offset = 0;
            if ( align == 1 ) {
                offset = ( w - lineWidth ) / 2;
            }
            else if ( align == 2 ) {
                offset = w - lineWidth;
            }
            drawLine( dst, font, line, x + offset, y + lineY, pixel );
            if ( pos < start ) {
                pos = start; // avoid an endless loop; the original repeats the same line until the height check stops it
            }
            ++pos;
            lineY += font.lineHeight;
        }
    }

    void drawLayout( fheroes2::Image & dst, const Layout & layout, const int32_t offsetX, const int32_t offsetY, const int pressedId )
    {
        for ( const Item & it : layout.items ) {
            if ( it.hidden ) {
                continue;
            }
            const int32_t x = offsetX + it.x;
            const int32_t y = offsetY + it.y;
            switch ( it.type ) {
            case ITEM_IMAGE:
                // FUN_0047d640: flags 0x800 = .bmp, 0x400 = filled box in the item colour, anything else draws nothing.
                if ( it.flags == 0x800 ) {
                    const fheroes2::Image bmp = readBmp( it.name );
                    fheroes2::Copy( bmp, 0, 0, dst, x, y, std::min( it.w, bmp.width() ), std::min( it.h, bmp.height() ) );
                }
                else if ( it.flags == 0x400 ) {
                    fheroes2::Fill( dst, x, y, it.w, it.h, static_cast<uint8_t>( it.colour ) );
                }
                else if ( it.flags == 0x1000 && it.picture ) {
                    fheroes2::Copy( *it.picture, 0, 0, dst, x, y, std::min( it.w, it.picture->width() ), std::min( it.h, it.picture->height() ) );
                }
                break;
            case ITEM_EDIT:
                drawText( dst, it.text, it.name, x, y, it.w, it.h, it.colour, it.align );
                break;
            case ITEM_STRIP:
                // Shadow strip next to a dialog: drawn with the darkening table [how HoMM1 draws it: unconfirmed, note v51].
                fheroes2::ApplyPalette( dst, x, y, dst, x, y, it.w, it.h, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
                break;
            case ITEM_ICON:
            case ITEM_BUTTON: {
                const bool mono = ( it.type == ITEM_ICON && it.flags == 0x80 );
                const std::vector<fheroes2::Sprite> & icn = mono ? getIcnMono( it.name ) : getIcn( it.name );
                const int frame = ( it.type == ITEM_BUTTON && it.id == pressedId ) ? it.pressedFrame : it.frame;
                if ( frame >= 0 && frame < static_cast<int>( icn.size() ) ) {
                    const fheroes2::Sprite & s = icn[frame];
                    if ( mono ) {
                        drawMono( dst, s, x, y, static_cast<uint8_t>( it.monoColour ) );
                    }
                    else if ( it.mirror ) {
                        // FUN_0047a360 -> FUN_0047d7b6 with the flip flag: the rightmost column is x - frame x offset (drawn leftwards).
                        fheroes2::Blit( s, 0, 0, dst, x - s.x() - s.width() + 1, y + s.y(), s.width(), s.height(), true );
                    }
                    else {
                        fheroes2::Blit( s, dst, x + s.x(), y + s.y() );
                    }
                }
                break;
            }
            case ITEM_TEXT:
                drawText( dst, it.text, it.name, x, y, it.w, it.h, it.colour, it.align );
                break;
            default:
                break;
            }
            if ( it.greyed ) {
                // The original darkens with its table at 0x4a3e2c (DOS: H1::darkenTable, note v108).
                fheroes2::ApplyPalette( dst, x, y, dst, x, y, it.w, it.h, H1::darkenTable() );
            }
        }
    }

    void TextEdit::start()
    {
        before = text;
        text.clear();
        cursor = 0;
    }

    std::string TextEdit::shown() const
    {
        const size_t at = std::min( cursor, text.size() );
        return text.substr( 0, at ) + "_" + text.substr( at );
    }

    int TextEdit::key( const int keyValue, const int32_t modifiers, const Item & field )
    {
        const fheroes2::Key k = static_cast<fheroes2::Key>( keyValue );
        cursor = std::min( cursor, text.size() );
        switch ( k ) {
        case fheroes2::Key::KEY_ENTER:
        case fheroes2::Key::KEY_KP_ENTER:
            return 1;
        case fheroes2::Key::KEY_ESCAPE:
            text = before;
            cursor = text.size();
            return 2;
        case fheroes2::Key::KEY_LEFT:
            if ( cursor > 0 ) {
                --cursor;
            }
            return 0;
        case fheroes2::Key::KEY_RIGHT:
            if ( cursor < text.size() ) {
                ++cursor;
            }
            return 0;
        case fheroes2::Key::KEY_BACKSPACE:
            if ( cursor > 0 ) {
                text.erase( cursor - 1, 1 );
                --cursor;
            }
            return 0;
        case fheroes2::Key::KEY_DELETE:
            if ( cursor < text.size() ) {
                text.erase( cursor, 1 );
            }
            return 0;
        case fheroes2::Key::KEY_HOME:
        case fheroes2::Key::KEY_END:
            return 0; // no character, nothing happens
        default:
            break;
        }
        if ( field.maxLength > 0 && static_cast<int>( text.size() ) + 1 >= field.maxLength ) {
            return 0;
        }
        std::string edited = text;
        const size_t newCursor = fheroes2::InsertKeySym( edited, cursor, k, modifiers );
        if ( edited.size() != text.size() + 1 ) {
            return 0;
        }
        const std::string before2 = text;
        const size_t cursor2 = cursor;
        text = edited;
        cursor = newCursor;
        // The shown text (with '_') must still fit one line of the field (FUN_0047ade0 against +0x3d = 1).
        if ( countLines( shown(), field.name, field.w ) > 1 ) {
            text = before2;
            cursor = cursor2;
        }
        return 0;
    }

    int countLines( const std::string & text, const std::string & fontName, const int32_t width )
    {
        const Font & font = getFont( fontName );
        if ( Korean::active() ) {
            const std::string ko = Korean::translate( text );
            if ( Korean::hasNonAscii( ko ) ) {
                return static_cast<int>( Korean::breakLines( font, ko, width ).size() );
            }
        }
        const int32_t length = static_cast<int32_t>( text.size() );
        int count = 0;
        int32_t pos = 0;
        while ( pos < length && text[pos] != '\0' ) {
            const int32_t start = pos;
            int32_t lineWidth = 0;
            while ( pos < length && text[pos] != '\0' && text[pos] != '\n' && lineWidth <= width ) {
                lineWidth += glyphWidth( font, glyphIndex( text[pos] ) ) + 1;
                ++pos;
            }
            if ( width < lineWidth ) {
                do {
                    --pos;
                } while ( pos >= start && text[pos] != ' ' );
                if ( pos < start ) {
                    pos = start; // no space: the original would loop on this line
                }
            }
            ++count;
            ++pos;
        }
        return count;
    }

    void drawMonoSprite( fheroes2::Image & dst, const fheroes2::Sprite & sprite, const int32_t x, const int32_t y, const uint8_t colour )
    {
        drawMono( dst, sprite, x, y, colour );
    }

    int32_t textWidth( const std::string & text, const std::string & fontName )
    {
        const Font & font = getFont( fontName );
        if ( Korean::active() && Korean::hasNonAscii( text ) ) {
            int32_t kw = 0;
            for ( const uint32_t cp : Korean::decode( text ) ) {
                kw += Korean::advance( font, cp );
            }
            return kw;
        }
        int32_t w = 0;
        for ( const char c : text ) {
            w += glyphWidth( font, glyphIndex( c ) ) + 1;
        }
        return w;
    }

    int runLayoutDialogImpl( const Layout & layout, int32_t x, int32_t y, bool popup, const std::function<const char *( int )> * help, int32_t helpX,
                             const std::function<bool()> * tick = nullptr );

    // Picture of the right-click help popup of the dialog running now (spell pictures, note v171).
    const std::function<MessagePicture( int )> * helpPictureOf = nullptr;

    int messageBox( const std::string & text, const int style, int32_t x, int32_t y )
    {
        return messageBox( text, style, x, y, MessagePicture(), MessagePicture(), false );
    }

    int messageBox( const std::string & text, const int style, int32_t x, int32_t y, const MessagePicture first, const MessagePicture second,
                    const bool orBetween )
    {
        // FUN_00422fc2: lines of bigfont at width 0xf0 -> height; buttons add 0x27; the tallest picture adds its height + 12;
        // window evntwin<n>.bin with n = (height - 12) / 45 (0..7).
        const MessagePicture pictures[2] = { first, second };
        // Picture height used for the window size, by kind (first switch of FUN_00422fc2).
        const auto sizeHeight = []( const int kind ) -> int32_t {
            // kind 16 (H1-INTENDED(D16), extension mode): a primary skill picture of heroscrn.icn.
            static const int32_t h[17] = { 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x1a, 0x4c, 0x34, 0x37, 0x1c, 0x39, 0x3e, 0x3b, 0x4c, 0x6f, 0x5d };
            return ( kind >= 0 && kind < 17 ) ? h[kind] : 0;
        };
        int32_t tallest = 0;
        for ( const MessagePicture & p : pictures ) {
            tallest = std::max( tallest, sizeHeight( p.kind ) );
        }
        int32_t height = countLines( text, "bigfont.fnt", 0xf0 ) * 0x10;
        if ( style != MESSAGE_POPUP ) {
            height += 0x27;
        }
        if ( tallest != 0 ) {
            height += tallest + 0xc;
        }
        int n = ( height - 0xc ) / 0x2d;
        if ( n > 7 ) {
            n = 7;
        }
        if ( n < 1 && style != MESSAGE_POPUP ) {
            n = 1;
        }
        const int32_t boxHeight = n * 0x2d + 0x81;
        if ( x == -1 || 0x27e < x + 0x11e ) {
            // 0x61 on the adventure screen (not ported yet), 0xb1 elsewhere.
            x = 0xb1;
        }
        if ( y == -1 || 0x1de < y + boxHeight ) {
            y = ( 0x1e0 - boxHeight ) / 2;
            if ( 0x1c < y ) {
                y = 0x1c;
            }
        }

        Layout layout = loadLayout( "evntwin" + std::to_string( n ) + ".bin" );
        if ( Item * it = layout.find( 1 ) ) {
            it->text = text;
        }
        // Buttons: Cancel 0x8001 shown for styles 3 and 6, OK 0x8002 for 1, 3 and 5, Yes 0x8005 / No 0x8006 for 2 (message 6 with value 6).
        auto hide = [&layout]( const int id ) {
            if ( Item * it = layout.find( id ) ) {
                it->hidden = true;
                it->inactive = true;
            }
        };
        if ( style != MESSAGE_CANCEL_ONLY && style != MESSAGE_OKAY_CANCEL ) {
            hide( 0x8001 );
        }
        if ( style != MESSAGE_OKAY_ONLY && style != MESSAGE_OKAY && style != MESSAGE_OKAY_CANCEL ) {
            hide( 0x8002 );
        }
        if ( style != MESSAGE_YES_NO ) {
            hide( 0x8005 );
            hide( 0x8006 );
        }

        // Pictures (second loop of FUN_00422fc2): an icon item centred at 1/2 (one picture) or 1/3 and 2/3 of the width 0x11e, a caption
        // of smalfont (100 x 12, centred) under it, extra layers for artifacts, crests and portraits.
        const int32_t width = 0x11e;
        int32_t pictureY = 0;
        const auto addIcon = [&layout]( const std::string & icn, const int frame, const int32_t ix, const int32_t iy, const int32_t iw, const int32_t ih ) {
            Item icon;
            icon.type = ITEM_ICON;
            icon.name = icn;
            icon.frame = frame;
            icon.x = ix;
            icon.y = iy;
            icon.w = iw;
            icon.h = ih;
            icon.flags = 0x10;
            layout.items.push_back( icon );
        };
        for ( int k = 0; k < 2; ++k ) {
            const MessagePicture & p = pictures[k];
            if ( p.kind < 0 || p.kind > 16 ) {
                break;
            }
            std::string caption;
            std::string icn = "resource.icn";
            int frame = p.kind;
            if ( p.kind < 7 ) {
                if ( p.value > 0 ) {
                    caption = std::to_string( p.value );
                }
                else if ( p.value < 0 ) {
                    caption = std::to_string( -p.value ) + "/day";
                }
            }
            else if ( p.kind == 8 ) {
                caption = H1::spellName( p.value );
                icn = "spells.icn";
                frame = p.value;
            }
            else if ( p.kind == 9 ) {
                icn = "brcrest.icn";
                frame = p.value;
            }
            else if ( p.kind == 15 ) {
                icn = "surrendr.icn";
                frame = 4;
            }
            else if ( p.kind == 16 ) {
                // value 0 attack, 1 defense, 2 spell power, 3 knowledge: the hero screen pictures (herowind.bin ids 66, 67, 69, 70).
                static const int skillFrame[4] = { 4, 1, 2, 0 };
                icn = "heroscrn.icn";
                frame = skillFrame[std::clamp( p.value, 0, 3 )];
                caption = "+1";
            }
            else if ( p.kind >= 10 && p.kind <= 14 ) {
                icn = "expmrl.icn";
                frame = p.kind - 10;
                if ( p.kind == 14 && p.value != -1 ) {
                    caption = std::to_string( p.value );
                }
            }
            static const int32_t sizes[17][2] = { { 0x26, 0x20 }, { 0x26, 0x20 }, { 0x26, 0x20 }, { 0x26, 0x20 }, { 0x26, 0x20 }, { 0x26, 0x20 },
                                                  { 0x4c, 0x1a }, { 0x4c, 0x4c }, { 0x26, 0x28 }, { 0x32, 0x37 }, { 0x40, 0x1c }, { 0x40, 0x39 },
                                                  { 0x40, 0x3e }, { 0x40, 0x3b }, { 0x40, 0x40 }, { 0x6f, 0x69 }, { 0x52, 0x5d } };
            const int32_t pw = sizes[p.kind][0];
            int32_t ph = sizes[p.kind][1];
            if ( !caption.empty() ) {
                ph += 0xc;
            }
            const int32_t centreX = ( k == 0 ) ? ( pictures[1].kind == -1 ? width / 2 : width / 3 ) : ( width * 2 ) / 3;
            pictureY = boxHeight - ph - ( style != MESSAGE_POPUP ? 0x42 : 0x1b );
            if ( ph < tallest ) {
                pictureY -= ( tallest - ph ) / 2;
            }
            const int32_t left = centreX - pw / 2;
            addIcon( icn, frame, left, pictureY, pw, ph );
            if ( p.kind == 7 ) {
                addIcon( "artifact.icn", p.value, left + 6, pictureY + 6, 0x4c, 0x4c );
            }
            else if ( p.kind == 9 ) {
                addIcon( "brcrest.icn", 4, left - 4, pictureY - 4, 0x3a, 0x37 );
            }
            else if ( p.kind == 15 ) {
                char name[24];
                std::snprintf( name, sizeof( name ), "port%04d.icn", p.value );
                addIcon( name, 0, left + 5, pictureY + 5, 0x65, 0x5f );
            }
            Item label;
            label.type = ITEM_TEXT;
            label.name = "smalfont.fnt";
            label.colour = 1;
            label.align = 1;
            label.x = centreX - 0x32;
            label.y = pictureY + ph - 10;
            label.w = 100;
            label.h = 0xc;
            label.text = caption;
            layout.items.push_back( label );
        }
        if ( orBetween ) {
            Item orText;
            orText.type = ITEM_TEXT;
            orText.name = "smalfont.fnt";
            orText.colour = 1;
            orText.align = 1;
            orText.x = width / 2 - 0x11;
            orText.y = pictureY + 0x1e;
            orText.w = 0x28;
            orText.h = 0xc;
            orText.text = "or";
            layout.items.push_back( orText );
        }

        return runLayoutDialog( layout, x, y, style == MESSAGE_POPUP );
    }

    int runLayoutDialog( const Layout & layout, const int32_t x, const int32_t y, const bool popup )
    {
        return runLayoutDialogImpl( layout, x, y, popup, nullptr, 0 );
    }

    int runLayoutDialog( const Layout & layout, const int32_t x, const int32_t y, const std::function<const char *( int )> & help, const int32_t helpX,
                         const std::function<MessagePicture( int )> & helpPicture )
    {
        const std::function<MessagePicture( int )> * previous = helpPictureOf;
        helpPictureOf = &helpPicture;
        const int result = runLayoutDialogImpl( layout, x, y, false, &help, helpX );
        helpPictureOf = previous;
        return result;
    }

    int runLayoutDialog( const Layout & layout, const int32_t x, const int32_t y, const std::function<const char *( int )> & help, const int32_t helpX )
    {
        return runLayoutDialogImpl( layout, x, y, false, &help, helpX );
    }

    int lastEditKey()
    {
        return editKey();
    }

    void applyEditKey( Item & field )
    {
        const int key = editKey();
        if ( key == -1 ) {
            if ( !field.text.empty() ) {
                field.text.pop_back();
            }
        }
        else if ( key >= 0 && key <= 9 && ( field.maxLength <= 0 || static_cast<int>( field.text.size() ) < field.maxLength ) ) {
            field.text += static_cast<char>( '0' + key );
        }
    }

    int runLayoutDialogTicking( const Layout & layout, const int32_t x, const int32_t y, const std::function<bool()> & tick )
    {
        return runLayoutDialogImpl( layout, x, y, false, nullptr, 0, &tick );
    }

    int & editKey()
    {
        static int key = -2;
        return key;
    }

    int runLayoutDialogImpl( const Layout & layout, const int32_t x, const int32_t y, const bool popup, const std::function<const char *( int )> * help,
                             const int32_t helpX, const std::function<bool()> * tick )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        const int32_t offsetX = ( display.width() - 640 ) / 2 + x;
        const int32_t offsetY = ( display.height() - 480 ) / 2 + y;
        auto draw = [&]( const int pressedId ) {
            fheroes2::Image frame = saved;
            drawLayout( frame, layout, offsetX, offsetY, pressedId );
            fheroes2::Copy( frame, display );
            display.render();
        };
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        draw( -1 );

        int result = 0;
        if ( popup ) {
            // FUN_00424f04: wait for a mouse button to be let go.
            while ( le.HandleEvents() && ( le.isMouseRightButtonPressed() || le.isMouseLeftButtonPressed() ) ) {
            }
        }
        else {
            int pressed = -1;
            while ( le.HandleEvents() ) {
                const fheroes2::Point m = le.getMouseCursorPos();
                int over = -1;
                for ( const Item & it : layout.items ) {
                    const bool clickable = it.type == ITEM_BUTTON
                                           || ( ( it.type == ITEM_ICON || it.type == ITEM_TEXT || it.type == ITEM_IMAGE )
                                                && std::find( layout.clickableIcons.begin(), layout.clickableIcons.end(), it.id ) != layout.clickableIcons.end() );
                    if ( clickable && !it.hidden && !it.inactive && m.x >= offsetX + it.x && m.y >= offsetY + it.y && m.x < offsetX + it.x + it.w
                         && m.y < offsetY + it.y + it.h ) {
                        over = it.id;
                    }
                }
                const int newPressed = le.isMouseLeftButtonPressed() ? over : -1;
                if ( newPressed != pressed ) {
                    pressed = newPressed;
                    draw( pressed );
                }
                else if ( tick != nullptr && ( *tick )() ) {
                    draw( pressed );
                }
                // Edit field (FUN_0047ddd0): digit keys and Backspace go to the first shown, active edit item; the caller reads
                // UI::lastEditKey(), changes the text and runs the dialog again (as the original handlers read the field with atoi).
                if ( le.isAnyKeyPressed() ) {
                    const fheroes2::Key key = le.getPressedKeyValue();
                    int digit = -2;
                    if ( key >= fheroes2::Key::KEY_0 && key <= fheroes2::Key::KEY_9 ) {
                        digit = static_cast<int>( key ) - static_cast<int>( fheroes2::Key::KEY_0 );
                    }
                    else if ( key >= fheroes2::Key::KEY_KP_0 && key <= fheroes2::Key::KEY_KP_9 ) {
                        digit = static_cast<int>( key ) - static_cast<int>( fheroes2::Key::KEY_KP_0 );
                    }
                    else if ( key == fheroes2::Key::KEY_BACKSPACE ) {
                        digit = -1;
                    }
                    if ( digit != -2 ) {
                        bool typed = false;
                        for ( const Item & it : layout.items ) {
                            if ( it.type == ITEM_EDIT && !it.hidden && !it.inactive ) {
                                editKey() = digit;
                                result = it.id;
                                typed = true;
                                break;
                            }
                        }
                        if ( typed ) {
                            break;
                        }
                    }
                }
                if ( over != -1 && le.MouseClickLeft() ) {
                    result = over;
                    break;
                }
                if ( help != nullptr && over != -1 && le.isMouseRightButtonPressed() ) {
                    const char * text = ( *help )( over );
                    if ( text != nullptr ) {
                        messageBox( text, MESSAGE_POPUP, helpX, -1, helpPictureOf != nullptr ? ( *helpPictureOf )( over ) : MessagePicture(), MessagePicture() );
                        draw( -1 );
                    }
                }
                for ( const Item & it : layout.items ) {
                    if ( it.type != ITEM_BUTTON || it.hidden || it.inactive ) {
                        continue;
                    }
                    // DOS scan codes: 28 Enter, 1 Esc, 72 up arrow, 80 down arrow.
                    if ( ( it.hotkey == 28 && le.isKeyPressed( fheroes2::Key::KEY_ENTER ) ) || ( it.hotkey == 1 && le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) )
                         || ( it.hotkey == 72 && le.isKeyPressed( fheroes2::Key::KEY_UP ) ) || ( it.hotkey == 80 && le.isKeyPressed( fheroes2::Key::KEY_DOWN ) ) ) {
                        result = it.id;
                    }
                }
                if ( result != 0 ) {
                    break;
                }
            }
        }
        fheroes2::Copy( saved, display );
        display.render();
        return result;
    }
}
