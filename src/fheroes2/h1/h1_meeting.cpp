// Heroes I hero meeting window (homm1-remaster project), note v122.
#include "h1_heroes.h"
#include "h1_meeting.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "cursor.h"
#include "h1_armywin.h"
#include "h1_battle.h"
#include "h1_gfx.h"
#include "h1_heroscreen.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "localevent.h"
#include "monster.h"
#include "screen.h"

namespace
{
    namespace UI = H1::UI;

    constexpr int magicBook = 0x25; // '%'
    constexpr int fizbin = 0x0c; // Fizbin of Misfortune

    int creatureOf( const Troop * troop )
    {
        if ( troop == nullptr || !troop->isValid() ) {
            return -1;
        }
        for ( int c = 0; c < H1::creatureCount(); ++c ) { // H1-INTENDED(D16): 36 in extension games
            if ( H1::creatureRow( c ).monsterId == troop->GetID() ) {
                return c;
            }
        }
        return -1;
    }

    int artifactOf( const Artifact & art )
    {
        if ( !art.isValid() ) {
            return -1;
        }
        return art.GetID() == Artifact::MAGIC_BOOK ? magicBook : H1::h1ArtifactFromEngine( art.GetID() );
    }

    struct Meeting
    {
        Heroes * heroes[2]; // side 0 = +0x38 (right), side 1 = +0x3c (left), as the original indexes them (+0x38 + side x 4)
        UI::Layout layout;
        // +0x40 selected side, +0x41 target side, +0x42 selected slot, +0x43 target slot, +0x44 kind (0 army, 1 artifact); -1 = none.
        int selSide = -1;
        int tgtSide = -1;
        int selSlot = -1;
        int tgtSlot = -1;
        int kind = -1;

        Army & army( const int side ) const
        {
            return heroes[side]->GetArmy();
        }

        Troop * troop( const int side, const int slot ) const
        {
            return army( side ).GetTroop( static_cast<size_t>( slot ) );
        }

        int creature( const int side, const int slot ) const
        {
            return creatureOf( troop( side, slot ) );
        }

        int stacks( const int side ) const
        {
            int n = 0;
            for ( int k = 0; k < 5; ++k ) {
                if ( creature( side, k ) >= 0 ) {
                    ++n;
                }
            }
            return n;
        }

        // FUN_004678a0: slot of a creature (-1 = an empty slot) in an army, or -1 when absent.
        int find( const int side, const int c ) const
        {
            for ( int k = 0; k < 5; ++k ) {
                if ( creature( side, k ) == c ) {
                    return k;
                }
            }
            return -1;
        }

        Artifact * artifactSlot( const int side, const int slot ) const
        {
            BagArtifacts & bag = heroes[side]->GetBagArtifacts();
            return ( slot >= 0 && static_cast<size_t>( slot ) < bag.size() ) ? &bag[slot] : nullptr;
        }

        int artifact( const int side, const int slot ) const
        {
            const Artifact * art = artifactSlot( side, slot );
            return art != nullptr ? artifactOf( *art ) : -1;
        }

        void reset()
        {
            // FUN_0044f943
            selSide = tgtSide = selSlot = tgtSlot = kind = -1;
        }

        void update()
        {
            // FUN_00450d98: stats (67..70 left, 72..75 right), stacks (78..82 / 83..87, counts 116..120 / 121..125), artifacts (88..101 / 102..115).
            const int sideOf[2] = { 1, 0 }; // left column first
            for ( int column = 0; column < 2; ++column ) {
                const int side = sideOf[column];
                const Heroes & hero = *heroes[side];
                const int stats[4] = { hero.GetAttack(), hero.GetDefense(), hero.GetPower(), hero.GetKnowledge() };
                for ( int i = 0; i < 4; ++i ) {
                    if ( UI::Item * it = layout.find( ( column == 0 ? 67 : 72 ) + i ) ) {
                        it->text = std::to_string( stats[i] );
                    }
                }
                for ( int k = 0; k < 5; ++k ) {
                    const int c = creature( side, k );
                    UI::Item * icon = layout.find( ( column == 0 ? 78 : 83 ) + k );
                    UI::Item * count = layout.find( ( column == 0 ? 116 : 121 ) + k );
                    if ( icon != nullptr ) {
                        icon->hidden = c < 0;
                        icon->frame = std::max( c, 0 );
                    }
                    if ( count != nullptr ) {
                        count->hidden = c < 0;
                        count->text = c < 0 ? "" : std::to_string( troop( side, k )->GetCount() );
                    }
                }
                for ( int k = 0; k < 14; ++k ) {
                    const int a = artifact( side, k );
                    if ( UI::Item * icon = layout.find( ( column == 0 ? 88 : 102 ) + k ) ) {
                        icon->hidden = a < 0;
                        icon->frame = std::max( a, 0 );
                    }
                }
            }
        }

        // FUN_00450b0a: move, combine or exchange stacks (no Shift).
        void moveStacks()
        {
            const int c = creature( selSide, selSlot );
            if ( selSide != tgtSide ) {
                if ( stacks( selSide ) == 1 ) {
                    // A hero's last stack only changes places with a different creature.
                    if ( creature( tgtSide, tgtSlot ) == -1 || find( tgtSide, c ) >= 0 ) {
                        return;
                    }
                }
                if ( const int k = find( tgtSide, c ); k >= 0 ) {
                    Troop * t = troop( tgtSide, k );
                    t->SetCount( t->GetCount() + troop( selSide, selSlot )->GetCount() );
                    troop( selSide, selSlot )->Reset();
                    return;
                }
                const int target = creature( tgtSide, tgtSlot );
                if ( const int k = find( selSide, target ); k >= 0 ) {
                    // The target's creature is in the selected army (or both are empty slots): it joins slot k, then the selected stack moves.
                    Troop * t = troop( tgtSide, tgtSlot );
                    if ( target >= 0 ) {
                        Troop * s = troop( selSide, k );
                        s->SetCount( s->GetCount() + t->GetCount() );
                        t->Reset();
                    }
                    if ( k == selSlot ) {
                        return;
                    }
                    Army::SwapTroops( *troop( selSide, selSlot ), *t );
                    return;
                }
            }
            // FUN_00467c2b: exchange the two slots.
            Army::SwapTroops( *troop( selSide, selSlot ), *troop( tgtSide, tgtSlot ) );
        }

        // FUN_0045122a: splitwin.bin at (0xb1, 0x14): how many to move (FUN_00446f16: 0 .. count - 1).
        void split()
        {
            UI::Layout win = UI::loadLayout( "splitwin.bin" );
            Troop * src = troop( selSide, selSlot );
            const int total = static_cast<int>( src->GetCount() );
            const int c = creature( selSide, selSlot );
            if ( UI::Item * it = win.find( 1 ) ) {
                it->text = std::string( "Move how many " ) + H1::creatureNameOne( c ) + " troops from " + heroes[selSide]->GetName() + " to "
                           + heroes[tgtSide]->GetName() + "?";
            }
            int amount = 0;
            int answer = 0;
            while ( true ) {
                if ( UI::Item * it = win.find( 0x44 ) ) {
                    it->text = std::to_string( amount );
                }
                answer = UI::runLayoutDialog( win, 0xb1, 0x14, false );
                if ( answer == 0x44 ) {
                    if ( UI::Item * it = win.find( 0x44 ) ) {
                        UI::applyEditKey( *it );
                        amount = std::clamp( std::atoi( it->text.c_str() ), 0, total - 1 );
                    }
                }
                else if ( answer == 0x45 ) {
                    amount = std::min( amount + 1, total - 1 );
                }
                else if ( answer == 0x46 ) {
                    amount = std::max( amount - 1, 0 );
                }
                else {
                    break;
                }
            }
            // OK (0x7802 in the Windows build = 0x8002 here).
            if ( answer != 0x8002 ) {
                return;
            }
            if ( const int k = find( tgtSide, c ); k >= 0 ) {
                Troop * t = troop( tgtSide, k );
                t->SetCount( t->GetCount() + static_cast<uint32_t>( amount ) );
            }
            else {
                int slot = tgtSlot;
                if ( creature( tgtSide, slot ) != -1 ) {
                    for ( int k = 0; k < 5; ++k ) {
                        if ( creature( tgtSide, k ) == -1 ) {
                            slot = k;
                            break;
                        }
                    }
                }
                troop( tgtSide, slot )->Set( Monster( src->GetID() ), static_cast<uint32_t>( amount ) );
            }
            src->SetCount( static_cast<uint32_t>( total - amount ) );
            if ( src->GetCount() == 0 ) {
                src->Reset();
            }
        }

        // FUN_004509f8: exchange two artifact slots (the stat bonuses FUN_00403fb9 follow the engine's artifacts).
        void swapArtifacts()
        {
            Artifact * a = artifactSlot( selSide, selSlot );
            Artifact * b = artifactSlot( tgtSide, tgtSlot );
            if ( a != nullptr && b != nullptr ) {
                std::swap( *a, *b );
            }
        }

        void clickStack( const int side, const int slot, const bool right, const bool shift )
        {
            if ( right ) {
                if ( Troop * t = troop( side, slot ); creatureOf( t ) >= 0 ) {
                    H1::showCreatureWindow( t->GetID(), static_cast<int>( t->GetCount() ), heroes[side], &army( side ), true, true );
                }
                return;
            }
            if ( kind == 0 ) {
                tgtSide = side;
                tgtSlot = slot;
                if ( selSide == tgtSide && selSlot == tgtSlot ) {
                    // FUN_00450953: the creature window (Dismiss hidden for the last stack), then nothing is selected.
                    Troop * t = troop( selSide, selSlot );
                    H1::showCreatureWindow( t->GetID(), static_cast<int>( t->GetCount() ), heroes[selSide], &army( selSide ), stacks( selSide ) == 1, false );
                    reset();
                    return;
                }
                const int target = creature( tgtSide, tgtSlot );
                if ( !shift || selSide == tgtSide || ( target != -1 && creature( selSide, selSlot ) != target ) ) {
                    moveStacks();
                }
                else {
                    split();
                }
                reset();
            }
            else if ( creature( side, slot ) == -1 ) {
                reset();
            }
            else {
                selSide = side;
                tgtSide = -1;
                kind = 0;
                selSlot = slot;
                tgtSlot = -1;
            }
        }

        void clickArtifact( const int side, const int slot, const bool right )
        {
            const int a = artifact( side, slot );
            if ( right ) {
                if ( a >= 0 ) {
                    UI::messageBox( H1::artifactInfoText( a ), UI::MESSAGE_POPUP, -1, 0x1c );
                }
                return;
            }
            if ( a == magicBook || a == fizbin ) {
                UI::messageBox( "This item can't be traded.", UI::MESSAGE_OKAY );
                return;
            }
            if ( kind == 1 ) {
                tgtSide = side;
                tgtSlot = slot;
                if ( selSide == tgtSide && selSlot == tgtSlot ) {
                    UI::messageBox( H1::artifactInfoText( a ), UI::MESSAGE_OKAY, -1, 0x1c );
                    reset();
                    return;
                }
                swapArtifacts();
                reset();
            }
            else if ( a == -1 ) {
                reset();
            }
            else {
                selSide = side;
                tgtSide = -1;
                kind = 1;
                selSlot = slot;
                tgtSlot = -1;
            }
        }

        // FUN_0044fd88: the selection frame (swapbtn.icn frame 2 in colour 0xe8), window coordinates.
        bool selectionFrame( int32_t & x, int32_t & y ) const
        {
            if ( selSide == -1 || selSlot == -1 ) {
                return false;
            }
            if ( kind == 0 ) {
                x = selSlot * 0x23 + ( selSide == 0 ? 0xfb : 0x17 );
                y = 0x93;
            }
            else {
                x = ( selSlot < 7 ? 0 : 0x23 ) + ( selSide == 0 ? 0x130 : 0x4b );
                y = ( selSlot % 7 ) * 0x23 + 0xc1;
            }
            return true;
        }
    };
}

namespace H1
{
    bool runMeetingWindow( Heroes & left, Heroes & right )
    {
        if ( !H1::isGraphicsMode() ) {
            return false;
        }
        Meeting m;
        m.heroes[0] = &right;
        m.heroes[1] = &left;
        m.layout = UI::loadLayout( "swapwin.bin" );
        if ( m.layout.width == 0 ) {
            return false;
        }
        // FUN_0044f990: portraits (65 left, 66 right), title 77 "%s meets %s".
        for ( int id = 65; id <= 66; ++id ) {
            if ( UI::Item * it = m.layout.find( id ) ) {
                char name[16];
                std::snprintf( name, sizeof( name ), "port%04d.icn", std::max( 0, H1::heroNumberOfId( ( id == 65 ? left : right ).getPortraitId() ) ) );
                it->name = name;
            }
        }
        if ( UI::Item * it = m.layout.find( 77 ) ) {
            it->text = left.GetName() + " meets " + right.GetName();
        }
        m.update();

        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = display;
        const int32_t ox = ( display.width() - 640 ) / 2 + 0x10;
        const int32_t oy = ( display.height() - 480 ) / 2 + 0x10;
        const std::vector<fheroes2::Sprite> & frames = H1::getIcnMono( "swapbtn.icn" );
        int pressed = -1;
        auto draw = [&]() {
            fheroes2::Image frame = saved;
            UI::drawLayout( frame, m.layout, ox, oy, pressed );
            int32_t x = 0;
            int32_t y = 0;
            if ( m.selectionFrame( x, y ) && frames.size() > 2 ) {
                UI::drawMonoSprite( frame, frames[2], ox + x, oy + y, 0xe8 );
            }
            fheroes2::Copy( frame, display );
            display.render();
        };
        auto itemAt = [&]( const fheroes2::Point & p ) {
            for ( const UI::Item & it : m.layout.items ) {
                if ( it.id >= 65 && it.id <= 0x73 && it.id != 77 && ( it.type == UI::ITEM_ICON ) && p.x >= ox + it.x && p.y >= oy + it.y
                     && p.x < ox + it.x + it.w && p.y < oy + it.y + it.h ) {
                    return it.id;
                }
                if ( it.type == UI::ITEM_BUTTON && p.x >= ox + it.x && p.y >= oy + it.y && p.x < ox + it.x + it.w && p.y < oy + it.y + it.h ) {
                    return it.id;
                }
            }
            return -1;
        };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        draw();
        while ( le.HandleEvents() ) {
            const int over = itemAt( le.getMouseCursorPos() );
            const int newPressed = ( over == 0x8000 && le.isMouseLeftButtonPressed() ) ? 0x8000 : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                draw();
            }
            if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                break; // Exit hotkey 1
            }
            const bool leftClick = over != -1 && le.MouseClickLeft();
            const bool rightClick = over != -1 && le.isMouseRightButtonPressed();
            if ( !leftClick && !rightClick ) {
                continue;
            }
            if ( over == 0x8000 ) {
                if ( leftClick ) {
                    break;
                }
                continue;
            }
            const bool shift = ( LocalEvent::getCurrentKeyModifiers() & fheroes2::KEY_MODIFIER_SHIFT ) != 0;
            if ( over == 0x41 || over == 0x42 ) {
                if ( leftClick ) {
                    // FUN_00447aad( hero, 1 ) [the engine "no dismiss" flag stands in for the 1, as in the castle screen].
                    H1::runHeroScreen( over == 0x41 ? left : right, false, true );
                    m.reset();
                }
            }
            else if ( over >= 0x4e && over <= 0x52 ) {
                m.clickStack( 1, over - 0x4e, rightClick, shift );
            }
            else if ( over >= 0x53 && over <= 0x57 ) {
                m.clickStack( 0, over - 0x53, rightClick, shift );
            }
            else if ( over >= 0x58 && over <= 0x65 ) {
                m.clickArtifact( 1, over - 0x58, rightClick );
            }
            else if ( over >= 0x66 && over <= 0x73 ) {
                m.clickArtifact( 0, over - 0x66, rightClick );
            }
            m.update();
            draw();
        }
        fheroes2::Copy( saved, display );
        display.render();
        return true;
    }
}
