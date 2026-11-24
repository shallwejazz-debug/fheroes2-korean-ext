// Heroes I kingdom overview (homm1-remaster project). See h1_overview.h.
#include "h1_overview.h"
#include "h1_newgame.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "artifact.h"
#include "castle.h"
#include "color.h"
#include "h1_gfx.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "kingdom.h"
#include "mp2.h"
#include "race.h"
#include "resource.h"
#include "screen.h"
#include "world.h"

namespace
{
    void blit( fheroes2::Image & dst, const int frame, const int32_t x, const int32_t y )
    {
        const std::vector<fheroes2::Sprite> & icn = H1::getIcn( "overview.icn" );
        if ( frame >= 0 && frame < static_cast<int>( icn.size() ) ) {
            fheroes2::Blit( icn[frame], dst, x + icn[frame].x(), y + icn[frame].y() );
        }
    }

    int heroClass( const Heroes & hero )
    {
        switch ( hero.GetRace() ) {
        case Race::BARB:
            return 1;
        case Race::SORC:
            return 2;
        case Race::WRLK:
            return 3;
        case Race::NECR:
            return 3; // H1-INTENDED(D16): necromancers counted under the warlock picture (the window has 4 class pictures)
        default:
            return 0;
        }
    }

    int artifactCount( const Kingdom & kingdom, const int h1Artifact )
    {
        const int id = H1::engineArtifactFromH1( h1Artifact );
        if ( id < 0 ) {
            return 0;
        }
        int n = 0;
        for ( const Heroes * h : kingdom.GetHeroes() ) {
            n += static_cast<int>( h->GetBagArtifacts().Count( Artifact( id ) ) );
        }
        return n;
    }
}

namespace H1
{
    int goldPerDay( const Kingdom & kingdom )
    {
        const PlayerColor color = kingdom.GetColor();
        int gold = 0;
        if ( world.CountCapturedObject( MP2::OBJ_DRAGON_CITY, color ) > 0 ) {
            gold += 1000;
        }
        gold += 1000 * static_cast<int>( world.CountCapturedMines( Resource::GOLD, color ) );
        for ( const Castle * c : kingdom.GetCastles() ) {
            gold += c->isCastle() ? 1000 : 0xfa;
        }
        gold += artifactCount( kingdom, 0x1a ) * 1000 + artifactCount( kingdom, 0x1b ) * 0x2ee + artifactCount( kingdom, 0x1c ) * 500;
        // Computer players get a difficulty factor here (player +0x21e = 1, 3, 4) - not shown for human players.
        return gold;
    }

    int computerLevel( const Kingdom & kingdom )
    {
        // Player +0x21e from the new game screen (note v139); saves made before it do not have the seats: Average [temporary].
        const int level = seatLevel( kingdom.GetColor() );
        return ( level > 0 ) ? level : 2;
    }

    Funds dailyIncome( const Kingdom & kingdom )
    {
        const PlayerColor color = kingdom.GetColor();
        Funds funds;
        funds.wood = 2 * static_cast<int32_t>( world.CountCapturedMines( Resource::WOOD, color ) );
        funds.ore = 2 * static_cast<int32_t>( world.CountCapturedMines( Resource::ORE, color ) );
        funds.mercury = static_cast<int32_t>( world.CountCapturedMines( Resource::MERCURY, color ) );
        funds.sulfur = static_cast<int32_t>( world.CountCapturedMines( Resource::SULFUR, color ) );
        funds.crystal = static_cast<int32_t>( world.CountCapturedMines( Resource::CRYSTAL, color ) );
        funds.gems = static_cast<int32_t>( world.CountCapturedMines( Resource::GEMS, color ) );

        int gold = goldPerDay( kingdom );
        if ( kingdom.isControlAI() ) {
            const int level = computerLevel( kingdom );
            // Doubles 0.75 / 1.29 / 1.45 (0x48d020 .. 0x48d030), then __ftol (cut towards zero).
            if ( level == 1 ) {
                gold = static_cast<int>( gold * 0.75 );
            }
            else if ( level == 3 ) {
                gold = static_cast<int>( gold * 1.29 );
            }
            else if ( level == 4 ) {
                gold = static_cast<int>( gold * 1.45 );
            }
            if ( level > 2 ) {
                funds.wood += 1;
                funds.ore += 1;
            }
            // The day counter (+0x206) still holds the day that ended: 1..6 -> resource 0..5. The engine has already moved to the new day.
            const int endedDay = static_cast<int>( world.GetDay() ) == 1 ? 7 : static_cast<int>( world.GetDay() ) - 1;
            if ( level > 3 && endedDay >= 1 && endedDay <= 6 ) {
                int32_t * h1Order[6] = { &funds.wood, &funds.mercury, &funds.ore, &funds.sulfur, &funds.crystal, &funds.gems };
                *h1Order[endedDay - 1] += 1;
            }
        }
        if ( H1::isExtension() ) {
            // H1-INTENDED(D16) extension items: endless resources +1 a day each, Tax Lien -250 gold a day (per item a hero carries).
            for ( const Heroes * hero : kingdom.GetHeroes() ) {
                const auto count = [hero]( const int h1Artifact ) {
                    const int id = H1::engineArtifactFromH1( h1Artifact );
                    int32_t n = 0;
                    for ( const Artifact & art : hero->GetBagArtifacts() ) {
                        if ( id >= 0 && art.GetID() == id ) {
                            ++n;
                        }
                    }
                    return n;
                };
                funds.wood += count( 42 );
                funds.ore += count( 43 );
                funds.sulfur += count( 44 );
                funds.mercury += count( 45 );
                funds.gems += count( 46 );
                funds.crystal += count( 47 );
                gold -= 250 * count( 48 );
            }
        }
        funds.gold = gold;
        return funds;
    }

    void runKingdomOverview( const Kingdom & kingdom )
    {
        fheroes2::Image frame( 640, 480 );
        frame._disableTransformLayer();
        fheroes2::Fill( frame, 0, 0, 640, 480, 0 );
        const int colour = std::max( 0, Color::GetIndex( kingdom.GetColor() ) );
        const fheroes2::Image main = UI::readBmp( "overmain.bmp" );
        // overmain.bmp (544 x 480) at x 0x60; the banner overban<colour>.bmp (96 x 480) at x 0.
        fheroes2::Copy( main, 0, 0, frame, 0x60, 0, std::min( 640 - 0x60, main.width() ), std::min( 480, main.height() ) );
        char name[32];
        std::snprintf( name, sizeof( name ), "overban%01d.bmp", colour );
        const fheroes2::Image banner = UI::readBmp( name );
        fheroes2::Copy( banner, 0, 0, frame, 0, 0, std::min( 640, banner.width() ), std::min( 480, banner.height() ) );

        auto number = [&frame]( const int value, const int32_t x, const int32_t y, const int32_t w ) {
            UI::drawText( frame, std::to_string( value ), "bigfont.fnt", x, y, w, 0x10, 1, 1 );
        };

        // Heroes by class (hero +0x1c): picture = class at (0x88 j + 0x79, 0x20), frame 0xf at (+0x31, 0x43), number at (0x88 j + 0xa9, 0x4d).
        int count[8] = {};
        for ( const Heroes * h : kingdom.GetHeroes() ) {
            ++count[heroClass( *h )];
        }
        int j = 0;
        for ( int c = 0; c < 4; ++c ) {
            if ( count[c] == 0 ) {
                continue;
            }
            blit( frame, c, 0x88 * j + 0x79, 0x20 );
            blit( frame, 0xf, 0x88 * j + 0xaa, 0x43 );
            number( count[c], 0x88 * j + 0xa9, 0x4d, 0x21 );
            ++j;
        }
        // Castles (bit 0x40) and towns by race (castle +3): frames race + 4 at y 0x71 / race + 8 at y 0xc9, numbers at 0xad / 0x105.
        for ( int pass = 0; pass < 2; ++pass ) {
            std::fill( count, count + 8, 0 );
            for ( const Castle * c : kingdom.GetCastles() ) {
                if ( c->isCastle() == ( pass == 0 ) ) {
                    ++count[std::clamp( h1RaceFromEngine( c->GetRace() ), 0, 3 )]; // H1-INTENDED(D16): necromancer (4) under warlock
                }
            }
            j = 0;
            for ( int r = 0; r < 4; ++r ) {
                if ( count[r] == 0 ) {
                    continue;
                }
                blit( frame, r + ( pass == 0 ? 4 : 8 ), 0x88 * j + 100, pass == 0 ? 0x71 : 0xc9 );
                number( count[r], 0x88 * j + 100, pass == 0 ? 0xad : 0x105, 0x84 );
                ++j;
            }
        }
        // Mines by kind 0..6 (0 sawmill / wood, 1 alchemist / mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems, 6 gold): frame min(kind, 2) + 0xc,
        // kinds 2..6 also kind + 0xe over it; at (0x4d j + 100, 0x121), number at y 0x163, width 0x48.
        static const int resources[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
        j = 0;
        for ( int kind = 0; kind < 7; ++kind ) {
            const int n = static_cast<int>( world.CountCapturedMines( resources[kind], kingdom.GetColor() ) );
            if ( n == 0 ) {
                continue;
            }
            blit( frame, std::min( kind, 2 ) + 0xc, 0x4d * j + 100, 0x121 );
            if ( kind > 1 ) {
                blit( frame, kind + 0xe, 0x4d * j + 100, 0x121 );
            }
            UI::drawText( frame, std::to_string( n ), "bigfont.fnt", 0x4d * j + 100, 0x163, 0x48, 0x10, 1, 1 );
            ++j;
        }

        UI::Layout layout = UI::loadLayout( "overwind.bin" );
        // Table 0x492650 key 8.
        static const std::pair<int, const char *> labels[] = { { 200, "Heroes" },    { 201, "Castles" },  { 202, "Towns" },
                                                               { 203, "Mines" },     { 204, "Treasury" }, { 205, "Total Gold Per Day:" } };
        for ( const auto & [id, text] : labels ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->text = text;
            }
        }
        if ( UI::Item * it = layout.find( 0x40 ) ) {
            char title[96];
            std::snprintf( title, sizeof( title ), "Kingdom Overview     Month %d, Week %d, Day %d", static_cast<int>( world.GetMonth() ),
                           static_cast<int>( world.GetWeek() ), static_cast<int>( world.GetDay() ) );
            it->text = title;
        }
        if ( UI::Item * it = layout.find( 0x41 ) ) {
            it->text = std::to_string( goldPerDay( kingdom ) );
        }
        const Funds & f = kingdom.GetFunds();
        const int32_t values[7] = { f.wood, f.mercury, f.ore, f.sulfur, f.crystal, f.gems, f.gold };
        for ( int k = 0; k < 7; ++k ) {
            if ( UI::Item * it = layout.find( k + 1 ) ) {
                it->text = std::to_string( values[k] );
            }
        }
        UI::drawLayout( frame, layout, 0, 0, -1 );
        if ( world.CountCapturedObject( MP2::OBJ_DRAGON_CITY, kingdom.GetColor() ) > 0 ) {
            UI::drawText( frame, "You own Dragon City.", "smalfont.fnt", 100, 0x1c2, 400, 0xc, 1, 0 );
        }
        if ( world.CountCapturedObject( MP2::OBJ_LIGHTHOUSE, kingdom.GetColor() ) > 0 ) {
            UI::drawText( frame, "You own the Lighthouse.", "smalfont.fnt", 100, 0x1d1, 400, 0xc, 1, 0 );
        }

        // Shown full screen; the layout's Exit button closes it (FUN_00424203).
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        fheroes2::Copy( frame, 0, 0, display, ( display.width() - 640 ) / 2, ( display.height() - 480 ) / 2, 640, 480 );
        display.render();
        UI::Layout buttons;
        for ( const UI::Item & it : layout.items ) {
            if ( it.type == UI::ITEM_BUTTON ) {
                buttons.items.push_back( it );
            }
        }
        UI::runLayoutDialog( buttons, 0, 0, false );
        fheroes2::Copy( saved, display );
        display.render();
    }
}
