// Heroes I puzzle map (homm1-remaster project). See h1_puzzle.h.
#include "h1_puzzle.h"

#include <algorithm>

#include "h1_gfx.h"
#include "h1_ui.h"
#include "image.h"
#include "screen.h"

namespace H1
{
    void showPuzzle( const fheroes2::Image & map, const std::bitset<48> & opened )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        const fheroes2::Image saved = display;

        // FUN_0045e89c: the map picture in the game area (0x10, 0x10, 0x1c0 x 0x1c0), then puzzle.icn piece i (frames carry their own
        // place) over it for every obelisk not found (player +0x9b bit i). All 48 found: no pieces.
        fheroes2::Image area( 0x1c0, 0x1c0 );
        area._disableTransformLayer();
        fheroes2::Fill( area, 0, 0, area.width(), area.height(), 0 );
        fheroes2::Copy( map, 0, 0, area, 0, 0, std::min( map.width(), area.width() ), std::min( map.height(), area.height() ) );
        const std::vector<fheroes2::Sprite> & pieces = getIcn( "puzzle.icn" );
        for ( size_t i = 0; i < pieces.size() && i < 48; ++i ) {
            if ( !opened.test( i ) ) {
                // Piece positions are relative to the game area at (0x10, 0x10) [frames of puzzle.icn: e.g. piece 0 at (81, 62)].
                fheroes2::Blit( pieces[i], area, pieces[i].x() - 0x10, pieces[i].y() - 0x10 );
            }
        }
        fheroes2::Copy( area, 0, 0, display, ox + 0x10, oy + 0x10, area.width(), area.height() );
        display.render();

        // viewpuzl.bin at (0x1e0, 0x10) over the radar; its Exit button closes (handler FUN_004240ef).
        UI::Layout panel = UI::loadLayout( "viewpuzl.bin" );
        UI::runLayoutDialog( panel, 0x1e0, 0x10, false );

        fheroes2::Copy( saved, display );
        display.render();
    }
}
