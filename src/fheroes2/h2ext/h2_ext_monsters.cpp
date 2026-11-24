// H2-EXT: extension creatures. Numbers are [제안] values of docs/H2_EXTENSION_DESIGN_v5.md (not in the original game); the original
// creatures keep their original values. Pictures: the original creature recoloured (hue turned, saturation and brightness scaled),
// so every animation is the original one.
#include "h2_ext_monsters.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "h2_ext.h"
#include "icn.h"
#include "image_palette.h"
#include "monster.h"
#include "monster_info.h"
#include "speed.h"
#include "spell.h"
#include "translations.h"
#include "world.h"

namespace
{
    struct ExtMonster
    {
        int id;
        int base;
        const char * name;
        const char * plural;
        uint32_t attack;
        uint32_t defense;
        uint32_t damageMin;
        uint32_t damageMax;
        uint32_t hp;
        uint32_t speed;
        uint32_t gold;
        // colours
        double hue; // degrees
        double saturation; // multiplier
        double value; // multiplier
        // optional tint toward a colour (for grey creatures, whose hue cannot turn): weight 0..1
        uint8_t tintR = 0;
        uint8_t tintG = 0;
        uint8_t tintB = 0;
        double tint = 0;
    };

    // Warlock trial (owner 2026-11-18 21:09 "가자") and the other five races (23:46 "세팅해봐", docs/H2_EXTENSION_CONCEPT_v3.md).
    // Same order as the Monster::EXT_* numbers. Abilities: addMonsterData below and the battle code (H2-EXT marks).
    const std::array<ExtMonster, 39> extMonsters{ {
        { Monster::EXT_OBSIDIAN_GARGOYLE, Monster::GARGOYLE, gettext_noop( "Obsidian Gargoyle" ), gettext_noop( "Obsidian Gargoyles" ), 5, 9, 2, 3, 20, Speed::VERYFAST, 250, 0, 0.35, 0.55 },
        { Monster::EXT_STONE_GARGOYLE, Monster::GARGOYLE, gettext_noop( "Stone Gargoyle" ), gettext_noop( "Stone Gargoyles" ), 4, 8, 2, 3, 22, Speed::FAST, 250, 0, 1.0, 1.1, 205, 150, 85, 0.7 },
        { Monster::EXT_ROYAL_GRIFFIN, Monster::GRIFFIN, gettext_noop( "Royal Griffin" ), gettext_noop( "Royal Griffins" ), 7, 7, 3, 5, 30, Speed::FAST, 375, 0, 1.5, 1.1 },
        { Monster::EXT_BATTLE_GRIFFIN, Monster::GRIFFIN, gettext_noop( "Battle Griffin" ), gettext_noop( "Battle Griffins" ), 8, 5, 3, 5, 25, Speed::FAST, 375, -35, 1.3, 0.95 },
        { Monster::EXT_MINOTAUR_GLADIATOR, Monster::MINOTAUR, gettext_noop( "Minotaur Gladiator" ), gettext_noop( "Minotaur Gladiators" ), 11, 7, 6, 10, 40, Speed::AVERAGE, 500, 180, 1.0, 1.0 },
        { Monster::EXT_CHAOS_HYDRA, Monster::HYDRA, gettext_noop( "Chaos Hydra" ), gettext_noop( "Chaos Hydras" ), 10, 10, 6, 12, 100, Speed::VERYSLOW, 1000, 150, 1.2, 0.9 },
        { Monster::EXT_ACID_HYDRA, Monster::HYDRA, gettext_noop( "Acid Hydra" ), gettext_noop( "Acid Hydras" ), 9, 9, 6, 12, 85, Speed::SLOW, 1000, 40, 1.4, 1.05 },
        { Monster::EXT_CROSSBOWMAN, Monster::ARCHER, gettext_noop( "Crossbowman" ), gettext_noop( "Crossbowmen" ), 5, 4, 3, 5, 12, Speed::VERYSLOW, 200, 200, 1.1, 1.0 },
        { Monster::EXT_HALBERDIER, Monster::PIKEMAN, gettext_noop( "Halberdier" ), gettext_noop( "Halberdiers" ), 6, 9, 3, 4, 18, Speed::AVERAGE, 250, 120, 1.2, 1.0 },
        { Monster::EXT_SHIELDBEARER, Monster::SWORDSMAN, gettext_noop( "Shieldbearer" ), gettext_noop( "Shieldbearers" ), 6, 12, 4, 5, 30, Speed::SLOW, 300, 30, 1.3, 1.0 },
        { Monster::EXT_LANCER, Monster::CAVALRY, gettext_noop( "Lancer" ), gettext_noop( "Lancers" ), 11, 8, 5, 10, 35, Speed::VERYFAST, 375, 240, 1.2, 1.0 },
        { Monster::EXT_GUARDIAN_KNIGHT, Monster::PALADIN, gettext_noop( "Guardian Knight" ), gettext_noop( "Guardian Knights" ), 12, 14, 15, 25, 70, Speed::FAST, 1000, 45, 1.3, 1.1 },
        { Monster::EXT_ORC_BERSERKER, Monster::ORC, gettext_noop( "Orc Berserker" ), gettext_noop( "Orc Berserkers" ), 5, 3, 3, 4, 15, Speed::AVERAGE, 175, -30, 1.4, 1.0 },
        { Monster::EXT_DIRE_WOLF, Monster::WOLF, gettext_noop( "Dire Wolf" ), gettext_noop( "Dire Wolves" ), 7, 3, 3, 5, 25, Speed::VERYFAST, 250, 0, 0.5, 0.65 },
        { Monster::EXT_WINTER_WOLF, Monster::WOLF, gettext_noop( "Winter Wolf" ), gettext_noop( "Winter Wolves" ), 6, 3, 3, 5, 22, Speed::VERYFAST, 250, 0, 0.6, 1.2, 180, 220, 255, 0.6 },
        { Monster::EXT_OGRE_SHAMAN, Monster::OGRE, gettext_noop( "Ogre Shaman" ), gettext_noop( "Ogre Shamans" ), 9, 6, 4, 6, 50, Speed::SLOW, 500, 270, 1.2, 1.0 },
        { Monster::EXT_CAVE_TROLL, Monster::TROLL, gettext_noop( "Cave Troll" ), gettext_noop( "Cave Trolls" ), 11, 7, 7, 9, 60, Speed::AVERAGE, 700, 0, 0.5, 0.85, 150, 130, 110, 0.5 },
        { Monster::EXT_CYCLOPS_KING, Monster::CYCLOPS, gettext_noop( "Cyclops King" ), gettext_noop( "Cyclops Kings" ), 13, 10, 12, 24, 100, Speed::FAST, 1100, 40, 1.3, 1.05 },
        { Monster::EXT_ANCIENT_CYCLOPS, Monster::CYCLOPS, gettext_noop( "Ancient Cyclops" ), gettext_noop( "Ancient Cyclopes" ), 12, 9, 15, 25, 90, Speed::AVERAGE, 1100, 200, 0.6, 0.85 },
        { Monster::EXT_DWARF_GUARDIAN, Monster::DWARF, gettext_noop( "Dwarf Guardian" ), gettext_noop( "Dwarf Guardians" ), 5, 8, 2, 4, 25, Speed::VERYSLOW, 250, 200, 1.1, 1.0 },
        { Monster::EXT_FOREST_SNIPER, Monster::ELF, gettext_noop( "Forest Sniper" ), gettext_noop( "Forest Snipers" ), 5, 4, 4, 6, 15, Speed::AVERAGE, 300, 90, 1.2, 0.9 },
        { Monster::EXT_HEALING_DRUID, Monster::DRUID, gettext_noop( "Healing Druid" ), gettext_noop( "Healing Druids" ), 6, 6, 4, 6, 30, Speed::FAST, 400, 60, 1.2, 1.05 },
        { Monster::EXT_BATTLE_UNICORN, Monster::UNICORN, gettext_noop( "Battle Unicorn" ), gettext_noop( "Battle Unicorns" ), 11, 10, 7, 14, 50, Speed::FAST, 650, 0, 1.0, 1.0, 230, 190, 120, 0.5 },
        { Monster::EXT_SILVER_UNICORN, Monster::UNICORN, gettext_noop( "Silver Unicorn" ), gettext_noop( "Silver Unicorns" ), 10, 11, 7, 14, 45, Speed::VERYFAST, 650, 0, 0.2, 1.15 },
        { Monster::EXT_GREAT_PHOENIX, Monster::PHOENIX, gettext_noop( "Great Phoenix" ), gettext_noop( "Great Phoenixes" ), 13, 11, 20, 40, 120, Speed::ULTRAFAST, 2000, 200, 1.1, 1.0 },
        { Monster::EXT_FIREBIRD, Monster::PHOENIX, gettext_noop( "Firebird" ), gettext_noop( "Firebirds" ), 14, 9, 25, 45, 90, Speed::ULTRAFAST, 2000, -20, 1.4, 1.05 },
        { Monster::EXT_BATTLE_BOAR, Monster::BOAR, gettext_noop( "Battle Boar" ), gettext_noop( "Battle Boars" ), 6, 5, 2, 4, 20, Speed::VERYFAST, 200, 0, 1.2, 0.7 },
        { Monster::EXT_BOAR_RIDER, Monster::BOAR, gettext_noop( "Boar Rider" ), gettext_noop( "Boar Riders" ), 7, 3, 2, 4, 18, Speed::VERYFAST, 200, 30, 1.3, 1.0 },
        { Monster::EXT_MITHRIL_GOLEM, Monster::IRON_GOLEM, gettext_noop( "Mithril Golem" ), gettext_noop( "Mithril Golems" ), 6, 10, 4, 5, 28, Speed::AVERAGE, 350, 0, 1.0, 1.15, 170, 200, 230, 0.6 },
        { Monster::EXT_GREAT_ROC, Monster::ROC, gettext_noop( "Great Roc" ), gettext_noop( "Great Rocs" ), 8, 8, 4, 8, 50, Speed::FAST, 500, 30, 1.0, 0.8 },
        { Monster::EXT_STORM_ROC, Monster::ROC, gettext_noop( "Storm Roc" ), gettext_noop( "Storm Rocs" ), 8, 7, 4, 8, 45, Speed::AVERAGE, 500, 200, 0.9, 1.0 },
        { Monster::EXT_BATTLE_MAGE, Monster::MAGE, gettext_noop( "Battle Mage" ), gettext_noop( "Battle Mages" ), 12, 7, 9, 11, 30, Speed::FAST, 700, 120, 1.1, 1.0 },
        { Monster::EXT_ZEUS, Monster::GIANT, gettext_noop( "Zeus" ), gettext_noop( "Zeuses" ), 16, 13, 25, 35, 250, Speed::ULTRAFAST, 5000, 0, 1.0, 1.15, 150, 190, 255, 0.5 },
        { Monster::EXT_PLAGUE_ZOMBIE, Monster::ZOMBIE, gettext_noop( "Plague Zombie" ), gettext_noop( "Plague Zombies" ), 5, 3, 2, 3, 18, Speed::VERYSLOW, 200, 70, 1.5, 1.0 },
        { Monster::EXT_MUMMY_PRIEST, Monster::MUMMY, gettext_noop( "Mummy Priest" ), gettext_noop( "Mummy Priests" ), 6, 7, 3, 4, 28, Speed::AVERAGE, 300, 40, 2.0, 1.05 },
        { Monster::EXT_NOSFERATU, Monster::VAMPIRE, gettext_noop( "Nosferatu" ), gettext_noop( "Nosferatus" ), 8, 6, 5, 7, 35, Speed::FAST, 650, 240, 1.2, 1.0 },
        { Monster::EXT_DEMILICH, Monster::LICH, gettext_noop( "Demilich" ), gettext_noop( "Demiliches" ), 8, 12, 14, 18, 30, Speed::FAST, 900, 270, 1.3, 1.0 },
        { Monster::EXT_WRAITH_DRAGON, Monster::BONE_DRAGON, gettext_noop( "Wraith Dragon" ), gettext_noop( "Wraith Dragons" ), 12, 10, 25, 45, 180, Speed::FAST, 2000, 0, 1.0, 1.1, 140, 220, 200, 0.6 },
        { Monster::EXT_PLAGUE_DRAGON, Monster::BONE_DRAGON, gettext_noop( "Plague Dragon" ), gettext_noop( "Plague Dragons" ), 12, 9, 25, 45, 170, Speed::AVERAGE, 2000, 90, 1.6, 1.0 },
    } };

    void removeAbility( fheroes2::MonsterBattleStats & stats, const fheroes2::MonsterAbilityType type )
    {
        stats.abilities.erase( std::remove_if( stats.abilities.begin(), stats.abilities.end(),
                                               [type]( const fheroes2::MonsterAbility & a ) { return a.type == type; } ),
                               stats.abilities.end() );
    }

    using fheroes2::MonsterAbilityType;

    void setSpellCaster( fheroes2::MonsterBattleStats & b, const uint32_t percentage, const int spell )
    {
        removeAbility( b, MonsterAbilityType::SPELL_CASTER );
        b.abilities.emplace_back( MonsterAbilityType::SPELL_CASTER, percentage, static_cast<uint32_t>( spell ) );
    }

    // Abilities that the engine already has (● in the concept book). New rules (◆) are in the battle code.
    void applyAbilities( const int id, fheroes2::MonsterBattleStats & b, fheroes2::MonsterGeneralStats & g )
    {
        switch ( id ) {
        case Monster::EXT_BATTLE_GRIFFIN:
            // two attacks instead of unlimited retaliation
            removeAbility( b, MonsterAbilityType::UNLIMITED_RETALIATION );
            b.abilities.emplace_back( MonsterAbilityType::DOUBLE_MELEE_ATTACK );
            break;
        case Monster::EXT_CROSSBOWMAN:
            b.shots = 12;
            break;
        case Monster::EXT_GUARDIAN_KNIGHT:
            removeAbility( b, MonsterAbilityType::DOUBLE_MELEE_ATTACK );
            break;
        case Monster::EXT_ORC_BERSERKER:
        case Monster::EXT_CAVE_TROLL:
            b.shots = 0; // melee only
            break;
        case Monster::EXT_WINTER_WOLF:
            b.abilities.emplace_back( MonsterAbilityType::COLD_SPELL_IMMUNITY );
            setSpellCaster( b, 20, Spell::SLOW );
            break;
        case Monster::EXT_OGRE_SHAMAN:
            setSpellCaster( b, 30, Spell::CURSE );
            break;
        case Monster::EXT_CYCLOPS_KING:
            setSpellCaster( b, 30, Spell::PARALYZE );
            break;
        case Monster::EXT_ANCIENT_CYCLOPS:
            removeAbility( b, MonsterAbilityType::TWO_CELL_MELEE_ATTACK );
            removeAbility( b, MonsterAbilityType::SPELL_CASTER );
            b.abilities.emplace_back( MonsterAbilityType::ALL_ADJACENT_CELL_MELEE_ATTACK );
            break;
        case Monster::EXT_DWARF_GUARDIAN:
            removeAbility( b, MonsterAbilityType::MAGIC_RESISTANCE );
            b.abilities.emplace_back( MonsterAbilityType::MAGIC_RESISTANCE, 40, 0 );
            break;
        case Monster::EXT_FOREST_SNIPER:
            removeAbility( b, MonsterAbilityType::DOUBLE_SHOOTING );
            b.shots = 16;
            break;
        case Monster::EXT_HEALING_DRUID:
            b.shots = 8;
            break;
        case Monster::EXT_BATTLE_UNICORN:
            setSpellCaster( b, 30, Spell::BLIND );
            break;
        case Monster::EXT_SILVER_UNICORN:
            removeAbility( b, MonsterAbilityType::SPELL_CASTER );
            b.abilities.emplace_back( MonsterAbilityType::MAGIC_RESISTANCE, 50, 0 );
            break;
        case Monster::EXT_FIREBIRD:
            b.abilities.emplace_back( MonsterAbilityType::NO_ENEMY_RETALIATION );
            break;
        case Monster::EXT_MITHRIL_GOLEM:
            removeAbility( b, MonsterAbilityType::ELEMENTAL_SPELL_DAMAGE_REDUCTION );
            removeAbility( b, MonsterAbilityType::CERTAIN_SPELL_DAMAGE_REDUCTION );
            break;
        case Monster::EXT_STORM_ROC:
            b.abilities.emplace_back( MonsterAbilityType::TWO_CELL_MELEE_ATTACK );
            break;
        case Monster::EXT_BATTLE_MAGE:
            b.shots = 8;
            break;
        case Monster::EXT_ZEUS:
            g.cost.gems = 2;
            break;
        case Monster::EXT_MUMMY_PRIEST:
            setSpellCaster( b, 20, Spell::BLIND );
            break;
        case Monster::EXT_NOSFERATU:
            b.abilities.emplace_back( MonsterAbilityType::HP_DRAIN );
            // blood thirst (battle_action.cpp) instead of the enemy morale -1 (owner 2026-11-20 17:52)
            break;
        case Monster::EXT_DEMILICH:
            removeAbility( b, MonsterAbilityType::AREA_SHOT );
            break;
        case Monster::EXT_PLAGUE_DRAGON:
            removeAbility( b, MonsterAbilityType::MORAL_DECREMENT );
            b.abilities.emplace_back( MonsterAbilityType::MORAL_DECREMENT, 100, 2 );
            break;
        default:
            break;
        }
    }

}

namespace H2Ext
{
    void addMonsterData( std::vector<fheroes2::MonsterData> & data )
    {
        for ( size_t i = 0; i < extMonsters.size(); ++i ) {
            const ExtMonster & e = extMonsters[i];
            if ( static_cast<size_t>( e.id ) >= data.size() || static_cast<size_t>( e.base ) >= data.size() ) {
                continue;
            }
            fheroes2::MonsterData copy = data[e.base];
            copy.icnId = ICN::H2EXT_MONSTER_BATTLE_FIRST + static_cast<int>( i );
            fheroes2::MonsterBattleStats & b = copy.battleStats;
            b.attack = e.attack;
            b.defense = e.defense;
            b.damageMin = e.damageMin;
            b.damageMax = e.damageMax;
            b.hp = e.hp;
            b.speed = e.speed;
            fheroes2::MonsterGeneralStats & g = copy.generalStats;
            g.untranslatedName = e.name;
            g.untranslatedPluralName = e.plural;
            g.cost.gold = e.gold;
            applyAbilities( e.id, b, g );
            data[e.id] = copy;
        }
    }

    int extMonsterIndex( const int monsterId )
    {
        for ( size_t i = 0; i < extMonsters.size(); ++i ) {
            if ( extMonsters[i].id == monsterId ) {
                return static_cast<int>( i );
            }
        }
        return -1;
    }

    int extMonsterCount()
    {
        return static_cast<int>( extMonsters.size() );
    }

    int extMonsterId( const int index )
    {
        return ( index >= 0 && index < extMonsterCount() ) ? extMonsters[index].id : Monster::UNKNOWN;
    }

    int extMonsterBase( const int index )
    {
        return ( index >= 0 && index < extMonsterCount() ) ? extMonsters[index].base : Monster::UNKNOWN;
    }

    std::vector<int> upgradeChoices( const int monsterId )
    {
        if ( !isOn() ) {
            return {};
        }
        switch ( monsterId ) {
        // Knight
        case Monster::ARCHER:
            return { Monster::RANGER, Monster::EXT_CROSSBOWMAN };
        case Monster::PIKEMAN:
            return { Monster::VETERAN_PIKEMAN, Monster::EXT_HALBERDIER };
        case Monster::SWORDSMAN:
            return { Monster::MASTER_SWORDSMAN, Monster::EXT_SHIELDBEARER };
        case Monster::CAVALRY:
            return { Monster::CHAMPION, Monster::EXT_LANCER };
        case Monster::PALADIN:
            return { Monster::CRUSADER, Monster::EXT_GUARDIAN_KNIGHT };
        // Barbarian
        case Monster::ORC:
            return { Monster::ORC_CHIEF, Monster::EXT_ORC_BERSERKER };
        case Monster::WOLF:
            return { Monster::EXT_DIRE_WOLF, Monster::EXT_WINTER_WOLF };
        case Monster::OGRE:
            return { Monster::OGRE_LORD, Monster::EXT_OGRE_SHAMAN };
        case Monster::TROLL:
            return { Monster::WAR_TROLL, Monster::EXT_CAVE_TROLL };
        case Monster::CYCLOPS:
            return { Monster::EXT_CYCLOPS_KING, Monster::EXT_ANCIENT_CYCLOPS };
        // Sorceress
        case Monster::DWARF:
            return { Monster::BATTLE_DWARF, Monster::EXT_DWARF_GUARDIAN };
        case Monster::ELF:
            return { Monster::GRAND_ELF, Monster::EXT_FOREST_SNIPER };
        case Monster::DRUID:
            return { Monster::GREATER_DRUID, Monster::EXT_HEALING_DRUID };
        case Monster::UNICORN:
            return { Monster::EXT_BATTLE_UNICORN, Monster::EXT_SILVER_UNICORN };
        case Monster::PHOENIX:
            return { Monster::EXT_GREAT_PHOENIX, Monster::EXT_FIREBIRD };
        // Warlock
        case Monster::GARGOYLE:
            return { Monster::EXT_OBSIDIAN_GARGOYLE, Monster::EXT_STONE_GARGOYLE };
        case Monster::GRIFFIN:
            return { Monster::EXT_ROYAL_GRIFFIN, Monster::EXT_BATTLE_GRIFFIN };
        case Monster::MINOTAUR:
            return { Monster::MINOTAUR_KING, Monster::EXT_MINOTAUR_GLADIATOR };
        case Monster::HYDRA:
            return { Monster::EXT_CHAOS_HYDRA, Monster::EXT_ACID_HYDRA };
        case Monster::GREEN_DRAGON:
            // owner 20:12: Green upgrades once to Red or Black (original numbers)
            return { Monster::RED_DRAGON, Monster::BLACK_DRAGON };
        // Wizard
        case Monster::BOAR:
            return { Monster::EXT_BATTLE_BOAR, Monster::EXT_BOAR_RIDER };
        case Monster::IRON_GOLEM:
            return { Monster::STEEL_GOLEM, Monster::EXT_MITHRIL_GOLEM };
        case Monster::ROC:
            return { Monster::EXT_GREAT_ROC, Monster::EXT_STORM_ROC };
        case Monster::MAGE:
            return { Monster::ARCHMAGE, Monster::EXT_BATTLE_MAGE };
        case Monster::GIANT:
            return { Monster::TITAN, Monster::EXT_ZEUS };
        // Necromancer
        case Monster::ZOMBIE:
            return { Monster::MUTANT_ZOMBIE, Monster::EXT_PLAGUE_ZOMBIE };
        case Monster::MUMMY:
            return { Monster::ROYAL_MUMMY, Monster::EXT_MUMMY_PRIEST };
        case Monster::VAMPIRE:
            return { Monster::VAMPIRE_LORD, Monster::EXT_NOSFERATU };
        case Monster::LICH:
            return { Monster::POWER_LICH, Monster::EXT_DEMILICH };
        case Monster::BONE_DRAGON:
            return { Monster::EXT_WRAITH_DRAGON, Monster::EXT_PLAGUE_DRAGON };
        default:
            return {};
        }
    }

    int aiBranch( const int originalId, const int32_t castleIndex )
    {
        // owner 2026-11-19 00:18 (question answer "성마다 무작위로 고정"): each castle keeps one branch per tier for the whole game,
        // chosen at random from the map seed, the castle and the creature (the engine's strength value cannot see the new abilities)
        const std::vector<int> choices = upgradeChoices( originalId );
        if ( choices.size() < 2 ) {
            return -1;
        }
        uint32_t h = world.GetMapSeed() ^ ( static_cast<uint32_t>( castleIndex ) * 2654435761U ) ^ ( static_cast<uint32_t>( originalId ) * 40503U );
        h ^= h >> 16;
        h *= 0x7feb352dU;
        h ^= h >> 15;
        h *= 0x846ca68bU;
        h ^= h >> 16;
        return choices[h % choices.size()];
    }

    std::vector<uint8_t> makeRecolorPalette( const double hue, const double saturation, const double value, const double tint, const uint8_t tintR,
                                             const uint8_t tintG, const uint8_t tintB )
    {
        std::vector<uint8_t> pal;
        struct
        {
            double hue;
            double saturation;
            double value;
            double tint;
            uint8_t tintR;
            uint8_t tintG;
            uint8_t tintB;
        } const e{ hue, saturation, value, tint, tintR, tintG, tintB };
        const fheroes2::RGB * rgb = fheroes2::getRGBGamePalette();
        pal.resize( 256 );
        auto toHsv = []( const fheroes2::RGB & c, double & h, double & s, double & v ) {
            const double r = c.r / 255.0;
            const double g = c.g / 255.0;
            const double b = c.b / 255.0;
            const double mx = std::max( { r, g, b } );
            const double mn = std::min( { r, g, b } );
            v = mx;
            s = mx > 0 ? ( mx - mn ) / mx : 0;
            const double d = mx - mn;
            if ( d <= 0 ) {
                h = 0;
            }
            else if ( mx == r ) {
                h = 60 * std::fmod( ( g - b ) / d, 6.0 );
            }
            else if ( mx == g ) {
                h = 60 * ( ( b - r ) / d + 2 );
            }
            else {
                h = 60 * ( ( r - g ) / d + 4 );
            }
            if ( h < 0 ) {
                h += 360;
            }
        };
        for ( int i = 0; i < 256; ++i ) {
            pal[i] = static_cast<uint8_t>( i );
            // the colour cycling entries and the reserved black/transparent entries stay as they are
            if ( i == 0 || ( i >= 214 && i <= 241 ) ) {
                continue;
            }
            double h = 0;
            double s = 0;
            double v = 0;
            toHsv( rgb[i], h, s, v );
            h = std::fmod( h + e.hue + 360.0, 360.0 );
            s = std::min( 1.0, s * e.saturation );
            v = std::min( 1.0, v * e.value );
            // back to RGB
            const double c = v * s;
            const double x = c * ( 1 - std::fabs( std::fmod( h / 60.0, 2.0 ) - 1 ) );
            const double m = v - c;
            double r = 0;
            double g = 0;
            double b = 0;
            if ( h < 60 ) {
                r = c, g = x;
            }
            else if ( h < 120 ) {
                r = x, g = c;
            }
            else if ( h < 180 ) {
                g = c, b = x;
            }
            else if ( h < 240 ) {
                g = x, b = c;
            }
            else if ( h < 300 ) {
                r = x, b = c;
            }
            else {
                r = c, b = x;
            }
            int R = static_cast<int>( ( r + m ) * 255 );
            int G = static_cast<int>( ( g + m ) * 255 );
            int B = static_cast<int>( ( b + m ) * 255 );
            if ( e.tint > 0 ) {
                // keep the brightness, move the colour toward the tint
                const double lum = ( 0.3 * R + 0.59 * G + 0.11 * B ) / 255.0;
                const double tl = ( 0.3 * e.tintR + 0.59 * e.tintG + 0.11 * e.tintB ) / 255.0;
                const double k = tl > 0 ? lum / tl : 0;
                R = static_cast<int>( R * ( 1 - e.tint ) + std::min( 255.0, e.tintR * k ) * e.tint );
                G = static_cast<int>( G * ( 1 - e.tint ) + std::min( 255.0, e.tintG * k ) * e.tint );
                B = static_cast<int>( B * ( 1 - e.tint ) + std::min( 255.0, e.tintB * k ) * e.tint );
            }
            int best = i;
            int bestDist = 1 << 30;
            for ( int j = 1; j < 256; ++j ) {
                if ( j >= 214 && j <= 241 ) {
                    continue;
                }
                const int dr = rgb[j].r - R;
                const int dg = rgb[j].g - G;
                const int db = rgb[j].b - B;
                const int dist = dr * dr + dg * dg + db * db;
                if ( dist < bestDist ) {
                    bestDist = dist;
                    best = j;
                }
            }
            pal[i] = static_cast<uint8_t>( best );
        }
        return pal;
    }

    const std::vector<uint8_t> & extMonsterPalette( const int index )
    {
        static std::vector<std::vector<uint8_t>> cache( extMonsters.size() );
        std::vector<uint8_t> & pal = cache[index];
        if ( pal.empty() ) {
            const ExtMonster & e = extMonsters[index];
            pal = makeRecolorPalette( e.hue, e.saturation, e.value, e.tint, e.tintR, e.tintG, e.tintB );
        }
        return pal;
    }

    int turnSpell( const int monsterId )
    {
        switch ( monsterId ) {
        case Monster::EXT_ORC_BERSERKER:
            return Spell::BLOODLUST;
        case Monster::EXT_CAVE_TROLL:
            return Spell::STONESKIN;
        case Monster::EXT_HEALING_DRUID:
            return Spell::CURE;
        case Monster::EXT_DEMILICH:
            return Spell::SLOW;
        default:
            return Spell::NONE;
        }
    }

    std::vector<std::string> extAbilityDescriptions( const int monsterId )
    {
        switch ( monsterId ) {
        case Monster::EXT_STONE_GARGOYLE:
            return { _( "Takes half damage from shots." ) };
        case Monster::EXT_MINOTAUR_GLADIATOR:
            return { _( "Luck +1." ) };
        case Monster::EXT_ACID_HYDRA:
            return { _( "Acid: lowers the defense of units it hits by 2 (up to 6)." ) };
        case Monster::EXT_CROSSBOWMAN:
            return { _( "No penalty when shooting over castle walls." ) };
        case Monster::EXT_HALBERDIER:
            return { _( "+50% damage against two-cell creatures." ) };
        case Monster::EXT_SHIELDBEARER:
            return { _( "Shield wall: it and the units next to it take 30% less damage from shots." ) };
        case Monster::EXT_LANCER:
        case Monster::EXT_BOAR_RIDER:
            return { _( "Charge: +5% damage for each cell walked before the attack." ) };
        case Monster::EXT_GUARDIAN_KNIGHT:
            return { _( "Army morale +1." ) };
        case Monster::EXT_FOREST_SNIPER:
            return { _( "Aimed shot: counts half of the target's defense." ) };
        case Monster::EXT_GREAT_PHOENIX:
            return { _( "Rebirth: once a battle, a third of the stack rises when destroyed." ) };
        case Monster::EXT_MITHRIL_GOLEM:
            return { _( "Damage from spells -75%." ) };
        case Monster::EXT_ZEUS:
            return { _( "30% chance of a lightning strike (10 damage per Zeus)." ) };
        case Monster::EXT_DIRE_WOLF:
            return { _( "20% chance of a critical bite: double damage." ) };
        case Monster::EXT_OBSIDIAN_GARGOYLE:
            return { _( "20% chance that a blow does it no harm." ) };
        case Monster::EXT_ROYAL_GRIFFIN:
            return { _( "20% chance of double damage when it retaliates." ) };
        case Monster::EXT_CHAOS_HYDRA:
            return { _( "25% chance of acid: lowers the defense of units it hits by 2 (up to 6)." ) };
        case Monster::EXT_BATTLE_BOAR:
            return { _( "20% chance to knock the target down: it skips its next turn." ) };
        case Monster::EXT_GREAT_ROC:
            return { _( "20% chance to slow the target." ) };
        case Monster::EXT_BATTLE_MAGE:
            return { _( "25% chance that an attack spell strikes the target as well (Magic Arrow, Cold Ray, Lightning Bolt or Fireball; spell power = half the number of Battle Mages)." ) };
        case Monster::EXT_PLAGUE_ZOMBIE:
            return { _( "Plague: lowers the attack of units it hits by 2 (up to 6)." ) };
        case Monster::EXT_WRAITH_DRAGON:
            return { _( "20% chance to frighten: the target loses its next turn." ) };
        case Monster::EXT_NOSFERATU:
            return { _( "Blood thirst: attack +1 each time its attack kills, up to +5 in a battle." ) };
        case Monster::EXT_ORC_BERSERKER:
        case Monster::EXT_CAVE_TROLL:
        case Monster::EXT_HEALING_DRUID:
        case Monster::EXT_DEMILICH: {
            const Spell spell( turnSpell( monsterId ) );
            std::string text = spell.isApplyToEnemies() ? _( "Each turn casts %{spell} on an enemy." ) : _( "Each turn casts %{spell} on an ally." );
            const size_t pos = text.find( "%{spell}" );
            if ( pos != std::string::npos ) {
                text.replace( pos, 8, spell.GetName() );
            }
            return { text };
        }
        case Monster::RED_DRAGON:
            if ( isOn() ) {
                return { _( "Hero's Fireball and Fireblast +20%." ) };
            }
            break;
        default:
            break;
        }
        return {};
    }
}
