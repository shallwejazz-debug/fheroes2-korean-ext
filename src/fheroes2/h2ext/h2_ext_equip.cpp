/***************************************************************************
 *   H2-EXT: equipment window (docs/H2_ITEM_CONCEPT_v5.md 5-1, owner 09:01  *
 *   "2번에 기타만 3번처럼": worn slots big, misc big, backpack 15x4 small) *
 ***************************************************************************/

#include "h2_ext_equip.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "game_assets.h"
#include "game_hotkeys.h"
#include "spell_book.h"
#include "artifact.h"
#include "cursor.h"
#include "dialog.h"
#include "h2_ext_items.h"
#include "h2_ext_sets.h"
#include "heroes.h"
#include "icn.h"
#include "image.h"
#include "localevent.h"
#include "pal.h"
#include "screen.h"
#include "translations.h"
#include "ui_button.h"
#include "ui_dialog.h"
#include "ui_text.h"

namespace
{
    struct SlotPlace
    {
        size_t index;
        int32_t x;
        int32_t y;
        bool big;
    };

    // Positions inside the 640x480 window (results/h2korean/equip_window_final_v1.png)
    std::vector<SlotPlace> slotPlaces()
    {
        std::vector<SlotPlace> places = {
            { H2Ext::SLOT_CLOAK, 222, 26, true },   { H2Ext::SLOT_HEAD, 318, 26, true },          { H2Ext::SLOT_NECK, 414, 26, true },
            { H2Ext::SLOT_WEAPON, 222, 112, true }, { H2Ext::SLOT_SHIELD, 414, 112, true },       { H2Ext::SLOT_FEET, 162, 200, true },
            { H2Ext::SLOT_HANDS, 234, 200, true },  { H2Ext::SLOT_BODY, 318, 200, true },         { H2Ext::SLOT_RING_FIRST, 402, 200, true },
            { H2Ext::SLOT_RING_FIRST + 1, 470, 200, true },
        };
        for ( size_t n = 0; n < H2Ext::SLOT_MISC_COUNT; ++n ) {
            places.push_back( { H2Ext::SLOT_MISC_FIRST + n, static_cast<int32_t>( 16 + ( n % 2 ) * 72 ), static_cast<int32_t>( 42 + ( n / 2 ) * 72 ), true } );
        }
        places.push_back( { H2Ext::SLOT_BOOK, 88, 186, true } );
        for ( size_t n = 0; n < H2Ext::BACKPACK_COUNT; ++n ) {
            places.push_back(
                { H2Ext::WORN_COUNT + n, static_cast<int32_t>( 12 + ( n % 15 ) * 41 ), static_cast<int32_t>( 294 + ( n / 15 ) * 41 ), false } );
        }
        return places;
    }

    // The name written in an empty worn slot (owner 2026-11-19 18:57 "그게 좋겠다")
    const char * slotLabel( const size_t index )
    {
        switch ( index ) {
        case H2Ext::SLOT_HEAD:
            return _( "Head" );
        case H2Ext::SLOT_NECK:
            return _( "Neck" );
        case H2Ext::SLOT_CLOAK:
            return _( "Cloak" );
        case H2Ext::SLOT_WEAPON:
            return _( "Weapon" );
        case H2Ext::SLOT_SHIELD:
            return _( "Shield" );
        case H2Ext::SLOT_BODY:
            return _( "Body" );
        case H2Ext::SLOT_HANDS:
            return _( "Hands" );
        case H2Ext::SLOT_FEET:
            return _( "Feet" );
        default:
            break;
        }
        if ( index == H2Ext::SLOT_RING_FIRST || index == H2Ext::SLOT_RING_FIRST + 1 ) {
            return _( "Ring" );
        }
        if ( index >= H2Ext::SLOT_MISC_FIRST && index < H2Ext::SLOT_MISC_FIRST + H2Ext::SLOT_MISC_COUNT ) {
            return _( "Misc." );
        }
        return "";
    }

    const uint8_t colorGold = 0;
}

namespace H2Ext
{
    bool showEquipmentWindow( Heroes & hero, const bool readonly )
    {
        BagArtifacts & bag = hero.GetBagArtifacts();
        if ( !isExtBag( bag ) ) {
            return false;
        }

        fheroes2::Display & display = fheroes2::Display::instance();
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        const fheroes2::Rect roi( ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2, ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2,
                                  fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT );
        fheroes2::ImageRestorer restorer( display, roi.x, roi.y, roi.width, roi.height );

        const uint8_t gold = fheroes2::GetColorId( 0xDC, 0xB4, 0x3C );
        const uint8_t goldLight = fheroes2::GetColorId( 0xFC, 0xF0, 0x98 );
        const uint8_t panelDark = fheroes2::GetColorId( 0x34, 0x1C, 0x10 );
        const uint8_t slotDark = fheroes2::GetColorId( 0x28, 0x14, 0x0C );
        const uint8_t edgeBrown = fheroes2::GetColorId( 0x94, 0x6C, 0x28 );
        (void)colorGold;

        const std::vector<SlotPlace> places = slotPlaces();
        const fheroes2::Rect portraitRect( roi.x + 300, roi.y + 98, 101, 93 );
        const fheroes2::Rect catalogRect( roi.x + 538, roi.y + 244, 86, 16 );
        // join / split a misc set (docs/H2_EXTRAS_CONCEPT_v1.md 5)
        const fheroes2::Rect combineRect( roi.x + 16, roi.y + 253, 136, 15 );
        auto combinableSet = [&bag]() {
            for ( size_t si = 0; si < artifactSets().size(); ++si ) {
                if ( canCombineSet( bag, static_cast<int>( si ) ) ) {
                    return static_cast<int>( si );
                }
            }
            return -1;
        };

        const fheroes2::Sprite & exitReleased = Assets::getImage( ICN::BUTTON_SMALL_EXIT_GOOD, 0 );
        fheroes2::Button buttonExit( roi.x + 632 - exitReleased.width(), roi.y + 2, ICN::BUTTON_SMALL_EXIT_GOOD, 0, 1 );

        int selected = -1;
        bool changed = false;

        auto slotRect = [&roi]( const SlotPlace & p ) {
            return p.big ? fheroes2::Rect( roi.x + p.x, roi.y + p.y, 64, 64 ) : fheroes2::Rect( roi.x + p.x, roi.y + p.y, 32, 32 );
        };

        auto redraw = [&]() {
            fheroes2::Fill( display, roi.x, roi.y, roi.width, roi.height, fheroes2::GetColorId( 0x70, 0x12, 0x0E ) );
            fheroes2::DrawRect( display, roi, gold );

            // title
            fheroes2::Fill( display, roi.x + 8, roi.y + 4, 624, 16, panelDark );
            const fheroes2::Text title( std::string( _( "Equipment" ) ) + " - " + hero.GetName(), fheroes2::FontType::smallYellow() );
            title.draw( roi.x + ( roi.width - title.width() ) / 2, roi.y + 7, display );

            // panels
            fheroes2::Fill( display, roi.x + 8, roi.y + 24, 624, 246, panelDark );
            fheroes2::DrawRect( display, { roi.x + 8, roi.y + 24, 624, 246 }, edgeBrown );
            fheroes2::Text( _( "Misc." ), fheroes2::FontType::smallYellow() ).draw( roi.x + 16, roi.y + 29, display );
            fheroes2::Fill( display, roi.x + 8, roi.y + 276, 624, 198, panelDark );
            fheroes2::DrawRect( display, { roi.x + 8, roi.y + 276, 624, 198 }, edgeBrown );
            fheroes2::Text( _( "Backpack (no effect)" ), fheroes2::FontType::smallYellow() ).draw( roi.x + 12, roi.y + 280, display );

            // set effects panel (filled by the set rules, docs/H2_ITEM_CONCEPT_v5.md 6)
            fheroes2::Fill( display, roi.x + 536, roi.y + 40, 90, 222, fheroes2::GetColorId( 0x28, 0x14, 0x08 ) );
            fheroes2::DrawRect( display, { roi.x + 536, roi.y + 40, 90, 222 }, edgeBrown );
            fheroes2::Text( _( "Set effects" ), fheroes2::FontType::smallYellow() ).draw( roi.x + 541, roi.y + 45, display );
            {
                int32_t ty = roi.y + 60;
                const std::vector<SetDefinition> & sets = artifactSets();
                for ( size_t si = 0; si < sets.size() && ty < roi.y + 238; ++si ) {
                    const int worn = wornPieces( bag, static_cast<int>( si ) );
                    if ( worn == 0 ) {
                        continue;
                    }
                    const bool anyActive = worn >= sets[si].tiers.front().pieces;
                    fheroes2::Text head( std::to_string( sets[si].number ) + " " + _( sets[si].name ) + " " + std::to_string( worn ) + "/"
                                             + std::to_string( sets[si].pieces.size() ),
                                         anyActive ? fheroes2::FontType::smallYellow() : fheroes2::FontType::smallWhite() );
                    if ( ty + head.height( 82 ) > roi.y + 241 ) {
                        break;
                    }
                    head.draw( roi.x + 541, ty, 82, display );
                    ty += head.height( 82 ) + 1;
                    bool nextShown = false;
                    for ( const SetTier & tier : sets[si].tiers ) {
                        // the tiers that are on, and the next one
                        if ( worn < tier.pieces ) {
                            if ( nextShown ) {
                                break;
                            }
                            nextShown = true;
                        }
                        const fheroes2::Text line( _( tier.text ), worn >= tier.pieces ? fheroes2::FontType::smallYellow()
                                                                                         : fheroes2::FontType{ fheroes2::FontSize::SMALL, fheroes2::FontColor::GRAY } );
                        if ( ty + line.height( 78 ) > roi.y + 241 ) {
                            ty = roi.y + 999;
                            break;
                        }
                        line.draw( roi.x + 545, ty, 78, display );
                        ty += line.height( 78 ) + 1;
                    }
                    ty += 4;
                }
            }

            // set catalog button at the bottom of the set panel (owner 2026-11-19 16:26, docs/H2_EXTRAS_CONCEPT_v1.md 4)
            fheroes2::Fill( display, catalogRect.x, catalogRect.y, catalogRect.width, catalogRect.height, fheroes2::GetColorId( 0x50, 0x30, 0x10 ) );
            fheroes2::DrawRect( display, catalogRect, gold );
            {
                const fheroes2::Text label( std::string( _( "Set catalog" ) ) + " (S)", fheroes2::FontType::smallYellow() );
                label.draw( catalogRect.x + ( catalogRect.width - label.width() ) / 2, catalogRect.y + 4, display );
            }

            // join / split button: shown when a misc set can be joined, or when a combined artifact is selected
            {
                const bool canSplit = selected >= 0 && setOfCombinedArtifact( bag[static_cast<size_t>( selected )].GetID() ) >= 0;
                if ( !readonly && ( canSplit || combinableSet() >= 0 ) ) {
                    fheroes2::Fill( display, combineRect.x, combineRect.y, combineRect.width, combineRect.height, fheroes2::GetColorId( 0x50, 0x30, 0x10 ) );
                    fheroes2::DrawRect( display, combineRect, gold );
                    const fheroes2::Text label( std::string( canSplit ? _( "Split" ) : _( "Join set" ) ) + " (C)", fheroes2::FontType::smallYellow() );
                    label.draw( combineRect.x + ( combineRect.width - label.width() ) / 2, combineRect.y + 4, display );
                }
            }

            // portrait
            fheroes2::DrawRect( display, { portraitRect.x - 2, portraitRect.y - 2, portraitRect.width + 4, portraitRect.height + 4 }, gold );
            hero.PortraitRedraw( portraitRect.x, portraitRect.y, PORT_BIG, display );

            for ( const SlotPlace & p : places ) {
                const fheroes2::Rect r = slotRect( p );
                fheroes2::DrawRect( display, { r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, slotDark );
                fheroes2::DrawRect( display, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, static_cast<int>( p.index ) == selected ? goldLight : gold );
                fheroes2::Fill( display, r.x, r.y, r.width, r.height, slotDark );

                const Artifact & art = bag[p.index];
                if ( art.isValid() ) {
                    // a worn piece of a complete set shines (gold rings around the slot)
                    if ( p.index < WORN_COUNT && isSetComplete( bag, setIndexOf( art.GetID() ) ) ) {
                        fheroes2::DrawRect( display, { r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, fheroes2::GetColorId( 0xFC, 0xD4, 0x3C ) );
                        fheroes2::DrawRect( display, { r.x - 4, r.y - 4, r.width + 8, r.height + 8 }, fheroes2::GetColorId( 0xC8, 0x8C, 0x20 ) );
                        fheroes2::DrawRect( display, { r.x - 5, r.y - 5, r.width + 10, r.height + 10 }, fheroes2::GetColorId( 0x80, 0x50, 0x14 ) );
                        fheroes2::DrawRect( display, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, goldLight );
                    }
                    const fheroes2::Sprite & sprite
                        = p.big ? Assets::getImage( ICN::ARTIFACT, art.IndexSprite64() ) : Assets::getImage( ICN::ARTFX, art.IndexSprite32() );
                    fheroes2::Blit( sprite, display, r.x, r.y );
                    drawArtifactSetBadge( display, r.x + r.width, r.y + r.height, art.GetID(), bag );
                }
                else if ( p.index < WORN_COUNT ) {
                    // owner 2026-11-19 18:29 "공란으로" / 18:57 "그게 좋겠다": an empty worn slot shows only the name of the slot
                    const fheroes2::Text label( slotLabel( p.index ), fheroes2::FontType{ fheroes2::FontSize::SMALL, fheroes2::FontColor::GRAY } );
                    label.draw( r.x + ( r.width - label.width() ) / 2, r.y + ( r.height - label.height() ) / 2 + 2, display );
                }
                // owner 18:57 "아이템 들면 그 부분이 빛난다던가": while an artifact is selected, the worn slots it can go to shine
                if ( selected >= 0 && static_cast<int>( p.index ) != selected && p.index < WORN_COUNT && bag[static_cast<size_t>( selected )].isValid()
                     && slotAccepts( p.index, bag[static_cast<size_t>( selected )].GetID() ) ) {
                    fheroes2::DrawRect( display, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, fheroes2::GetColorId( 0x9C, 0xF4, 0x8C ) );
                    fheroes2::DrawRect( display, { r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, fheroes2::GetColorId( 0x5C, 0xD0, 0x4C ) );
                    fheroes2::DrawRect( display, { r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, fheroes2::GetColorId( 0x2C, 0x8C, 0x24 ) );
                }
                if ( static_cast<int>( p.index ) == selected ) {
                    fheroes2::DrawRect( display, { r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, goldLight );
                }
            }

            buttonExit.draw();
            display.render( roi );
        };

        redraw();

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );
            if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
                break;
            }
            if ( !readonly && ( le.MouseClickLeft( combineRect ) || le.isKeyPressed( fheroes2::Key::KEY_C ) ) ) {
                if ( selected >= 0 && splitCombined( bag, static_cast<size_t>( selected ) ) ) {
                    changed = true;
                }
                else if ( const int si = combinableSet(); si >= 0 && combineSet( bag, si ) ) {
                    changed = true;
                }
                selected = -1;
                redraw();
                continue;
            }
            if ( le.MouseClickLeft( catalogRect ) || le.isKeyPressed( fheroes2::Key::KEY_S ) ) {
                showSetCatalog( hero );
                redraw();
                continue;
            }

            for ( const SlotPlace & p : places ) {
                const fheroes2::Rect r = slotRect( p );
                if ( le.isMouseRightButtonPressedInArea( r ) ) {
                    if ( bag[p.index].isValid() ) {
                        fheroes2::ArtifactDialogElement popup( bag[p.index] );
                        popup.setSetBag( &bag );
                        popup.showPopup( Dialog::ZERO );
                    }
                    break;
                }
                if ( !le.MouseClickLeft( r ) ) {
                    continue;
                }

                const int index = static_cast<int>( p.index );
                if ( index == static_cast<int>( SLOT_BOOK ) ) {
                    // the magic book stays in its slot; a click opens it
                    selected = -1;
                    if ( bag[SLOT_BOOK].isValid() ) {
                        hero.OpenSpellBook( SpellBook::Filter::ALL, false, false, {} );
                    }
                }
                else if ( readonly ) {
                    selected = -1;
                }
                else if ( selected < 0 ) {
                    if ( bag[p.index].isValid() ) {
                        selected = index;
                    }
                }
                else if ( selected == index ) {
                    selected = -1;
                }
                else {
                    // move or swap when both artifacts fit the other place
                    Artifact & from = bag[static_cast<size_t>( selected )];
                    Artifact & to = bag[p.index];
                    if ( slotAccepts( p.index, from.GetID() ) && ( !to.isValid() || slotAccepts( static_cast<size_t>( selected ), to.GetID() ) ) ) {
                        std::swap( from, to );
                        changed = true;
                        selected = -1;
                    }
                    else if ( to.isValid() ) {
                        selected = index;
                    }
                }
                redraw();
                break;
            }
        }

        return changed;
    }

    void showTradeWindow( Heroes & left, Heroes & right )
    {
        if ( !isExtBag( left.GetBagArtifacts() ) || !isExtBag( right.GetBagArtifacts() ) ) {
            return;
        }
        fheroes2::Display & display = fheroes2::Display::instance();
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        const fheroes2::Rect roi( ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2, ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2,
                                  fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT );
        fheroes2::ImageRestorer restorer( display, roi.x, roi.y, roi.width, roi.height );

        const uint8_t gold = fheroes2::GetColorId( 0xDC, 0xB4, 0x3C );
        const uint8_t panelDark = fheroes2::GetColorId( 0x34, 0x1C, 0x10 );
        const uint8_t slotDark = fheroes2::GetColorId( 0x28, 0x14, 0x0C );
        const uint8_t edgeBrown = fheroes2::GetColorId( 0x94, 0x6C, 0x28 );

        const fheroes2::Sprite & exitReleased = Assets::getImage( ICN::BUTTON_SMALL_EXIT_GOOD, 0 );
        fheroes2::Button buttonExit( roi.x + 632 - exitReleased.width(), roi.y + 2, ICN::BUTTON_SMALL_EXIT_GOOD, 0, 1 );

        std::array<Heroes *, 2> heroes{ &left, &right };
        auto cellRect = [&roi]( const int side, const size_t i ) {
            return fheroes2::Rect( roi.x + 12 + side * 316 + static_cast<int32_t>( i % 8 ) * 37, roi.y + 60 + static_cast<int32_t>( i / 8 ) * 37, 32, 32 );
        };

        auto redraw = [&]() {
            fheroes2::Fill( display, roi.x, roi.y, roi.width, roi.height, fheroes2::GetColorId( 0x70, 0x12, 0x0E ) );
            fheroes2::DrawRect( display, roi, gold );
            const fheroes2::Text title( _( "Trade artifacts" ), fheroes2::FontType::normalYellow() );
            title.draw( roi.x + ( roi.width - title.width() ) / 2, roi.y + 8, display );
            const fheroes2::Text hint( _( "Click an artifact to give it to the other hero." ), fheroes2::FontType::smallWhite() );
            hint.draw( roi.x + ( roi.width - hint.width() ) / 2, roi.y + 460, display );
            for ( int side = 0; side < 2; ++side ) {
                const int32_t px = roi.x + 6 + side * 316;
                fheroes2::Fill( display, px, roi.y + 30, 304, 424, panelDark );
                fheroes2::DrawRect( display, { px, roi.y + 30, 304, 424 }, edgeBrown );
                fheroes2::Text( heroes[side]->GetName(), fheroes2::FontType::smallYellow() ).draw( px + 6, roi.y + 36, display );
                const BagArtifacts & bag = heroes[side]->GetBagArtifacts();
                for ( size_t i = 0; i < bag.size(); ++i ) {
                    const fheroes2::Rect r = cellRect( side, i );
                    fheroes2::DrawRect( display, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, i < WORN_COUNT ? gold : edgeBrown );
                    fheroes2::Fill( display, r.x, r.y, r.width, r.height, slotDark );
                    if ( bag[i].isValid() ) {
                        fheroes2::Blit( Assets::getImage( ICN::ARTFX, bag[i].IndexSprite32() ), display, r.x, r.y );
                        drawArtifactSetBadge( display, r.x + r.width, r.y + r.height, bag[i].GetID(), bag );
                    }
                }
            }
            buttonExit.draw();
            display.render( roi );
        };
        redraw();

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );
            if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
                break;
            }
            bool done = false;
            for ( int side = 0; side < 2 && !done; ++side ) {
                BagArtifacts & bag = heroes[side]->GetBagArtifacts();
                for ( size_t i = 0; i < bag.size(); ++i ) {
                    const fheroes2::Rect r = cellRect( side, i );
                    if ( le.isMouseRightButtonPressedInArea( r ) ) {
                        if ( bag[i].isValid() ) {
                            fheroes2::ArtifactDialogElement popup( bag[i] );
                            popup.setSetBag( &bag );
                            popup.showPopup( Dialog::ZERO );
                        }
                        done = true;
                        break;
                    }
                    if ( le.MouseClickLeft( r ) ) {
                        if ( bag[i].isValid() && i != SLOT_BOOK && pushToExtBag( heroes[1 - side]->GetBagArtifacts(), bag[i] ) ) {
                            bag[i].Reset();
                            heroes[1 - side]->GetBagArtifacts().assembleArtifactSetIfPossible();
                            redraw();
                        }
                        done = true;
                        break;
                    }
                }
            }
        }
    }

    void drawSetBadge( fheroes2::Image & dst, const int32_t right, const int32_t bottom, const int number, const bool complete )
    {
        // 3x5 digits
        static const std::array<const char *, 10> digits = { "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
                                                              "111100111001111", "111100111101111", "111001001001001", "111101111101111", "111101111001111" };
        const std::string text = std::to_string( number );
        const int32_t textWidth = static_cast<int32_t>( text.size() ) * 4 - 1;
        const int32_t w = std::max( 9, textWidth + 4 );
        const int32_t h = 9;
        const int32_t x = right - w - 1;
        const int32_t y = bottom - h - 1;
        const uint8_t edge = complete ? fheroes2::GetColorId( 0x78, 0x50, 0x00 ) : fheroes2::GetColorId( 0x00, 0x46, 0x00 );
        const uint8_t fill = complete ? fheroes2::GetColorId( 0xFF, 0xDC, 0x3C ) : fheroes2::GetColorId( 0x28, 0xB4, 0x28 );
        const uint8_t shine = complete ? fheroes2::GetColorId( 0xFF, 0xFF, 0xD0 ) : fheroes2::GetColorId( 0x8C, 0xF0, 0x8C );
        const uint8_t ink = complete ? fheroes2::GetColorId( 0x3C, 0x1E, 0x00 ) : fheroes2::GetColorId( 0xFF, 0xFF, 0xFF );
        fheroes2::Fill( dst, x + 1, y, w - 2, h, edge );
        fheroes2::Fill( dst, x, y + 1, w, h - 2, edge );
        fheroes2::Fill( dst, x + 1, y + 1, w - 2, h - 2, fill );
        fheroes2::Fill( dst, x + 2, y + 1, w - 4, 1, shine );
        int32_t cx = x + ( w - textWidth ) / 2;
        for ( const char c : text ) {
            const char * pattern = digits[static_cast<size_t>( c - '0' )];
            for ( int32_t py = 0; py < 5; ++py ) {
                for ( int32_t px = 0; px < 3; ++px ) {
                    if ( pattern[py * 3 + px] == '1' ) {
                        fheroes2::Fill( dst, cx + px, y + 2 + py, 1, 1, ink );
                    }
                }
            }
            cx += 4;
        }
    }

    void drawArtifactSetBadge( fheroes2::Image & dst, const int32_t right, const int32_t bottom, const int artifactId, const BagArtifacts & bag )
    {
        const int setIndex = setIndexOf( artifactId );
        if ( setIndex < 0 || !isExtBag( bag ) ) {
            return;
        }
        drawSetBadge( dst, right, bottom, artifactSets()[static_cast<size_t>( setIndex )].number, isSetComplete( bag, setIndex ) );
    }

    void showSetCatalog( const Heroes & hero )
    {
        const BagArtifacts & bag = hero.GetBagArtifacts();
        fheroes2::Display & display = fheroes2::Display::instance();
        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        const fheroes2::Rect roi( ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2, ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2,
                                  fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT );
        fheroes2::ImageRestorer restorer( display, roi.x, roi.y, roi.width, roi.height );

        const uint8_t gold = fheroes2::GetColorId( 0xDC, 0xB4, 0x3C );
        const uint8_t goldLight = fheroes2::GetColorId( 0xFC, 0xD4, 0x3C );
        const uint8_t panelDark = fheroes2::GetColorId( 0x34, 0x1C, 0x10 );
        const uint8_t slotDark = fheroes2::GetColorId( 0x28, 0x14, 0x0C );
        const uint8_t edgeBrown = fheroes2::GetColorId( 0x94, 0x6C, 0x28 );

        const fheroes2::Sprite & exitReleased = Assets::getImage( ICN::BUTTON_SMALL_EXIT_GOOD, 0 );
        fheroes2::Button buttonExit( roi.x + 632 - exitReleased.width(), roi.y + 2, ICN::BUTTON_SMALL_EXIT_GOOD, 0, 1 );

        const std::vector<SetDefinition> & sets = artifactSets();
        // 3 columns x 7 rows of sets (6 rows of 73 until set 19 "Wind Step" was added on 2026-11-21: it did not fit and was not shown;
        // owner 2026-11-24 21:02 "일부로 빼진 않았을텐데")
        const size_t maxShownSets = 21;
        auto cellRect = [&roi]( const size_t i ) {
            return fheroes2::Rect( roi.x + 8 + static_cast<int32_t>( i % 3 ) * 208, roi.y + 24 + static_cast<int32_t>( i / 3 ) * 62, 204, 60 );
        };
        auto owned = [&bag]( const int id ) {
            for ( size_t i = 0; i < bag.size(); ++i ) {
                if ( bag[i].GetID() == id ) {
                    return i < activeCount( bag ) ? 2 : 1; // 2 worn, 1 in the backpack
                }
            }
            return 0;
        };

        fheroes2::Fill( display, roi.x, roi.y, roi.width, roi.height, fheroes2::GetColorId( 0x70, 0x12, 0x0E ) );
        fheroes2::DrawRect( display, roi, gold );
        fheroes2::Fill( display, roi.x + 8, roi.y + 4, 624, 16, panelDark );
        const fheroes2::Text title( std::string( _( "Set catalog" ) ) + " - " + hero.GetName(), fheroes2::FontType::smallYellow() );
        title.draw( roi.x + ( roi.width - title.width() ) / 2, roi.y + 7, display );

        for ( size_t si = 0; si < sets.size() && si < maxShownSets; ++si ) {
            const SetDefinition & set = sets[si];
            const fheroes2::Rect cell = cellRect( si );
            const int worn = wornPieces( bag, static_cast<int>( si ) );
            const bool complete = isSetComplete( bag, static_cast<int>( si ) );
            fheroes2::Fill( display, cell.x, cell.y, cell.width, cell.height, panelDark );
            fheroes2::DrawRect( display, cell, complete ? goldLight : edgeBrown );

            drawSetBadge( display, cell.x + 16, cell.y + 14, set.number, complete );
            int tiersOn = 0;
            for ( const SetTier & tier : set.tiers ) {
                tiersOn += worn >= tier.pieces ? 1 : 0;
            }
            const fheroes2::Text name( std::string( _( set.name ) ) + "  " + std::to_string( worn ) + "/" + std::to_string( set.pieces.size() ),
                                       worn > 0 ? fheroes2::FontType::smallYellow() : fheroes2::FontType::smallWhite() );
            name.draw( cell.x + 20, cell.y + 5, cell.width - 24, display );

            for ( size_t pi = 0; pi < set.pieces.size(); ++pi ) {
                const fheroes2::Rect r( cell.x + 6 + static_cast<int32_t>( pi ) * 39, cell.y + 21, 32, 32 );
                const int state = owned( set.pieces[pi] );
                fheroes2::DrawRect( display, { r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, state == 2 ? goldLight : slotDark );
                const fheroes2::Sprite & sprite = Assets::getImage( ICN::ARTFX, Artifact( set.pieces[pi] ).IndexSprite32() );
                if ( state > 0 ) {
                    fheroes2::Blit( sprite, display, r.x, r.y );
                }
                else {
                    // not owned: dark grey
                    fheroes2::Image grey = sprite;
                    fheroes2::ApplyPalette( grey, PAL::GetPalette( PAL::PaletteType::GRAY ) );
                    fheroes2::ApplyPalette( grey, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
                    fheroes2::Blit( grey, display, r.x, r.y );
                }
            }
            const fheroes2::Text tiers( std::to_string( tiersOn ) + "/" + std::to_string( set.tiers.size() ), tiersOn > 0 ? fheroes2::FontType::smallYellow()
                                                                                                                       : fheroes2::FontType{ fheroes2::FontSize::SMALL, fheroes2::FontColor::GRAY } );
            // on the name line: the 60 point cell has no free line under the pieces
            tiers.draw( cell.x + cell.width - tiers.width() - 6, cell.y + 5, display );
        }
        const fheroes2::Text hint( _( "Right click a set: its effects. Bright = owned, gold frame = worn." ), fheroes2::FontType::smallWhite() );
        hint.draw( roi.x + ( roi.width - hint.width() ) / 2, roi.y + 464, display );
        buttonExit.draw();
        display.render( roi );

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );
            if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() || le.isKeyPressed( fheroes2::Key::KEY_S ) ) {
                break;
            }
            for ( size_t si = 0; si < sets.size() && si < maxShownSets; ++si ) {
                if ( le.isMouseRightButtonPressedInArea( cellRect( si ) ) || le.MouseClickLeft( cellRect( si ) ) ) {
                    // the set text as in the artifact information, coloured for this hero
                    // (owner 2026-11-23 14:50: the name once, as "Set N: name"; the pieces as pictures; no empty lines)
                    const SetDefinition & shown = sets[si];
                    fheroes2::MultiFontText body;
                    appendSetDescription( body, shown.pieces.front(), &bag, false );

                    const int32_t pieceCount = static_cast<int32_t>( shown.pieces.size() );
                    fheroes2::Image row( pieceCount * 39 - 5, 34 );
                    row.reset();
                    for ( int32_t pi = 0; pi < pieceCount; ++pi ) {
                        const int pieceId = shown.pieces[static_cast<size_t>( pi )];
                        fheroes2::DrawRect( row, { pi * 39, 0, 34, 34 }, owned( pieceId ) == 2 ? goldLight : slotDark );
                        const fheroes2::Sprite & sprite = Assets::getImage( ICN::ARTFX, Artifact( pieceId ).IndexSprite32() );
                        fheroes2::Copy( sprite, 0, 0, row, pi * 39 + 1, 1, 32, 32 );
                    }
                    const fheroes2::CustomImageDialogElement rowElement( std::move( row ) );

                    fheroes2::showMessage( fheroes2::Text( std::string( _( "Set" ) ) + " " + std::to_string( shown.number ) + ": " + _( shown.name ),
                                                           fheroes2::FontType::normalYellow() ),
                                           body, le.isMouseRightButtonPressedInArea( cellRect( si ) ) ? Dialog::ZERO : Dialog::OK, { &rowElement } );
                    break;
                }
            }
        }
    }
}

