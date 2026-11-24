// H2-EXT: extension creatures (upgrade branches), docs/H2_EXTENSION_DESIGN_v5.md section 2.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fheroes2
{
    struct MonsterData;
}

namespace H2Ext
{
    // Fills the data of the extension creatures (Monster::EXT_*) from their original creature; called once by monster_info.cpp.
    void addMonsterData( std::vector<fheroes2::MonsterData> & data );

    // Index 0.. of an extension creature, or -1.
    int extMonsterIndex( int monsterId );
    int extMonsterCount();
    // Monster::EXT_* number of extension creature `index`.
    int extMonsterId( int index );
    // The original creature an extension creature is drawn from (same animation, other colours).
    int extMonsterBase( int index );

    // Upgrade choices of a creature in an extension game (empty = none). The first one is branch A.
    std::vector<int> upgradeChoices( int monsterId );

    // The branch a computer player takes for `originalId` in the castle at map tile `castleIndex`: fixed for each castle and tier,
    // random from the map seed (owner 00:18); -1 when the creature has no choice.
    int aiBranch( int originalId, int32_t castleIndex );

    // Extension abilities shown in the creature information window (right click), translated; empty when none (owner 23:30
    // "우클릭으로 봤을 때 특능들 다 보이게"). The Red Dragon line only in an extension game.
    std::vector<std::string> extAbilityDescriptions( int monsterId );

    // The spell a creature casts by itself at the start of each of its turns (Spell::NONE when none).
    int turnSpell( int monsterId );

    // A palette remap (256 entries): hue turn in degrees, saturation and value multipliers, optional tint toward a colour (weight 0..1).
    std::vector<uint8_t> makeRecolorPalette( double hue, double saturation, double value, double tint, uint8_t tintR, uint8_t tintG, uint8_t tintB );

    // Palette remap (256 entries) for the colours of extension creature `index`.
    const std::vector<uint8_t> & extMonsterPalette( int index );
}
