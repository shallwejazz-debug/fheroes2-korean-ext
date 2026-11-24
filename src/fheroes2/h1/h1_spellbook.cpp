// Heroes I spellbook window (homm1-remaster project). See h1_spellbook.h.
#include "h1_spellbook.h"

#include <algorithm>
#include <string>
#include <vector>

#include "h1_spells.h"
#include "h1_ui.h"
#include "heroes_base.h"
#include "spell.h"
#include "spell_book.h"

namespace
{
    enum : int
    {
        ID_PREVIOUS = 2,
        ID_NEXT = 3,
        ID_ADVENTURE = 4,
        ID_COMBAT = 5,
        ID_ICON = 6, // 6..9
        ID_TEXT = 10, // 10..13
        ID_CLOSE = 0x8000
    };

    struct Entry
    {
        Spell spell;
        int h1 = -1;
        int number = 0; // shown in brackets: uses left (HoMM1 map) or spell points
    };

    // The hero's spells of one page in HoMM1 order (combat spells 0..18, adventure 19..28). The original book keeps the order spells
    // were learnt in (hero +0x66); the engine book has no order, so the HoMM1 spell number is used [temporary].
    std::vector<Entry> pageSpells( const HeroBase & hero, const bool combat )
    {
        std::vector<Entry> list;
        if ( H1::spellCountsActive() ) {
            H1::limitUsesToKnowledge( hero );
            // The hero's book slots of the half, in the order learnt (hero +0x66 / +0x83).
            const size_t from = combat ? 0 : 0x13;
            const size_t to = combat ? 0x13 : 0x1d;
            for ( size_t k = from; k < to; ++k ) {
                if ( hero.h1BookSpell[k] != 0 && hero.h1BookCount[k] > 0 ) {
                    const int h1 = hero.h1BookSpell[k] - 1;
                    list.push_back( { Spell( H1::engineSpell( h1 ) ), h1, hero.h1BookCount[k] } );
                }
            }
            return list;
        }
        const SpellStorage spells = hero.getAllSpells();
        for ( const Spell & spell : spells ) {
            if ( spell.isCombat() != combat ) {
                continue;
            }
            list.push_back( { spell, H1::h1SpellFromEngine( spell.GetID() ), static_cast<int>( spell.spellPoints( &hero ) ) } );
        }
        std::sort( list.begin(), list.end(), []( const Entry & a, const Entry & b ) {
            const int ka = a.h1 < 0 ? 100 + a.spell.GetID() : a.h1;
            const int kb = b.h1 < 0 ? 100 + b.spell.GetID() : b.h1;
            return ka < kb;
        } );
        return list;
    }
}

namespace H1
{
    Spell openSpellBook( const HeroBase & hero, const bool combatOnly, const bool adventureOnly, const bool canCast )
    {
        // FUN_004170ba: "No spells to cast." when FUN_00447570( hero, mode ) is 0: mode 0 counts the combat half, mode 1 the adventure half,
        // mode 2 both.
        const bool noCombat = pageSpells( hero, true ).empty();
        const bool noAdventure = pageSpells( hero, false ).empty();
        if ( combatOnly ? noCombat : ( adventureOnly ? noAdventure : ( noCombat && noAdventure ) ) ) {
            UI::messageBox( "No spells to cast.", UI::MESSAGE_OKAY );
            return Spell::NONE;
        }

        // Mode 0 (combat cast) and 2 (view): (0x92, 0x91); mode 1 (adventure cast): (0x61, 0x91). Mode 0 hides the adventure tab,
        // mode 1 the combat tab. Mode 2 starts on the adventure page.
        const int32_t x = adventureOnly ? 0x61 : 0x92;
        const int32_t y = 0x91;
        bool combat = combatOnly;
        size_t first = 0;

        UI::Layout layout = UI::loadLayout( "spellwin.bin" );
        layout.clickableIcons = { ID_ICON, ID_ICON + 1, ID_ICON + 2, ID_ICON + 3 };
        if ( UI::Item * it = layout.find( combatOnly ? ID_ADVENTURE : ID_COMBAT ); it != nullptr && ( combatOnly || adventureOnly ) ) {
            it->hidden = true;
            it->inactive = true;
        }

        std::vector<Entry> shown;
        auto draw = [&]() {
            // FUN_0041740c: 4 spells from the page start; slots past the last spell are hidden. Text "%s (%d)" (0x490b80): uses left
            // (on maps without HoMM1 data: the engine spell point cost).
            shown = pageSpells( hero, combat );
            for ( int k = 0; k < 4; ++k ) {
                UI::Item * icon = layout.find( ID_ICON + k );
                UI::Item * text = layout.find( ID_TEXT + k );
                const bool has = first + k < shown.size();
                if ( icon != nullptr ) {
                    icon->hidden = !has || shown[first + k].h1 < 0;
                    icon->inactive = !has;
                    if ( has && shown[first + k].h1 >= 0 ) {
                        icon->frame = shown[first + k].h1;
                    }
                }
                if ( text != nullptr ) {
                    text->hidden = !has;
                    if ( has ) {
                        const Entry & e = shown[first + k];
                        const std::string name = e.h1 >= 0 ? spellName( e.h1 ) : e.spell.GetName();
                        text->text = name + " (" + std::to_string( e.number ) + ")";
                    }
                }
            }
        };

        auto help = [&]( const int id ) -> const char * {
            switch ( id ) {
            case ID_PREVIOUS:
                return "View previous page";
            case ID_NEXT:
                return "View next page";
            case ID_ADVENTURE:
                return "View adventure spells";
            case ID_COMBAT:
                return "View combat spells";
            case ID_CLOSE:
                return "Close Spellbook";
            default:
                break;
            }
            if ( id >= ID_ICON && id < ID_ICON + 4 && first + ( id - ID_ICON ) < shown.size() ) {
                const Entry & e = shown[first + ( id - ID_ICON )];
                // Table 0x493640 with the spell picture (FUN_004175ec: FUN_00422fc2( text, 4, -1, -1, 8, spell ), note v171).
                return e.h1 >= 0 ? spellDescription( e.h1 ) : nullptr;
            }
            return nullptr;
        };
        auto helpPicture = [&]( const int id ) {
            UI::MessagePicture p;
            if ( id >= ID_ICON && id < ID_ICON + 4 && first + ( id - ID_ICON ) < shown.size() && shown[first + ( id - ID_ICON )].h1 >= 0 ) {
                p.kind = 8;
                p.value = shown[first + ( id - ID_ICON )].h1;
            }
            return p;
        };

        while ( true ) {
            draw();
            const int id = UI::runLayoutDialog( layout, x, y, help, 0xb1, helpPicture );
            switch ( id ) {
            case ID_PREVIOUS:
                first = first >= 4 ? first - 4 : 0;
                continue;
            case ID_NEXT:
                if ( first + 4 < shown.size() ) {
                    first += 4;
                }
                continue;
            case ID_ADVENTURE:
                combat = false;
                first = 0;
                continue;
            case ID_COMBAT:
                combat = true;
                first = 0;
                continue;
            default:
                break;
            }
            if ( id >= ID_ICON && id < ID_ICON + 4 && first + ( id - ID_ICON ) < shown.size() ) {
                const Entry & e = shown[first + ( id - ID_ICON )];
                if ( !canCast ) {
                    // View only: the description box (FUN_00422fc2 style 1 with the spell picture, kind 8).
                    if ( e.h1 >= 0 ) {
                        UI::MessagePicture picture;
                        picture.kind = 8;
                        picture.value = e.h1;
                        UI::messageBox( spellDescription( e.h1 ), UI::MESSAGE_OKAY, -1, -1, picture, UI::MessagePicture() );
                    }
                    continue;
                }
                std::string why;
                if ( !hero.CanCastSpell( e.spell, &why ) ) {
                    // Engine rule (spell points etc.), message in the HoMM1 box.
                    UI::messageBox( why, UI::MESSAGE_OKAY );
                    continue;
                }
                return e.spell;
            }
            return Spell::NONE;
        }
    }
}
