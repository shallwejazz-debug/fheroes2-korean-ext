// Heroes I right-click quick info windows (homm1-remaster project), note v121.
#include "h1_heroes.h"
#include "h1_quickinfo.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "army.h"
#include "army_troop.h"
#include "castle.h"
#include "game_interface.h"
#include "h1_battle.h"
#include "h1_gfx.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "h1_visit.h"
#include "heroes.h"
#include "interface_gamearea.h"
#include "kingdom.h"
#include "maps_tiles.h"
#include "monster.h"
#include "mp2.h"
#include "screen.h"
#include "settings.h"
#include "world.h"

namespace
{
    using H1::UI::Item;
    using H1::UI::Layout;

    // FUN_0045cd58 with 0: count words of a stack seen by another player.
    const char * countWord( const int count )
    {
        return count < 5 ? "Few" : ( count < 10 ? "Several" : ( count < 20 ? "Pack" : ( count < 50 ? "Lots" : ( count < 100 ? "Horde" : "Zounds!" ) ) ) );
    }

    int colourIndex( const PlayerColor color )
    {
        const int index = Color::GetIndex( color );
        return ( color == PlayerColor::NONE || index < 0 || index > 3 ) ? -1 : index;
    }

    int creatureOf( const Troop & troop )
    {
        if ( !troop.isValid() ) {
            return -1;
        }
        for ( int c = 0; c < H1::creatureCount(); ++c ) { // H1-INTENDED(D16): 36 in extension games
            if ( H1::creatureRow( c ).monsterId == troop.GetID() ) {
                return c;
            }
        }
        return -1;
    }

    void addIcon( Layout & layout, const int32_t x, const int32_t y, const int creature )
    {
        // FUN_0047bb00(x, y, 32, 32, "mons32.icn", creature, 0, 0xffff, 0x10, 1).
        Item it;
        it.type = H1::UI::ITEM_ICON;
        it.x = x;
        it.y = y;
        it.w = 32;
        it.h = 32;
        it.name = H1::smallCreatureIcn( creature ); // H1-INTENDED(D16): 28..35 from h1/ext/mons32x.icn
        it.frame = H1::smallCreatureFrame( creature );
        layout.items.push_back( it );
    }

    void addText( Layout & layout, const int32_t x, const int32_t y, const int32_t w, const std::string & text )
    {
        // FUN_0047cee0(x, y, w, 12, text, "smalfont.fnt", colour 1, 0xffff, 0x200): byte +0x2a = 1 (centred).
        Item it;
        it.type = H1::UI::ITEM_TEXT;
        it.x = x;
        it.y = y;
        it.w = w;
        it.h = 12;
        it.name = "smalfont.fnt";
        it.colour = 1;
        it.align = 1;
        it.text = text;
        layout.items.push_back( it );
    }

    void setText( Layout & layout, const int id, const std::string & text )
    {
        if ( Item * it = layout.find( id ); it != nullptr ) {
            it->text = text;
        }
    }

    void setFrame( Layout & layout, const int id, const int frame )
    {
        if ( Item * it = layout.find( id ); it != nullptr ) {
            it->frame = frame;
        }
    }

    void setFlags( Layout & layout, const PlayerColor owner )
    {
        // Message 4 to items 8 / 9: flag32.icn frames colour x 2 and colour x 2 + 1; no owner (town only): message 6 with 4 hides them.
        const int colour = colourIndex( owner );
        for ( int id = 8; id <= 9; ++id ) {
            if ( Item * it = layout.find( id ); it != nullptr ) {
                if ( colour < 0 ) {
                    it->hidden = true;
                }
                else {
                    it->frame = colour * 2 + ( id - 8 );
                }
            }
        }
    }

    // Stacks of another player's hero or a town (FUN_0045c027 / FUN_0045ce3a): 1..3 stacks in one row, 4 as 2 + 2, 5 as 2 + 3.
    void addStackRows( Layout & layout, const std::vector<std::pair<int, std::string>> & stacks, const int32_t width, const int32_t baseY,
                       const int32_t oneRowY, const int32_t textOffset, const int32_t textWidth, const bool townShift )
    {
        const int n = static_cast<int>( stacks.size() );
        int first = 2;
        int second = 3;
        int32_t y = baseY;
        if ( n >= 1 && n < 4 ) {
            y = oneRowY;
            first = n;
            second = 0;
        }
        else if ( n == 4 ) {
            second = 2;
        }
        int k = 0;
        int32_t step = width / first;
        for ( int i = 0; i < first && k < n; ++i, ++k ) {
            // FUN_0045ce3a: with 5 stacks the two of the first row move 12 pixels inwards.
            const int32_t shift = ( townShift && n == 5 ) ? ( i == 0 ? 12 : -12 ) : 0;
            const int32_t iconX = step * i + ( step - 32 ) / 2 + 9 + shift;
            addIcon( layout, iconX, y, stacks[k].first );
            if ( townShift ) {
                addText( layout, iconX - 14, y + 0x1e, textWidth, stacks[k].second );
            }
            else {
                addText( layout, step * i + 9, y + 0x1e, step, stacks[k].second );
            }
        }
        if ( second > 0 ) {
            step = width / second;
            for ( int i = 0; i < second && k < n; ++i, ++k ) {
                const int32_t iconX = step * i + ( step - 32 ) / 2 + 9;
                addIcon( layout, iconX, y + 0x2c, stacks[k].first );
                if ( townShift ) {
                    addText( layout, iconX - 14, y + 0x4a, textWidth, stacks[k].second );
                }
                else {
                    addText( layout, step * i + 9, y + textOffset, step, stacks[k].second );
                }
            }
        }
    }

    // FUN_004593aa with the view's top-left at (x - 7, y - 7) while the window is shown, then back (FUN_0045c027 / FUN_0045ce3a).
    class ViewCentre
    {
    public:
        explicit ViewCentre( const fheroes2::Point & tile )
        {
            Interface::AdventureMap & map = Interface::AdventureMap::Get();
            _saved = map.getGameArea().getCurrentCenterInPixels();
            map.getGameArea().SetCenter( tile );
            map.redraw( Interface::REDRAW_GAMEAREA );
            fheroes2::Display::instance().render();
        }

        ViewCentre( const ViewCentre & ) = delete;
        ViewCentre & operator=( const ViewCentre & ) = delete;

        ~ViewCentre()
        {
            Interface::AdventureMap & map = Interface::AdventureMap::Get();
            map.getGameArea().SetCenterInPixels( _saved );
            map.redraw( Interface::REDRAW_GAMEAREA );
            fheroes2::Display::instance().render();
        }

    private:
        fheroes2::Point _saved;
    };

    void showHero( const Heroes & hero, int32_t x, int32_t y )
    {
        const PlayerColor viewer = Settings::Get().CurrentColor();
        const bool own = hero.GetColor() == viewer;
        Layout layout = H1::UI::loadLayout( own ? "qhero0.bin" : "qhero1.bin" );
        if ( layout.width == 0 ) {
            return;
        }
        // Item 2: miniport.icn frame = hero number; 8 / 9 flags; 1 the name ("%s").
        setFrame( layout, 2, std::max( 0, H1::heroNumberOfId( hero.getPortraitId() ) ) );
        setFlags( layout, hero.GetColor() );
        setText( layout, 1, hero.GetName() );

        const Army & army = hero.GetArmy();
        std::vector<std::pair<int, int>> stacks;
        for ( size_t i = 0; i < 5; ++i ) {
            const Troop * troop = army.GetTroop( i );
            const int creature = troop != nullptr ? creatureOf( *troop ) : -1;
            if ( creature >= 0 ) {
                stacks.emplace_back( creature, static_cast<int>( troop->GetCount() ) );
            }
        }
        if ( own ) {
            // Items 3..6: attack, defense, spell power, knowledge ("%d" of +0x30..+0x33); then the stacks with exact counts in one row.
            const int stats[4] = { hero.GetAttack(), hero.GetDefense(), hero.GetPower(), hero.GetKnowledge() };
            for ( int i = 0; i < 4; ++i ) {
                setText( layout, 3 + i, std::to_string( stats[i] ) );
            }
            const int n = static_cast<int>( stacks.size() );
            const int32_t left = ( n * -0x20 + 0xa0 ) / 2 + 9;
            for ( int i = 0; i < n; ++i ) {
                addIcon( layout, i * 32 + left, 0x6e, stacks[i].first );
                addText( layout, i * 32 + left, 0x8c, 32, std::to_string( stacks[i].second ) );
            }
        }
        else if ( !stacks.empty() ) {
            std::vector<std::pair<int, std::string>> words;
            for ( const auto & s : stacks ) {
                words.emplace_back( s.first, countWord( s.second ) );
            }
            addStackRows( layout, words, 0xa0, 0x41, 0x57, 0x4a, 0, false );
        }
        // H1-INTENDED(D14): the original moves the view to the hero while this window is shown (FUN_0045c027 -> FUN_004593aa, note v121);
        // the owner chose to keep the view still (2026-11-17 19:02).
        H1::UI::runLayoutDialog( layout, x, y, true );
    }

    void showCastle( const Castle & castle, int32_t x, int32_t y )
    {
        const PlayerColor viewer = Settings::Get().CurrentColor();
        Layout layout = H1::UI::loadLayout( "qtown1.bin" );
        if ( layout.width == 0 ) {
            return;
        }
        // How much the viewer sees: 3 own town (exact counts), else the viewer's towns with a thieves' guild (FUN_0041f247, at most 2):
        // 2 count words, 1 "???", 0 "Unknown".
        int detail = 3;
        if ( castle.GetColor() != viewer ) {
            detail = 0;
            for ( const Castle * own : world.GetKingdom( viewer ).GetCastles() ) {
                if ( own != nullptr && own->isBuild( BUILD_THIEVESGUILD ) ) {
                    ++detail;
                }
            }
            detail = std::min( detail, 2 );
        }
        // Item 2: locators.icn 0xc + race (town) or 0x10 + race (castle built, bit 0x40).
        const int race = std::max( 0, H1::h1RaceFromEngine( castle.GetRace() ) );
        // H1-INTENDED(D16): necromancer (race 4) frames 22 / 23 appended (make_ext_necro_locators.py).
        setFrame( layout, 2, race == 4 ? ( castle.isCastle() ? 23 : 22 ) : ( castle.isCastle() ? 0x10 : 0xc ) + race );
        setFlags( layout, castle.GetColor() );
        // [Name: the engine castle name stands in for FUN_00427056 (table 0x493380 / campaign names).]
        setText( layout, 1, castle.GetName() );

        const Army & army = castle.GetArmy();
        std::vector<std::pair<int, std::string>> stacks;
        for ( size_t i = 0; i < 5; ++i ) {
            const Troop * troop = army.GetTroop( i );
            const int creature = troop != nullptr ? creatureOf( *troop ) : -1;
            if ( creature >= 0 ) {
                const int count = static_cast<int>( troop->GetCount() );
                stacks.emplace_back( creature, detail == 3 ? std::to_string( count ) : ( detail == 2 ? std::string( countWord( count ) ) : std::string( "???" ) ) );
            }
        }
        if ( detail == 0 || stacks.empty() ) {
            addText( layout, 0, 100, 0xd2, detail == 0 ? "Unknown" : "None" );
        }
        else {
            addStackRows( layout, stacks, 0xc0, 0x4b, 0x61, 0, 0x3c, true );
        }
        const ViewCentre centre( castle.GetCenter() );
        H1::UI::runLayoutDialog( layout, x, y, true );
    }

    // Place of a window next to the clicked tile (offset = view column / row x 32, the view starts at 16, 16): x = offset - dx, at least
    // 16, and moved to the given place when it would pass 0x1d0.
    int32_t place( const int32_t offset, const int32_t d, const int32_t size, const int32_t fallback )
    {
        int32_t v = offset - d;
        if ( v < 0x10 ) {
            v = 0x10;
        }
        if ( 0x1d0 < v + size ) {
            v = fallback;
        }
        return v;
    }
}

namespace H1
{
    bool showMapQuickInfo( const int32_t tileIndex )
    {
        if ( !H1::isGraphicsMode() || tileIndex < 0 || tileIndex >= world.w() * world.h() ) {
            return false;
        }
        const Interface::GameArea & area = Interface::AdventureMap::Get().getGameArea();
        const fheroes2::Point mp( tileIndex % world.w(), tileIndex / world.w() );
        const fheroes2::Point pos = area.GetRelativeTilePosition( mp );
        const fheroes2::Rect & roi = area.GetROI();
        // Column / row x 32 of the original = the tile's place in the view (the engine view may be shifted by half a tile).
        const int32_t col = pos.x - roi.x;
        const int32_t row = pos.y - roi.y;

        const Maps::Tile & tile = world.getTile( tileIndex );
        const PlayerColor viewer = Settings::Get().CurrentColor();
        if ( viewer == PlayerColor::NONE || !tile.isFog( viewer ) ) {
            const MP2::MapObjectType type = tile.getMainObjectType();
            if ( type == MP2::OBJ_CASTLE || type == MP2::OBJ_NON_ACTION_CASTLE ) {
                if ( const Castle * castle = world.getCastle( tile.GetCenter() ); castle != nullptr ) {
                    showCastle( *castle, place( col, 0x59, 0xd2, 0xfe ), place( row, 0x46, 0xac, 0x124 ) );
                    return true;
                }
            }
            if ( type == MP2::OBJ_HERO ) {
                if ( const Heroes * hero = tile.getHero(); hero != nullptr ) {
                    showHero( *hero, place( col, 0x49, 0xb2, 0x11e ), place( row, 0x41, 0xa2, 0x12e ) );
                    return true;
                }
            }
        }
        Layout layout = H1::UI::loadLayout( "qwikinfo.bin" );
        if ( layout.width == 0 ) {
            return false;
        }
        setText( layout, 1, H1::tileQuickInfoText( tileIndex ) );
        H1::UI::runLayoutDialog( layout, place( col, 0x39, 0x92, 0x13e ), place( row, 0x19, 0x52, 0x17e ), true );
        return true;
    }

    bool showHeroListQuickInfo( const Heroes & hero, const int slot )
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        showHero( hero, 0x12e, std::clamp( slot, 0, 3 ) * 0x1e + 0x6f );
        return true;
    }

    bool showCastleListQuickInfo( const Castle & castle )
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        showCastle( castle, 0x156, 0xb0 );
        return true;
    }
}
