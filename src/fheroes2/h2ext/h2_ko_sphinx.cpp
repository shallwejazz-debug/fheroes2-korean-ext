/***************************************************************************
 *   H2-KOREAN: Sphinx answer chosen from four, see h2_ko_sphinx.h          *
 ***************************************************************************/

#include "h2_ko_sphinx.h"

#include <algorithm>
#include <array>
#include <vector>

#include "cursor.h"
#include "localevent.h"
#include "maps_objects.h"
#include "rand.h"
#include "screen.h"
#include "tools.h"
#include "translations.h"
#include "ui_text.h"
#include "ui_window.h"
#include "world.h"

namespace
{
    // [확인] the first answers of the 9 Sphinxes in the 59 original maps and campaigns (h2korean/tools/extract_map_text.py), used as wrong choices
    const std::array<const char *, 9> originalAnswers = { "mirror", "Waste", "Archibald", "Mr Ed", "Stairs", "man", "Tomorrow", "an echo", "darkness" };

    std::string shown( const std::string & answer )
    {
        const char * ko = Translation::koreanOf( answer );
        return ko != nullptr ? ko : answer;
    }
}

namespace H2Korean
{
    std::string chooseSphinxAnswer( const std::string & title, const fheroes2::TextBase & question, const MapSphinx & sphinx )
    {
        if ( sphinx.answers.empty() ) {
            return {};
        }
        const auto isCorrect = [&sphinx]( const std::string & candidate ) {
            const std::string low = StringLower( candidate );
            return std::any_of( sphinx.answers.begin(), sphinx.answers.end(), [&low]( const std::string & a ) { return StringLower( a ) == low; } );
        };

        // wrong choices: the other Sphinxes of this map first, then the original answers
        std::vector<std::string> pool;
        for ( int32_t i = 0; i < world.w() * world.h(); ++i ) {
            const MapSphinx * other = dynamic_cast<const MapSphinx *>( world.GetMapObject( static_cast<uint32_t>( i ) ) );
            if ( other != nullptr && other != &sphinx && !other->answers.empty() ) {
                pool.push_back( other->answers.front() );
            }
        }
        for ( const char * a : originalAnswers ) {
            pool.emplace_back( a );
        }
        std::vector<std::string> wrong;
        for ( const std::string & a : pool ) {
            if ( !isCorrect( a ) && std::none_of( wrong.begin(), wrong.end(), [&a]( const std::string & w ) { return StringLower( w ) == StringLower( a ); } ) ) {
                wrong.push_back( a );
            }
        }
        Rand::Shuffle( wrong );
        std::vector<std::string> options( wrong.begin(), wrong.begin() + std::min<size_t>( 3, wrong.size() ) );
        options.push_back( sphinx.answers.front() );
        Rand::Shuffle( options );

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t width = 400;
        const fheroes2::Text header( title, fheroes2::FontType::normalYellow() );
        const int32_t questionHeight = question.height( width - 40 );
        const int32_t rowHeight = 26;
        const int32_t height = 30 + questionHeight + 14 + static_cast<int32_t>( options.size() ) * rowHeight + 20;
        fheroes2::StandardWindow background( width, height, true, display );
        const fheroes2::Rect area = background.activeArea();

        header.draw( area.x + ( area.width - header.width() ) / 2, area.y + 10, display );
        question.draw( area.x + 20, area.y + 30, width - 40, display );
        std::vector<fheroes2::Rect> rows;
        int32_t y = area.y + 30 + questionHeight + 14;
        for ( const std::string & option : options ) {
            const fheroes2::Rect r( area.x + 40, y, width - 80, rowHeight - 4 );
            rows.push_back( r );
            fheroes2::Text text( shown( option ), fheroes2::FontType::normalWhite() );
            background.applyTextBackgroundShading( r );
            text.draw( r.x + ( r.width - text.width() ) / 2, r.y + ( r.height - text.height() ) / 2 + 2, display );
            y += rowHeight;
        }
        display.render( background.totalArea() );

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            for ( size_t i = 0; i < rows.size(); ++i ) {
                if ( le.MouseClickLeft( rows[i] ) ) {
                    return options[i];
                }
            }
        }
        return {};
    }
}
