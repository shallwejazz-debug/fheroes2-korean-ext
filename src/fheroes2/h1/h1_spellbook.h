// Heroes I spellbook window (homm1-remaster project): spellwin.bin, FUN_004170ba / FUN_00417351 / FUN_0041740c / FUN_004175ec.
// HoMM1 maps use the HoMM1 use counts (owner decision 2026-11-15 12:56, note v84).
#pragma once

class HeroBase;
class Spell;

namespace H1
{
    // combatOnly: mode 0 (combat cast, adventure tab hidden); adventureOnly: mode 1 (adventure cast, combat tab hidden); neither: mode 2
    // (view, both tabs, starts on the adventure page). canCast: a click on a spell picks it.
    // Returns the spell picked (Spell::NONE when closed).
    Spell openSpellBook( const HeroBase & hero, bool combatOnly, bool adventureOnly, bool canCast );
}
