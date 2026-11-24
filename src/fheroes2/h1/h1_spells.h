// Heroes I mage guild spells (homm1-remaster project): castle spell slots +0x2c (FUN_0041b364), spell names / texts (tables 0x493148,
// 0x493640). See docs/analysis note v68.
#pragma once

#include <array>
#include <cstdint>

class Castle;
class HeroBase;

namespace H1
{
    constexpr int spellCount = 29;

    const char * spellName( int spell );
    const char * spellDescription( int spell );

    // The castle's 9 mage guild spells (slots 0..2 level 1, 3..4 level 2, 5..6 level 3, 7..8 level 4), as made at new game by the original
    // rule (FUN_0041b364). Games without them (older saves) get them made on first use, kept for this run only.
    const std::array<int, 9> & guildSpells( const Castle & castle );

    // Engine spell standing in for a HoMM1 spell: same name, or for the four HoMM1 names the engine lacks Protection -> Stoneskin (same
    // description text), Turn Undead -> Holy Word, Berzerker -> Berserker, Storm -> Elemental Storm [temporary: effects not yet compared].
    int engineSpell( int spell );

    // HoMM1 spell number of an engine spell (inverse of engineSpell), -1 if none.
    int h1SpellFromEngine( int engineSpellId );

    // True on a HoMM1 map in Heroes I mode: heroes use the HoMM1 spellbook slots and use counts (owner decision 2026-11-15, note v84).
    bool spellCountsActive();

    // FUN_00447877( hero, spell, count, queryOnly ): combat spells 0..18 go to slots 0..18, adventure spells 19..28 to slots 19..28; the
    // first slot holding the spell or empty gets the spell with uses = count (not added). Returns count minus the uses it had. queryOnly:
    // nothing is written.
    int learnSpell( HeroBase & hero, int spell, int count, bool queryOnly );

    // FUN_00447660( hero, spell ): one use less; with 1 use left the spell leaves the book and the later slots of its half move up.
    void useSpell( HeroBase & hero, int spell );

    // FUN_00403fb9: when Knowledge goes down (an artifact leaves the hero) every count above the new Knowledge is cut to it. The engine
    // works Knowledge out from the artifacts each time, so the cut is made whenever the counts are read [temporary: same result unless
    // Knowledge goes down and up again in between].
    void limitUsesToKnowledge( const HeroBase & hero );

    // Uses left of a HoMM1 spell (0 when not in the book or spell < 0).
    int spellUses( const HeroBase & hero, int spell );

    // Shrine spell at new game (FUN_00415a33, byte 9 of 0xa2 tiles = spell + 1): roll 0..9 - 0..3 a level 1 pick, 4..7 level 2, 8..9 level 3
    // (tables 0x490fb8 / 0x490fc0 / 0x490fc8).
    int randomShrineSpell();

    // New game: every HoMM1 shrine tile (byte 8 = 0xa2) gets its spell by randomShrineSpell (kept in the engine tile, saved with the game).
    void initShrineSpells();

    // FUN_00401000 case 0x22 (hero visits a shrine): the text "Shrine ... spell '<name>'."; with a Magic Book the spell is learnt with
    // Knowledge uses (again, even when known), without one "Unfortunately, you have no Magic Book ...". Returns false when not a HoMM1 map.
    bool visitShrine( HeroBase & hero, int32_t tileIndex );

    // New game (FUN_00415a33 / FUN_0041b364 calls): every castle gets its 9 guild spells, weighted for castles of computer players, and keeps
    // them in its mage guild (saved with the game).
    void initGuildSpells();

    // New game: HoMM1 heroes start with an empty book (no call of FUN_00447877 when heroes are made; the engine's starting spells are
    // not used on HoMM1 maps).
    void clearHeroBooks();

    // FUN_0043c3e0 part: the hero learns the guild spells of the built levels (table 0x49232c: 3, 5, 7, 9 slots).
    void learnGuildSpells( const Castle & castle, HeroBase & hero );
}
