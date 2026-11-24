// Heroes I mage guild spells (homm1-remaster project). See h1_spells.h.
#include "h1_spells.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

#include "castle.h"
#include "color.h"
#include "heroes_base.h"
#include "players.h"
#include "rand.h"
#include "settings.h"
#include "spell.h"
#include "spell_storage.h"
#include "mageguild.h"
#include "world.h"
#include "heroes.h"
#include "h1_ui.h"
#include "mp2.h"
#include "maps_tiles_helper.h"
#include "maps_tiles.h"
#include "h1_gfx.h"
#include "h1_mode.h"

namespace
{
    const char * const names[29] = { "Fireball", "Lightning Bolt", "Teleport", "Cure", "Resurrect", "Haste", "Slow", "Blind", "Bless", "Protection", "Curse", "Turn Undead", "Anti-Magic", "Dispel Magic", "Berzerker", "Armageddon", "Storm", "Meteor Shower", "Paralyze", "View Mines", "View Resources", "View Artifacts", "View Towns", "View Heroes", "View All", "Identify Hero", "Summon Boat", "Dimension Door", "Town Gate" };

    const char * const descriptions[29] = { "Fireball\n\nCauses a giant fireball to strike the selected area, damaging all nearby creatures.",
                                            "Lightning Bolt\n\nCauses a bolt of electrical energy to strike the selected creature.",
                                            "Teleport\n\nTeleports the creature you select to any open position on the battlefield.",
                                            "Cure\n\nRemoves all negative spells cast upon your forces.",
                                            "Resurrect\n\nResurrects creatures from a damaged monster group.",
                                            "Haste\n\nIncreases the speed of any creature to 'very fast'.",
                                            "Slow\n\nSlows down even the fastest enemy creature.",
                                            "Blind\n\nClouds the affected creatures' eyes, preventing them from moving.",
                                            "Bless\n\nCauses the selected creatures to inflict maximum damage.",
                                            "Protection\n\nMagically increases the defense skill of the selected creatures.",
                                            "Curse\n\nCauses the selected creatures to inflict minimum damage.",
                                            "Turn Undead\n\nInstantly sends a group of ghosts back to the grave.",
                                            "Anti-Magic\n\nPrevents harmful magic against the selected creatures.",
                                            "Dispel Magic\n\nRemoves all magic spells from all parties in the battle.",
                                            "Berserk\n\nCauses a creature to attack its nearest neighbor.",
                                            "Armageddon\n\nHoly terror strikes the battlefield, causing severe damage to all creatures.",
                                            "Elemental Storm\n\nMagical elements pour down on the battlefield, damaging all creatures.",
                                            "Meteor Shower\n\nA rain of rocks strikes an area of the battlefield, damaging all nearby creatures.",
                                            "Paralyze\n\nThe targeted creatures are paralyzed, unable to move or retaliate.",
                                            "View Mines\n\nCauses all mines across the land to become visible.",
                                            "View Resources\n\nCauses all resources across the land to become visible.",
                                            "View Artifacts\n\nCauses all artifacts across the land to become visible.",
                                            "View Towns\n\nCauses all towns and castles across the land to become visible.",
                                            "View Heroes\n\nCauses all Heroes across the land to become visible.",
                                            "View All\n\nCauses the entire land to become visible.",
                                            "Identify Hero\n\nAllows the caster to view detailed information on enemy Heroes.",
                                            "Summon Boat\n\nSummons the nearest unoccupied, friendly boat to an adjacent shore location.  A friendly boat is one which you just built or were the most recent player to occupy.",
                                            "Dimension Door\n\nAllows the caster to magically transport himself to a nearby location.",
                                            "Town Gate\n\nReturns the caster to any town or castle currently owned." };

    // Table 0x490fb8: 8 picks per spell level (some spells twice).
    const int levelPicks[4][8] = { { 9, 13, 6, 8, 10, 20, 19, 8 }, { 1, 5, 3, 7, 11, 21, 26, 12 }, { 0, 18, 14, 16, 22, 25, 23, 2 }, { 27, 4, 15, 17, 28, 24, 28, 27 } };
    // Table 0x490f98 bit 1 (combat spell) and 0x490f58 (AI weight).
    const int spellFlags[29] = { 3, 3, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
    const int spellWeight[29] = { 500, 350, 300, 400, 550, 900, 400, 500, 300, 350, 250, 0, 100, 150, 1000, 2000, 1700, 700, 700, 0, 0, 0, 0, 0, 0, 0, 0, 1200, 0 };

    std::map<int32_t, std::array<int, 9>> & store()
    {
        static std::map<int32_t, std::array<int, 9>> guilds;
        return guilds;
    }

    std::string simple( const std::string & s )
    {
        std::string out;
        for ( const char ch : s ) {
            if ( std::isalnum( static_cast<unsigned char>( ch ) ) ) {
                out += static_cast<char>( std::tolower( static_cast<unsigned char>( ch ) ) );
            }
        }
        return out;
    }
}

namespace H1
{
    const char * spellName( const int spell )
    {
        return ( spell >= 0 && spell < spellCount ) ? names[spell] : "";
    }

    const char * spellDescription( const int spell )
    {
        return ( spell >= 0 && spell < spellCount ) ? descriptions[spell] : "";
    }

    std::array<int, 9> makeGuildSpells( const bool computer )
    {
        std::array<int, 9> spells{};
        bool used[29] = {};
        for ( int slot = 0; slot < 9; ++slot ) {
            const int level = slot < 3 ? 0 : ( slot < 5 ? 1 : ( slot < 7 ? 2 : 3 ) );
            int spell = 0;
            while ( true ) {
                spell = levelPicks[level][Rand::Get( 0, 7 )];
                int keep = 1500;
                if ( computer ) {
                    keep = ( ( spellFlags[spell] & 1 ) ? 4 : 1 ) * spellWeight[spell] + 50;
                }
                if ( spell == 27 ) {
                    keep = 1500;
                }
                if ( used[spell] ) {
                    continue;
                }
                if ( static_cast<int>( Rand::Get( 1, 1500 ) ) < keep ) {
                    break;
                }
            }
            spells[slot] = spell;
            used[spell] = true;
        }
        return spells;
    }

    bool isComputerCastle( const Castle & castle )
    {
        // FUN_0041b364 param_2: castles of a player that is not human (DAT_004a9b60) are weighted; others (and castles without an owner)
        // keep every pick unless the 1..1500 roll is 1500.
        const Player * player = Players::Get( castle.GetColor() );
        return castle.GetColor() != PlayerColor::NONE && player != nullptr && !player->isControlHuman();
    }
}

namespace H1
{
    const std::array<int, 9> & guildSpells( const Castle & castle )
    {
        auto & guilds = store();
        const int32_t key = castle.GetIndex();
        // Kept in the castle's mage guild (new game, saved games).
        const SpellStorage & kept = castle.GetMageGuild().getH1Spells();
        if ( kept.size() == 9 ) {
            std::array<int, 9> spells{};
            bool ok = true;
            for ( size_t k = 0; k < 9; ++k ) {
                spells[k] = h1SpellFromEngine( kept[k].GetID() );
                ok = ok && spells[k] >= 0;
            }
            if ( ok ) {
                return guilds[key] = spells;
            }
        }
        auto found = guilds.find( key );
        if ( found != guilds.end() ) {
            return found->second;
        }
        return guilds.emplace( key, makeGuildSpells( isComputerCastle( castle ) ) ).first->second;
    }

    void initGuildSpells()
    {
        store().clear();
        // Castle-list order, as the original walks its castle records.
        for ( int slot = 0; slot < 36; ++slot ) {
            int32_t x = 0;
            int32_t y = 0;
            if ( !getListPosition( false, slot, x, y ) ) {
                continue;
            }
            Castle * castle = world.getCastleEntrance( { x, y } );
            if ( castle == nullptr ) {
                continue;
            }
            const std::array<int, 9> spells = makeGuildSpells( isComputerCastle( *castle ) );
            SpellStorage storage;
            for ( const int spell : spells ) {
                storage.push_back( Spell( engineSpell( spell ) ) );
            }
            castle->getMageGuildForH1().setH1Spells( storage );
        }
    }

    int h1SpellFromEngine( const int engineSpellId )
    {
        for ( int spell = 0; spell < spellCount; ++spell ) {
            if ( engineSpell( spell ) == engineSpellId && engineSpellId != Spell::NONE ) {
                return spell;
            }
        }
        return -1;
    }

    int engineSpell( const int spell )
    {
        if ( spell < 0 || spell >= spellCount ) {
            return Spell::NONE;
        }
        // The four HoMM1 names the engine does not have.
        switch ( spell ) {
        case 9:
            return Spell::STONESKIN;
        case 11:
            return Spell::HOLYWORD;
        case 14:
            return Spell::BERSERKER;
        case 16:
            return Spell::ELEMENTALSTORM;
        default:
            break;
        }
        const std::string wanted = simple( names[spell] );
        for ( int id = Spell::NONE + 1; id < Spell::SPELL_COUNT; ++id ) {
            if ( simple( Spell( id ).GetName() ) == wanted ) {
                return id;
            }
        }
        return Spell::NONE;
    }

    void learnGuildSpells( const Castle & castle, HeroBase & hero )
    {
        const int level = castle.GetLevelMageGuild(); // engine level 1..4 = HoMM1 castle +0x18 0..3, plus one
        if ( level < 1 || !hero.HaveSpellBook() ) {
            return;
        }
        static const int slots[4] = { 3, 5, 7, 9 };
        const std::array<int, 9> & spells = guildSpells( castle );
        for ( int k = 0; k < slots[std::min( level, 4 ) - 1]; ++k ) {
            const int id = engineSpell( spells[k] );
            if ( id != Spell::NONE ) {
                // FUN_00447877( hero, spell, hero +0x33, 0 ): what the third argument limits is unconfirmed.
                hero.AppendSpellToBook( Spell( id ), true );
            }
        }
    }

    // H1-INTENDED(D4): the original per-spell use counts, not HoMM2 spell points (docs/INTENDED_DIFFERENCES.md).
    bool spellCountsActive()
    {
        return isH1Mode() && getTileBytes( 0 ) != nullptr;
    }

    int learnSpell( HeroBase & hero, const int spell, const int count, const bool queryOnly )
    {
        if ( spell < 0 || spell >= spellCount ) {
            return 0;
        }
        const size_t from = spell < 0x13 ? 0 : 0x13;
        const size_t to = spell < 0x13 ? 0x13 : 0x1d;
        for ( size_t k = from; k < to; ++k ) {
            const int slot = static_cast<int>( hero.h1BookSpell[k] ) - 1;
            if ( slot == spell || slot < 0 ) {
                const int gained = ( slot == spell ) ? count - static_cast<int>( hero.h1BookCount[k] ) : count;
                if ( !queryOnly ) {
                    hero.h1BookSpell[k] = static_cast<uint8_t>( spell + 1 );
                    hero.h1BookCount[k] = static_cast<uint8_t>( std::clamp( count, 0, 255 ) );
                }
                return gained;
            }
        }
        return 0;
    }

    void useSpell( HeroBase & hero, const int spell )
    {
        if ( spell < 0 || spell >= spellCount ) {
            return;
        }
        const size_t from = spell < 0x13 ? 0 : 0x13;
        const size_t to = spell < 0x13 ? 0x13 : 0x1d;
        size_t k = from;
        while ( k < to && static_cast<int>( hero.h1BookSpell[k] ) - 1 != spell ) {
            ++k;
        }
        if ( k >= to ) {
            return;
        }
        if ( hero.h1BookCount[k] < 2 ) {
            for ( size_t j = k; j + 1 < to; ++j ) {
                hero.h1BookSpell[j] = hero.h1BookSpell[j + 1];
                hero.h1BookCount[j] = hero.h1BookCount[j + 1];
            }
            hero.h1BookSpell[to - 1] = 0;
            hero.h1BookCount[to - 1] = 0;
        }
        else {
            --hero.h1BookCount[k];
        }
    }

    void limitUsesToKnowledge( const HeroBase & hero )
    {
        const int knowledge = static_cast<int>( hero.GetKnowledge() );
        for ( uint8_t & count : hero.h1BookCount ) {
            if ( static_cast<int>( count ) > knowledge ) {
                count = static_cast<uint8_t>( std::max( knowledge, 0 ) );
            }
        }
    }

    int spellUses( const HeroBase & hero, const int spell )
    {
        if ( spell < 0 || spell >= spellCount ) {
            return 0;
        }
        limitUsesToKnowledge( hero );
        for ( size_t k = 0; k < 29; ++k ) {
            if ( static_cast<int>( hero.h1BookSpell[k] ) - 1 == spell ) {
                return hero.h1BookCount[k];
            }
        }
        return 0;
    }

    int randomShrineSpell()
    {
        const int roll = static_cast<int>( Rand::Get( 0, 9 ) );
        const int level = roll < 4 ? 0 : ( roll < 8 ? 1 : 2 );
        return levelPicks[level][Rand::Get( 0, 7 )];
    }

    void initShrineSpells()
    {
        for ( int32_t index = 0; index < 72 * 72; ++index ) {
            const uint8_t * t = getTileBytes( index );
            if ( t == nullptr || t[8] != 0xa2 ) {
                continue;
            }
            Maps::Tile & tile = world.getTile( index );
            const MP2::MapObjectType type = tile.getMainObjectType( false );
            if ( type == MP2::OBJ_SHRINE_FIRST_CIRCLE || type == MP2::OBJ_SHRINE_SECOND_CIRCLE || type == MP2::OBJ_SHRINE_THIRD_CIRCLE ) {
                Maps::setSpellOnTile( tile, engineSpell( randomShrineSpell() ) );
            }
        }
    }

    bool visitShrine( HeroBase & hero, const int32_t tileIndex )
    {
        if ( !spellCountsActive() ) {
            return false;
        }
        const int spell = h1SpellFromEngine( Maps::getSpellFromTile( world.getTile( tileIndex ) ).GetID() );
        // "%s'%s'." (0x49010c) with the text 0x4934f4 and the spell name (table 0x493144 by byte 9 = spell + 1).
        std::string text = std::string( "Shrine\n\nNestled in a small hidden shrine is an ancient wooden altar.  Upon the altar, a golden plaque bears an inscription "
                                        "with the secret to the ancient magical spell " )
                           + "'" + spellName( spell ) + "'.";
        if ( !hero.HaveSpellBook() ) {
            text += "  Unfortunately, you have no Magic Book to record the spell with.";
        }
        else if ( spell >= 0 ) {
            learnSpell( hero, spell, static_cast<int>( hero.GetKnowledge() ), false );
        }
        // FUN_004035e2( -1, 1, text, 8, spell ): message with the spell picture.
        UI::MessagePicture picture;
        if ( spell >= 0 ) {
            picture.kind = 8;
            picture.value = spell;
        }
        UI::messageBox( text, UI::MESSAGE_OKAY, -1, -1, picture, UI::MessagePicture() );
        return true;
    }

    void clearHeroBooks()
    {
        for ( int id = Heroes::UNKNOWN + 1; id < Heroes::HEROES_COUNT; ++id ) {
            if ( Heroes * hero = world.GetHeroes( id ); hero != nullptr ) {
                hero->h1BookSpell.fill( 0 );
                hero->h1BookCount.fill( 0 );
            }
        }
    }
}
