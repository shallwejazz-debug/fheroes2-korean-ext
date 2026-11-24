/***************************************************************************
 *   H2-EXT: Heroes II random map from the new game window, see h2_ext_randommap.h
 ***************************************************************************/

#include "h2_ext_randommap.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

#include "cursor.h"
#include "dialog.h"
#include "dialog_random_map_generator.h"
#include "game_hotkeys.h"
#include "ground.h"
#include "localevent.h"
#include "map_format_helper.h"
#include "map_format_info.h"
#include "map_random_generator.h"
#include "maps.h"
#include "maps_tiles.h"
#include "screen.h"
#include "system.h"
#include "tools.h"
#include "translations.h"
#include "ui_dialog.h"
#include "ui_text.h"
#include "ui_window.h"
#include "world.h"
#include "world_object_uid.h"

namespace
{
    bool generatingForGame = false;

    const char * markerSuffix = ".h2r";

    // The four engine map sizes (editor_mainmenu.cpp mapSizes), chosen from a small list window. 0 = cancelled.
    int32_t chooseMapSize()
    {
        const std::array<std::pair<const char *, int32_t>, 4> sizes = { { { gettext_noop( "Small Maps" ), Maps::SMALL },
                                                                          { gettext_noop( "Medium Maps" ), Maps::MEDIUM },
                                                                          { gettext_noop( "Large Maps" ), Maps::LARGE },
                                                                          { gettext_noop( "Extra Large Maps" ), Maps::XLARGE } } };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t width = 300;
        const int32_t rowHeight = 26;
        const int32_t height = 40 + static_cast<int32_t>( sizes.size() ) * rowHeight + 20;
        fheroes2::StandardWindow background( width, height, true, display );
        const fheroes2::Rect area = background.activeArea();

        const fheroes2::Text header( _( "Map Size" ), fheroes2::FontType::normalYellow() );
        header.draw( area.x + ( area.width - header.width() ) / 2, area.y + 12, display );

        std::vector<fheroes2::Rect> rows;
        int32_t y = area.y + 40;
        for ( const auto & [name, size] : sizes ) {
            const fheroes2::Rect r( area.x + 40, y, area.width - 80, rowHeight - 4 );
            rows.push_back( r );
            const fheroes2::Text text( std::string( _( name ) ) + "  " + std::to_string( size ) + " x " + std::to_string( size ), fheroes2::FontType::normalWhite() );
            background.applyTextBackgroundShading( r );
            text.draw( r.x + ( r.width - text.width() ) / 2, r.y + ( r.height - text.height() ) / 2 + 2, display );
            y += rowHeight;
        }
        display.render( background.totalArea() );

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            for ( size_t i = 0; i < rows.size(); ++i ) {
                if ( le.MouseClickLeft( rows[i] ) ) {
                    return sizes[i].second;
                }
            }
            if ( Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_CANCEL ) || le.MouseClickRight() ) {
                break;
            }
        }
        return 0;
    }

    std::string mapDirectory()
    {
        // The editor's save place (EditorInterface::saveMapToFile).
        const std::string dataPath = System::GetDataDirectory( "fheroes2" );
        if ( dataPath.empty() ) {
            return {};
        }
        std::string dir = System::concatPath( dataPath, "maps" );
        if ( !System::IsDirectory( dir ) && !System::MakeDirectory( dir ) ) {
            return {};
        }
        std::string corrected;
        if ( !System::GetCaseInsensitivePath( dir, corrected ) ) {
            return {};
        }
        return corrected;
    }
}

namespace H2Ext::RandomMap
{
    bool isGeneratingForGame()
    {
        return generatingForGame;
    }

    bool prepareEmptyMap( Maps::Map_Format::MapFormat & map, const int32_t width )
    {
        // Same steps as EditorInterface::generateNewMap without its editor-only checks.
        if ( width <= 0 ) {
            return false;
        }
        map = {};
        world.generateUninitializedMap( width );
        if ( world.w() != width || world.h() != width ) {
            return false;
        }
        map.width = width;
        const int32_t tilesCount = width * width;
        map.tiles.resize( tilesCount );
        for ( int32_t i = 0; i < tilesCount; ++i ) {
            world.getTile( i ).setIndex( i );
            Maps::setTerrainOnTile( map, i, Maps::Ground::WATER );
        }
        Maps::resetObjectUID();
        return true;
    }

    std::string createForGame()
    {
        const int32_t width = chooseMapSize();
        if ( width <= 0 ) {
            return {};
        }

        Maps::Random_Generator::Configuration config;
        if ( !fheroes2::randomMapGeneratorDialog( config, width ) ) {
            return {};
        }

        const std::string dir = mapDirectory();
        if ( dir.empty() ) {
            fheroes2::showStandardTextMessage( _( "Error" ), _( "Unable to locate data directory to save the map." ), Dialog::OK );
            return {};
        }

        Maps::Map_Format::MapFormat map;
        generatingForGame = true;
        const bool generated = Maps::Random_Generator::generateMap( map, config, width, width );
        generatingForGame = false;
        if ( !generated ) {
            fheroes2::showStandardTextMessage( _( "Warning" ), _( "Failed to generate a random map with given parameters." ), Dialog::OK );
            return {};
        }

        // The generator names the map "Random map <seed>"; the file takes the same number.
        std::string number = map.name;
        const size_t space = number.rfind( ' ' );
        if ( space != std::string::npos ) {
            number = number.substr( space + 1 );
        }
        std::string name = _( "Random map %{number}" );
        StringReplace( name, "%{number}", number );
        map.name = name;
        std::string description = _( "Random map of %{size} x %{size} squares for %{count} players." );
        StringReplace( description, "%{size}", width );
        StringReplace( description, "%{count}", config.playerCount );
        map.description = description;

        const std::string path = System::concatPath( dir, "H2RND" + number + ".fh2m" );
        if ( !Maps::Map_Format::saveMap( path, map ) ) {
            fheroes2::showStandardTextMessage( _( "Error" ), _( "Failed to save the map." ), Dialog::OK );
            return {};
        }
        std::ofstream marker( path + markerSuffix, std::ios::binary );
        marker << "H2-EXT generated map\n";
        if ( !marker ) {
            return {};
        }
        return path;
    }

    bool isGameRandomMap( const std::string & mapPath )
    {
        return !mapPath.empty() && System::IsFile( mapPath + markerSuffix );
    }
}
