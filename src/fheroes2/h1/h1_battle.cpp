// Heroes I battle (homm1-remaster project). See h1_battle.h.
// Function names in the comments are HEROESW.EXE addresses (Windows version, owner baseline). Unit fields are given as the original
// unit record offsets (+0x..) where it helps to compare with the decompiled code.
#include "h1_battle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "battle.h"
#include "color.h"
#include "cursor.h"
#include "h1_armywin.h"
#include "h1_castle.h"
#include "h1_cpanel.h"
#include "castle.h"
#include "maps.h"
#include "world.h"
#include "h1_gfx.h"
#include "h1_heroes.h"
#include "h1_newgame.h"
#include "h1_overview.h"
#include "h1_rules.h"
#include "h1_spellbook.h"
#include "h1_sound.h"
#include "h1_spells.h"
#include "game_assets.h"
#include "game_hotkeys.h"
#include "icn.h"
#include "rand.h"
#include "settings.h"
#include "spell.h"
#include "h1_ui.h"
#include "heroes.h"
#include "kingdom.h"
#include "resource.h"
#include "image.h"
#include "localevent.h"
#include "screen.h"
#include "timing.h"

namespace
{
    // Creature picture files, names, worth and experience: H1::creatureFile / creatureNameOne / creatureNameMany / creatureWorth /
    // creatureExperience (h1_rules.cpp; tables 0x493068, 0x4930d8, 0x491e78 for the 28 HoMM1 creatures, H1-INTENDED(D16) 28..35).

    // Neighbour table 0x490fd8: 45 cells x 6 directions (0 up-right, 1 right, 2 down-right, 3 down-left, 4 left, 5 up-left); columns 0
    // and 8 are not part of the field (all -1, and nothing leads to them).
    const int8_t neighbourTable[45][6]
        = { { -1, -1, -1, -1, -1, -1 }, { -1, 2, 10, -1, -1, -1 }, { -1, 3, 11, 10, 1, -1 },     { -1, 4, 12, 11, 2, -1 },   { -1, 5, 13, 12, 3, -1 },
            { -1, 6, 14, 13, 4, -1 },   { -1, 7, 15, 14, 5, -1 },  { -1, -1, 16, 15, 6, -1 },    { -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1 },
            { 2, 11, 20, 19, -1, 1 },   { 3, 12, 21, 20, 10, 2 },  { 4, 13, 22, 21, 11, 3 },     { 5, 14, 23, 22, 12, 4 },   { 6, 15, 24, 23, 13, 5 },
            { 7, 16, 25, 24, 14, 6 },   { -1, -1, -1, 25, 15, 7 }, { -1, -1, -1, -1, -1, -1 },   { -1, -1, -1, -1, -1, -1 }, { 10, 20, 28, -1, -1, -1 },
            { 11, 21, 29, 28, 19, 10 }, { 12, 22, 30, 29, 20, 11 }, { 13, 23, 31, 30, 21, 12 },  { 14, 24, 32, 31, 22, 13 }, { 15, 25, 33, 32, 23, 14 },
            { 16, -1, 34, 33, 24, 15 }, { -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1 },  { 20, 29, 38, 37, -1, 19 }, { 21, 30, 39, 38, 28, 20 },
            { 22, 31, 40, 39, 29, 21 }, { 23, 32, 41, 40, 30, 22 }, { 24, 33, 42, 41, 31, 23 },  { 25, 34, 43, 42, 32, 24 }, { -1, -1, -1, 43, 33, 25 },
            { -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1 }, { 28, 38, -1, -1, -1, -1 },  { 29, 39, -1, -1, 37, 28 }, { 30, 40, -1, -1, 38, 29 },
            { 31, 41, -1, -1, 39, 30 }, { 32, 42, -1, -1, 40, 31 }, { 33, 43, -1, -1, 41, 32 },  { 34, -1, -1, -1, 42, 33 }, { -1, -1, -1, -1, -1, -1 } };

    // Tables 0x490e70 / 0x490e90 by terrain group (tile number / 20).
    const char * const groundFiles[7] = { "boat", "grass", "snow", "swamp", "lava", "desert", "dgrass" };

    enum : int
    {
        CREATURE_PIKEMAN = 2,
        CREATURE_SWORDSMAN = 3,
        CREATURE_CAVALRY = 4,
        CREATURE_PALADIN = 5,
        CREATURE_GOBLIN = 6,
        CREATURE_OGRE = 9,
        CREATURE_WOLF = 8,
        CREATURE_TROLL = 10,
        CREATURE_CYCLOPS = 11,
        CREATURE_SPRITE = 12,
        CREATURE_DWARF = 13,
        CREATURE_ELF = 14,
        CREATURE_UNICORN = 16,
        CREATURE_GARGOYLE = 19,
        CREATURE_GRIFFIN = 20,
        CREATURE_MINOTAUR = 21,
        CREATURE_HYDRA = 22,
        CREATURE_DRAGON = 23,
        CREATURE_ROGUE = 24,
        CREATURE_GHOST = 26,
        CREATURE_GENIE = 27,
        // H1-INTENDED(D16) extension creatures used by battle rules (second batch, note v241)
        CREATURE_ZOMBIE = 36,
        CREATURE_MUMMY = 37,
        CREATURE_VAMPIRE = 38,
        CREATURE_BONE_DRAGON = 39
    };

    enum : int
    {
        SPELL_BLIND = 7,
        SPELL_CURSE = 10,
        SPELL_ANTI_MAGIC = 12,
        SPELL_BERSERK = 14,
        SPELL_PARALYZE = 18
    };

    // Unit flags +0x25.
    enum : uint32_t
    {
        FLAG_TWO_HEX = 1,
        FLAG_FLY = 2,
        FLAG_SHOOTER = 4,
        FLAG_HITS_BEHIND = 8,
        FLAG_DEAD = 0x10,
        FLAG_MORALE_TURN = 0x20,
        FLAG_RETALIATED = 0x40,
        FLAG_DONE = 0x80,
        // H1-INTENDED(D16) extension mode only (cleared with the others at a new round, newRound keeps 0x1f):
        FLAG_WAITING = 0x100, // "Wait": acts after the others this round
        FLAG_WAITED = 0x200, // has waited this round (once only)
        FLAG_RAGED = 0x400 // ogres: war cry used this battle (not cleared at a new round, see newRound)
    };

    // Actions of FUN_0044e18c (DAT_004c2ca0).
    enum : int
    {
        ACTION_NONE = 0,
        ACTION_SPELL = 1,
        ACTION_MOVE = 2,
        ACTION_WAIT = 3,
        ACTION_RETREAT = 4,
        ACTION_SURRENDER = 5,
        ACTION_MOVE_ATTACK = 6,
        ACTION_DELAY = 7 // H1-INTENDED(D16) extension: "Wait" (key W), the unit acts again at the end of the round
    };

    struct Cell
    {
        int32_t x = 0; // +0x40
        int32_t y = 0; // +0x42
        int ground = 0; // +0x45 frame of <ground>.xtl
        int obstacle = -1; // +0x47 frame of <ground>.obj
        int side = -1; // +0x48
        int slot = -1; // +0x49
    };

    struct Unit
    {
        bool exists = false;
        int idSide = -1; // +0x00: the side the path search treats as "this unit" (the original changes it on purpose)
        int idSlot = -1; // +0x01
        int dir = 0; // +0x02 attack direction
        int dest = -1; // +0x05
        int creature = -1; // +0x06
        int cell = -1; // +0x07
        int facing = 0; // +0x0a: 0 looks right (attacking side), 1 looks left
        int startCount = 0; // +0x0d
        int count = 0; // +0x0f
        int hurt = 0; // +0x11 damage taken by the first creature
        int luck = 0; // +0x15
        int hp = 0; // +0x16
        int speed = 0; // +0x17
        int attack = 0; // +0x19
        int defense = 0; // +0x1a
        int damageMin = 0; // +0x1b
        int damageMax = 0; // +0x1c
        int shots = 0; // +0x1e
        uint32_t flags = 0; // +0x25
        int side = 0; // +0x2d
        int slot = 0; // +0x2e
        int spell = -1; // +0x51 spell on the unit
        int spellKind = -1; // +0x52 how it ends: 3 after its rounds, 2 when hit (Blind, Paralyze), 1 after one blow (Berserk)
        int spellRounds = 0; // +0x53 rounds left (the caster's spell power, 3 for a creature's blow)
        int damageState = 0; // +0x13: 1 always minimum damage, 3 always maximum
        int baseSpeed = 0; // +0x14
        int armySlot = -1;
        // Picture state (+0x08 file: 0 .std, 1 .wlk, 2 .atk; +0x09 frame; +0x0b vertical step per walk frame) and a drawing offset.
        int animState = 0;
        int animFrame = 1;
        int animStep = 0;
        int32_t offsetX = 0;
        int32_t offsetY = 0;
    };

    void put( fheroes2::Image & dst, const std::vector<fheroes2::Sprite> & icn, const int frame, const int32_t x, const int32_t y, const bool mirror )
    {
        if ( frame < 0 || static_cast<size_t>( frame ) >= icn.size() ) {
            return;
        }
        const fheroes2::Sprite & s = icn[frame];
        const int32_t left = mirror ? ( x - s.x() - s.width() + 1 ) : ( x + s.x() );
        fheroes2::Blit( s, dst, left, y + s.y(), mirror );
    }

    bool moreTrees( const int32_t x, const int32_t y )
    {
        static const int dir[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
        int trees = 0;
        int mountains = 0;
        for ( int d = 0; d < 3; ++d ) {
            for ( int k = 0; k < 8; ++k ) {
                const int32_t tx = dir[k][0] * d + x;
                const int32_t ty = dir[k][1] * d + y;
                if ( tx < 0 || ty < 0 || tx >= 72 || ty >= 72 ) {
                    continue;
                }
                const uint8_t * t = H1::getTileBytes( ty * 72 + tx );
                if ( t == nullptr ) {
                    continue;
                }
                const int set = t[1] & 0xf;
                if ( set == 8 ) {
                    ++mountains;
                }
                else if ( set == 9 ) {
                    ++trees;
                }
            }
        }
        return mountains < trees;
    }

    std::string backgroundFile( const int group, const uint8_t * t, const int32_t x, const int32_t y )
    {
        if ( t != nullptr && ( t[8] & 0x7f ) == 0x0c ) {
            return "gravyard.bkg";
        }
        switch ( group ) {
        case 0:
            return "boat.bkg";
        case 1:
            return moreTrees( x, y ) ? "frstwgrs.bkg" : "mtnwgrsf.bkg";
        case 2:
            return moreTrees( x, y ) ? "snowfrst.bkg" : "snowmtnf.bkg";
        case 3:
            return "swamp.bkg";
        case 4:
            return "lava.bkg";
        case 5:
            return "desert.bkg";
        case 6:
            return moreTrees( x, y ) ? "frstwdrt.bkg" : "mtnwdrtf.bkg";
        default:
            return "frstwgrs.bkg";
        }
    }

    int playerColourIndex( const Army & army )
    {
        const int index = Color::GetIndex( army.GetColor() );
        return ( index >= 0 && index < 4 ) ? index : -1;
    }

    bool validCell( const int c )
    {
        return c >= 0 && c <= 0x2c;
    }

    int neighbour( const int c, const int d )
    {
        if ( !validCell( c ) || d < 0 || d > 5 ) {
            return -1;
        }
        return neighbourTable[c][d];
    }

    // FUN_00470140: the opposite direction.
    int opposite( const int d )
    {
        if ( d < 6 ) {
            return ( d + 3 ) % 6;
        }
        return d == 6 ? 7 : 6;
    }

    // FUN_0044e720: distance between two screen points as the original measures it.
    int screenDistance( const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2 )
    {
        const int dx = std::abs( x1 - x2 );
        const int dy = std::abs( y1 - y2 );
        if ( dx < dy ) {
            return dx / 2 + dy;
        }
        return dy / 2 + dx;
    }

    // FUN_00470198: the direction to try first from cell 'from' towards cell 'to', skipping directions whose bit is set in mask.
    int preferredDirection( const int from, const int to, const int mask )
    {
        if ( !validCell( from ) || !validCell( to ) ) {
            return -1;
        }
        const int fc = from % 9;
        const int fr = from / 9;
        const int tc = to % 9;
        const int tr = to / 9;
        const bool toRight = fc < tc;
        const bool toLeft = fc > tc;
        const bool up = fr > tr;
        const bool down = fr < tr;
        const bool evenRow = ( fr & 1 ) == 0;
        static const int sameUpEven[8] = { 0, 5, 1, 4, 2, 3, 6, 7 };
        static const int sameUpOdd[8] = { 5, 0, 4, 1, 3, 2, 6, 7 };
        static const int sameDownEven[8] = { 2, 3, 1, 4, 0, 5, 7, 6 };
        static const int sameDownOdd[8] = { 3, 2, 4, 1, 5, 0, 7, 6 };
        static const int leftUp[8] = { 5, 4, 0, 3, 1, 2, 6, 7 };
        static const int leftDown[8] = { 3, 4, 2, 5, 1, 0, 7, 6 };
        static const int leftSame[8] = { 4, 5, 3, 0, 2, 1, 7, 6 };
        static const int rightUp[8] = { 0, 1, 5, 2, 4, 3, 6, 7 };
        static const int rightDown[8] = { 2, 1, 3, 0, 5, 4, 7, 6 };
        static const int rightSame[8] = { 1, 0, 2, 5, 3, 4, 7, 6 };
        const int * order = nullptr;
        if ( toLeft == toRight ) {
            if ( up ) {
                order = evenRow ? sameUpEven : sameUpOdd;
            }
            else {
                order = evenRow ? sameDownEven : sameDownOdd;
            }
        }
        else if ( toLeft ) {
            order = up ? leftUp : ( down ? leftDown : leftSame );
        }
        else {
            order = up ? rightUp : ( down ? rightDown : rightSame );
        }
        for ( int k = 0; k < 8; ++k ) {
            if ( ( mask & ( 1 << order[k] ) ) == 0 ) {
                return order[k];
            }
        }
        return -1;
    }


    // Spell table 0x490f98 (bit 1: worth counting in the army strength, bit 0: scaled by spell power through the attack table) and
    // 0x490f58 (worth of one cast) for HoMM1 spells 0..28; 0x492330: weight of the number of casts left (0..20).
    const int spellFlags[29] = { 3, 3, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
    const int spellWorth[29] = { 500, 350, 300, 400, 550, 900, 400, 500, 300, 350, 250, 0, 100, 150, 1000, 2000, 1700, 700, 700, 0, 0, 0, 0, 0, 0, 0, 0, 1200, 0 };
    const float castsWeight[21] = { 0.0f, 1.0f, 1.7f, 2.2f, 2.6f, 2.95f, 3.27f, 3.56f, 3.81f, 4.04f, 4.25f, 4.45f, 4.64f, 4.83f, 5.01f, 5.19f, 5.36f, 5.53f, 5.68f, 5.82f, 5.96f };
    // Artifact worth 0x491960 (artifacts 0..36).
    const int artifactWorth[37] = { 9000, 22000, 18000, 14000, 6000, 4000, 4000, 5600, 1200, 1200, 1200, 1200, -1200, 2000, 1800, 1800, 2000, 1000, 3600,
                                    5600, 4000, 5040, 2700, 3900, 4950, 5850, 7000, 6000, 4000, 4500, 2250, 1200, 1200, 1200, 1200, 3500, 1500 };

    H1::StrengthInput strengthInput( const int creatures[5], const int counts[5], const Heroes * hero, const int moraleValue, const int luckValue )
    {
        H1::StrengthInput in;
        for ( int k = 0; k < 5; ++k ) {
            in.creatures[k] = creatures[k];
            in.counts[k] = counts[k];
        }
        if ( hero != nullptr ) {
            in.hero = true;
            in.attack = hero->GetAttack();
            in.defense = hero->GetDefense();
            in.power = hero->GetPower();
            for ( size_t k = 0; k < 29; ++k ) {
                in.spells[k] = static_cast<int>( hero->h1BookSpell[k] ) - 1;
                in.casts[k] = static_cast<int>( hero->h1BookCount[k] );
            }
            in.morale = moraleValue;
            in.luck = luckValue;
        }
        return in;
    }

    // FUN_0042e009 with the third argument 1 and no castle part: strength of an army (creatures / counts, -1 = empty) under a hero.
    int armyStrength( const int creatures[5], const int counts[5], const Heroes * hero, const int moraleValue, const int luckValue )
    {
        H1::StrengthInput in = strengthInput( creatures, counts, hero, moraleValue, luckValue );
        in.corrections = true;
        return H1::armyStrengthOriginal( in );
    }

    class Fight
    {
    public:
        std::array<Cell, 45> cells;
        Unit units[2][5];
        int current = 0; // +0x6ba side whose unit acts
        int currentSlot = 0; // +0x6bb
        int speedLevel = 4; // +0x6bc
        bool fled[2] = { false, false }; // +0x6fd
        bool surrendered[2] = { false, false }; // +0x2b6
        int ghostKills[2] = { 0, 0 }; // +0x6e9
        bool hasHero[2] = { false, false }; // +0x2a6
        bool castCast[2] = { false, false }; // +0x2c4 a spell was cast this round [spells: step P4]
        bool castleDefender = false; // +0x29e: the defending side is a town or castle
        Army * sideArmy[2] = { nullptr, nullptr }; // +0x2ae: the side's army (creature window morale, FUN_00410e9f)
        const Castle * defenderCastle = nullptr; // +0x29e as a pointer (FUN_00467754 third argument)
        bool siege = false; // +0x6dd: that town has the Castle building - walls, keep and catapult (note v100)
        int castleRace = 0; // castle +3 (castle%02d.icn, keep%02d.icn)
        int castleBits = 0; // castle +0x16
        int castleGuild = 0; // castle +0x18
        int catapultShots[2] = { 1, 1 }; // +0x6c9: 2 with the Ballista (artifact 17)
        int catapultLeft[2] = { 0, 0 }; // +0x6cd
        int keepLeft[2] = { 0, 0 }; // +0x6d1
        int surrenderCost = 0; // DAT_004c2ca8 / DAT_004c2c78
        int morale[2] = { 0, 0 };
        int luck[2] = { 0, 0 };
        bool human[2] = { false, false };
        bool autoCombat = false; // +0x6bd
        bool lastGoodMorale = false; // DAT_004a2878
        // H1-INTENDED(D16) extension mode: the unit that walked this many cells before its blow (cavalry charge).
        const Unit * chargeUnit = nullptr;
        int chargeCells = 0;
        int lastWalked = 0; // H1-INTENDED(D16): cells of the last walkTo (a person's attack walks to its cell first, then strikes)

        // H1-INTENDED(D16) extension mode creature abilities (owner 2026-11-17 23:29, docs/EXTENSION_MODE_v1.md): minotaurs always have
        // good morale (at least +1), gargoyles are stone (no morale either way).
        int unitMorale( const int side, const Unit & u ) const
        {
            if ( !H1::isExtension() ) {
                return morale[side];
            }
            if ( u.creature == CREATURE_GARGOYLE ) {
                return 0;
            }
            if ( u.creature == CREATURE_MINOTAUR ) {
                return std::max( morale[side], 1 );
            }
            // Second batch (note v241): HoMM2's undead have no morale; a bone dragon lowers the other side's morale by 1.
            if ( u.creature == 29 || u.creature == 30 || ( u.creature >= CREATURE_ZOMBIE && u.creature <= CREATURE_BONE_DRAGON ) ) {
                return 0;
            }
            for ( const Unit & other : units[1 - side] ) {
                if ( other.exists && other.count > 0 && !( other.flags & FLAG_DEAD ) && other.creature == CREATURE_BONE_DRAGON ) {
                    return std::max( morale[side] - 1, -3 );
                }
            }
            return morale[side];
        }
        bool halfTroops = false; // DAT_004a72d4
        std::string message;
        H1::GameRandom random;

        // Action chosen (DAT_004c2ca0, DAT_004c2c9c, DAT_004c2c78).
        int action = ACTION_NONE;
        int actionCell = -1;
        int actionStand = -1;

        // Path search result (DAT_004a7bcc: +8 count, +0xda54 directions; entry 0 is the last step).
        std::vector<int> path;

        // Attack sectors around the cell under the mouse (+0x7b3, 24 entries), number of possible attack places (+0x7cc), last sector (+0x7cb).
        int sectors[24] = {};
        int attackPlaces = 0;
        int lastSector = -1;
        int standCell = -1; // +0x6c2

        Unit & unit( const int side, const int slot )
        {
            return units[side][slot];
        }

        Unit & acting()
        {
            return units[current][currentSlot];
        }

        bool isFree( const int c ) const
        {
            return validCell( c ) && cells[c].side == -1 && cells[c].obstacle == -1;
        }

        int otherCell( const Unit & u ) const
        {
            if ( !( u.flags & FLAG_TWO_HEX ) ) {
                return -1;
            }
            return u.facing == 0 ? u.cell + 1 : u.cell - 1;
        }

        // FUN_004387b0 [simplified: the cell records are rebuilt from the units after every change].
        void updateCells()
        {
            for ( Cell & c : cells ) {
                c.side = -1;
                c.slot = -1;
            }
            for ( int s = 0; s < 2; ++s ) {
                for ( int k = 0; k < 5; ++k ) {
                    const Unit & u = units[s][k];
                    if ( !u.exists || ( u.flags & FLAG_DEAD ) || !validCell( u.cell ) ) {
                        continue;
                    }
                    cells[u.cell].side = s;
                    cells[u.cell].slot = k;
                    const int t = otherCell( u );
                    if ( validCell( t ) ) {
                        cells[t].side = s;
                        cells[t].slot = k;
                    }
                }
            }
        }

        // FUN_0046fa68: neighbour with the unit's two extra directions (6, 7) of two-hex units.
        int step( const Unit & u, const int c, int d ) const
        {
            if ( c == -1 ) {
                return -1;
            }
            if ( d == 6 ) {
                d = ( u.facing == 0 ) ? 5 : 0;
            }
            else if ( d == 7 ) {
                d = ( u.facing == 0 ) ? 3 : 2;
            }
            return neighbour( c, d );
        }

        // FUN_0046f5d7: can the unit step from c in direction d (0..5).
        bool canStep( const Unit & u, const int c, const int d ) const
        {
            if ( !validCell( c ) ) {
                return false;
            }
            const int n = step( u, c, d );
            if ( !validCell( n ) ) {
                return false;
            }
            const bool headFree = cells[n].side == -1 && cells[n].obstacle == -1;
            if ( !( u.flags & FLAG_TWO_HEX ) ) {
                return headFree;
            }
            int t = -1;
            if ( u.facing == 0 ) {
                if ( d == 4 ) {
                    return headFree;
                }
                t = step( u, n, 1 );
            }
            else {
                if ( d == 1 ) {
                    return headFree;
                }
                t = step( u, n, 4 );
            }
            bool tailFree = validCell( t ) && cells[t].side == -1 && cells[t].obstacle == -1;
            if ( d != 1 && d != 4 ) {
                tailFree = headFree && tailFree;
            }
            return tailFree;
        }

        // FUN_0046f470: directions the unit cannot step to (bits 6 and 7 always set).
        int blockedSteps( const Unit & u, const int c ) const
        {
            int mask = 0;
            for ( int d = 0; d < 6; ++d ) {
                if ( !canStep( u, c, d ) ) {
                    mask |= 1 << d;
                }
            }
            return mask | 0xc0;
        }

        // FUN_0046f7d3: the cell next to c in direction d (two-hex units measured from the right end), tested by mode:
        // 0 = the unit idSide / idSlot is there, 1 = an enemy of the acting side is there, 2 = any unit is there.
        bool touches( const Unit & u, int c, const int d, const int mode, const int wanted, int & out ) const
        {
            out = -1;
            if ( !validCell( c ) ) {
                return false;
            }
            if ( !( u.flags & FLAG_TWO_HEX ) ) {
                out = step( u, c, d );
            }
            else if ( d == 6 ) {
                out = step( u, c, u.facing == 1 ? 5 : 0 );
            }
            else if ( d == 7 ) {
                out = step( u, c, u.facing == 1 ? 3 : 2 );
            }
            else {
                if ( u.facing == 0 ) {
                    if ( d < 3 ) {
                        c = step( u, c, 1 );
                    }
                }
                else if ( d > 2 ) {
                    c = step( u, c, 4 );
                }
                if ( c == -1 ) {
                    return false;
                }
                out = step( u, c, d );
            }
            if ( !validCell( out ) ) {
                return false;
            }
            if ( wanted != -1 && out != wanted ) {
                return false;
            }
            const int occupant = cells[out].side;
            if ( mode == 0 ) {
                return u.idSide == occupant && cells[out].slot == u.idSlot;
            }
            if ( mode == 1 ) {
                return occupant == 1 - current;
            }
            if ( mode == 2 ) {
                return occupant != -1;
            }
            return false;
        }

        // FUN_0046f4eb: directions where touches() fails (one-hex units: bits 6, 7 always set). 0xff = nothing there.
        int touchMask( const Unit & u, const int c, const int mode, const int wanted ) const
        {
            const bool two = ( u.flags & FLAG_TWO_HEX ) != 0;
            int mask = two ? 0 : 0xc0;
            const int count = two ? 8 : 6;
            for ( int d = 0; d < count; ++d ) {
                int out = -1;
                if ( !touches( u, c, d, mode, wanted, out ) ) {
                    mask |= 1 << d;
                }
            }
            return mask;
        }

        // FUN_0044e820: path search. With u.idSide == -1 it looks for a way to cell 'to'; otherwise for a cell next to the unit
        // idSide / idSlot (next to cell 'to' itself when exactNeighbour). Steps are limited to u.speed. Fills path; returns its length.
        int findPath( const int from, const int to, Unit & u, const bool exactNeighbour )
        {
            path.clear();
            if ( !validCell( from ) || !validCell( to ) ) {
                return 0;
            }
            struct Node
            {
                int cell;
                int steps;
            };
            std::vector<Node> queue; // kept with the largest steps first; taken from the end (FUN_0044eb20)
            int enteredBy[45];
            int stepsTo[45];
            bool seen[45];
            for ( int k = 0; k < 45; ++k ) {
                enteredBy[k] = 0;
                stepsTo[k] = 0;
                seen[k] = false;
            }
            const int maxSteps = u.speed;
            auto push = [&]( const int c, const int d, const int steps ) {
                if ( !validCell( c ) ) {
                    return;
                }
                if ( maxSteps != 0 && steps > maxSteps ) {
                    return;
                }
                if ( seen[c] && steps >= stepsTo[c] ) {
                    return;
                }
                if ( queue.size() >= 0x400 ) {
                    return;
                }
                // Binary search: first place whose steps <= this one; equal entries go in front of the older ones.
                size_t lo = 0;
                size_t hi = queue.size();
                while ( lo < hi ) {
                    const size_t mid = ( lo + hi ) / 2;
                    if ( steps < queue[mid].steps ) {
                        lo = mid + 1;
                    }
                    else {
                        hi = mid;
                    }
                }
                queue.insert( queue.begin() + static_cast<std::ptrdiff_t>( lo ), Node{ c, steps } );
                seen[c] = true;
                enteredBy[c] = d;
                stepsTo[c] = steps;
            };
            const int wanted = exactNeighbour ? to : -1;
            int best = -1;
            int bestDistance = 0x280;
            int firstDirection = -1;
            push( from, u.facing == 1 ? 4 : 1, 0 );
            while ( !queue.empty() ) {
                const Node node = queue.back();
                queue.pop_back();
                if ( node.steps > u.speed ) {
                    continue;
                }
                const int c = node.cell;
                const int distance = screenDistance( cells[c].x, cells[c].y, cells[to].x, cells[to].y );
                if ( u.idSide != -1 ) {
                    const int mask = touchMask( u, c, 0, wanted );
                    if ( mask != 0xff ) {
                        for ( int d = 0; d < 8; ++d ) {
                            if ( ( mask & ( 1 << d ) ) == 0 ) {
                                firstDirection = d;
                                break;
                            }
                        }
                        best = c;
                        break;
                    }
                }
                if ( distance < bestDistance ) {
                    best = c;
                    bestDistance = distance;
                    if ( distance == 0 ) {
                        break;
                    }
                }
                const int blocked = blockedSteps( u, c );
                for ( int d = 0; d < 8; ++d ) {
                    if ( ( blocked & ( 1 << d ) ) == 0 ) {
                        push( step( u, c, d ), d, node.steps + 1 );
                    }
                }
            }
            if ( u.idSide == -1 ) {
                if ( best != to ) {
                    return 0;
                }
            }
            else if ( firstDirection < 0 ) {
                return 0;
            }
            else {
                path.push_back( firstDirection );
            }
            int c = best;
            while ( c != from && validCell( c ) ) {
                const int d = enteredBy[c] & 0xf;
                path.push_back( d );
                if ( path.size() > 0xff ) {
                    break;
                }
                c = step( u, c, opposite( d ) );
            }
            return static_cast<int>( path.size() );
        }

        // FUN_0046f280.
        int walkPath( Unit & u, const int from, int to, const bool unlimited, const bool exactNeighbour )
        {
            if ( !validCell( from ) || !validCell( to ) ) {
                return 0;
            }
            const int speed = u.speed;
            if ( unlimited ) {
                u.speed = 99;
            }
            int found = findPath( from, to, u, exactNeighbour );
            if ( found == 0 && ( u.flags & FLAG_TWO_HEX ) && !exactNeighbour ) {
                to = step( u, to, u.facing == 0 ? 4 : 1 );
                found = validCell( to ) ? findPath( from, to, u, false ) : 0;
            }
            u.speed = speed;
            return found;
        }

        // FUN_0041fd70: can a flier land on cell c (two-hex: may move the landing cell by one).
        bool canLand( const Unit & u, int & c ) const
        {
            if ( !validCell( c ) || c % 9 == 0 || c % 9 == 8 ) {
                return false;
            }
            if ( cells[c].side != -1 || cells[c].obstacle != -1 ) {
                return false;
            }
            if ( !( u.flags & FLAG_TWO_HEX ) ) {
                return true;
            }
            auto freeForMe = [this]( const int t ) {
                if ( !validCell( t ) ) {
                    return false;
                }
                const Cell & cell = cells[t];
                return ( cell.side == -1 || ( cell.side == current && cell.slot == currentSlot ) ) && cell.obstacle == -1;
            };
            const int t = step( u, c, u.facing == 0 ? 1 : 4 );
            if ( freeForMe( t ) ) {
                return true;
            }
            const int t2 = step( u, c, u.facing == 0 ? 4 : 1 );
            if ( freeForMe( t2 ) ) {
                c = t2;
                return true;
            }
            return false;
        }

        // FUN_0041ffd7: flier. Without a target (idSide -1): can it land on c. With a target: where to land next to it (dest, dir).
        bool canFly( Unit & u, int c, const bool exactNeighbour )
        {
            if ( !validCell( c ) ) {
                return false;
            }
            if ( u.idSide < 0 || u.idSide > 1 || u.idSlot < 0 || u.idSlot > 4 ) {
                if ( !canLand( u, c ) ) {
                    return false;
                }
                u.dest = c;
                return true;
            }
            const Unit & target = units[u.idSide][u.idSlot];
            int aim = exactNeighbour ? c : target.cell;
            if ( !validCell( aim ) ) {
                return false;
            }
            int mask = touchMask( u, u.cell, 0, -1 );
            while ( mask != 0xff ) {
                const int d = preferredDirection( u.cell, aim, mask );
                if ( d < 0 ) {
                    break;
                }
                int out = -1;
                if ( touches( u, u.cell, d, 0, -1, out ) ) {
                    u.dir = d;
                    u.dest = u.cell;
                    return true;
                }
                mask |= 1 << d;
            }
            auto tryAround = [&]( const int centre, int tried ) {
                while ( tried != 0x3f ) {
                    const int d = preferredDirection( centre, u.cell, tried );
                    if ( d < 0 ) {
                        break;
                    }
                    int landing = step( u, centre, d );
                    if ( validCell( landing ) && canLand( u, landing ) ) {
                        u.dest = landing;
                        if ( !( u.flags & FLAG_TWO_HEX ) ) {
                            u.dir = opposite( d );
                        }
                        else {
                            const int m = touchMask( u, u.dest, 0, -1 );
                            for ( int k = 0; k < 8; ++k ) {
                                if ( ( ~m & ( 1 << k ) ) != 0 ) {
                                    u.dir = k;
                                }
                            }
                        }
                        return true;
                    }
                    tried |= 1 << d;
                }
                return false;
            };
            int tried = 0;
            if ( ( target.flags & FLAG_TWO_HEX ) && !exactNeighbour ) {
                aim = ( target.facing == 0 ) ? aim + 1 : aim - 1;
                tried = ( target.facing == 0 ) ? 0x10 : 2;
            }
            if ( tryAround( aim, tried ) ) {
                return true;
            }
            if ( ( target.flags & FLAG_TWO_HEX ) && !exactNeighbour ) {
                aim = ( target.facing == 0 ) ? aim - 1 : aim + 1;
                tried = ( target.facing == 0 ) ? 2 : 0x10;
                int t = tried;
                while ( t != 0x3f ) {
                    const int d = preferredDirection( aim, u.cell, t );
                    if ( d < 0 ) {
                        break;
                    }
                    int landing = step( u, aim, d );
                    if ( validCell( landing ) && canLand( u, landing ) ) {
                        u.dest = landing;
                        u.dir = preferredDirection( u.dest, aim, 0 );
                        return true;
                    }
                    t |= 1 << d;
                }
            }
            return false;
        }

        // FUN_0046f3d2: can the unit get to c this turn (or next to the unit idSide / idSlot).
        bool canReach( Unit & u, const int c, const bool exactNeighbour )
        {
            if ( !validCell( c ) ) {
                return false;
            }
            if ( !( u.flags & FLAG_FLY ) ) {
                if ( walkPath( u, u.cell, c, false, exactNeighbour ) == 0 ) {
                    return false;
                }
                u.dest = c;
                return true;
            }
            return canFly( u, c, exactNeighbour );
        }

        // FUN_00407726 (the picture moving is left out): one step in direction d. Units keep the way they face.
        // FUN_00407726: one step, 6 frames of .wlk 75 ms each (note v40 4). Units do not turn round: against their facing the frames run
        // 5 -> 0 from the new cell. Up / down rows move 16 pixels a frame.
        void stepUnit( Unit & u, const int d )
        {
            const int n = step( u, u.cell, d );
            if ( !validCell( n ) ) {
                return;
            }
            const int yStep = ( d == 0 || d == 5 ) ? -0x10 : ( ( d == 2 || d == 3 ) ? 0x10 : 0 );
            const bool backward = ( d < 3 ) ? ( u.facing != 0 ) : ( u.facing == 0 );
            creatureSound( u, "move" );
            if ( !backward ) {
                for ( int f = 0; f < 6; ++f ) {
                    showFrame( u, 1, f, yStep, 0x4b );
                }
                u.cell = n;
                updateCells();
            }
            else {
                u.cell = n;
                updateCells();
                for ( int f = 5; f >= 0; --f ) {
                    showFrame( u, 1, f, -yStep, 0x4b );
                }
            }
            u.animState = 0;
            u.animFrame = 1;
            u.animStep = 0;
        }

        // FUN_00420466: a flier goes to cell c in (largest row / column difference x 6) frames of .wlk [the frame order when flying
        // against the facing: not checked].
        void flyTo( Unit & u, const int c )
        {
            if ( !validCell( c ) || !validCell( u.cell ) || c == u.cell ) {
                u.cell = c;
                updateCells();
                return;
            }
            const int n = std::max( std::abs( c % 9 - u.cell % 9 ), std::abs( c / 9 - u.cell / 9 ) );
            const int frames = std::max( n, 1 ) * 6;
            const int32_t dx = ( cells[c].x - cells[u.cell].x ) / frames;
            const int32_t dy = ( cells[c].y - cells[u.cell].y ) / frames;
            for ( int k = 0; k < frames; ++k ) {
                if ( k % 6 == 1 ) {
                    creatureSound( u, "move" ); // FUN_00420466: at frame 1, 7, 13 ...
                }
                u.offsetX = dx * k;
                u.offsetY = dy * k;
                showFrame( u, 1, k % 6, 0, 0x4b );
            }
            u.offsetX = 0;
            u.offsetY = 0;
            u.cell = c;
            updateCells();
            u.animState = 0;
            u.animFrame = 1;
            u.animStep = 0;
        }

        // Battle speed (owner 2026-11-15 16:09: a basic option, default = the original speed): percent of the original frame times.
        int speedPercent = 100;

        void showFrame( Unit & u, const int state, const int frame, const int stepY, const uint32_t ms )
        {
            u.animState = state;
            u.animFrame = frame;
            u.animStep = stepY;
            delay( ms );
        }

        // Sounds (note v107): only when the battle is shown. Returns the handle for waitSound.
        int sound( const std::string & name, const int volume = 0x7f, const int group = H1::SOUND_GROUP_EFFECT ) const
        {
            return visible ? H1::playSound( name, volume, group ) : -1;
        }

        void waitSound( const int handle ) const
        {
            if ( visible ) {
                H1::waitSound( handle, -1 );
            }
        }

        // FUN_00406b67: the creature's sounds move%02d / atksnd%02d / wince%02d / shoot%02d (shooters only) by creature number,
        // volume 0x40, group 3.
        // H1-INTENDED(D16) extension items (h1_rules.cpp 38..63): whether the hero of a side carries one.
        bool sideHas( const int side, const int h1Artifact ) const
        {
            return H1::isExtension() && side >= 0 && side < 2 && H1::heroHasArtifact( heroes[side], h1Artifact );
        }

        // Spell damage with the extension items: Everhot Lava Rock (54) / Lightning Rod (55) +50 % for the caster's fire (Fireball 0,
        // Armageddon 15, Meteor Shower 17) / lightning (Lightning Bolt 1, Storm 16) spells; Fire Cloak (52) / Lightning Helm (53) halve them
        // on the wearer's troops.
        int spellDamageOn( const Unit & target, const int damage, const int spell, const int casterSide ) const
        {
            if ( !H1::isExtension() || spell < 0 ) {
                return damage;
            }
            const bool fire = ( spell == 0 || spell == 15 || spell == 17 );
            const bool lightning = ( spell == 1 || spell == 16 );
            int d = damage;
            if ( ( fire && sideHas( casterSide, 54 ) ) || ( lightning && sideHas( casterSide, 55 ) ) ) {
                d = d * 3 / 2;
            }
            if ( ( fire && sideHas( target.side, 52 ) ) || ( lightning && sideHas( target.side, 53 ) ) ) {
                d = d / 2;
            }
            return std::max( d, 1 );
        }

        // Turn Undead's targets: the ghost; H1-INTENDED(D16) in extension games also skeletons and power liches (HoMM2 undead).
        static bool isUndead( const int creature )
        {
            return creature == CREATURE_GHOST || ( H1::isExtension() && ( creature == 29 || creature == 30 || ( creature >= 36 && creature <= 39 ) ) );
        }

        int creatureSound( const Unit & u, const char * kind ) const
        {
            if ( u.creature < 0 ) {
                return -1;
            }
            char name[32];
            std::snprintf( name, sizeof( name ), "%s%02d.82m", kind, u.creature );
            return sound( name, 0x40, H1::SOUND_GROUP_CREATURE );
        }

        // FUN_004076c5 [simplified]: a unit that was hit and lives shows frame 2 for a moment. The wince sound (FUN_0040ad9c plays it for
        // every unit hit by the blow, killed ones too).
        void wince( Unit & u )
        {
            creatureSound( u, "wince" );
            if ( u.count > 0 && !( u.flags & FLAG_DEAD ) ) {
                showFrame( u, 0, 2, 0, 0x69 );
                showFrame( u, 0, 1, 0, 0 );
            }
        }

        std::string unitName( const Unit & u ) const
        {
            if ( u.creature < 0 ) {
                return {};
            }
            return u.count < 2 ? H1::creatureNameOne( u.creature ) : H1::creatureNameMany( u.creature );
        }

        static std::string capitalised( std::string s )
        {
            if ( !s.empty() && s[0] >= 'a' && s[0] <= 'z' ) {
                s[0] = static_cast<char>( s[0] - 0x20 );
            }
            return s;
        }

        // FUN_0040a771: luck of this blow.
        void rollLuck( Unit & u )
        {
            u.luck = 0;
            if ( !hasHero[u.side] ) {
                return;
            }
            const int value = luck[u.side];
            if ( value > 0 && random.get( 1, 12 ) <= value ) {
                u.luck = 1;
                sound( "goodluck.82m" );
                setMessage( "Good luck shines on the " + unitName( u ) );
            }
            if ( value < 0 && random.get( 1, 12 ) < -value ) {
                u.luck = -1;
                sound( "badluck.82m" );
                setMessage( "Bad luck descends on the " + unitName( u ) );
            }
        }

        // FUN_0040a9ba and FUN_0040ac26: damage of attacker on target; returns the creatures killed.
        int strike( Unit & attacker, Unit & target, const bool shooting, int & damageOut )
        {
            // FUN_0040a9ba (h1_rules.cpp strikeDamage; damage state 1 = Curse, Blind, Paralyze minimum, 3 = Bless maximum, v97).
            H1::StrikeInput in;
            in.count = attacker.count;
            in.damageState = attacker.damageState;
            in.damageMin = attacker.damageMin;
            in.damageMax = attacker.damageMax;
            in.attack = attacker.attack;
            if ( H1::isExtension() && attacker.creature == CREATURE_GOBLIN ) {
                in.attack += std::min( attacker.count / 20, 4 ); // H1-INTENDED(D16): goblin swarm, +1 attack per 20, at most +4
            }
            in.luck = attacker.luck;
            in.creature = attacker.creature;
            in.shooterInMelee = ( attacker.flags & FLAG_SHOOTER ) && !shooting;
            in.targetDefense = target.defense;
            // H1-INTENDED(D16): the Golden Bow (50) takes the wall's +4 away.
            in.extraDefense = ( shooting && wallBetween( attacker, target ) && !sideHas( attacker.side, 50 ) ) ? 4 : 0;
            in.targetCount = target.count;
            in.targetHp = target.hp;
            int damage = H1::strikeDamage( in, [this]( const int low, const int high ) { return random.get( low, high ); }, halfTroops );
            if ( H1::isExtension() ) {
                // H1-INTENDED(D16): cavalry charge +5 % per cell walked before the blow (pikemen are not hurt by it); swordsmen's shields
                // take 30 % off shots.
                if ( !shooting && &attacker == chargeUnit && attacker.creature == CREATURE_CAVALRY && target.creature != CREATURE_PIKEMAN ) {
                    damage += damage * 5 * chargeCells / 100;
                }
                if ( shooting && target.creature == CREATURE_SWORDSMAN ) {
                    damage = std::max( 1, damage * 7 / 10 );
                }
            }
            damageOut = damage;
            return applyDamage( target, damage );
        }

        // FUN_0040ac26: damage into dead creatures; a unit under Blind or Paralyze (+0x52 = 2) loses its turn, the spell ends, and
        // (except Blind) it cannot retaliate this round.
        int applyDamage( Unit & target, const int damage )
        {
            const int total = damage + target.hurt;
            int killed = total / std::max( target.hp, 1 );
            target.hurt = total % std::max( target.hp, 1 );
            killed = std::min( killed, target.count );
            target.count -= killed;
            if ( target.count < 1 ) {
                target.count = 0;
                target.flags |= FLAG_DEAD;
                target.hurt = 0;
            }
            if ( target.spellKind == 2 ) {
                target.flags |= FLAG_DONE;
                if ( target.spell != SPELL_BLIND ) {
                    target.flags |= FLAG_RETALIATED;
                }
                removeSpell( target );
            }
            updateCells();
            return killed;
        }

        // FUN_0040b7aa: the spell on a unit ends.
        void removeSpell( Unit & u )
        {
            switch ( u.spell ) {
            case 5:
            case 6:
            case 7:
            case 8:
            case 10:
                u.damageState = 0;
                u.speed = u.baseSpeed;
                u.flags |= static_cast<uint32_t>( H1::creatureRow( u.creature ).flags ) & FLAG_FLY;
                break;
            case 9:
                u.defense -= 3;
                break;
            default:
                break;
            }
            u.spell = -1;
            u.spellKind = -1;
        }

        std::string blowText( const Unit & attacker, const int damage, const int killed, const Unit * target ) const
        {
            if ( halfTroops ) {
                return capitalised( unitName( attacker ) + ( attacker.count > 1 ? " destroy" : " destroys" ) + " half the enemy troops!" );
            }
            std::string s = unitName( attacker ) + ( attacker.count > 1 ? " do " : " does " ) + std::to_string( damage ) + " Damage.";
            if ( killed > 0 && target != nullptr ) {
                s += "  " + std::to_string( killed ) + " " + ( killed < 2 ? H1::creatureNameOne( target->creature ) : H1::creatureNameMany( target->creature ) )
                     + ( killed > 1 ? " perish." : " perishes." );
            }
            return capitalised( s );
        }

        // FUN_00408bf6: the hydra bites every enemy next to it (each once), no retaliation.
        void hydraAttack( Unit & u )
        {
            rollLuck( u );
            creatureSound( u, "atksnd" );
            // FUN_00408bf6: frames 3..7, the bites, then 7..3 (no frame 5 base for the hydra).
            for ( int k = 0; k < 5; ++k ) {
                showFrame( u, 0, k + 3, 0, 0x69 );
            }
            const int mask = touchMask( u, u.cell, u.spell == SPELL_BERSERK ? 2 : 1, -1 );
            bool hit[2][5] = {};
            int damageSum = 0;
            int killedSum = 0;
            for ( int d = 0; d < 8; ++d ) {
                if ( mask & ( 1 << d ) ) {
                    continue;
                }
                int base = u.cell;
                if ( ( u.flags & FLAG_TWO_HEX ) && ( ( u.facing == 1 && d > 2 ) || ( u.facing == 0 && ( d < 3 || d > 5 ) ) ) ) {
                    base = ( u.facing == 1 ) ? u.cell - 1 : u.cell + 1;
                }
                const int c = step( u, base, d );
                if ( !validCell( c ) || cells[c].side < 0 || cells[c].slot < 0 ) {
                    continue;
                }
                const int s = cells[c].side;
                const int k = cells[c].slot;
                if ( hit[s][k] ) {
                    continue;
                }
                hit[s][k] = true;
                int damage = 0;
                killedSum += strike( u, units[s][k], false, damage );
                creatureSound( units[s][k], "wince" );
                damageSum += damage;
            }
            std::string s = unitName( u ) + ( u.count > 1 ? " do " : " does " ) + std::to_string( damageSum ) + " Damage.";
            if ( killedSum > 0 ) {
                s += "  " + std::to_string( killedSum ) + ( killedSum > 1 ? " creatures perish." : " creature perishes." );
            }
            setMessage( capitalised( s ) );
            for ( int k = 4; k >= 0; --k ) {
                showFrame( u, 0, k + 3, 0, 0x69 );
            }
            showFrame( u, 0, 1, 0, 0 );
            u.luck = 0;
        }

        // Cyclops / unicorn blow on a target (FUN_00409388): 20 % chance, not on Anti-Magic or dragons, dwarves resist.
        bool trySpecial( Unit * target, const int spell, const bool unicornDwarfRoll )
        {
            if ( target == nullptr || target->spell == SPELL_ANTI_MAGIC || target->creature == CREATURE_DRAGON ) {
                return false;
            }
            if ( H1::isExtension() && target->creature == CREATURE_GARGOYLE ) {
                return false; // H1-INTENDED(D16): stone, not blinded or paralyzed
            }
            if ( target->creature == CREATURE_DWARF ) {
                if ( unicornDwarfRoll ) {
                    const int r = random.get( 0, 0x7f );
                    if ( ( std::abs( r ) & 3 ) == 1 ) {
                        return false;
                    }
                }
                else if ( random.get( 0, 4 ) == 1 ) {
                    return false;
                }
            }
            if ( ( target->flags & FLAG_DEAD ) == 0 ) {
                castSpell( spell, target->cell, true, -1 );
                return true;
            }
            return false;
        }

        // FUN_00409388: melee blow of u in direction u.dir. retaliating: this is a retaliation or a second blow.
        void melee( Unit & u, const bool retaliating )
        {
            if ( retaliating ) {
                current = 1 - current;
            }
            Unit * target = nullptr;
            Unit * behind = nullptr;
            bool special = false;
            const int savedDir = u.dir;
            if ( u.creature == CREATURE_HYDRA ) {
                hydraAttack( u );
                if ( u.spellKind == 1 && !retaliating ) {
                    removeSpell( u ); // Berserk ends after one blow
                }
            }
            else {
                const int savedFacing = u.facing;
                if ( u.dir < 3 ) {
                    u.facing = 0;
                }
                else if ( u.dir < 6 ) {
                    u.facing = 1;
                }
                rollLuck( u );
                // FUN_00409388 frames (105 ms): 3, 4, attack (6 up / 7 straight / 8 down), + 3 for creatures hitting the cell behind.
                const int attackFrame = ( u.dir == 5 || u.dir == 0 || u.dir == 6 ) ? 6 : ( ( u.dir == 3 || u.dir == 2 || u.dir == 7 ) ? 8 : 7 );
                showFrame( u, 0, 3, 0, 0x69 );
                creatureSound( u, "atksnd" ); // FUN_00409388: after frame 3
                showFrame( u, 0, 4, 0, 0x69 );
                showFrame( u, 0, attackFrame, 0, 0x69 );
                if ( u.flags & FLAG_HITS_BEHIND ) {
                    showFrame( u, 0, attackFrame + 3, 0, 0x69 );
                }
                int base = u.cell;
                if ( ( u.flags & FLAG_TWO_HEX ) && ( ( savedFacing == 1 && u.dir > 2 ) || ( savedFacing == 0 && ( u.dir < 3 || u.dir > 5 ) ) ) ) {
                    base = ( savedFacing == 1 ) ? u.cell - 1 : u.cell + 1;
                }
                const int targetCell = step( u, base, u.dir );
                int damage = 0;
                int killed = 0;
                halfTroops = false;
                if ( validCell( targetCell ) && cells[targetCell].side >= 0 && cells[targetCell].slot >= 0 ) {
                    target = &units[cells[targetCell].side][cells[targetCell].slot];
                    const int behindCell = step( u, targetCell, u.dir );
                    const bool behindHit = ( u.flags & FLAG_HITS_BEHIND ) && u.dir < 6 && validCell( behindCell ) && cells[behindCell].side >= 0
                                           && cells[behindCell].slot >= 0 && cells[targetCell].slot != cells[behindCell].slot;
                    if ( behindHit ) {
                        behind = &units[cells[behindCell].side][cells[behindCell].slot];
                    }
                    killed = strike( u, *target, false, damage );
                    if ( behind != nullptr ) {
                        int killedBehind = 0;
                        // The original keeps the second target's damage for the message, and the first target's kills.
                        killedBehind = strike( u, *behind, false, damage );
                        (void)killedBehind;
                    }
                }
                setMessage( blowText( u, damage, killed, target ) );
                halfTroops = false;
                if ( behind != nullptr ) {
                    showFrame( u, 0, attackFrame + 6, 0, 0x69 );
                }
                if ( target != nullptr ) {
                    wince( *target );
                }
                if ( behind != nullptr ) {
                    wince( *behind );
                }
                if ( u.flags & FLAG_HITS_BEHIND ) {
                    showFrame( u, 0, attackFrame + 3, 0, 0x69 );
                    showFrame( u, 0, attackFrame, 0, 0x69 );
                }
                showFrame( u, 0, 4, 0, 0x69 );
                showFrame( u, 0, 3, 0, 0x69 );
                showFrame( u, 0, 1, 0, 0 );
                if ( u.creature == CREATURE_CYCLOPS ) {
                    if ( random.get( 1, 5 ) == 3 ) {
                        special = trySpecial( target, SPELL_PARALYZE, false );
                    }
                    else if ( random.get( 1, 5 ) == 3 ) {
                        special = trySpecial( behind, SPELL_PARALYZE, false );
                    }
                }
                else if ( H1::isExtension() && u.creature == 28 ) {
                    // H1-INTENDED(D16): medusa's stone gaze (HoMM2: 20 % petrify) as HoMM1's paralysis, 1 in 5 like the cyclops.
                    if ( random.get( 1, 5 ) == 3 ) {
                        special = trySpecial( target, SPELL_PARALYZE, false );
                    }
                }
                else if ( u.creature == CREATURE_UNICORN ) {
                    if ( random.get( 1, 5 ) == 3 ) {
                        special = trySpecial( target, SPELL_BLIND, true );
                    }
                }
                else if ( H1::isExtension() && u.creature == CREATURE_MUMMY ) {
                    // H1-INTENDED(D16): mummy's curse (HoMM2: 20 %), 1 in 5 like the cyclops; the target still strikes back (HoMM2).
                    if ( random.get( 1, 5 ) == 3 && target != nullptr && target->count > 0 ) {
                        trySpecial( target, SPELL_CURSE, false );
                    }
                }
                else if ( u.creature == CREATURE_GHOST ) {
                    ghostKills[u.side] = killed;
                }
                u.luck = 0;
                u.facing = savedFacing;
                if ( u.spellKind == 1 && !retaliating ) {
                    removeSpell( u ); // Berserk ends after one blow
                }
                if ( u.creature == CREATURE_GHOST ) {
                    u.count += ghostKills[u.side];
                }
                // Retaliation.
                if ( target != nullptr && target->count > 0 && target->spell != SPELL_PARALYZE
                     && ( target->creature == CREATURE_GRIFFIN || !( target->flags & FLAG_RETALIATED ) ) && u.creature != CREATURE_ROGUE
                     && u.creature != CREATURE_SPRITE && !( H1::isExtension() && u.creature == CREATURE_VAMPIRE ) // H1-INTENDED(D16): no strike back
                     && !special && !retaliating ) {
                    target->dir = opposite( u.dir );
                    if ( target->flags & FLAG_TWO_HEX ) {
                        // [The original compares with a value the decompiler shows unclearly; taken as the attacker's cell.]
                        if ( step( *target, target->cell, target->facing == 0 ? 0 : 5 ) == u.cell ) {
                            target->dir = 6;
                        }
                        if ( step( *target, target->cell, target->facing == 0 ? 2 : 3 ) == u.cell ) {
                            target->dir = 7;
                        }
                    }
                    melee( *target, true );
                    target->flags |= FLAG_RETALIATED;
                    if ( target->creature == CREATURE_GHOST ) {
                        target->count += ghostKills[target->side];
                    }
                }
            }
            // Wolves and paladins strike twice (no retaliation on the second blow).
            if ( ( u.creature == CREATURE_WOLF || u.creature == CREATURE_PALADIN ) && target != nullptr && target->count > 0 && !retaliating
                 && u.spell != SPELL_PARALYZE && u.count > 0 ) {
                const int d = u.dir;
                u.dir = savedDir;
                melee( u, true );
                u.dir = d;
            }
            u.idSide = -1;
            if ( retaliating ) {
                current = 1 - current;
            }
        }

        // FUN_00407f77: shot at the unit idSide / idSlot. No retaliation; elves shoot twice.
        void shoot( Unit & u )
        {
            if ( u.idSide < 0 || u.idSlot < 0 ) {
                return;
            }
            Unit & target = units[u.idSide][u.idSlot];
            const int timesToShoot = ( u.creature == CREATURE_ELF ) ? 2 : 1;
            for ( int k = 0; k < timesToShoot; ++k ) {
                if ( target.count <= 0 || u.shots <= 0 ) {
                    break;
                }
                const int savedFacing = u.facing;
                // FUN_00407f77: face the target (left when it is in a column to the left, or the same column on an odd row).
                const int sc = u.cell % 9;
                const int sr = u.cell / 9;
                const int tc = target.cell % 9;
                const int tr = target.cell / 9;
                u.facing = ( sc < tc || ( ( sr & 1 ) == 0 && sc == tc ) ) ? 0 : 1;
                rollLuck( u );
                if ( !sideHas( u.side, 49 ) ) { // H1-INTENDED(D16): Ammo Cart - endless shots
                    --u.shots;
                }
                creatureSound( u, "shoot" ); // FUN_00407f77: when the shooting picture starts
                for ( int f = 1; f <= 4; ++f ) {
                    showFrame( u, 2, f, 0, 0x4b );
                }
                flyProjectile( u, target, sc, sr, tc, tr );
                int damage = 0;
                halfTroops = false;
                const int killed = strike( u, target, true, damage );
                setMessage( blowText( u, damage, killed, &target ) );
                wince( target );
                halfTroops = false;
                u.luck = 0;
                u.facing = savedFacing;
                showFrame( u, 0, 1, 0, 0 );
            }
            u.idSide = -1;
            u.idSlot = -1;
        }

        // FUN_00407f77: the missile (.atk frames 5..9 by slope, 7 level) in (largest column / row difference x 2) moves from x +/- 0x50,
        // y - 0x5a (+ table -20 / -15 / 0 / 15 / 20 by frame) to the target cell's y - 0x5a.
        int projectileFrame = -1;
        int projectileCreature = -1;
        bool projectileMirror = false;
        int32_t projectileX = 0;
        int32_t projectileY = 0;

        void flyProjectile( const Unit & u, const Unit & target, const int sc, const int sr, int tc, const int tr )
        {
            if ( target.flags & FLAG_TWO_HEX ) {
                tc += ( target.facing == 1 ) ? -1 : 1;
            }
            int colDiff = tc - sc;
            if ( colDiff < 0 ) {
                colDiff = -colDiff;
            }
            const int rowDiff = std::abs( tr - sr );
            const int n = std::max( colDiff, rowDiff );
            const int sign = ( tr < sr ) ? -1 : ( ( sr < tr ) ? 1 : 0 );
            int frame = 7;
            if ( rowDiff > 1 ) {
                frame = 7 + sign;
            }
            if ( colDiff < 4 ) {
                if ( rowDiff > 2 ) {
                    frame += sign;
                }
                if ( rowDiff == 1 ) {
                    frame += sign;
                }
            }
            static const int lift[5] = { -20, -15, 0, 15, 20 };
            const int32_t x0 = cells[u.cell].x + ( u.facing == 0 ? 0x50 : -0x50 );
            const int32_t y0 = cells[u.cell].y - 0x5a + lift[std::clamp( frame - 5, 0, 4 )];
            const int targetCell = tr * 9 + std::clamp( tc, 0, 8 );
            const int32_t x1 = cells[targetCell].x;
            const int32_t y1 = cells[tr * 9].y - 0x5a;
            const int stepsCount = std::max( n * 2, 1 );
            projectileCreature = u.creature;
            projectileMirror = ( u.facing == 1 );
            projectileFrame = frame;
            for ( int k = 0; k < stepsCount; ++k ) {
                projectileX = x0 + ( x1 - x0 ) * k / stepsCount;
                projectileY = y0 + ( y1 - y0 ) * k / stepsCount;
                delay( 0x1e );
            }
            projectileFrame = -1;
        }

        // FUN_0040a5a8: walk (or fly) next to the target and strike.
        void walkAndStrike( Unit & u, const int c )
        {
            if ( !( u.flags & FLAG_FLY ) ) {
                if ( !( u.flags & FLAG_HITS_BEHIND ) || u.dest != u.cell ) {
                    const int found = walkPath( u, u.cell, c, true, false );
                    if ( found == 0 ) {
                        return;
                    }
                    if ( found == 1 ) {
                        u.dir = path[0];
                        melee( u, false );
                        return;
                    }
                    const std::vector<int> route = path;
                    int index = found;
                    int walked = 0;
                    do {
                        --index;
                        if ( index == 0 ) {
                            u.dir = route[0];
                            chargeUnit = &u;
                            chargeCells = walked;
                            melee( u, false );
                            chargeUnit = nullptr;
                            chargeCells = 0;
                            return;
                        }
                        stepUnit( u, route[index] );
                        ++walked;
                    } while ( walked < u.speed || index == 1 );
                    return;
                }
                melee( u, false );
                return;
            }
            if ( u.cell != c ) {
                flyTo( u, c );
            }
            melee( u, false );
        }

        // FUN_0040a485: walk to an empty cell (at most speed steps of the way).
        void walkTo( Unit & u, const int c )
        {
            u.idSide = -1;
            u.idSlot = -1;
            const int found = walkPath( u, u.cell, c, true, false );
            if ( found == 0 ) {
                return;
            }
            const std::vector<int> route = path;
            int walked = 0;
            for ( int index = found - 1; index >= 0; --index ) {
                stepUnit( u, route[index] );
                ++walked;
                if ( walked >= u.speed ) {
                    break;
                }
            }
            lastWalked = walked;
        }

        // FUN_0040ba3a: move to cell c, or attack what is there. moveOnly: give up when the cell holds another unit.
        void moveOrAttack( Unit & u, const int c, const bool moveOnly )
        {
            u.idSide = -1;
            u.idSlot = -1;
            if ( !validCell( c ) ) {
                return;
            }
            const Cell & cell = cells[c];
            if ( cell.side == -1 || ( cell.side == current && cell.slot == currentSlot ) ) {
                if ( !( u.flags & FLAG_FLY ) ) {
                    walkTo( u, c );
                }
                else {
                    u.dest = c;
                    if ( !canFly( u, u.dest, false ) ) {
                        return;
                    }
                    flyTo( u, u.dest );
                }
                return;
            }
            if ( moveOnly ) {
                return;
            }
            u.idSide = cell.side;
            u.idSlot = cell.slot;
            u.dest = c;
            const int nextTo = touchMask( u, u.cell, 0, -1 );
            if ( ( u.flags & FLAG_FLY ) && nextTo == 0xff && u.dest != u.cell && !canFly( u, u.dest, false ) ) {
                return;
            }
            const int enemiesNear = touchMask( u, u.cell, u.spell == SPELL_BERSERK ? 2 : 1, -1 );
            if ( enemiesNear == 0xff && u.shots > 0 ) {
                shoot( u );
            }
            else if ( nextTo == 0xff ) {
                walkAndStrike( u, u.dest );
            }
            else {
                for ( int d = 0; d < 8; ++d ) {
                    if ( d >= 6 && !( u.flags & FLAG_TWO_HEX ) ) {
                        continue;
                    }
                    int base = u.cell;
                    if ( ( u.flags & FLAG_TWO_HEX ) && u.facing == 0 && d < 3 ) {
                        base += 1;
                    }
                    if ( ( u.flags & FLAG_TWO_HEX ) && u.facing == 1 && d > 2 && d < 6 ) {
                        base -= 1;
                    }
                    if ( d > 5 ) {
                        base += ( u.facing == 0 ) ? 1 : -1;
                    }
                    const int n = step( u, base, d );
                    if ( validCell( n ) && cells[n].side == u.idSide && cells[n].slot == u.idSlot ) {
                        u.dir = d;
                    }
                }
                melee( u, false );
            }
            u.idSide = -1;
            u.idSlot = -1;
        }

        // FUN_0046c895: has this side won (the other side surrendered, fled or has no living unit).
        bool hasWon( const int side ) const
        {
            const int other = 1 - side;
            if ( surrendered[other] || fled[other] ) {
                return true;
            }
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = units[other][k];
                if ( u.exists && !( u.flags & FLAG_DEAD ) ) {
                    return false;
                }
            }
            return true;
        }

        // FUN_0044b675: is the battle over; winner -1 for both sides gone (+0x7d2 = 0xff).
        bool isOver( int & winner ) const
        {
            const bool a = hasWon( current );
            const bool b = hasWon( 1 - current );
            if ( a ) {
                winner = b ? -1 : current;
                return true;
            }
            if ( b ) {
                winner = 1 - current;
                return true;
            }
            return false;
        }

        // FUN_0046c4c6: bad morale freezes the unit.
        bool badMorale( const int side, const int slot )
        {
            Unit & u = units[side][slot];
            const int m = unitMorale( side, u );
            if ( m < 0 && random.get( 1, 12 ) <= -m ) {
                // +0x2b8 of the side = DAT_004a9b60 of its player: 1 for a human player (note v102). Computer units escape 1 time in 4.
                if ( !human[side] && random.get( 1, 4 ) == 1 ) {
                    return false;
                }
                const int handle = sound( "badmrle.82m" );
                setMessage( std::string( "Low morale causes the " ) + unitName( u ) + " to freeze in panic." );
                u.flags |= FLAG_DONE;
                delay( 500 );
                waitSound( handle ); // FUN_0046c4c6 waits for the sound
                return true;
            }
            return false;
        }

        // FUN_0046c2ed: good morale gives one more turn (not twice in a row).
        void goodMorale( const int side, const int slot )
        {
            if ( lastGoodMorale ) {
                lastGoodMorale = false;
                return;
            }
            Unit & u = units[side][slot];
            if ( u.count == 0 ) {
                return;
            }
            const int m = unitMorale( side, u );
            if ( m > 0 && random.get( 1, 24 ) <= m ) {
                lastGoodMorale = true;
                const int handle = sound( "goodmrle.82m" );
                setMessage( std::string( "High morale enables the " ) + unitName( u ) + " to attack again." );
                u.flags &= ~FLAG_DONE;
                u.flags |= FLAG_MORALE_TURN;
                delay( 500 );
                waitSound( handle ); // FUN_0046c2ed waits for the sound
            }
        }

        // FUN_0046c68c: next unit to act. checkMorale: bad morale may freeze the unit chosen.
        bool selectNext( const bool checkMorale )
        {
            int side = current;
            for ( int pass = 0; pass <= 4; ++pass ) {
                for ( int k = 0; k < 2; ++k ) {
                    side ^= 1;
                    int slot = 0;
                    for ( ; slot < 5; ++slot ) {
                        const Unit & u = units[side][slot];
                        bool skip = !u.exists || ( u.flags & ( FLAG_DEAD | FLAG_DONE | FLAG_WAITING ) ) || u.spell == SPELL_PARALYZE || u.spell == SPELL_BLIND
                                    || ( u.speed != speedLevel && !( u.flags & FLAG_MORALE_TURN ) );
                        if ( !skip && pass == 0 && !( u.flags & FLAG_MORALE_TURN ) ) {
                            skip = true;
                        }
                        if ( !skip && checkMorale && badMorale( side, slot ) ) {
                            skip = true;
                        }
                        if ( !skip ) {
                            break;
                        }
                    }
                    if ( slot != 5 ) {
                        current = side;
                        currentSlot = slot;
                        return true;
                    }
                }
                if ( pass != 0 ) {
                    --speedLevel;
                    if ( speedLevel == 0 ) {
                        speedLevel = 4;
                    }
                }
            }
            return false;
        }

        // H1-INTENDED(D16) extension "Wait": when every other unit has acted, the waiting ones act, slowest first (as Heroes III), the
        // side that did not act last first on a tie.
        bool selectWaiting()
        {
            int bestSide = -1;
            int bestSlot = -1;
            for ( int k = 0; k < 2; ++k ) {
                const int side = current ^ 1 ^ k;
                for ( int slot = 0; slot < 5; ++slot ) {
                    const Unit & u = units[side][slot];
                    if ( !u.exists || !( u.flags & FLAG_WAITING ) || ( u.flags & ( FLAG_DEAD | FLAG_DONE ) ) || u.count <= 0 || u.spell == SPELL_PARALYZE
                         || u.spell == SPELL_BLIND ) {
                        continue;
                    }
                    if ( bestSide < 0 || u.speed < units[bestSide][bestSlot].speed ) {
                        bestSide = side;
                        bestSlot = slot;
                    }
                }
            }
            if ( bestSide < 0 ) {
                return false;
            }
            units[bestSide][bestSlot].flags &= ~FLAG_WAITING;
            current = bestSide;
            currentSlot = bestSlot;
            return true;
        }

        // H1-INTENDED(D16) extension: ogres, the first time they get to act in a battle, give a war cry: one stack of their side
        // (chosen at random, maybe themselves) gets +3 attack for the rest of the battle.
        void ogreWarCry()
        {
            if ( !H1::isExtension() ) {
                return;
            }
            Unit & u = acting();
            if ( !u.exists || u.count <= 0 || u.creature != CREATURE_OGRE || ( u.flags & FLAG_RAGED ) ) {
                return;
            }
            u.flags |= FLAG_RAGED;
            std::vector<int> slots;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & a = units[current][k];
                if ( a.exists && a.count > 0 && !( a.flags & FLAG_DEAD ) ) {
                    slots.push_back( k );
                }
            }
            if ( slots.empty() ) {
                return;
            }
            Unit & t = units[current][slots[static_cast<size_t>( random.get( 0, static_cast<int>( slots.size() ) - 1 ) )]];
            t.attack += 3;
            setMessage( "The Ogres' war cry enrages the " + unitName( t ) + " (+3 Attack)." );
            if ( visible ) {
                redraw();
                delay( 700 );
            }
        }

        // FUN_0044b53c: new round (also: each side may cast one spell again).
        void newRound()
        {
            for ( int s = 0; s < 2; ++s ) {
                for ( int k = 0; k < 5; ++k ) {
                    Unit & u = units[s][k];
                    if ( !u.exists || u.count <= 0 ) {
                        continue;
                    }
                    u.flags &= 0x1f | FLAG_RAGED; // (FLAG_RAGED: extension mode only, never set in the original rules)
                    if ( u.creature == CREATURE_TROLL ) {
                        u.hurt = 0;
                    }
                    if ( u.spellRounds > 0 ) {
                        --u.spellRounds;
                        if ( u.spellRounds == 0 ) {
                            removeSpell( u );
                        }
                    }
                }
            }
            speedLevel = 4;
            castCast[0] = false;
            castCast[1] = false;
            catapultLeft[0] = catapultShots[0];
            catapultLeft[1] = catapultShots[1];
            keepLeft[0] = 1;
            keepLeft[1] = 1;
        }

        // FUN_0043de80: does the computer flee now (note v94). [Castle battles: the defender never flees - always false for now.]
        bool computerRetreats()
        {
            const int me = current;
            const Heroes * hero = heroes[me];
            if ( hero == nullptr || ( me == 0 && castleDefender ) ) {
                return false;
            }
            if ( hero->GetKingdom().GetHeroes().empty() ) { // player +0x262: number of heroes
                return false;
            }
            int strength[2] = { 0, 0 };
            int artifacts[2] = { 0, 0 };
            for ( int s = 0; s < 2; ++s ) {
                int creatures[5];
                int counts[5];
                for ( int k = 0; k < 5; ++k ) {
                    const Unit & u = units[s][k];
                    if ( !u.exists || u.count < 1 ) {
                        creatures[k] = -1;
                        counts[k] = 0;
                        continue;
                    }
                    creatures[k] = u.creature;
                    counts[k] = ( u.flags & FLAG_DONE ) ? u.count : static_cast<int>( u.count * 1.2 );
                }
                strength[s] = armyStrength( creatures, counts, heroes[s], morale[s], luck[s] );
                if ( heroes[s] != nullptr ) {
                    for ( const Artifact & art : heroes[s]->GetBagArtifacts() ) {
                        const int a = art.isValid() ? H1::h1ArtifactFromEngine( art.GetID() ) : -1;
                        if ( a >= 0 && a < 37 ) {
                            artifacts[s] += artifactWorth[a];
                        }
                    }
                }
            }
            strength[1 - me] = static_cast<int>( strength[1 - me] * 1.1 );
            if ( artifacts[me] < 1000 ) {
                return false;
            }
            float limit = 0.16f;
            if ( artifacts[me] > 10000 ) {
                limit = static_cast<float>( limit + 0.06 );
            }
            else if ( artifacts[me] > 5000 ) {
                limit = static_cast<float>( limit + 0.05 );
            }
            else if ( artifacts[me] > 0 ) {
                limit = static_cast<float>( limit + 0.04 );
            }
            const int mine = strength[me];
            if ( mine > 40000 ) {
                limit = static_cast<float>( limit - static_cast<double>( mine / 20000 ) );
            }
            else if ( mine > 30000 ) {
                limit = static_cast<float>( limit - 0.08 );
            }
            else if ( mine > 15000 ) {
                limit = static_cast<float>( limit - 0.06 );
            }
            else if ( mine > 5000 ) {
                limit = static_cast<float>( limit - 0.04 );
            }
            else if ( mine > 2500 ) {
                limit = static_cast<float>( limit - 0.02 );
            }
            // Hero +0x29 (experience) / 200000, at most 0.03.
            float experiencePart = static_cast<float>( static_cast<int>( hero->GetExperience() / 200000 ) );
            if ( !( experiencePart <= 0.03 ) ) {
                experiencePart = 0.03f;
            }
            limit = limit + experiencePart;
            if ( me == 1 ) {
                limit = static_cast<float>( limit - 0.06 );
            }
            // Player +0x21e: computer level (1..4; 4 for a human player in Auto Combat [unconfirmed]).
            const int level = human[me] ? 4 : H1::computerLevel( hero->GetKingdom() );
            limit = static_cast<float>( limit - ( 4 - level ) * 0.03 );
            const double ratio = static_cast<double>( mine ) / static_cast<double>( strength[0] + strength[1] );
            return ratio < limit;
        }

        // ---- Spells (notes v97, v98) ----

        int actionSpell = -1; // DAT_004c2c78 for action 1
        int actionCell2 = -1; // DAT_004c2c80 (Teleport destination)

        int heroPower( const int side ) const
        {
            return heroes[side] != nullptr ? heroes[side]->GetPower() : 0;
        }

        // FUN_0040f298 / FUN_0040f69f: sides 0, 1 or 2 (both); badOnly (Cure): only Slow, Blind, Curse, Berserk, Paralyze end.
        void dispel( const int sides, const bool badOnly )
        {
            const int from = ( sides == 2 ) ? 0 : sides;
            const int to = ( sides == 2 ) ? 1 : sides;
            for ( int s = from; s <= to; ++s ) {
                for ( int k = 0; k < 5; ++k ) {
                    Unit & u = units[s][k];
                    if ( !u.exists || u.spell == SPELL_ANTI_MAGIC || u.spell == 13 || u.creature == CREATURE_DRAGON || u.spell < 0 ) {
                        continue;
                    }
                    if ( sides == 2 && sideHas( s, 61 ) ) {
                        continue; // H1-INTENDED(D16): Wand of Negation - the enemy's Dispel Magic leaves this side's spells
                    }
                    if ( !badOnly || u.spell == 6 || u.spell == 7 || u.spell == 10 || u.spell == 14 || u.spell == 18 ) {
                        removeSpell( u );
                    }
                }
            }
        }

        // FUN_0040f7cd / FUN_0040fbff / FUN_00410038 / FUN_0041032b: damage to the units on the cells (each unit once); dragons and
        // Anti-Magic units are not hit, dwarves resist when (0..127) & 3 == 1.
        void areaDamage( const std::vector<int> & area, const int damage, const std::string & text, const bool textOnlyWhenHit, const int spell = -1 )
        {
            bool hit[2][5] = {};
            bool any = false;
            for ( const int c : area ) {
                if ( !validCell( c ) || cells[c].side < 0 || cells[c].slot < 0 ) {
                    continue;
                }
                Unit & u = units[cells[c].side][cells[c].slot];
                if ( u.creature == CREATURE_DRAGON || u.spell == SPELL_ANTI_MAGIC ) {
                    continue;
                }
                if ( u.creature == CREATURE_DWARF && ( std::abs( random.get( 0, 0x7f ) ) & 3 ) == 1 ) {
                    continue;
                }
                if ( hit[cells[c].side][cells[c].slot] ) {
                    continue;
                }
                hit[cells[c].side][cells[c].slot] = true;
                applyDamage( u, spellDamageOn( u, damage, spell, current ) ); // H1-INTENDED(D16): items
                any = true;
            }
            if ( any || !textOnlyWhenHit ) {
                setMessage( text );
            }
        }

        std::vector<int> allCells() const
        {
            std::vector<int> area;
            for ( int c = 0; c < 45; ++c ) {
                area.push_back( c );
            }
            return area;
        }

        // FUN_0040e464: cast spell on cell (second cell: Teleport). byCreature: a creature's blow (no book, length 3).
        void castSpell( const int spell, const int cell, const bool byCreature, const int cell2 )
        {
            const int side = current;
            Unit * target = nullptr;
            const bool noTarget = ( spell == 0 || spell == 17 || spell == 16 || spell == 15 || spell == 3 || spell == 13 );
            if ( !noTarget && validCell( cell ) && cells[cell].side >= 0 && cells[cell].slot >= 0 ) {
                target = &units[cells[cell].side][cells[cell].slot];
            }
            if ( !byCreature && heroes[side] != nullptr ) {
                H1::useSpell( *const_cast<Heroes *>( heroes[side] ), spell );
            }
            if ( !byCreature ) {
                castCast[side] = true;
            }
            const bool resistible = !( spell == 0 || spell == 2 || spell == 3 || spell == 4 || spell == 5 || spell == 7 || spell == 8 || spell == 9 || spell == 12
                                       || spell == 15 || spell == 16 || spell == 17 );
            if ( H1::isExtension() && target != nullptr && target->creature == CREATURE_GARGOYLE && ( spell == SPELL_BLIND || spell == SPELL_PARALYZE ) ) {
                // H1-INTENDED(D16): gargoyles are stone.
                const int handle = sound( "rsbryfzl.82m" );
                setMessage( "Gargoyles are made of stone and are not affected!" );
                waitSound( handle );
                return;
            }
            if ( target != nullptr && H1::isExtension() ) {
                // H1-INTENDED(D16): pendants - Holy (56) Curse, Serenity (57) Berzerker, Seeing-eye (58) Blind, Kinetic (59) Paralyze,
                // Death (60) Turn Undead.
                const int pendant = ( spell == 10 ) ? 56 : ( spell == 14 ) ? 57 : ( spell == 7 ) ? 58 : ( spell == 18 ) ? 59 : ( spell == 11 ) ? 60 : -1;
                if ( pendant >= 0 && sideHas( target->side, pendant ) ) {
                    const int handle = sound( "rsbryfzl.82m" );
                    setMessage( std::string( "The " ) + H1::artifactName( pendant ) + " protects the " + unitName( *target ) + "!" );
                    waitSound( handle );
                    return;
                }
            }
            if ( resistible && target != nullptr ) {
                if ( target->creature == CREATURE_DRAGON ) {
                    const int handle = sound( "rsbryfzl.82m" );
                    setMessage( "Dragons are not affected by magic!" );
                    waitSound( handle );
                    return;
                }
                if ( target->creature == CREATURE_DWARF && random.get( 0, 4 ) == 1 ) {
                    const int handle = sound( "rsbryfzl.82m" );
                    setMessage( "The Dwarves' magic resistance canceled the spell!" );
                    waitSound( handle );
                    return;
                }
            }
            // FUN_0040e464: spell%02d by spell number, played before the effect and waited for at the end.
            char spellSoundName[32];
            std::snprintf( spellSoundName, sizeof( spellSoundName ), "spell%02d.82m", spell );
            int spellSound = sound( spellSoundName );
            const int power = heroPower( side );
            auto setOn = [this]( Unit & u, const int s, const int kind ) {
                removeSpell( u );
                u.spell = s;
                u.spellKind = kind;
            };
            switch ( spell ) {
            case 0: {
                std::vector<int> area{ cell };
                for ( int d = 0; d < 6; ++d ) {
                    area.push_back( neighbour( cell, d ) );
                }
                areaDamage( area, power * 10, "The fireball does " + std::to_string( power * 10 ) + " damage.", true, 0 );
                break;
            }
            case 1:
                if ( target != nullptr ) {
                    const int boltDamage = spellDamageOn( *target, power * 25, 1, side ); // H1-INTENDED(D16): items
                    setMessage( "The lightning bolt does " + std::to_string( boltDamage ) + " damage to the " + unitName( *target ) + "." );
                    applyDamage( *target, boltDamage );
                }
                break;
            case 2:
                if ( target != nullptr && validCell( cell2 ) ) {
                    int head = cell2;
                    auto blocked = [&]( const int c ) {
                        return c == -1 || ( cells[c].side != -1 && ( cells[c].side != target->side || cells[c].slot != target->slot ) ) || cells[c].obstacle != -1;
                    };
                    if ( target->flags & FLAG_TWO_HEX ) {
                        if ( target->facing == 0 && blocked( step( *target, cell2, 1 ) ) ) {
                            head = cell2 - 1;
                        }
                        if ( target->facing == 1 && blocked( step( *target, cell2, 4 ) ) ) {
                            head = head + 1;
                        }
                    }
                    // FUN_0040e464 Teleport: wait for the spell sound, then telein.82m as the unit appears.
                    waitSound( spellSound );
                    spellSound = sound( "telein.82m" );
                    target->cell = head;
                    updateCells();
                }
                break;
            case 3:
                dispel( side, true );
                break;
            case 4:
                if ( target != nullptr ) {
                    const int before = target->count;
                    // H1-INTENDED(D16): the Ankh (62) doubles what Resurrect raises.
                    const int raise = power * 0x32 * ( sideHas( side, 62 ) ? 2 : 1 );
                    target->count = std::min( target->count + raise / std::max( target->hp, 1 ), target->startCount );
                    const int risen = target->count - before;
                    setMessage( std::to_string( risen ) + " " + ( risen < 2 ? H1::creatureNameOne( target->creature ) : H1::creatureNameMany( target->creature ) )
                                + ( risen < 2 ? " rises from the dead!" : " rise from the dead!" ) );
                }
                break;
            case 5:
                if ( target != nullptr ) {
                    speedLevel = 4;
                    setOn( *target, 5, 3 );
                    target->speed = 4;
                }
                break;
            case 6:
                if ( target != nullptr ) {
                    setOn( *target, 6, 3 );
                    target->speed = 1;
                    target->flags &= ~static_cast<uint32_t>( FLAG_FLY );
                }
                break;
            case 7:
                if ( target != nullptr ) {
                    setOn( *target, 7, 2 );
                    target->speed = 0;
                    target->damageState = 1;
                }
                break;
            case 8:
                if ( target != nullptr ) {
                    setOn( *target, 8, 3 );
                    target->damageState = 3;
                }
                break;
            case 9:
                if ( target != nullptr ) {
                    setOn( *target, 9, 3 );
                    target->defense += 3;
                }
                break;
            case 10:
                if ( target != nullptr ) {
                    setOn( *target, 10, 3 );
                    target->damageState = 1;
                }
                break;
            case 11:
                if ( target != nullptr ) {
                    target->count = 0;
                    target->flags |= FLAG_DEAD;
                    updateCells();
                }
                break;
            case 12:
                if ( target != nullptr ) {
                    setOn( *target, 12, 3 );
                }
                break;
            case 13:
                dispel( 2, false );
                break;
            case 14:
                if ( target != nullptr ) {
                    setOn( *target, 14, 1 );
                }
                break;
            case 15:
                areaDamage( allCells(), power * 50, "The armaggedon does " + std::to_string( power * 50 ) + " damage.", false, 15 );
                break;
            case 16:
                areaDamage( allCells(), power * 25, "The elemental storm does " + std::to_string( power * 25 ) + " damage.", false, 16 );
                break;
            case 17: {
                std::vector<int> area{ cell };
                for ( int d = 0; d < 6; ++d ) {
                    area.push_back( neighbour( cell, d ) );
                }
                areaDamage( area, power * 25, "The meteor shower does " + std::to_string( power * 25 ) + " damage.", true, 17 );
                break;
            }
            case 18:
                if ( target != nullptr ) {
                    setOn( *target, 18, 2 );
                    target->damageState = 1;
                }
                break;
            default:
                break;
            }
            if ( target != nullptr && target->spellKind >= 0 && target->spell == spell ) {
                target->spellRounds = byCreature ? 3 : power;
            }
            delay( 400 );
            waitSound( spellSound );
        }

        // FUN_0040e04c: may spell be cast on cell (teleportTarget: the second step of Teleport, for the unit on cell teleportFrom).
        bool validSpellTarget( const int spell, const int cell, const int teleportFrom )
        {
            if ( !validCell( cell ) ) {
                return false;
            }
            const Unit * u = nullptr;
            if ( spell != 0 && spell != 17 && cells[cell].side != -1 && cells[cell].slot >= 0 ) {
                u = &units[cells[cell].side][cells[cell].slot];
                if ( u->spell == SPELL_ANTI_MAGIC || u->spell == 13 || u->creature == CREATURE_DRAGON ) {
                    return false;
                }
            }
            switch ( spell ) {
            case 0:
            case 17:
                return cell % 9 != 0 && cell % 9 != 8;
            case 1:
            case 6:
            case 7:
            case 10:
            case 14:
            case 18:
                return cells[cell].side == 1 - current;
            case 2:
                if ( teleportFrom >= 0 ) {
                    if ( cell == teleportFrom || cells[teleportFrom].side < 0 ) {
                        return false;
                    }
                    int c = cell;
                    return canLand( units[cells[teleportFrom].side][cells[teleportFrom].slot], c );
                }
                return cells[cell].side == current;
            case 3:
            case 4:
            case 5:
            case 8:
            case 9:
            case 12:
                return cells[cell].side == current;
            case 11:
                return u != nullptr && isUndead( u->creature );
            default:
                return true;
            }
        }

        // FUN_00465251: the computer's worth of a spell on a unit (w = creature worth x number).
        int spellEffectWorth( const Unit & u, const int spell ) const
        {
            const int w = H1::creatureWorth( u.creature ) * u.count;
            int value = 0;
            switch ( spell ) {
            case 5:
                if ( !( u.flags & FLAG_FLY ) && !( u.flags & FLAG_SHOOTER ) ) {
                    if ( u.speed < 2 ) {
                        value = static_cast<int>( w * static_cast<double>( 0.33f ) );
                    }
                    else if ( u.speed < 3 ) {
                        value = static_cast<int>( w * static_cast<double>( 0.33f ) / 2.0 );
                    }
                }
                break;
            case 6:
                if ( !( u.flags & FLAG_SHOOTER ) ) {
                    if ( u.flags & FLAG_FLY ) {
                        value = static_cast<int>( w * static_cast<double>( -0.11f ) * 3.0 );
                    }
                    else {
                        value = static_cast<int>( static_cast<double>( u.speed - 1 ) * w * static_cast<double>( -0.11f ) );
                    }
                }
                break;
            case 7:
                value = static_cast<int>( w * static_cast<double>( -0.6f ) );
                break;
            case 8:
                value = static_cast<int>( w * static_cast<double>( 0.18f ) );
                break;
            case 9:
                value = static_cast<int>( w * static_cast<double>( 0.24f ) );
                break;
            case 10:
                value = static_cast<int>( w * static_cast<double>( -0.18f ) );
                break;
            case 12:
                value = static_cast<int>( w * static_cast<double>( 0.15f ) );
                break;
            case 14:
                value = static_cast<int>( w * static_cast<double>( -0.6f ) );
                break;
            case 18:
                value = static_cast<int>( w * static_cast<double>( -0.7f ) );
                break;
            default:
                break;
            }
            if ( u.creature == CREATURE_DRAGON || u.spell == SPELL_ANTI_MAGIC ) {
                return 0;
            }
            if ( u.creature == CREATURE_DWARF && value < 0 ) {
                value = static_cast<int>( value * 0.75 );
            }
            return value;
        }

        // FUN_0046565b: worth of Cure (side, badOnly) / Dispel (sides 2).
        int spellsOnSidesWorth( const int sides, const bool badOnly ) const
        {
            int total = 0;
            int s = ( sides == 2 ) ? current : sides;
            while ( true ) {
                int good = 0;
                int bad = 0;
                for ( int k = 0; k < 5; ++k ) {
                    const Unit & u = units[s][k];
                    if ( !u.exists || u.count <= 0 ) {
                        continue;
                    }
                    if ( u.spell == 5 || u.spell == 8 || u.spell == 9 || u.spell == 12 ) {
                        good += spellEffectWorth( u, u.spell );
                    }
                    else if ( u.spell == 6 || u.spell == 7 || u.spell == 10 || u.spell == 14 || u.spell == 18 ) {
                        bad -= spellEffectWorth( u, u.spell );
                    }
                }
                if ( badOnly ) {
                    good = 0;
                }
                if ( sides == 2 ) {
                    total += ( s == current ) ? ( bad - good ) : ( good - bad );
                }
                else {
                    total += bad;
                }
                if ( sides == 2 && s == current ) {
                    s = 1 - current;
                    continue;
                }
                break;
            }
            return total;
        }

        // FUN_004659c7: the computer's worth of a damage spell centred on cell.
        int damageSpellWorth( const int spell, const int multiplier, const int cell ) const
        {
            const int damage = heroPower( current ) * multiplier;
            std::vector<int> area;
            if ( spell == 0 || spell == 17 ) {
                for ( int d = 0; d < 6; ++d ) {
                    area.push_back( neighbour( cell, d ) ); // the centre itself is not counted (original)
                }
            }
            else if ( spell == 1 ) {
                area.push_back( cell );
            }
            else {
                for ( int c = 1; c < 0x2c; c = ( ( c + 2 ) % 9 == 0 ) ? c + 3 : c + 1 ) {
                    area.push_back( c );
                }
            }
            bool seen[2][5] = {};
            int wiped[2] = { 0, 0 };
            int lost[2] = { 0, 0 };
            int lostWorth[2] = { 0, 0 };
            for ( const int c : area ) {
                if ( !validCell( c ) || cells[c].side < 0 || cells[c].slot < 0 ) {
                    continue;
                }
                const int s = cells[c].side;
                const Unit & u = units[s][cells[c].slot];
                if ( u.hp == 0 || seen[s][cells[c].slot] ) {
                    continue;
                }
                seen[s][cells[c].slot] = true;
                if ( u.creature == CREATURE_DRAGON || u.spell == SPELL_ANTI_MAGIC ) {
                    continue;
                }
                const int d = ( u.creature == CREATURE_DWARF ) ? static_cast<int>( damage * 0.75 ) : damage;
                int killed = d / u.hp;
                int rest = d % u.hp;
                if ( u.hp <= u.hurt + rest ) {
                    ++killed;
                    rest -= u.hp - u.hurt;
                }
                if ( u.count <= killed ) {
                    killed = u.count;
                    rest = 0;
                    ++wiped[s];
                }
                const int worthOne = H1::creatureWorth( u.creature );
                lost[s] = static_cast<int>( ( rest * 0.75 + static_cast<double>( u.hp * killed ) ) * worthOne / u.hp + lost[s] );
                lostWorth[s] += worthOne * u.hp * killed / u.hp;
            }
            int stacks[2] = { 0, 0 };
            for ( int s = 0; s < 2; ++s ) {
                for ( int k = 0; k < 5; ++k ) {
                    if ( units[s][k].exists ) {
                        ++stacks[s];
                    }
                }
            }
            const int me = current;
            if ( wiped[0] < stacks[0] && wiped[1] < stacks[1] ) {
                return lost[1 - me] - lost[me];
            }
            if ( lostWorth[me] < 1 ) {
                return 100000000 - spellWorth[spell];
            }
            return lostWorth[1 - me] - lostWorth[me];
        }

        // FUN_00464d9d: best cell and worth for one spell.
        int spellWorthAt( const int spell, int & bestCell )
        {
            int kind = 0;
            int targetSide = 0;
            switch ( spell ) {
            case 0:
            case 17:
                kind = 1;
                break;
            case 1:
            case 6:
            case 7:
            case 10:
            case 11:
            case 14:
            case 18:
                kind = 3;
                targetSide = 1 - current;
                break;
            case 2:
            case 4:
            case 5:
            case 8:
            case 9:
            case 12:
                kind = 2;
                targetSide = current;
                break;
            case 3:
            case 13:
            case 15:
            case 16:
                kind = 0;
                break;
            default:
                return 0;
            }
            auto nextCell = []( int c ) { return ( ( c + 2 ) % 9 == 0 ) ? c + 3 : c + 1; };
            auto nextOfSide = [&]( int c, int & out ) {
                while ( c <= 0x2b ) {
                    if ( cells[c].side == targetSide ) {
                        out = c;
                        return false;
                    }
                    c = nextCell( c );
                }
                out = -1;
                return true;
            };
            int best = 0;
            int c = 1;
            bool done = false;
            if ( kind == 2 || kind == 3 ) {
                done = nextOfSide( 1, c );
            }
            const int shift = ( heroPower( current ) == 1 ) ? 2 : ( heroPower( current ) == 2 ? 1 : 0 );
            while ( !done ) {
                const Unit * u = ( validCell( c ) && cells[c].side >= 0 && cells[c].slot >= 0 ) ? &units[cells[c].side][cells[c].slot] : nullptr;
                int value = 0;
                switch ( spell ) {
                case 0:
                    value = damageSpellWorth( spell, 10, c );
                    break;
                case 1:
                case 16:
                case 17:
                    value = damageSpellWorth( spell, 0x19, c );
                    break;
                case 15:
                    value = damageSpellWorth( spell, 0x32, c );
                    break;
                case 3:
                    value = spellsOnSidesWorth( current, true );
                    break;
                case 13:
                    value = spellsOnSidesWorth( 2, false );
                    break;
                case 4:
                    if ( u != nullptr && u->creature != CREATURE_DRAGON && u->spell != SPELL_ANTI_MAGIC ) {
                        int n = heroPower( current ) * 0x32 / std::max( u->hp, 1 );
                        if ( u->startCount < u->count + n ) {
                            n = u->startCount - u->count;
                        }
                        value = H1::creatureWorth( u->creature ) * n;
                    }
                    break;
                case 5:
                case 8:
                case 9:
                case 12:
                    if ( u != nullptr ) {
                        if ( spell == 12 && heroes[1 - current] == nullptr ) {
                            value = 0;
                        }
                        else {
                            value = spellEffectWorth( *u, spell ) >> shift;
                        }
                        if ( u->spell >= 0 ) {
                            value -= spellEffectWorth( *u, u->spell );
                        }
                    }
                    break;
                case 6:
                case 7:
                case 10:
                case 14:
                case 18:
                    if ( u != nullptr ) {
                        value = -( spellEffectWorth( *u, spell ) >> shift );
                        if ( u->spell >= 0 ) {
                            value += spellEffectWorth( *u, u->spell );
                        }
                    }
                    break;
                case 11:
                    value = ( u != nullptr && isUndead( u->creature ) ) ? H1::creatureWorth( u->creature ) * u->count : 0;
                    break;
                default:
                    break;
                }
                if ( best < value ) {
                    best = value;
                    bestCell = c;
                }
                if ( kind == 0 ) {
                    done = true;
                }
                else if ( kind == 1 ) {
                    c = nextCell( c );
                    done = c > 0x2b;
                }
                else {
                    done = nextOfSide( c + 1, c );
                }
            }
            return best;
        }

        // FUN_00464be0: the computer's spell for this turn (true: action 1 set).
        bool computerSpell()
        {
            const Heroes * hero = heroes[current];
            if ( hero == nullptr ) {
                return false;
            }
            int best = 0;
            int bestSpell = -1;
            int bestCell = -1;
            for ( size_t k = 0; k < hero->h1BookSpell.size(); ++k ) {
                const int spell = static_cast<int>( hero->h1BookSpell[k] ) - 1;
                if ( spell < 0 || spell > 28 || !( spellFlags[spell] & 2 ) || H1::spellUses( *hero, spell ) <= 0 ) {
                    continue;
                }
                int cell = -1;
                const int value = spellWorthAt( spell, cell );
                if ( best < value ) {
                    best = value;
                    bestSpell = spell;
                    bestCell = cell;
                }
            }
            if ( best < 1 ) {
                return false;
            }
            action = ACTION_SPELL;
            actionSpell = bestSpell;
            actionCell = bestCell;
            actionCell2 = -1;
            return true;
        }

        // FUN_0040b85c: a unit under Berserk acts by itself (C rand() in the original; the engine's random numbers here [temporary]).
        void berserkChoice()
        {
            Unit & u = acting();
            action = ACTION_NONE;
            for ( int guard = 0; guard < 1000; ++guard ) {
                const int m = touchMask( u, u.cell, 2, -1 );
                if ( m == 0xff ) {
                    if ( !( u.flags & FLAG_FLY ) ) {
                        const int d = static_cast<int>( Rand::Get( 0, 5 ) );
                        if ( canStep( u, u.cell, d ) ) {
                            action = ACTION_MOVE;
                            actionCell = step( u, u.cell, d );
                        }
                        break;
                    }
                    const int c = static_cast<int>( Rand::Get( 1, 0x2b ) );
                    if ( cells[c].side == -1 ) {
                        action = ACTION_MOVE;
                        actionCell = c;
                        continue;
                    }
                    u.idSide = cells[c].side;
                    u.idSlot = cells[c].slot;
                    if ( canFly( u, c, false ) ) {
                        action = ACTION_MOVE;
                        actionCell = c;
                        break;
                    }
                    continue;
                }
                const int d = static_cast<int>( Rand::Get( 0, 7 ) );
                if ( ( m & ( 1 << d ) ) == 0 ) {
                    int out = -1;
                    touches( u, u.cell, d, 2, -1, out );
                    action = ACTION_MOVE;
                    actionCell = out;
                    break;
                }
            }
            u.idSide = -1;
            u.idSlot = -1;
            if ( action == ACTION_NONE ) {
                action = ACTION_WAIT;
            }
        }

        // FUN_0040db10 / FUN_0040ddb7: the player picks a spell from the book and its target. Returns true when action 1 is set.
        bool playerSpell()
        {
            const Heroes * hero = heroes[current];
            if ( hero == nullptr ) {
                H1::UI::messageBox( "You have no hero to cast a spell.", H1::UI::MESSAGE_OKAY );
                return false;
            }
            if ( castCast[current] ) {
                H1::UI::messageBox( "You have already cast a spell this round.", H1::UI::MESSAGE_OKAY );
                return false;
            }
            const Spell picked = H1::openSpellBook( *hero, true, false, true );
            redraw();
            const int spell = H1::h1SpellFromEngine( picked.GetID() );
            if ( spell < 0 ) {
                return false;
            }
            if ( spell == 3 || spell == 13 || spell == 15 || spell == 16 ) {
                action = ACTION_SPELL;
                actionSpell = spell;
                actionCell = -1;
                actionCell2 = -1;
                return true;
            }
            LocalEvent & le = LocalEvent::Get();
            fheroes2::Display & display = fheroes2::Display::instance();
            const int32_t ox = ( display.width() - 640 ) / 2;
            const int32_t oy = ( display.height() - 480 ) / 2;
            // spelmous.mse names spells.icn (there is no spelmous.icn): its frame with the hotspot (note v159).
            auto cursorTo = []( const int f ) { H1::setMouseCursor( "spelmous.mse", f ); };
            int first = -1; // Teleport: the unit picked
            int hover = -2;
            while ( le.HandleEvents() ) {
                if ( le.MouseClickRight() || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                    return false;
                }
                const fheroes2::Point & m = le.getMouseCursorPos();
                const fheroes2::Point fp = fieldPoint( m.x - ox, m.y - oy ); // H1-INTENDED(D16): 0.8 view
                const int c = cellAt( fp.x, fp.y );
                const bool ok = validSpellTarget( spell, c, first );
                if ( c != hover ) {
                    hover = c;
                    if ( ok ) {
                        cursorTo( spell );
                        if ( spell == 2 && first >= 0 ) {
                            message = "Teleport Here";
                        }
                        else if ( spell == 0 || spell == 17 ) {
                            message = std::string( "Cast " ) + H1::spellName( spell );
                        }
                        else {
                            message = std::string( "Cast " ) + H1::spellName( spell ) + " on " + H1::creatureNameOne( units[cells[c].side][cells[c].slot].creature );
                        }
                    }
                    else {
                        cursorTo( 0x13 );
                        message = ( spell == 2 && first >= 0 ) ? "Invalid Teleport Destination" : "Select Spell Target";
                    }
                    redraw();
                }
                if ( le.MouseClickLeft() && ok ) {
                    if ( spell == 2 && first < 0 ) {
                        first = c;
                        hover = -2;
                        setMessage( "Select teleport destination." );
                        continue;
                    }
                    action = ACTION_SPELL;
                    actionSpell = spell;
                    actionCell = ( spell == 2 ) ? first : c;
                    actionCell2 = ( spell == 2 ) ? c : -1;
                    return true;
                }
            }
            return false;
        }

        // ---- Sieges (note v100) ----

        // FUN_00407f77: a shot from column 4 or less at column 6 or more passes the wall of the row worked out below; while that
        // wall stands (frame 8 or 10) the target's defense counts 4 more.
        bool wallBetween( const Unit & shooter, const Unit & target ) const
        {
            if ( !siege || !validCell( shooter.cell ) || !validCell( target.cell ) ) {
                return false;
            }
            const int sc = shooter.cell % 9;
            const int sr = shooter.cell / 9;
            const int tc = target.cell % 9;
            const int tr = target.cell / 9;
            if ( sc > 4 || tc < 6 ) {
                return false;
            }
            int row = tr;
            if ( std::abs( tr - sr ) >= 2 ) {
                row -= ( tr - sr ) / 2;
            }
            if ( std::abs( tr - sr ) % 2 == 1 ) {
                row += ( sr < tr ) ? -1 : 1;
            }
            row = std::clamp( row, 0, 4 );
            const int wall = cells[row * 9 + 5].obstacle;
            return wall == 10 || wall == 8;
        }

        // FUN_0046c968: one catapult stone at the walls (column 5).
        void catapultShot()
        {
            bool anyWall = false;
            for ( int r = 0; r < 5; ++r ) {
                if ( cells[r * 9 + 5].obstacle != -1 ) {
                    anyWall = true;
                }
            }
            if ( !anyWall ) {
                return;
            }
            const int w1 = cells[1 * 9 + 5].obstacle;
            const int w3 = cells[3 * 9 + 5].obstacle;
            int row = 3;
            if ( ( w1 == 10 || w1 == -1 ) && ( w3 == 10 || w3 == -1 ) ) {
                row = random.get( 0, 4 );
                while ( cells[row * 9 + 5].obstacle == -1 ) {
                    row = random.get( 0, 4 );
                }
            }
            else if ( w1 == -1 ) {
                row = 3;
            }
            else if ( w3 == -1 ) {
                row = 1;
            }
            else if ( w1 == 8 ) {
                row = ( w3 == 8 ) ? ( random.get( 0, 1 ) == 0 ? 1 : 3 ) : 1;
            }
            Cell & wall = cells[row * 9 + 5];
            const bool wasDamaged = ( wall.obstacle == 10 );
            int r = random.get( 0, 0x96 );
            // DAT_004a9b60 of the attacking player = 0 for a computer player (note v102): its stones hit harder.
            if ( !human[1] ) {
                r -= 0xf;
            }
            // FUN_0046c968: catsnd00 as the stone flies, waited for, then catsnd02 where it lands [the stone's flight is not drawn yet].
            waitSound( sound( "catsnd00.82m" ) );
            sound( "catsnd02.82m" );
            wall.obstacle = ( r < 0x1e || wasDamaged ) ? -1 : 10;
            delay( 500 );
        }

        // FUN_0046d70b: the garrison (keep) shoots at an attacking unit.
        void garrisonShot()
        {
            int best = -1;
            int bestKind = -1;
            int bestWorth = 0;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = units[1][k];
                if ( !u.exists || u.count <= 0 ) {
                    continue;
                }
                const int kind = ( u.flags & FLAG_SHOOTER ) ? 2 : ( ( u.flags & FLAG_FLY ) ? 1 : 0 );
                const int w = H1::creatureWorth( u.creature ) * u.count;
                if ( bestKind < kind || ( kind == bestKind && bestWorth < w ) ) {
                    bestKind = kind;
                    best = k;
                    bestWorth = w;
                }
            }
            if ( best == -1 ) {
                return;
            }
            Unit & target = units[1][best];
            // FUN_0046d70b: shoot15 for castle kinds 1 and 3 (castle +3), shoot01 for the others.
            sound( ( castleRace == 1 || castleRace == 3 ) ? "shoot15.82m" : "shoot01.82m" );
            int attack = 2;
            if ( heroes[0] != nullptr ) {
                attack = heroes[0]->GetAttack() + 2;
            }
            if ( castleBits & 1 ) {
                attack += castleGuild + 1;
            }
            const int difference = std::clamp( attack - target.defense, -20, 20 );
            int shots = 5;
            for ( int b = 7; b < 13; ++b ) {
                if ( castleBits & ( 1 << b ) ) {
                    shots += 4;
                }
            }
            for ( int b = 0; b < 5; ++b ) {
                if ( castleBits & ( 1 << b ) ) {
                    ++shots;
                }
            }
            int sum = 0;
            for ( int k = 0; k < shots; ++k ) {
                sum += random.get( 2, 3 );
            }
            int damage = static_cast<int>( sum * static_cast<double>( H1::attackDefenseMultiplier( difference ) ) );
            if ( damage < 1 ) {
                damage = 1;
            }
            const std::string name = ( target.count > 1 ) ? H1::creatureNameMany( target.creature ) : H1::creatureNameOne( target.creature );
            const int killed = applyDamage( target, damage );
            if ( killed < 1 ) {
                setMessage( "Garrison does " + std::to_string( damage ) + " Damage." );
            }
            else {
                setMessage( "Garrison does " + std::to_string( damage ) + " Damage. " + std::to_string( killed ) + " "
                            + ( killed > 1 ? H1::creatureNameMany( target.creature ) : H1::creatureNameOne( target.creature ) ) + ( killed > 1 ? " perish." : " perishes." ) );
            }
            (void)name;
            delay( 500 );
        }

        // FUN_0044ddef: at the side's first turn of a round, its catapult (attacker) or keep (defender) fires.
        void siegeWeapons()
        {
            if ( !siege ) {
                return;
            }
            if ( current == 1 ) {
                while ( catapultLeft[1] > 0 ) {
                    catapultShot();
                    --catapultLeft[1];
                }
            }
            else {
                while ( keepLeft[0] > 0 ) {
                    garrisonShot();
                    --keepLeft[0];
                }
            }
        }

        // ---- The computer's choice (FUN_0043e65b and helpers) ----

        int worth( const Unit & u ) const
        {
            return H1::creatureWorth( u.creature ) * u.count;
        }

        // FUN_0043f029 / FUN_0043f0f3 / FUN_0043f1ae: shooters with shots, fliers, the rest.
        int maskOf( const int side, const int kind ) const
        {
            int mask = 0;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = units[side][k];
                if ( !u.exists || ( u.flags & FLAG_DEAD ) ) {
                    continue;
                }
                bool in = false;
                if ( kind == 0 ) {
                    in = ( u.flags & FLAG_SHOOTER ) && u.shots > 0;
                }
                else if ( kind == 1 ) {
                    in = ( u.flags & FLAG_FLY ) != 0;
                }
                else {
                    in = !( u.flags & FLAG_FLY ) && ( !( u.flags & FLAG_SHOOTER ) || u.shots < 1 );
                }
                if ( in ) {
                    mask |= 1 << k;
                }
            }
            return mask;
        }

        // FUN_0043f4f4.
        int worthOf( const int side, const int mask ) const
        {
            int sum = 0;
            for ( int k = 0; k < 5; ++k ) {
                if ( ( mask & ( 1 << k ) ) && units[side][k].exists && !( units[side][k].flags & FLAG_DEAD ) ) {
                    sum += worth( units[side][k] );
                }
            }
            return sum;
        }

        // FUN_0043f285 (most worth) / FUN_0043f338 (least worth, used by ghosts).
        int pick( const int side, const int mask, const bool least ) const
        {
            int best = -1;
            int bestWorth = least ? 999999999 : 0;
            for ( int k = 0; k < 5; ++k ) {
                if ( !( mask & ( 1 << k ) ) ) {
                    continue;
                }
                const int w = units[side][k].exists ? worth( units[side][k] ) : 0;
                if ( least ? ( w < bestWorth ) : ( bestWorth < w ) ) {
                    best = k;
                    bestWorth = w;
                }
            }
            return best;
        }

        // FUN_0043f3eb: nearest on screen.
        int nearest( const Unit & u, const int side, const int mask ) const
        {
            int best = -1;
            int bestDistance = 0x280;
            for ( int k = 0; k < 5; ++k ) {
                if ( !( mask & ( 1 << k ) ) ) {
                    continue;
                }
                const Unit & t = units[side][k];
                if ( !validCell( t.cell ) || !validCell( u.cell ) ) {
                    continue;
                }
                const int d = screenDistance( cells[u.cell].x, cells[u.cell].y, cells[t.cell].x, cells[t.cell].y );
                if ( d < bestDistance ) {
                    best = k;
                    bestDistance = d;
                }
            }
            return best;
        }

        // FUN_0043f75f: strike an enemy standing next to the unit (the most worth; ghosts the least).
        bool attackNeighbour( Unit & u )
        {
            const int enemy = 1 - current;
            const int m = touchMask( u, u.cell, 1, -1 );
            int slots = 0;
            for ( int d = 0; d < 8; ++d ) {
                if ( m & ( 1 << d ) ) {
                    continue;
                }
                int n = step( u, u.cell, d );
                if ( ( validCell( n ) && ( u.flags & FLAG_TWO_HEX ) && cells[n].side != enemy )
                     || ( validCell( n ) && cells[n].slot == currentSlot && cells[n].side == current ) ) {
                    const int base = ( u.facing == 0 ) ? u.cell + 1 : u.cell - 1;
                    if ( n % 9 != 0 && n % 9 != 8 ) {
                        n = step( u, base, d );
                    }
                    if ( !validCell( n ) || cells[n].side != enemy ) {
                        n = -1;
                    }
                }
                if ( validCell( n ) && cells[n].slot >= 0 ) {
                    slots |= 1 << cells[n].slot;
                }
            }
            const int k = pick( enemy, slots, u.creature == CREATURE_GHOST );
            if ( k == -1 ) {
                return false;
            }
            action = ACTION_MOVE;
            actionCell = units[enemy][k].cell;
            return true;
        }

        // FUN_0043f5af: strike a unit of mask that can be reached this turn.
        bool attackReachable( Unit & u, const int side, int mask )
        {
            while ( mask != 0 ) {
                const int k = pick( side, mask, u.creature == CREATURE_GHOST );
                if ( k == -1 ) {
                    break;
                }
                u.idSide = side;
                u.idSlot = k;
                const Unit & t = units[side][k];
                int c = t.cell;
                u.dest = c;
                if ( canReach( u, c, false ) ) {
                    action = ACTION_MOVE;
                    actionCell = c;
                    return true;
                }
                if ( t.flags & FLAG_TWO_HEX ) {
                    c += ( t.facing == 1 ) ? -1 : 1;
                    u.dest = c;
                    if ( canReach( u, c, false ) ) {
                        action = ACTION_MOVE;
                        actionCell = c;
                        return true;
                    }
                }
                mask &= ~( 1 << k );
            }
            return false;
        }

        // FUN_0043fc17: go towards the nearest unit of mask (stand still when already next to it).
        bool approach( Unit & u, const int side, const int mask )
        {
            const int k = nearest( u, side, mask );
            if ( k == -1 ) {
                return false;
            }
            const Unit & t = units[side][k];
            int c = t.cell;
            u.idSide = side;
            u.idSlot = k;
            if ( touchMask( u, u.cell, 0, -1 ) != 0xff ) {
                action = ACTION_WAIT;
                return true;
            }
            const int speed = u.speed;
            u.speed = 0x7f;
            int found = findPath( u.cell, c, u, true );
            if ( found == 0 && ( t.flags & FLAG_TWO_HEX ) ) {
                c += ( t.facing == 0 ) ? 1 : -1;
                if ( c != -1 ) {
                    found = findPath( u.cell, c, u, true );
                }
            }
            u.speed = speed;
            if ( found < 2 ) {
                return false;
            }
            action = ACTION_MOVE;
            int at = u.cell;
            int left = u.speed;
            for ( int index = found - 1; index >= 1 && left != 0; --index ) {
                at = step( u, at, path[index] );
                --left;
            }
            actionCell = at;
            return true;
        }

        // FUN_0043fa08: stand in front of the nearest own shooter.
        bool guardShooter( Unit & u, const int side, const int mask )
        {
            u.idSide = -1;
            u.idSlot = -1;
            const int k = nearest( u, side, mask );
            if ( k == -1 ) {
                return false;
            }
            const Unit & t = units[side][k];
            int offset = ( t.flags & FLAG_TWO_HEX ) ? 2 : 1;
            if ( u.facing != 0 ) {
                offset = -offset;
            }
            const int c = t.cell + offset;
            if ( c % 9 == 8 || c % 9 == 0 ) {
                return approach( u, side, mask );
            }
            const int speed = u.speed;
            u.speed = 0x7f;
            const int found = findPath( u.cell, c, u, true );
            u.speed = speed;
            if ( found < 1 ) {
                return approach( u, side, mask );
            }
            action = ACTION_MOVE;
            int at = u.cell;
            int left = u.speed;
            for ( int index = found - 1; index >= 0 && left != 0; --index ) {
                at = step( u, at, path[index] );
                --left;
            }
            actionCell = at;
            return true;
        }

        // FUN_0043e65b (the retreat test FUN_0043de80 and hero spells FUN_00464be0 are asked before it).
        void computerChoice()
        {
            Unit & u = acting();
            const int me = current;
            const int enemy = 1 - current;
            action = ACTION_NONE;
            actionCell = -1;
            actionStand = -1;
            const int shooters[2] = { maskOf( 0, 0 ), maskOf( 1, 0 ) };
            const int fliers[2] = { maskOf( 0, 1 ), maskOf( 1, 1 ) };
            const int others[2] = { maskOf( 0, 2 ), maskOf( 1, 2 ) };
            const int all = worthOf( me, shooters[me] | fliers[me] | others[me] );
            int myShooters = worthOf( me, shooters[me] );
            int theirShooters = worthOf( enemy, shooters[enemy] );
            if ( siege ) {
                // The castle counts as shooters: (5 + 4 per dwelling + 1 per building 0..4) x 100.
                int castleWorth = 5;
                for ( int b = 7; b < 13; ++b ) {
                    if ( castleBits & ( 1 << b ) ) {
                        castleWorth += 4;
                    }
                }
                for ( int b = 0; b < 5; ++b ) {
                    if ( castleBits & ( 1 << b ) ) {
                        ++castleWorth;
                    }
                }
                if ( me == 0 ) {
                    myShooters += castleWorth * 100;
                }
                else {
                    theirShooters += castleWorth * 100;
                }
            }
            const bool shootersStrong = ( all + 4 ) / 5 < myShooters;
            const bool shootersWeaker = myShooters < theirShooters;
            int kind = 3;
            if ( u.flags & FLAG_SHOOTER ) {
                kind = ( u.shots < 1 ) ? 3 : 1;
            }
            else if ( u.flags & FLAG_FLY ) {
                kind = 2;
            }
            bool done = false;
            if ( kind == 1 ) {
                if ( attackNeighbour( u ) ) {
                    done = true;
                }
                else {
                    int k = pick( enemy, shooters[enemy], false );
                    if ( k == -1 ) {
                        k = pick( enemy, fliers[enemy], false );
                    }
                    if ( k == -1 && others[enemy] != 0 ) {
                        k = nearest( u, enemy, others[enemy] );
                    }
                    if ( k != -1 ) {
                        action = ACTION_MOVE;
                        actionCell = units[enemy][k].cell;
                        done = true;
                    }
                }
            }
            else if ( kind == 2 ) {
                done = attackReachable( u, enemy, shooters[enemy] ) || attackReachable( u, enemy, fliers[enemy] ) || attackReachable( u, enemy, others[enemy] );
            }
            else {
                if ( attackNeighbour( u ) ) {
                    done = true;
                }
                else if ( !shootersStrong || shootersWeaker ) {
                    done = attackReachable( u, enemy, shooters[enemy] ) || attackReachable( u, enemy, others[enemy] ) || attackReachable( u, enemy, fliers[enemy] );
                }
                else {
                    done = guardShooter( u, me, shooters[me] );
                }
                if ( !done ) {
                    done = approach( u, enemy, shooters[enemy] ) || approach( u, enemy, others[enemy] ) || approach( u, enemy, fliers[enemy] );
                }
                if ( !done && me == 1 && siege && u.cell % 9 < 4 ) {
                    // Attacking a castle: stand in front of the wall (column 4) of the unit's row.
                    const int c = ( u.cell / 9 ) * 9 + 4;
                    if ( validCell( c ) && cells[c].side == -1 && cells[c].obstacle == -1 ) {
                        action = ACTION_MOVE;
                        actionCell = c;
                        done = true;
                    }
                }
            }
            if ( !done ) {
                action = ACTION_WAIT;
            }
            u.idSide = -1;
            u.idSlot = -1;
            // An empty cell next to an enemy: strike that enemy instead.
            if ( action == ACTION_MOVE && actionCell > 0 && actionCell < 0x2c && cells[actionCell].side == -1 ) {
                for ( int d = 0; d < 6; ++d ) {
                    const int n = step( u, actionCell, d );
                    if ( n > 0 && n < 0x2c && cells[n].side == enemy ) {
                        actionCell = n;
                        return;
                    }
                }
            }
        }

        // ---- The player's choice (FUN_0044af80, FUN_0044b7d2, FUN_0044a157, FUN_0044a940) ----

        // FUN_0046c22f: cell under a point of the field.
        static int cellAt( const int32_t x, const int32_t y )
        {
            if ( y < 0x3c ) {
                return -1;
            }
            const int row = ( y - 0x3c ) / 0x50;
            int col = 0;
            if ( ( row & 1 ) == 0 ) {
                col = ( x - 0x1b ) / 0x4e;
                if ( x < 0x1b ) {
                    col = -1;
                }
            }
            else if ( x < 0x42 ) {
                col = -1;
            }
            else {
                col = ( x - 0x42 ) / 0x4e;
            }
            if ( row >= 5 ) {
                return -1;
            }
            return row * 9 + col + 1;
        }

        // FUN_0044a081: may the unit stand on c (-2 = no cell needed).
        bool mayStand( const int c ) const
        {
            if ( c == -2 ) {
                return true;
            }
            if ( c == -1 || c % 9 == 8 || c % 9 == 0 || !validCell( c ) ) {
                return false;
            }
            if ( cells[c].obstacle != -1 ) {
                return false;
            }
            if ( cells[c].side != -1 && ( cells[c].side != current || cells[c].slot != currentSlot ) ) {
                return false;
            }
            return true;
        }

        // FUN_0044b7d2: what a click on cell c does. 0 nothing, 1 walk, 2 fly, 3 shoot, 4 own hero, 5 own unit, 7 strike, 13 enemy hero.
        int cursorFor( const int c )
        {
            if ( c == -1 ) {
                return 0;
            }
            if ( c == 9 ) {
                return hasHero[1] ? ( current == 1 ? 4 : 13 ) : 0;
            }
            if ( c == 0x1a ) {
                return hasHero[0] ? ( current == 0 ? 4 : 13 ) : 0;
            }
            if ( c % 9 == 8 || c % 9 == 0 || !validCell( c ) ) {
                return 0;
            }
            Unit & u = acting();
            u.idSide = -1;
            u.idSlot = -1;
            if ( cells[c].obstacle != -1 ) {
                return 0;
            }
            const int occupant = cells[c].side;
            if ( occupant == -1 ) {
                if ( canReach( u, c, false ) ) {
                    return ( u.flags & FLAG_FLY ) ? 2 : 1;
                }
                return 0;
            }
            if ( occupant == current ) {
                return 5;
            }
            u.idSide = occupant;
            u.idSlot = cells[c].slot;
            if ( u.shots > 0 && touchMask( u, u.cell, 1, -1 ) == 0xff ) {
                return 3;
            }
            if ( canReach( u, c, true ) ) {
                return 7;
            }
            u.idSide = -1;
            u.idSlot = -1;
            return 0;
        }

        // FUN_0044a157: which places next to the target cell the unit can strike from, spread over 24 mouse sectors.
        void computeSectors( const int target )
        {
            Unit & u = acting();
            const int tSide = u.idSide;
            const int tSlot = u.idSlot;
            u.idSide = -1;
            u.idSlot = -1;
            const bool two = ( u.flags & FLAG_TWO_HEX ) != 0;
            const Unit * t = ( tSide >= 0 && tSlot >= 0 ) ? &units[tSide][tSlot] : nullptr;
            int head[8];
            int tail[8];
            bool freeHere[8];
            bool usable[8];
            for ( int k = 0; k < 8; ++k ) {
                if ( k == 6 || k == 7 ) {
                    if ( !two ) {
                        head[k] = -1;
                    }
                    else if ( u.facing == 0 ) {
                        head[k] = neighbour( target, k == 6 ? 5 : 3 );
                    }
                    else {
                        head[k] = neighbour( target, k == 6 ? 0 : 2 );
                    }
                }
                else {
                    head[k] = neighbour( target, k );
                }
                if ( !two || head[k] == -1 ) {
                    tail[k] = -2;
                }
                else if ( u.facing == 0 ) {
                    if ( k == 3 || k == 4 || k == 5 ) {
                        head[k] = ( head[k] % 9 == 1 ) ? -1 : head[k] - 1;
                    }
                    tail[k] = ( head[k] % 9 == 7 ) ? -1 : head[k] + 1;
                }
                else {
                    if ( k == 0 || k == 1 || k == 2 ) {
                        head[k] = ( head[k] % 9 == 7 ) ? -1 : head[k] + 1;
                    }
                    tail[k] = ( head[k] % 9 == 1 ) ? -1 : head[k] - 1;
                }
                freeHere[k] = mayStand( head[k] ) && mayStand( tail[k] );
            }
            for ( int k = 0; k < 8; ++k ) {
                if ( !( u.flags & FLAG_FLY ) ) {
                    usable[k] = freeHere[k] && ( u.cell == head[k] || canReach( u, head[k], true ) );
                }
                else {
                    usable[k] = freeHere[k];
                }
            }
            attackPlaces = 0;
            for ( int k = 0; k < 8; ++k ) {
                if ( usable[k] ) {
                    ++attackPlaces;
                }
            }
            if ( attackPlaces == 0 ) {
                usable[6] = true;
            }
            for ( int & s : sectors ) {
                s = -1;
            }
            auto isTarget = [&]( const int c ) { return validCell( c ) && cells[c].side == tSide && cells[c].slot == tSlot; };
            for ( int k = 0; k < 8; ++k ) {
                int value = k;
                const int index = k < 6 ? ( k + 3 ) % 6 : ( k == 6 ? 7 : 6 );
                if ( !usable[index] ) {
                    continue;
                }
                if ( t != nullptr && ( t->flags & FLAG_TWO_HEX ) ) {
                    if ( k == 0 && isTarget( target - 1 ) ) {
                        value = 6;
                    }
                    else if ( k == 5 && isTarget( target + 1 ) ) {
                        value = 6;
                    }
                    else if ( k == 2 && isTarget( target - 1 ) ) {
                        value = 7;
                    }
                    else if ( k == 3 && isTarget( target + 1 ) ) {
                        value = 7;
                    }
                }
                if ( k < 6 ) {
                    for ( int s = 0; s < 4; ++s ) {
                        sectors[index * 4 + s] = value;
                    }
                }
                else if ( k == 6 ) {
                    sectors[11] = sectors[12] = sectors[13] = value;
                }
                else {
                    sectors[0] = sectors[1] = sectors[23] = value;
                }
            }
            int missing = 24;
            while ( missing > 0 ) {
                for ( int i = 0; i < 24; ++i ) {
                    if ( sectors[i] == -1 ) {
                        const int next = ( i + 1 ) % 24;
                        const int prev = ( i + 23 ) % 24;
                        if ( sectors[next] >= 0 && sectors[next] <= 7 ) {
                            sectors[i] = sectors[next] + 10;
                        }
                        else if ( sectors[prev] >= 0 && sectors[prev] <= 7 ) {
                            sectors[i] = sectors[prev] + 10;
                        }
                    }
                }
                missing = 0;
                for ( int & s : sectors ) {
                    if ( s < 10 ) {
                        if ( s == -1 ) {
                            ++missing;
                        }
                    }
                    else {
                        s -= 10;
                    }
                }
                if ( attackPlaces == 0 && missing == 24 ) {
                    break; // [cannot happen in the original: usable[6] is forced on above]
                }
            }
            u.idSide = tSide;
            u.idSlot = tSlot;
        }

        // FUN_0044a940: the sector under the mouse -> the cell to strike from (standCell); returns the cursor frame 7.. or -1.
        int pointSector( const int32_t mx, const int32_t my, int target )
        {
            if ( autoCombat || !( attackPlaces > 1 || lastSector < 0 ) ) {
                return -1;
            }
            const int col = target % 9;
            const int row = target / 9;
            int32_t lx = mx - ( col - 1 ) * 0x4e;
            lx += ( ( row & 1 ) == 0 ) ? -0x1b : -0x42;
            const int32_t dy = my - row * 0x50 - 100;
            const int32_t dx = lx - 0x27;
            int q = 0;
            if ( dx < 0 ) {
                q = ( dy < 0 ) ? 0x12 : 0xc;
            }
            else if ( dy >= 0 ) {
                q = 6;
            }
            const float ratio = ( dy == 0 ) ? 1e9f : static_cast<float>( std::abs( dx ) ) / static_cast<float>( std::abs( dy ) );
            if ( q == 0 || q == 0xc ) {
                if ( ratio > 3.73f ) {
                    q += 5;
                }
                else if ( ratio > 1.73f ) {
                    q += 4;
                }
                else if ( ratio > 1.0f ) {
                    q += 3;
                }
                else if ( ratio > 0.58f ) {
                    q += 2;
                }
                else if ( ratio > 0.27f ) {
                    q += 1;
                }
            }
            else {
                if ( ratio < 0.27f ) {
                    q += 5;
                }
                else if ( ratio < 0.58f ) {
                    q += 4;
                }
                else if ( ratio < 1.0f ) {
                    q += 3;
                }
                else if ( ratio < 1.73f ) {
                    q += 2;
                }
                else if ( ratio < 3.73f ) {
                    q += 1;
                }
            }
            if ( sectors[q] == lastSector ) {
                return lastSector + 7;
            }
            lastSector = sectors[q];
            const int d = opposite( sectors[q] );
            Unit & u = acting();
            const bool two = ( u.flags & FLAG_TWO_HEX ) != 0;
            int local = d;
            int other = -1;
            if ( d == 6 || d == 7 ) {
                if ( !two ) {
                    if ( validCell( target - 1 ) && cells[target - 1].side == u.idSide && cells[target - 1].slot == u.idSlot ) {
                        target = target - 1;
                    }
                    local = ( d == 6 ) ? 0 : 2;
                }
                else if ( u.facing == 0 && d == 6 ) {
                    local = 5;
                    other = 0;
                }
                else if ( u.facing == 0 && d == 7 ) {
                    local = 3;
                    other = 2;
                }
                else if ( u.facing == 1 && d == 6 ) {
                    local = 0;
                    other = 5;
                }
                else {
                    local = 2;
                    other = 3;
                }
            }
            else if ( two ) {
                if ( u.facing == 0 && ( d == 5 || d == 4 || d == 3 ) ) {
                    target -= 1;
                }
                else if ( u.facing == 1 && ( d == 0 || d == 1 || d == 2 ) ) {
                    target += 1;
                }
            }
            standCell = neighbour( target, local );
            int tailCell = -2;
            if ( two ) {
                tailCell = ( u.facing == 1 ) ? standCell - 1 : standCell + 1;
            }
            if ( !mayStand( standCell ) || !mayStand( tailCell ) ) {
                if ( !two || ( d != 6 && d != 7 ) ) {
                    if ( other != -1 ) {
                        standCell = neighbour( target, other );
                    }
                }
                else if ( u.facing == 0 ) {
                    standCell += 1;
                }
                else {
                    standCell -= 1;
                }
            }
            return lastSector + 7;
        }

        // ---- Drawing ----

        const std::vector<fheroes2::Sprite> * xtl = nullptr;
        const std::vector<fheroes2::Sprite> * obj = nullptr;
        fheroes2::Image background;
        const Heroes * heroes[2] = { nullptr, nullptr };
        int heroColour[2] = { -1, -1 };
        H1::UI::Layout bar;

        // H1-INTENDED(D16) extension mode: the battle drawn at 0.8 (owner 2026-11-17, mock results/extension/battle_small_v8.png and
        // battle_presets_v2.png). The 640 x 460 field sits at (80, 46) of an 800 x 555 picture whose border holds more ground hexes, the
        // battle background widened, whole hero tents in the top corners (h1/ext/TENTW.ICN) and terrain props (the ground's .obj and
        // HoMM2 COBJ pictures, h1/ext/BTLPROPS.ICN); that picture is shrunk to 640 x 444 above the bar. Not for sieges and sea battles.
        bool bigView = false;
        fheroes2::Image surround;
        fheroes2::Image surroundFront; // props and tents, drawn over the field (see-through elsewhere)
        fheroes2::Rect tentRect[2]; // hero tents in field points (side 1 left, side 0 right)
        static constexpr int32_t bigW = 800;
        static constexpr int32_t bigH = 555;
        static constexpr int32_t bigX = 80;
        static constexpr int32_t bigY = 46;

        // A battle without a human side is played without being shown [whether the original shows it: unconfirmed].
        bool visible = true;

        // H1-INTENDED(D3): HoMM2 combat bar and options window (docs/INTENDED_DIFFERENCES.md). Do not change without the owner.
        // ---- Added battle view (owner decision 2026-11-15, not in HoMM1): the HoMM2 combat bar (HoMM2 cmbtwin.bin: textbar.icn 8 / 9 at
        // (49, 443) / (49, 463), SKIP 0/1 at (591, 443), AUTO 4/5 at (0, 443), OPTIONS 6/7 at (0, 461), status text (83, 446, 474, 16)),
        // the HoMM2 combat options window (cspanel.bin) and grid / movement shadow / cursor shadow drawn on the HoMM1 hexes. Off = original look.
        std::array<bool, 45> moveShade{};
        int cursorShade = -1;
        uint8_t shadeTable[256]{};
        uint8_t lineTable[256]{};
        bool shadeReady = false;
        std::vector<uint8_t> hexMask; // 1 inside the hex, 2 on its edge (from <ground>.xtl frame 4)
        int32_t hexW = 0;
        int32_t hexH = 0;
        int32_t hexX = 0;
        int32_t hexY = 0;

        static int viewOptions()
        {
            return Settings::Get().h1BattleView;
        }

        void prepareShades()
        {
            if ( shadeReady ) {
                return;
            }
            shadeReady = true;
            const std::vector<uint8_t> & pal = H1::getPalette();
            const auto nearest = [&pal]( const int r, const int g, const int b ) {
                int best = 0;
                int bestDistance = 1 << 30;
                for ( int j = 0; j < 224 && static_cast<size_t>( j * 3 + 2 ) < pal.size(); ++j ) {
                    const int dr = pal[j * 3] - r;
                    const int dg = pal[j * 3 + 1] - g;
                    const int db = pal[j * 3 + 2] - b;
                    const int d = dr * dr + dg * dg + db * db;
                    if ( d < bestDistance ) {
                        bestDistance = d;
                        best = j;
                    }
                }
                return static_cast<uint8_t>( best );
            };
            for ( int i = 0; i < 256; ++i ) {
                const int r = static_cast<size_t>( i * 3 + 2 ) < pal.size() ? pal[i * 3] : 0;
                const int g = static_cast<size_t>( i * 3 + 2 ) < pal.size() ? pal[i * 3 + 1] : 0;
                const int b = static_cast<size_t>( i * 3 + 2 ) < pal.size() ? pal[i * 3 + 2] : 0;
                shadeTable[i] = nearest( r * 3 / 5, g * 3 / 5, b * 3 / 5 );
                lineTable[i] = nearest( r * 2 / 5, g * 2 / 5, b * 2 / 5 );
            }
            if ( xtl != nullptr && xtl->size() > 4 ) {
                const fheroes2::Sprite & hex = ( *xtl )[4];
                hexW = hex.width();
                hexH = hex.height();
                hexX = hex.x();
                hexY = hex.y();
                hexMask.assign( static_cast<size_t>( hexW ) * hexH, 0 );
                const uint8_t * t = hex.transform();
                const auto inside = [&]( const int32_t x, const int32_t y ) {
                    return x >= 0 && y >= 0 && x < hexW && y < hexH && t[y * hexW + x] == 0;
                };
                for ( int32_t y = 0; y < hexH; ++y ) {
                    for ( int32_t x = 0; x < hexW; ++x ) {
                        if ( inside( x, y ) ) {
                            const bool edge = !inside( x - 1, y ) || !inside( x + 1, y ) || !inside( x, y - 1 ) || !inside( x, y + 1 );
                            hexMask[y * hexW + x] = edge ? 2 : 1;
                        }
                    }
                }
            }
        }

        // Darkens the hex of cell c: whole hex with shadeTable when fill, its edge with lineTable when edge.
        void shadeHex( fheroes2::Image & frame, const int c, const bool fill, const bool edge )
        {
            if ( hexMask.empty() || c < 0 || c >= 45 ) {
                return;
            }
            const int32_t left = cells[c].x + hexX;
            const int32_t top = cells[c].y + hexY;
            uint8_t * data = frame.image();
            const int32_t fw = frame.width();
            const int32_t fh = std::min<int32_t>( frame.height(), 443 );
            for ( int32_t y = 0; y < hexH; ++y ) {
                const int32_t py = top + y;
                if ( py < 0 || py >= fh ) {
                    continue;
                }
                for ( int32_t x = 0; x < hexW; ++x ) {
                    const uint8_t m = hexMask[y * hexW + x];
                    const int32_t px = left + x;
                    if ( m == 0 || px < 0 || px >= fw ) {
                        continue;
                    }
                    uint8_t & v = data[py * fw + px];
                    if ( edge && m == 2 ) {
                        v = lineTable[v];
                    }
                    else if ( fill ) {
                        v = shadeTable[v];
                    }
                }
            }
        }

        // Drawn row by row right after that row's ground, so the scenery band (laid after row 0) still covers the top of row 0 as in the original.
        void drawView( fheroes2::Image & frame, const int row )
        {
            const int view = viewOptions();
            if ( view == 0 ) {
                return;
            }
            prepareShades();
            for ( int c = row * 9; c < row * 9 + 9; ++c ) {
                if ( c % 9 == 0 || c % 9 == 8 ) {
                    continue;
                }
                const bool fill = ( ( view & 2 ) != 0 && moveShade[c] ) || ( ( view & 4 ) != 0 && c == cursorShade );
                shadeHex( frame, c, fill, ( view & 1 ) != 0 );
            }
        }

        // Cells the acting unit may move to (as cursorFor finds them), for the movement shadow.
        void computeMoveShade()
        {
            moveShade.fill( false );
            if ( ( viewOptions() & 2 ) == 0 ) {
                return;
            }
            Unit & u = acting();
            const int keepSide = u.idSide;
            const int keepSlot = u.idSlot;
            const int keepDest = u.dest;
            u.idSide = -1;
            u.idSlot = -1;
            for ( int c = 0; c < 45; ++c ) {
                if ( c % 9 == 0 || c % 9 == 8 || !validCell( c ) || cells[c].obstacle != -1 ) {
                    continue;
                }
                if ( c == u.cell || ( cells[c].side == -1 && canReach( u, c, false ) ) ) {
                    moveShade[c] = true;
                }
            }
            u.idSide = keepSide;
            u.idSlot = keepSlot;
            u.dest = keepDest;
        }

        void drawBar( fheroes2::Image & frame, const int pressed )
        {
            const auto blit = [&frame]( const int index, const int32_t x, const int32_t y ) {
                const fheroes2::Sprite & s = Assets::getImage( ICN::TEXTBAR, index );
                fheroes2::Blit( s, frame, x + s.x(), y + s.y() );
            };
            blit( 8, 49, 443 );
            blit( 9, 49, 463 );
            blit( pressed == 1 ? 1 : 0, 591, 443 );
            blit( pressed == 2 ? 5 : 4, 0, 443 );
            blit( pressed == 3 ? 7 : 6, 0, 461 );
            const H1::UI::Item * text = bar.find( 12 );
            H1::UI::drawText( frame, message, "bigfont.fnt", 83, 446, 474, 16, text != nullptr ? text->colour : 1, 1 );
        }

        // 0 none, 1 SKIP, 2 AUTO, 3 OPTIONS
        static int barButtonAt( const int32_t x, const int32_t y )
        {
            if ( y < 443 || y >= 480 ) {
                return 0;
            }
            if ( x >= 591 ) {
                return 1;
            }
            if ( x < 49 ) {
                return y < 461 ? 2 : 3;
            }
            return 0;
        }

        void showPressed( const int button )
        {
            barPressed = button;
            redraw();
            fheroes2::delayforMs( 100 );
            barPressed = 0;
            redraw();
        }

        int barPressed = 0;

        // HoMM2 combat options window (cspanel.bin at the centre: cspanbkg.icn 0 / 1, cspanel.icn icons 65 x 65, cspanbtn.icn OK at (128, 252)).
        // Slots used: speed (52, 47), auto combat spell casting (236, 47), grid (52, 157), movement shadow (144, 157), cursor shadow (236, 157).
        // HoMM2's Monster Info slot (144, 47) stays empty (no such window in HoMM1). Titles and help texts: HoMM2 HEROES2W.EXE strings.
        void optionsWindow()
        {
            fheroes2::Display & display = fheroes2::Display::instance();
            const int32_t ox = ( display.width() - 640 ) / 2 + 151;
            const int32_t oy = ( display.height() - 480 ) / 2 + 80;
            const fheroes2::ImageRestorer restorer( display, ox - 16, oy - 16, 337 + 32, 320 + 32 );
            Settings & conf = Settings::Get();
            bool changed = false;
            const auto onOff = []( const bool on ) { return on ? "On" : "Off"; };
            const auto draw = [&]( const bool okPressed ) {
                const auto blit = [&display, ox, oy]( const int icn, const int index, const int32_t x, const int32_t y ) {
                    const fheroes2::Sprite & s = Assets::getImage( icn, index );
                    fheroes2::Blit( s, display, ox + x + s.x(), oy + y + s.y() );
                };
                blit( ICN::CSPANBKG, 0, 0, 0 );
                blit( ICN::CSPANBKG, 1, 0, 0 );
                // H1-INTENDED(D2): combat speed 1x / 2x / 4x (docs/INTENDED_DIFFERENCES.md). Do not change without the owner.
                const int mult = H1::combatSpeedMultiplier();
                const int view = conf.h1BattleView;
                blit( ICN::CSPANEL, mult >= 4 ? 2 : ( mult >= 2 ? 1 : 0 ), 52, 47 );
                blit( ICN::CSPANEL, ( view & 8 ) ? 6 : 7, 236, 47 );
                blit( ICN::CSPANEL, ( view & 1 ) ? 9 : 8, 52, 157 );
                blit( ICN::CSPANEL, ( view & 2 ) ? 11 : 10, 144, 157 );
                blit( ICN::CSPANEL, ( view & 4 ) ? 13 : 12, 236, 157 );
                blit( ICN::CSPANBTN, okPressed ? 1 : 0, 128, 252 );
                const auto text = [&display, ox, oy]( const std::string & t, const int32_t x, const int32_t y, const int32_t w, const int32_t h ) {
                    H1::UI::drawText( display, t, "smalfont.fnt", ox + x, oy + y, w, h, 1, 1 );
                };
                text( "Speed", 42, 36, 85, 12 );
                text( std::to_string( mult ) + "x", 44, 115, 81, 28 );
                text( "Auto Combat\nSpell Casting", 226, 24, 85, 24 );
                text( onOff( ( view & 8 ) == 0 ), 221, 115, 95, 24 );
                text( "Grid", 42, 146, 85, 12 );
                text( onOff( view & 1 ), 52, 225, 65, 12 );
                text( "Shadow\nMovement", 134, 134, 85, 24 );
                text( onOff( view & 2 ), 144, 225, 65, 12 );
                text( "Shadow\nCursor", 226, 134, 85, 24 );
                text( onOff( view & 4 ), 221, 225, 95, 12 );
                display.render();
            };
            const fheroes2::Rect speedRoi( ox + 52, oy + 47, 65, 65 );
            const fheroes2::Rect slot[3] = { { ox + 52, oy + 157, 65, 65 }, { ox + 144, oy + 157, 65, 65 }, { ox + 236, oy + 157, 65, 65 } };
            const fheroes2::Rect okRoi( ox + 128, oy + 252, 96, 27 );
            // HoMM2 texts; the speed text adds "1x is the original speed" (owner's added option).
            const char * const help[4] = { "Set the speed of combat actions and animations.  1x is the original speed.",
                                           "Toggle the hex grid on or off.  The hex grid always underlies movement, even if turned off.  This switch only "
                                           "determines if the grid is visible.",
                                           "Toggle on or off a shadowing system showing where your creatures can move and attack.",
                                           "Toggle on or off a shadowing system showing where the hex grid that the mouse cursor is currently over is shadowed." };
            const fheroes2::Rect autoSpellRoi( ox + 236, oy + 47, 65, 65 );
            Cursor::Get().SetThemes( Cursor::POINTER );
            draw( false );
            LocalEvent & le = LocalEvent::Get();
            while ( le.HandleEvents() ) {
                bool redrawNeeded = false;
                if ( le.MouseClickLeft( speedRoi ) ) {
                    const int next = H1::combatSpeedMultiplier() >= 4 ? 1 : ( H1::combatSpeedMultiplier() >= 2 ? 3 : 2 );
                    conf.SetBattleSpeed( next );
                    speedPercent = 100 / H1::combatSpeedMultiplier();
                    redrawNeeded = true;
                }
                for ( int k = 0; k < 3; ++k ) {
                    if ( le.MouseClickLeft( slot[k] ) ) {
                        conf.h1BattleView ^= ( 1 << k );
                        redrawNeeded = true;
                    }
                }
                if ( le.MouseClickLeft( autoSpellRoi ) ) {
                    conf.h1BattleView ^= 8;
                    redrawNeeded = true;
                }
                if ( le.isMouseRightButtonPressedInArea( autoSpellRoi ) ) {
                    H1::UI::messageBox( "Toggle whether or not the computer will cast spells for you when auto combat is on.", H1::UI::MESSAGE_POPUP );
                }
                if ( le.isMouseRightButtonPressedInArea( speedRoi ) ) {
                    H1::UI::messageBox( help[0], H1::UI::MESSAGE_POPUP );
                }
                for ( int k = 0; k < 3; ++k ) {
                    if ( le.isMouseRightButtonPressedInArea( slot[k] ) ) {
                        H1::UI::messageBox( help[k + 1], H1::UI::MESSAGE_POPUP );
                    }
                }
                if ( redrawNeeded ) {
                    changed = true;
                    draw( false );
                }
                if ( le.MouseClickLeft( okRoi ) || Game::HotKeyCloseWindow() ) {
                    draw( true );
                    fheroes2::delayforMs( 100 );
                    break;
                }
            }
            if ( changed ) {
                conf.Save( Settings::configFileName );
            }
        }

        void setMessage( const std::string & text )
        {
            message = text;
            redraw();
        }

        void delay( const uint32_t ms )
        {
            if ( !visible ) {
                return;
            }
            redraw();
            // All battle pauses follow the combat speed (h1_cpanel: 1x = original).
            fheroes2::delayforMs( ms * static_cast<uint32_t>( speedPercent ) / 100 );
        }

        // FUN_0047da2c / FUN_0047d7b6: with the "step" flag (a unit walking up or down a row, frames 1..5) the frame's x offset is cut to
        // about a quarter: (x >> 1) - ((x - (x >> 1)) >> 1).
        static int32_t skewed( const int32_t x )
        {
            return ( x >> 1 ) - ( ( x - ( x >> 1 ) ) >> 1 );
        }

        static int32_t frameLeft( const fheroes2::Sprite & s, const int32_t x, const bool mirror, const bool skew )
        {
            const int32_t off = skew ? skewed( s.x() ) : s.x();
            return mirror ? ( x - off - s.width() + 1 ) : ( x + off );
        }

        static void putUnitFrame( fheroes2::Image & dst, const std::vector<fheroes2::Sprite> & icn, const int index, const int32_t x, const int32_t y,
                                  const bool mirror, const bool skew )
        {
            if ( index < 0 || static_cast<size_t>( index ) >= icn.size() ) {
                return;
            }
            const fheroes2::Sprite & s = icn[index];
            fheroes2::Blit( s, dst, frameLeft( s, x, mirror, skew ), y + s.y(), mirror );
        }

        // FUN_0047a730 -> FUN_0047da2c / FUN_0047db06: a shadow frame holds only runs of pixels (no colours); each pixel under it is darkened
        // with the table 0x4a3e2c.
        static void putShadow( fheroes2::Image & dst, const std::vector<fheroes2::Sprite> & icn, const int index, const int32_t x, const int32_t y,
                               const bool mirror, const bool skew )
        {
            if ( index < 0 || static_cast<size_t>( index ) >= icn.size() ) {
                return;
            }
            const fheroes2::Sprite & s = icn[index];
            if ( s.empty() ) {
                return;
            }
            const std::vector<uint8_t> & table = H1::darkenTable();
            const int32_t left = frameLeft( s, x, mirror, skew );
            const int32_t top = y + s.y();
            const int32_t w = s.width();
            const int32_t h = s.height();
            const uint8_t * t = s.transform();
            uint8_t * out = dst.image();
            const int32_t dw = dst.width();
            const int32_t dh = std::min<int32_t>( dst.height(), 0x1cc );
            for ( int32_t sy = 0; sy < h; ++sy ) {
                const int32_t py = top + sy;
                if ( py < 0 || py >= dh ) {
                    continue;
                }
                for ( int32_t sx = 0; sx < w; ++sx ) {
                    if ( t[sy * w + sx] != 0 ) {
                        continue;
                    }
                    const int32_t px = left + ( mirror ? ( w - 1 - sx ) : sx );
                    if ( px < 0 || px >= dw ) {
                        continue;
                    }
                    uint8_t & v = out[py * dw + px];
                    v = table[v];
                }
            }
        }

        void drawUnit( fheroes2::Image & frame, const Unit & u, const bool isCurrent )
        {
            Cell cell = cells[u.cell];
            cell.x += u.offsetX;
            cell.y += u.offsetY;
            const std::vector<fheroes2::Sprite> & std = H1::getIcn( std::string( H1::creatureFile( u.creature ) ) + ".std" );
            const bool mirror = ( u.facing == 1 );
            // FUN_00406e0c by state. Step flag: a vertical walking step (+0x0b) at frames 1..5.
            const bool skew = u.animStep != 0 && u.animFrame > 0 && u.animFrame < 6;
            if ( u.animState == 1 ) {
                const std::vector<fheroes2::Sprite> & wlk = H1::getIcn( std::string( H1::creatureFile( u.creature ) ) + ".wlk" );
                const int32_t y = cell.y + u.animFrame * u.animStep;
                putShadow( frame, H1::getIcnMono( std::string( H1::creatureFile( u.creature ) ) + ".wlk" ), u.animFrame + 6, cell.x, y, mirror, skew );
                putUnitFrame( frame, wlk, u.animFrame, cell.x, y, mirror, skew );
                return;
            }
            if ( u.animState == 2 ) {
                const std::vector<fheroes2::Sprite> & atk = H1::getIcn( std::string( H1::creatureFile( u.creature ) ) + ".atk" );
                if ( u.animFrame < 5 ) {
                    putShadow( frame, H1::getIcnMono( std::string( H1::creatureFile( u.creature ) ) + ".atk" ), u.animFrame + 9, cell.x, cell.y, mirror, skew );
                    putUnitFrame( frame, atk, 0, cell.x, cell.y, mirror, skew );
                }
                putUnitFrame( frame, atk, u.animFrame, cell.x, cell.y, mirror, skew );
                return;
            }
            // Shadow frame: + 0x0f for creatures 11, 17, 23, + 8 for creature 22, + 9 for the others.
            const int shadowAdd = ( u.creature == 11 || u.creature == 17 || u.creature == 23 ) ? 0xf : ( u.creature == 22 ? 8 : 9 );
            putShadow( frame, H1::getIcnMono( std::string( H1::creatureFile( u.creature ) ) + ".std" ), u.animFrame + shadowAdd, cell.x, cell.y, mirror, skew );
            if ( u.animFrame > 4 && u.creature != CREATURE_HYDRA ) {
                putUnitFrame( frame, std, 5, cell.x, cell.y, mirror, skew );
            }
            putUnitFrame( frame, std, u.animFrame, cell.x, cell.y, mirror, skew );
            if ( u.animFrame != 1 ) {
                return;
            }
            if ( isCurrent && !std.empty() ) {
                // FUN_00406e0c: frame 0 (outline) over the unit in colour 0xe4.
                // (drawMonoSprite adds the frame's own offsets.)
                const fheroes2::Sprite & outline = std[0];
                if ( mirror ) {
                    const fheroes2::Sprite flipped( fheroes2::Flip( outline, true, false ), 0, 0 );
                    H1::UI::drawMonoSprite( frame, flipped, cell.x - outline.x() - outline.width() + 1, cell.y + outline.y(), 0xe4 );
                }
                else {
                    H1::UI::drawMonoSprite( frame, outline, cell.x, cell.y, 0xe4 );
                }
            }
            if ( u.spell >= 0 ) {
                // FUN_00406e0c: the spell's picture over the unit.
                put( frame, H1::getIcn( "spells.icn" ), u.spell, cell.x, cell.y - 0x28, false );
            }
            const bool two = ( u.flags & FLAG_TWO_HEX ) != 0;
            const int32_t bx = two ? ( mirror ? cell.x - 0x5f : cell.x + 0x4b ) : ( mirror ? cell.x - 0x27 : cell.x + 8 );
            // H1-INTENDED(D13): the HoMM2 bar (D3) starts at row 443, over the count box of the bottom hex row (cell y 459, box 448..459;
            // the original bar starts at 460). That row's box goes up to just above the bar (owner decision 2026-11-17 19:02).
            int32_t by = cell.y - 0xb;
            const std::vector<fheroes2::Sprite> & tb = H1::getIcn( "textbar.icn" );
            const int32_t boxH = tb.size() > 5 ? tb[5].height() : 12;
            if ( by + boxH > 443 && !bigView ) {
                by = 443 - boxH;
            }
            put( frame, tb, 5, bx, by, false );
            H1::UI::drawText( frame, std::to_string( u.count ), "smalfont.fnt", bx, by - 1, 0x14, 0xc, 1, 1 );
        }

        // H1-INTENDED(D16): a point of the 640 x 480 screen picture -> a point of the field (what cellAt and the sectors use).
        fheroes2::Point fieldPoint( const int32_t x, const int32_t y ) const
        {
            if ( !bigView ) {
                return { x, y };
            }
            return { x * 5 / 4 - bigX, y * 5 / 4 - bigY };
        }

        // H1-INTENDED(D16): which hero tent (side) is under a field point in the 0.8 view, -1 none.
        int tentAt( const fheroes2::Point & p ) const
        {
            for ( int s = 0; s < 2; ++s ) {
                if ( bigView && hasHero[s] && p.x >= tentRect[s].x && p.x < tentRect[s].x + tentRect[s].width && p.y >= tentRect[s].y
                     && p.y < tentRect[s].y + tentRect[s].height ) {
                    return s;
                }
            }
            return -1;
        }

        // H1-INTENDED(D16): the border picture of the 0.8 view, made once a battle.
        void buildSurround( const int group, const std::string & backgroundName, const uint32_t seed )
        {
            surround = fheroes2::Image( bigW, bigH );
            surround._disableTransformLayer();
            fheroes2::Fill( surround, 0, 0, bigW, bigH, 0 );
            // Props and tents go on a layer over the field: behind it the field's bottom hex row cut the bushes under it (owner
            // 2026-11-18 08:52).
            surroundFront = fheroes2::Image( bigW, bigH );
            surroundFront.reset();
            uint32_t state = seed * 2654435761u + 7u;
            const auto rnd = [&state]( const int low, const int high ) {
                state = state * 1103515245u + 12345u;
                return low + static_cast<int>( ( state >> 16 ) % static_cast<uint32_t>( high - low + 1 ) );
            };
            const auto cellPoint = []( const int c, const int r ) {
                return fheroes2::Point( 0x1b + c * 0x4e - ( ( ( r % 2 ) + 2 ) % 2 == 0 ? 0x27 : 0 ) + bigX, r * 0x50 + 0x8b + bigY );
            };
            // More ground hexes around the field (under the field too, so no gap shows where the field's edge hexes are cut).
            for ( int r = -1; r < 8; ++r ) {
                for ( int c = -3; c < 12; ++c ) {
                    const fheroes2::Point p = cellPoint( c, r );
                    put( surround, *xtl, rnd( 4, 7 ), p.x, p.y, false );
                }
            }
            // The battle background (640 x 102) a little higher than in the normal view (the forest edge just above the top hex row);
            // its top rows stretched to the picture's top, the sides mirrored.
            const int32_t top = bigY - 42;
            if ( !background.empty() ) {
                fheroes2::Image band( 640, 102 + top );
                band._disableTransformLayer();
                fheroes2::Copy( background, 0, 0, band, 0, top, 640, std::min( background.height(), 102 ) );
                fheroes2::Resize( background, 0, 0, 640, 60, band, 0, 0, 640, 60 + top );
                fheroes2::Copy( band, 0, 0, surround, bigX, 0, 640, band.height() );
                for ( int32_t x = 0; x < bigX; ++x ) {
                    fheroes2::Copy( band, x, 0, surround, bigX - 1 - x, 0, 1, band.height() );
                    fheroes2::Copy( band, 639 - x, 0, surround, bigX + 640 + x, 0, 1, band.height() );
                }
            }
            // Props: key < 100 = frame of the ground's .obj, 100 + k = HoMM2 COBJ k (h1/ext/BTLPROPS.ICN). side = beside the field,
            // bottom = under the bottom row (low things only), extra = (key, x, bottom y) of the 800 x 555 picture (battle_presets_v2.py).
            struct Preset
            {
                std::vector<int> side;
                std::vector<int> bottom;
                std::vector<std::array<int, 3>> extra;
            };
            Preset preset;
            const std::string & b = backgroundName;
            if ( b == "gravyard.bkg" ) {
                preset = { { 127, 126, 101, 100 }, { 124, 107, 124, 112 }, { { { 101, 770, 200 } }, { { 100, 30, 300 } } } };
            }
            else if ( b == "swamp.bkg" ) {
                preset = { { 0, 113, 109, 117 }, { 108, 112, 117, 116 }, { { { 116, 765, 250 } } } };
            }
            else if ( b == "snowfrst.bkg" || b == "snowmtnf.bkg" ) {
                preset = { { 1, 1, 122, 105 }, { 106, 0, 107, 0 }, { { { 122, 30, 330 } } } };
            }
            else if ( b == "desert.bkg" ) {
                preset = { { 103, 125, 111, 1, 103 }, { 110, 123, 124, 110 }, { { { 103, 770, 160 } }, { { 125, 25, 330 } } } };
            }
            else if ( b == "lava.bkg" ) {
                preset = { { 129, 131, 0, 129 }, { 130, 1, 107 }, { { { 130, 765, 300 } } } };
            }
            else if ( b == "mtnwgrsf.bkg" || b == "mtnwdrtf.bkg" ) {
                preset = { { 105, 119, 105, 128 }, { 112, 106, 108, 104 }, { { { 119, 770, 300 } } } };
            }
            else if ( group == 6 ) {
                preset = { { 128, 128, 102, 127 }, { 104, 112, 106, 108 }, { { { 111, 760, 470 } }, { { 103, 770, 300 } } } };
            }
            else {
                preset = { { 2, 2, 128 }, { 104, 112, 106, 108 }, { { { 111, 760, 470 } }, { { 103, 770, 300 } } } };
            }
            struct Placed
            {
                int32_t y;
                int key;
                int32_t x;
                bool low = false; // under the bottom row: its top kept below the feet of that row's troops (owner 08:52)
            };
            std::vector<Placed> props;
            for ( int r = 0; r < 7; ++r ) {
                for ( const int c : { -1, 8 } ) {
                    if ( r < 2 && ( ( c == -1 && hasHero[1] ) || ( c == 8 && hasHero[0] ) ) ) {
                        continue; // hero tent there
                    }
                    const fheroes2::Point p = cellPoint( c, r );
                    const int32_t y = p.y + 12 + rnd( -12, 12 );
                    const int key = preset.side[static_cast<size_t>( rnd( 0, static_cast<int>( preset.side.size() ) - 1 ) )];
                    props.push_back( { y, key, p.x + rnd( -10, 10 ) + ( c == 8 ? 30 : -20 ) } );
                }
            }
            const int bottomCount = 6;
            for ( int k = 0; k < bottomCount; ++k ) {
                const int32_t y = bigH - 5 - rnd( 0, 8 );
                const int key = preset.bottom[static_cast<size_t>( rnd( 0, static_cast<int>( preset.bottom.size() ) - 1 ) )];
                props.push_back( { y, key, bigX + 60 + k * ( 640 - 120 ) / ( bottomCount - 1 ) + rnd( -20, 20 ), true } );
            }
            for ( const auto & e : preset.extra ) {
                props.push_back( { e[2], e[0], e[1] } );
            }
            std::stable_sort( props.begin(), props.end(), []( const Placed & a, const Placed & b2 ) { return a.y < b2.y; } );
            const std::vector<fheroes2::Sprite> & cobj = H1::getIcn( "btlprops.icn" );
            const int32_t bottomFeet = 4 * 0x50 + 0x8b + bigY;
            for ( const Placed & p : props ) {
                if ( p.key >= 100 && p.key - 100 < static_cast<int>( cobj.size() ) ) {
                    const int32_t h = cobj[p.key - 100].height();
                    put( surroundFront, cobj, p.key - 100, p.x, p.low ? std::max( p.y, bottomFeet + 4 + h ) : p.y, false );
                }
                else if ( p.key < 100 && obj != nullptr && p.key < static_cast<int>( obj->size() ) ) {
                    const fheroes2::Sprite & s = ( *obj )[p.key];
                    const int32_t y = p.low ? std::max( p.y, bottomFeet + 4 + s.height() ) : p.y;
                    fheroes2::Blit( s, surroundFront, p.x - s.width() / 2, y - s.height() );
                }
            }
            // Whole hero tents at the forest corners (side 1 left, side 0 right, mirrored).
            const std::vector<fheroes2::Sprite> & tents = H1::getIcn( "tentw.icn" );
            for ( int s = 0; s < 2; ++s ) {
                tentRect[s] = { -10000, -10000, 0, 0 };
                if ( !hasHero[s] || heroes[s] == nullptr ) {
                    continue;
                }
                const int cls = H1::heroClass( *heroes[s] );
                const int index = H1::pictureClass( cls ) * 5 + ( heroColour[s] >= 0 && heroColour[s] < 4 ? heroColour[s] + 1 : 0 ); // H1-INTENDED(D16): necromancer = warlock tent
                if ( cls < 0 || index >= static_cast<int>( tents.size() ) ) {
                    continue;
                }
                const fheroes2::Sprite & t = tents[index];
                const int32_t ty = bigY - 8;
                const int32_t tx = ( s == 1 ) ? 0 : bigW - t.width();
                fheroes2::Blit( t, surroundFront, tx, ty, s == 0 );
                tentRect[s] = { tx - bigX, ty - bigY, t.width(), t.height() };
            }
        }

        // H1-INTENDED(D16): 800 x 555 -> 640 x 444 (4 out of every 5 pixels; each new pixel mixes the 2 x 2 old ones it covers, then
        // takes the nearest HoMM1 colour, leaving out 224..255 which HoMM1 cycles).
        static void shrinkPicture( const fheroes2::Image & big, fheroes2::Image & out )
        {
            static std::vector<uint8_t> nearest( 64 * 64 * 64, 0xff );
            const std::vector<uint8_t> & pal = H1::getPalette();
            if ( pal.size() < 768 ) {
                return;
            }
            static const int weight[4][2] = { { 4, 1 }, { 3, 2 }, { 2, 3 }, { 1, 4 } };
            const uint8_t * in = big.image();
            uint8_t * dst = out.image();
            const int32_t inW = big.width();
            const int32_t outW = out.width();
            for ( int32_t y = 0; y < 444; ++y ) {
                const int32_t by = ( y / 4 ) * 5 + y % 4;
                const int * wy = weight[y % 4];
                for ( int32_t x = 0; x < 640; ++x ) {
                    const int32_t bx = ( x / 4 ) * 5 + x % 4;
                    const int * wx = weight[x % 4];
                    int r = 0;
                    int g = 0;
                    int b = 0;
                    for ( int j = 0; j < 2; ++j ) {
                        for ( int i = 0; i < 2; ++i ) {
                            const uint8_t v = in[( by + j ) * inW + bx + i];
                            const int w = wx[i] * wy[j];
                            r += pal[v * 3] * w;
                            g += pal[v * 3 + 1] * w;
                            b += pal[v * 3 + 2] * w;
                        }
                    }
                    r = std::min( 63, ( r + 12 ) / 25 );
                    g = std::min( 63, ( g + 12 ) / 25 );
                    b = std::min( 63, ( b + 12 ) / 25 );
                    uint8_t & n = nearest[static_cast<size_t>( ( r << 12 ) | ( g << 6 ) | b )];
                    if ( n == 0xff ) {
                        int best = 1 << 30;
                        for ( int k = 0; k < 224; ++k ) {
                            const int dr = pal[k * 3] - r;
                            const int dg = pal[k * 3 + 1] - g;
                            const int db = pal[k * 3 + 2] - b;
                            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                            if ( d < best ) {
                                best = d;
                                n = static_cast<uint8_t>( k );
                            }
                        }
                    }
                    dst[y * outW + x] = n;
                }
            }
        }

        void redraw()
        {
            if ( !visible ) {
                return;
            }
            fheroes2::Display & display = fheroes2::Display::instance();
            const int32_t ox = ( display.width() - 640 ) / 2;
            const int32_t oy = ( display.height() - 480 ) / 2;
            fheroes2::Image frame( 640, 480 );
            if ( bigView ) {
                frame.reset(); // H1-INTENDED(D16): see-through where the field draws nothing (the border picture shows there)
            }
            else {
                frame._disableTransformLayer();
                fheroes2::Fill( frame, 0, 0, 640, 480, 0 );
            }
            const std::vector<fheroes2::Sprite> & tent = H1::getIcn( "tent.icn" );
            for ( int r = 0; r < 5; ++r ) {
                for ( int c = 0; c < 9; ++c ) {
                    const Cell & cell = cells[r * 9 + c];
                    // H1-INTENDED(D16): in the 0.8 view the edge columns (whose hexes end the field at the screen side) come from the border
                    // picture, plain ground with the props beside the field.
                    if ( !( bigView && ( c == 0 || c == 8 ) ) ) {
                        put( frame, *xtl, cell.ground, cell.x, cell.y, false );
                    }
                }
                drawView( frame, r );
                if ( siege ) {
                    // FUN_00438de7: the wall's foot, castle frame 5 (odd rows) / 6 (even rows) at the wall cell.
                    put( frame, castleIcn(), ( r & 1 ) == 0 ? 6 : 5, cells[r * 9 + 5].x, cells[r * 9 + 5].y, false );
                }
                if ( r == 0 && !bigView ) {
                    fheroes2::Copy( background, 0, 0, frame, 0, 0, std::min( background.width(), 640 ), std::min( background.height(), 460 ) );
                    if ( siege ) {
                        put( frame, castleIcn(), 7, 0x27f, 0x8b, false );
                        put( frame, castleIcn(), 7, 0x211, 0x8b, false );
                    }
                }
            }
            for ( int r = 0; r < 5; ++r ) {
                if ( r == 3 && siege ) {
                    // FUN_00438fbb: the catapult (frame 0 at rest) [its shadow frame 0xf, drawn by FUN_0047a730, is left out].
                    put( frame, H1::getIcn( "catapult.icn" ), 0, 0x1b, 0x17b, false );
                }
                for ( int c = 0; c < 9; ++c ) {
                    const Cell & cell = cells[r * 9 + c];
                    if ( siege && c == 5 ) {
                        // FUN_0044f700 / FUN_0044f2fc: a standing wall (8, or 10 when cracked) flipped at x + 0x1c; joint frame 9 below it.
                        if ( cell.obstacle == 8 || cell.obstacle == 10 ) {
                            put( frame, castleIcn(), cell.obstacle, cell.x + 0x1c, cell.y, true );
                            if ( r != 4 ) {
                                if ( ( r & 1 ) == 0 ) {
                                    put( frame, castleIcn(), 9, cell.x, cell.y, false );
                                }
                                else {
                                    put( frame, castleIcn(), 9, cell.x + 0x1c, cell.y, true );
                                }
                            }
                        }
                        continue;
                    }
                    if ( cell.obstacle >= 0 ) {
                        put( frame, *obj, cell.obstacle, cell.x, cell.y, false );
                    }
                }
                if ( r == 0 && siege ) {
                    char name[16];
                    std::snprintf( name, sizeof( name ), "keep%02d.icn", castleRace );
                    put( frame, H1::getIcn( name ), 0, 0x22d, 0, false );
                }
                for ( int c = 1; c < 8; ++c ) {
                    const Cell & cell = cells[r * 9 + c];
                    if ( cell.side < 0 ) {
                        continue;
                    }
                    const Unit & u = units[cell.side][cell.slot];
                    if ( u.cell != r * 9 + c ) {
                        continue;
                    }
                    drawUnit( frame, u, cell.side == current && cell.slot == currentSlot );
                }
                if ( r == 1 && heroes[1] != nullptr && !bigView ) {
                    const int cls = H1::heroClass( *heroes[1] );
                    if ( cls >= 0 ) {
                        put( frame, tent, H1::pictureClass( cls ), 0, 0x59, false );
                        if ( heroColour[1] >= 0 ) {
                            put( frame, tent, heroColour[1] + 4, 0x2f, 0x9b, false );
                        }
                        put( frame, tent, H1::pictureClass( cls ) + 8, 0x36, 0xad, false );
                    }
                }
                if ( r == 2 && heroes[0] != nullptr && !bigView ) {
                    const int cls = H1::heroClass( *heroes[0] );
                    if ( cls >= 0 ) {
                        put( frame, tent, H1::pictureClass( cls ), 0x27f, 0xa9, true );
                        if ( heroColour[0] >= 0 ) {
                            put( frame, tent, heroColour[0] + 4, 0x250, 0xeb, true );
                        }
                        put( frame, tent, H1::pictureClass( cls ) + 8, 0x238, 0xfd, false );
                    }
                }
            }
            if ( projectileFrame >= 0 && projectileCreature >= 0 ) {
                put( frame, H1::getIcn( std::string( H1::creatureFile( projectileCreature ) ) + ".atk" ), projectileFrame, projectileX, projectileY,
                     projectileMirror );
            }
            if ( bigView ) {
                // H1-INTENDED(D16): field onto the border picture, shrunk to 0.8, then the bar.
                fheroes2::Image big( surround );
                fheroes2::Blit( frame, 0, 0, big, bigX, bigY, 640, 480 );
                fheroes2::Blit( surroundFront, big );
                fheroes2::Image out( 640, 480 );
                out._disableTransformLayer();
                fheroes2::Fill( out, 0, 0, 640, 480, 0 );
                shrinkPicture( big, out );
                drawBar( out, barPressed );
                fheroes2::Copy( out, 0, 0, display, ox, oy, 640, 480 );
                display.render();
                return;
            }
            drawBar( frame, barPressed );
            fheroes2::Copy( frame, 0, 0, display, ox, oy, 640, 480 );
            display.render();
        }

        const std::vector<fheroes2::Sprite> & castleIcn() const
        {
            char name[16];
            std::snprintf( name, sizeof( name ), "castle%02d.icn", castleRace );
            return H1::getIcn( name );
        }

        void setCursor( const int frameIndex )
        {
            // cmbtmous.mse: the frame of cmbtmous.icn with its hotspot (note v159).
            H1::setMouseCursor( "cmbtmous.mse", frameIndex );
        }

        // Waits for the player's choice for the acting unit (FUN_00449d70 / FUN_0044af80); sets action.
        void playerChoice()
        {
            playerChoiceLoop();
            // [added view] the shadows belong to the choice only.
            moveShade.fill( false );
            cursorShade = -1;
        }

        void playerChoiceLoop()
        {
            action = ACTION_NONE;
            actionCell = -1;
            actionStand = -1;
            LocalEvent & le = LocalEvent::Get();
            fheroes2::Display & display = fheroes2::Display::instance();
            const int32_t ox = ( display.width() - 640 ) / 2;
            const int32_t oy = ( display.height() - 480 ) / 2;
            int hoverCell = -2;
            int hoverType = 0;
            int lastCursor = -1;
            cursorShade = -1;
            computeMoveShade();
            redraw();
            while ( le.HandleEvents() ) {
                const fheroes2::Point & m = le.getMouseCursorPos();
                const int32_t mx = m.x - ox;
                const int32_t my = m.y - oy;
                const fheroes2::Point fp = fieldPoint( mx, my ); // H1-INTENDED(D16): 0.8 view
                int cursorFrame = 6;
                if ( my < 443 ) {
                    const int tentSide = tentAt( fp ); // H1-INTENDED(D16): the tents of the 0.8 view stand for the hero cells 9 / 0x1a
                    const int c = ( tentSide == 1 ) ? 9 : ( tentSide == 0 ? 0x1a : cellAt( fp.x, fp.y ) );
                    if ( c != hoverCell ) {
                        hoverCell = c;
                        if ( viewOptions() & 4 ) {
                            const int shade = ( c >= 0 && c < 45 ) ? c : -1;
                            if ( shade != cursorShade ) {
                                cursorShade = shade;
                                redraw();
                            }
                        }
                        lastSector = -1;
                        hoverType = cursorFor( c );
                        if ( hoverType == 7 ) {
                            computeSectors( c );
                        }
                    }
                    if ( hoverType == 7 ) {
                        const int f = pointSector( fp.x, fp.y, c );
                        cursorFrame = ( f >= 0 ) ? f : lastSector + 7;
                    }
                    else {
                        cursorFrame = ( hoverType == 13 ) ? 5 : hoverType;
                    }
                }
                else {
                    hoverCell = -2;
                    if ( cursorShade != -1 ) {
                        cursorShade = -1;
                        redraw();
                    }
                }
                if ( cursorFrame != lastCursor ) {
                    setCursor( cursorFrame );
                    lastCursor = cursorFrame;
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_H ) && hasHero[current] ) {
                    // FUN_0044af80 key 0x23: own hero window.
                    heroWindow( current );
                    if ( action != ACTION_NONE ) {
                        return;
                    }
                    hoverCell = -2;
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_C ) ) {
                    // FUN_0044af80 key 0x2e: cast a spell.
                    if ( playerSpell() ) {
                        return;
                    }
                    hoverCell = -2;
                    redraw();
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_SPACE ) ) {
                    action = ACTION_WAIT;
                    return;
                }
                if ( H1::isExtension() && le.isKeyPressed( fheroes2::Key::KEY_W ) && !( acting().flags & FLAG_WAITED ) ) {
                    action = ACTION_DELAY; // H1-INTENDED(D16)
                    return;
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_T ) ) {
                    // FUN_0044af80 key 0x14: the acting unit's creature window.
                    unitInfo( current, currentSlot, false );
                    hoverCell = -2;
                    lastCursor = -1;
                }
                if ( le.isMouseRightButtonPressed() && my < 443 ) {
                    // FUN_0044af80 event 0x0c with bit 0x200 -> FUN_0044bae8( cell under the cursor ): hero cells 9 / 0x1a show that side's
                    // hero window, a cell with a unit (no obstacle, side 0 or 1) its creature window, both as right-click views.
                    const int tentSide = tentAt( fp ); // H1-INTENDED(D16): the tents of the 0.8 view
                    const int c = ( tentSide == 1 ) ? 9 : ( tentSide == 0 ? 0x1a : cellAt( fp.x, fp.y ) );
                    if ( c == 9 ) {
                        if ( hasHero[1] ) {
                            heroWindow( 1, true );
                        }
                    }
                    else if ( c == 0x1a ) {
                        if ( hasHero[0] ) {
                            heroWindow( 0, true );
                        }
                    }
                    else if ( c >= 0 && c % 9 != 8 && validCell( c ) && cells[c].obstacle == -1 && ( cells[c].side == 0 || cells[c].side == 1 ) ) {
                        unitInfo( cells[c].side, cells[c].slot, true );
                    }
                    hoverCell = -2;
                    lastCursor = -1;
                }
                if ( le.MouseClickLeft() ) {
                    if ( my >= 443 ) {
                        // [added] HoMM2 bar in place of cmbtwin.bin (whose AUTO is x < 0x33 and SKIP x >= 0x24e): same actions, plus OPTIONS.
                        const int button = barButtonAt( mx, my );
                        if ( button != 0 ) {
                            showPressed( button );
                        }
                        if ( button == 2 ) {
                            autoCombat = true;
                            cursorShade = -1;
                            return;
                        }
                        if ( button == 1 ) {
                            action = ACTION_WAIT;
                            cursorShade = -1;
                            return;
                        }
                        if ( button == 3 ) {
                            optionsWindow();
                            computeMoveShade();
                            hoverCell = -2;
                            lastCursor = -1;
                            redraw();
                        }
                        continue;
                    }
                    switch ( hoverType ) {
                    case 1:
                    case 2:
                    case 3:
                        action = ACTION_MOVE;
                        actionCell = hoverCell;
                        return;
                    case 7:
                        actionCell = hoverCell;
                        if ( human[current] && !autoCombat ) {
                            action = ACTION_MOVE_ATTACK;
                            actionStand = standCell;
                        }
                        else {
                            action = ACTION_MOVE;
                        }
                        return;
                    case 4:
                        heroWindow( current );
                        if ( action != ACTION_NONE ) {
                            return;
                        }
                        hoverCell = -2;
                        break;
                    case 5:
                        // FUN_0044bcc4 case 5: a unit of the acting side - its creature window.
                        unitInfo( current, cells[hoverCell].slot, false );
                        hoverCell = -2;
                        lastCursor = -1;
                        break;
                    case 13:
                        heroWindow( 1 - current );
                        hoverCell = -2;
                        break;
                    default:
                        break;
                    }
                }
            }
            action = ACTION_WAIT;
        }

        // FUN_0044da67: surrender price = for each living unit of the acting side (creature gold / 2) x number; window surrendr.bin at
        // (0x55, 0x50) with the enemy hero's portrait. Returns true for Accept (0x8002).
        bool askSurrender()
        {
            surrenderCost = 0;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = units[current][k];
                if ( u.exists && u.count > 0 ) {
                    surrenderCost += ( H1::creatureRow( u.creature ).gold / 2 ) * u.count;
                }
            }
            if ( sideHas( current, 51 ) ) {
                // H1-INTENDED(D16): Statesman's Quill - one tenth of the army's value instead of half.
                surrenderCost /= 5;
            }
            const Heroes * enemyHero = heroes[1 - current];
            if ( enemyHero == nullptr ) {
                return false;
            }
            H1::UI::Layout layout = H1::UI::loadLayout( "surrendr.bin" );
            char name[16];
            std::snprintf( name, sizeof( name ), "port%04d.icn", std::max( 0, H1::heroNumberOfId( enemyHero->getPortraitId() ) ) );
            if ( H1::UI::Item * portrait = layout.find( 1 ) ) {
                portrait->name = name;
            }
            if ( H1::UI::Item * text = layout.find( 2 ) ) {
                // Original text (0x4a1570), including its missing closing quote.
                text->text = enemyHero->GetName() + " states:\n\n\"I will accept your surrender and grant you and your troops safe passage for the price of "
                             + std::to_string( surrenderCost ) + " gold.";
            }
            return H1::UI::runLayoutDialog( layout, 0x55, 0x50, false ) == 0x8002;
        }

        // FUN_00410e9f: the creature window (FUN_00417c59) for a battle unit, next to it: x = cell x (+0x40) - 0x50 (- 0x2b more when it
        // looks left), 0 when below 0, 0x97 when x + 0x1e8 > 0x280; y = cell y (+0x42) - 0xa4, 0 when below 0, 0xe6 when y + 0xe5 > 0x1cc.
        // No dismiss (param_6 = 1); popup = the right-click view (FUN_0044bae8), else the left-click window with its Exit (note v192).
        void unitInfo( const int side, const int slot, const bool popup )
        {
            if ( side < 0 || side > 1 || slot < 0 || slot > 4 ) {
                return;
            }
            const Unit & u = units[side][slot];
            if ( !u.exists || u.count < 1 || u.cell < 0 || u.creature < 0 ) {
                return;
            }
            const Cell & cell = cells[u.cell];
            H1::BattleUnitView view;
            view.x = cell.x - ( ( u.facing == 1 ? 0x2b : 0 ) + 0x50 );
            if ( view.x < 0 ) {
                view.x = 0;
            }
            if ( 0x280 < view.x + 0x1e8 ) {
                view.x = 0x97;
            }
            view.y = cell.y - 0xa4;
            if ( view.y < 0 ) {
                view.y = 0;
            }
            if ( 0x1cc < view.y + 0xe5 ) {
                view.y = 0xe6;
            }
            view.facing = u.facing;
            view.shotsLeft = u.shots;
            view.spell9 = ( u.spell == 9 );
            const int monsterId = H1::creatureRow( u.creature ).monsterId;
            H1::showCreatureWindow( monsterId, u.count, heroes[side], sideArmy[side], true, popup, side == 0 ? defenderCastle : nullptr, 0x14, &view );
            redraw();
        }

        // FUN_00410710: hero window vgenwin.bin at (0xc3, 0x3c) - portrait, flag, "<name> the <class>", attack / defense / spell power /
        // knowledge / morale / luck; buttons 10 Cast Spell, 11 Retreat, 12 Surrender greyed when not allowed; then FUN_0044bcc4 on the button.
        void heroWindow( const int side, const bool popup = false )
        {
            const Heroes * hero = heroes[side];
            if ( hero == nullptr ) {
                return;
            }
            H1::UI::Layout layout = H1::UI::loadLayout( "vgenwin.bin" );
            char name[16];
            std::snprintf( name, sizeof( name ), "port%04d.icn", std::max( 0, H1::heroNumberOfId( hero->getPortraitId() ) ) );
            if ( H1::UI::Item * portrait = layout.find( 2 ) ) {
                portrait->name = name;
            }
            if ( H1::UI::Item * flag = layout.find( 3 ) ) {
                flag->frame = heroColour[side] + 1; // player colour + 1
            }
            static const char * const classNames[5] = { "Knight", "Barbarian", "Sorceress", "Warlock", "Necromancer" }; // H1-INTENDED(D16)
            static const char * const moraleNames[7] = { "Treason", "Awful", "Poor", "Normal", "Good", "Great", "Blood!" };
            static const char * const luckNames[7] = { "Cursed", "Awful", "Bad", "Normal", "Good", "Great", "Irish" };
            const int cls = H1::heroClass( *hero );
            if ( H1::UI::Item * title = layout.find( 1 ) ) {
                title->text = hero->GetName() + " the " + ( cls >= 0 ? classNames[cls] : "" );
            }
            if ( H1::UI::Item * stats = layout.find( 4 ) ) {
                stats->text = "\nAttack: " + std::to_string( hero->GetAttack() ) + "\nDefense: " + std::to_string( hero->GetDefense() ) + "\nSpell Power: "
                              + std::to_string( hero->GetPower() ) + "\nKnowledge: " + std::to_string( hero->GetKnowledge() ) + "\nMorale: "
                              + moraleNames[std::clamp( morale[side], -3, 3 ) + 3] + "\nLuck: " + luckNames[std::clamp( luck[side], -3, 3 ) + 3] + "\n";
            }
            auto grey = [&layout]( const int id ) {
                if ( H1::UI::Item * button = layout.find( id ) ) {
                    button->greyed = true;
                    button->inactive = true;
                }
            };
            // FUN_00410710( side, buttons, popup ): a right click (FUN_0044bae8) passes buttons = 0, popup = 1: all three greyed, shown while
            // the button is held.
            const bool myTurn = ( side == current ) && !popup;
            if ( !hero->HaveSpellBook() || castCast[side] || !myTurn ) {
                grey( 10 );
            }
            if ( heroes[1 - current] == nullptr || !myTurn ) {
                grey( 12 );
            }
            if ( !myTurn || ( side == 0 && castleDefender ) || fled[0] || fled[1] ) {
                grey( 11 );
            }
            const int pressed = H1::UI::runLayoutDialog( layout, 0xc3, 0x3c, popup );
            redraw();
            if ( popup ) {
                return;
            }
            switch ( pressed ) {
            case 10:
                playerSpell();
                redraw();
                break;
            case 11:
                if ( H1::UI::messageBox( "Are you sure you want to retreat?", H1::UI::MESSAGE_YES_NO, 0xc3, 0x3c ) == 0x8005 ) {
                    action = ACTION_RETREAT;
                }
                redraw();
                break;
            case 12:
                if ( askSurrender() ) {
                    if ( static_cast<int>( hero->GetKingdom().GetFunds().gold ) < surrenderCost ) {
                        H1::UI::messageBox( "You don't have enough gold!", H1::UI::MESSAGE_OKAY );
                    }
                    else {
                        action = ACTION_SURRENDER;
                    }
                }
                redraw();
                break;
            default:
                break;
            }
        }

        // FUN_0044e18c: carries out the action; returns true when the acting unit's turn is over.
        bool carryOut()
        {
            Unit & u = acting();
            switch ( action ) {
            case ACTION_SPELL:
                // Casting does not end the unit's turn (unless the unit died, v88).
                castSpell( actionSpell, actionCell, false, actionCell2 );
                return u.count < 1;
            case ACTION_MOVE:
                moveOrAttack( u, actionCell, false );
                u.flags |= FLAG_DONE;
                {
                    int w = 0;
                    if ( isOver( w ) ) {
                        return true;
                    }
                }
                goodMorale( current, currentSlot );
                return true;
            case ACTION_WAIT:
                u.flags |= FLAG_DONE;
                return true;
            case ACTION_DELAY:
                // H1-INTENDED(D16): back of the round (selectWaiting), once a round.
                u.flags |= FLAG_WAITING | FLAG_WAITED;
                setMessage( unitName( u ) + " wait." );
                return true;
            case ACTION_RETREAT:
                fled[current] = true;
                return false;
            case ACTION_SURRENDER:
                surrendered[current] = true;
                return false;
            case ACTION_MOVE_ATTACK:
                lastWalked = 0;
                if ( actionStand != -1 && u.cell != actionStand ) {
                    moveOrAttack( u, actionStand, true );
                }
                // H1-INTENDED(D16): cavalry charge also for a person's attack (walk to the chosen cell, then strike).
                chargeUnit = &u;
                chargeCells = lastWalked;
                moveOrAttack( u, actionCell, false );
                chargeUnit = nullptr;
                chargeCells = 0;
                u.flags |= FLAG_DONE;
                {
                    int w = 0;
                    if ( isOver( w ) ) {
                        return true;
                    }
                }
                goodMorale( current, currentSlot );
                return true;
            default:
                return false;
            }
        }
    };

    void fillUnits( Fight & fight, const int side, const Army & army )
    {
        const Heroes * hero = dynamic_cast<const Heroes *>( army.GetCommander() );
        for ( int k = 0; k < 5; ++k ) {
            Unit & u = fight.units[side][k];
            u = Unit();
            const Troop * troop = army.GetTroop( static_cast<size_t>( k ) );
            if ( troop == nullptr || !troop->isValid() ) {
                continue;
            }
            const int creature = H1::creatureFromMonster( troop->GetID() );
            if ( creature < 0 || creature >= H1::creatureCount() ) { // H1-INTENDED(D16): 36 in extension games
                continue;
            }
            const H1::CreatureRow & row = H1::creatureRow( creature );
            // FUN_004069c2.
            u.exists = true;
            u.creature = creature;
            u.armySlot = k;
            u.side = side;
            u.slot = k;
            u.count = static_cast<int>( troop->GetCount() );
            u.startCount = u.count;
            u.hp = row.hp;
            u.speed = row.speedLevel;
            u.baseSpeed = u.speed;
            u.attack = row.attack + ( hero != nullptr ? hero->GetAttack() : 0 );
            u.defense = row.defense + ( hero != nullptr ? hero->GetDefense() : 0 );
            u.damageMin = row.damageMin;
            u.damageMax = row.damageMax;
            u.shots = row.shots;
            u.flags = static_cast<uint32_t>( row.flags ) & 0xf;
            u.facing = side ^ 1;
            u.cell = k * 9 + ( side == 1 ? 1 : 7 );
            if ( u.flags & FLAG_TWO_HEX ) {
                // Side 1 (looking right): the other cell is column 2; side 0: column 6.
                u.cell = k * 9 + ( side == 1 ? 1 : 7 );
            }
        }
    }

    // FUN_0044c5a4: "Battlefield Casualties" (y 0x104), then for side 1 "Attacker" (y 0x118) and side 0 "Defender" (y 0x159): the creatures
    // that lost men (mons32.icn, 0x28 apart, centred in 0x140) with the number lost under them, or "None".
    void addCasualties( H1::UI::Layout & layout, const Fight & fight )
    {
        auto text = [&layout]( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const std::string & s ) {
            H1::UI::Item it;
            it.type = H1::UI::ITEM_TEXT;
            it.x = x;
            it.y = y;
            it.w = w;
            it.h = h;
            it.name = "smalfont.fnt";
            it.colour = 1; // [the original passes 1 and 0x83e here; read as colour 1, centred]
            it.align = 1;
            it.text = s;
            layout.items.push_back( it );
        };
        text( 0, 0x104, 0x140, 0x14, "Battlefield Casualties" );
        for ( int side = 1; side >= 0; --side ) {
            const int32_t y = ( side == 1 ) ? 0x118 : 0x159;
            text( 0, y, 0x140, 0x14, side == 1 ? "Attacker" : "Defender" );
            std::vector<std::pair<int, int>> lost;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = fight.units[side][k];
                if ( u.exists && u.count < u.startCount ) {
                    lost.emplace_back( u.creature, u.startCount - u.count );
                }
            }
            if ( lost.empty() ) {
                text( 0, y + 0x12, 0x140, 0x14, "None" );
                continue;
            }
            const int32_t left = ( static_cast<int32_t>( lost.size() ) * -0x28 + 0x140 ) / 2 + 3;
            for ( size_t k = 0; k < lost.size(); ++k ) {
                const int32_t x = static_cast<int32_t>( k ) * 0x28 + left;
                H1::UI::Item icon;
                icon.type = H1::UI::ITEM_ICON;
                icon.x = x;
                icon.y = y + 0xf;
                icon.w = 0x20;
                icon.h = 0x1c;
                icon.name = H1::smallCreatureIcn( lost[k].first ); // H1-INTENDED(D16): 28..35 from h1/ext/mons32x.icn
                icon.frame = H1::smallCreatureFrame( lost[k].first );
                layout.items.push_back( icon );
                text( x, y + 0x2e, 0x20, 0xc, std::to_string( lost[k].second ) );
            }
        }
    }

    // FUN_0044cd7d (win, wincmbt.bin, picture id 1 cycling frames 1..6 every 90 ms) and FUN_0044d51e (loss, losecmbt.bin with
    // losecmbt.bmp scrolling 2 pixels and losewalk.icn frames 0..7 every 180 ms), both at (0x9f, 2).
    // [Captured artifacts shown one by one after OKAY (FUN_0044c19a / FUN_0044c2aa): later step.]
    void showBattleEnd( const Fight & fight, const bool won, const std::string & message, const std::vector<int> & captured )
    {
        // FUN_0044cd7d: track 0x2c when the local human side won, 0x2b otherwise.
        H1::playMusic( won ? 0x2c : 0x2b );
        H1::UI::Layout layout = H1::UI::loadLayout( won ? "wincmbt.bin" : "losecmbt.bin" );
        H1::UI::Item * text = layout.find( 101 );
        if ( text != nullptr ) {
            text->text = message;
        }
        addCasualties( layout, fight );
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        const fheroes2::Image strip = won ? fheroes2::Image() : H1::UI::readBmp( "losecmbt.bmp" );
        const std::vector<fheroes2::Sprite> & walker = H1::getIcn( "losewalk.icn" );
        fheroes2::TimeDelay timer( won ? 90 : 180 );
        int frame = 0;
        int scroll = 0;
        int counter = 0;
        auto tick = [&]() {
            if ( !timer.isPassed() ) {
                return false;
            }
            timer.reset();
            if ( won ) {
                ++counter;
                H1::UI::Item * picture = layout.find( 1 );
                if ( picture != nullptr ) {
                    picture->frame = counter % 6 + 1;
                }
                return true;
            }
            if ( !strip.empty() ) {
                fheroes2::Copy( strip, scroll, 0, display, ox + 0xd0, oy + 0x28, std::min( 0xdf, strip.width() - scroll ), std::min( 0x7d, strip.height() ) );
            }
            if ( !walker.empty() ) {
                const fheroes2::Sprite & s = walker[static_cast<size_t>( frame ) % walker.size()];
                fheroes2::Blit( s, display, ox + 0x10e + s.x(), oy + 0x8c + s.y() );
            }
            display.render( fheroes2::Rect( ox + 0xd0, oy + 0x28, 0xdf, 0x7d ) );
            frame = ( frame + 1 ) & 7;
            scroll += 2;
            if ( scroll > 0x1a0 ) {
                scroll = 0;
            }
            return false;
        };
        H1::UI::runLayoutDialogTicking( layout, 0x9f, 2, tick );
        // FUN_0044bff7 / FUN_0044c19a / FUN_0044c2aa: after OKAY, each captured artifact in turn - "You have captured an enemy artifact!",
        // winloseb.icn 0 at (0x78, 0x136), artifact.icn frame = artifact at (0x80, 0x13e), its name in smalfont at (0, 0x18a, 0x140, 0xc).
        // [The pickup sound pickupNN.82M (NN = 1..5 at random) is not played yet.]
        const size_t baseItems = H1::UI::loadLayout( "wincmbt.bin" ).items.size();
        for ( const int artifact : captured ) {
            layout.items.resize( baseItems );
            if ( H1::UI::Item * t = layout.find( 101 ) ) {
                t->text = "You have captured an enemy artifact!";
            }
            H1::UI::Item frameIcon;
            frameIcon.type = H1::UI::ITEM_ICON;
            frameIcon.x = 0x78;
            frameIcon.y = 0x136;
            frameIcon.w = 0x50;
            frameIcon.h = 0x50;
            frameIcon.name = "winloseb.icn";
            frameIcon.frame = 0;
            layout.items.push_back( frameIcon );
            H1::UI::Item artIcon = frameIcon;
            artIcon.x = 0x80;
            artIcon.y = 0x13e;
            artIcon.w = 0x40;
            artIcon.h = 0x40;
            artIcon.name = "artifact.icn";
            artIcon.frame = artifact;
            layout.items.push_back( artIcon );
            H1::UI::Item name;
            name.type = H1::UI::ITEM_TEXT;
            name.x = 0;
            name.y = 0x18a;
            name.w = 0x140;
            name.h = 0xc;
            name.name = "smalfont.fnt";
            name.colour = 1;
            name.align = 1;
            name.text = H1::artifactName( artifact );
            layout.items.push_back( name );
            // FUN_0044c2aa: pickup01..05 (random 1..5) as the window is made [the original waits for its end before taking clicks].
            char sound[32];
            std::snprintf( sound, sizeof( sound ), "pickup%02d.82m", static_cast<int>( Rand::Get( 1, 5 ) ) );
            H1::playSound( sound );
            H1::UI::runLayoutDialogTicking( layout, 0x9f, 2, tick );
        }
    }
}

namespace H1
{
    int creatureWorthOf( const int creature )
    {
        return H1::creatureWorth( creature );
    }

    int armyStrengthFull( const int creatures[5], const int counts[5], const Heroes * hero, const int moraleValue, const int luckValue, const bool corrections,
                          const bool castle, const int castleBits )
    {
        // FUN_0042e009( army, hero, corrections, castle, castle number ) (notes v29, v95, v144, v186).
        H1::StrengthInput in = strengthInput( creatures, counts, hero, moraleValue, luckValue );
        in.corrections = corrections;
        in.castle = castle;
        in.castleBits = castleBits;
        return armyStrengthOriginal( in );
    }

    int armyStrengthOriginal( const StrengthInput & in )
    {
        // Constants of the machine code (0x42e009..0x42e76a): count bonus floats, x 0.7 (0x48d168) / x 1.2 (0x48d1d0) as doubles then stored
        // as a float, + 1.0f; hero table 0x4921e0 (attack + defense + 20, 0..40); spell power table 0x492288 (power, 5.0 above 40);
        // spell worth 0x490f58, flags 0x490f98, casts weight 0x492330; 3.5, 1.25, 1.5, 0.75, 0.9 as doubles. __ftol cuts towards zero.
        static const float heroTable[41] = { 0.63f, 0.63f, 0.63f, 0.63f, 0.63f, 0.63f, 0.64f, 0.65f, 0.67f, 0.68f, 0.7f,  0.72f, 0.74f, 0.76f,
                                             0.78f, 0.81f, 0.84f, 0.87f, 0.91f, 0.95f, 1.0f,  1.05f, 1.1f,  1.15f, 1.22f, 1.28f, 1.36f, 1.44f,
                                             1.53f, 1.63f, 1.74f, 1.86f, 1.99f, 2.14f, 2.3f,  2.48f, 2.67f, 2.86f, 2.86f, 2.86f, 2.86f };
        int army = 0;
        int spells = 0;
        int castlePart = 0;
        for ( int k = 0; k < 5; ++k ) {
            const int creature = in.creatures[k];
            if ( creature == -1 ) {
                continue;
            }
            const int n = in.counts[k];
            int value = H1::creatureWorth( creature ) * n;
            if ( in.corrections ) {
                float bonus = -0.14f;
                if ( n > 0xb4 ) bonus = 1.7f;
                else if ( n > 0x8c ) bonus = 1.3f;
                else if ( n > 0x64 ) bonus = 1.1f;
                else if ( n > 0x4b ) bonus = 0.95f;
                else if ( n > 0x32 ) bonus = 0.81f;
                else if ( n > 0x23 ) bonus = 0.57f;
                else if ( n > 0x17 ) bonus = 0.37f;
                else if ( n > 0x10 ) bonus = 0.25f;
                else if ( n > 0xb ) bonus = 0.13f;
                else if ( n > 8 ) bonus = 0.06f;
                else if ( n > 5 ) bonus = 0.0f;
                else if ( n > 3 ) bonus = -0.05f;
                else if ( n > 2 ) bonus = -0.1f;
                if ( ( H1::creatureRow( creature ).flags & 4 ) || creature == 0xc || creature == 0x18 ) {
                    bonus = static_cast<float>( bonus * 0.7 );
                }
                else if ( creature == 0x14 ) {
                    bonus = static_cast<float>( bonus * 1.2 );
                }
                value = static_cast<int>( ( static_cast<double>( bonus ) + 1.0 ) * value );
            }
            army += value;
        }
        if ( in.castle ) {
            int n = 5;
            for ( int b = 7; b < 0xd; ++b ) {
                if ( in.castleBits & ( 1 << b ) ) {
                    n += 4;
                }
            }
            for ( int b = 0; b < 5; ++b ) {
                if ( in.castleBits & ( 1 << b ) ) {
                    n += 1;
                }
            }
            castlePart = n * 0x78;
        }
        if ( in.corrections && in.hero ) {
            const double m = heroTable[std::clamp( in.attack + in.defense + 0x14, 0, 0x28 )];
            army = static_cast<int>( army * m );
            castlePart = static_cast<int>( castlePart * m );
            if ( in.morale > 0 ) {
                army = ( in.morale + 0x30 ) * army / 0x30;
            }
            else if ( in.morale < 0 ) {
                army = ( in.morale + 0x18 ) * army / 0x18;
            }
            if ( in.luck != 0 ) {
                army = ( in.luck + 0x10 ) * army / 16;
            }
            const float scale = ( in.power == 1 ) ? 0.25f : ( in.power == 2 ? 0.5f : 1.0f );
            int best = -1;
            for ( int k = 0; k < 29; ++k ) {
                const int spell = in.spells[k];
                if ( spell < 0 || spell > 28 || !( spellFlags[spell] & 2 ) ) {
                    continue;
                }
                const float multiplier = ( spellFlags[spell] & 1 ) ? ( in.power > 0x28 ? 5.0f : H1::attackDefenseMultiplier( in.power - 20 ) ) : scale;
                const int value = static_cast<int>( spellWorth[spell] * static_cast<double>( multiplier ) );
                const int casts = std::min( in.casts[k], 20 );
                spells = static_cast<int>( value * static_cast<double>( castsWeight[casts] ) + spells );
                if ( best < value ) {
                    best = value;
                }
            }
            if ( static_cast<double>( spells ) > best * 3.5 ) {
                spells = static_cast<int>( best * 3.5 );
            }
            if ( army * 2 < spells ) {
                spells = static_cast<int>( army * 1.25 );
            }
            else if ( static_cast<double>( spells ) > army * 1.5 ) {
                spells = army;
            }
            else if ( army < spells ) {
                spells = static_cast<int>( army * 0.75 );
            }
        }
        if ( army * 2 < castlePart ) {
            castlePart = static_cast<int>( army * 1.5 );
        }
        else if ( static_cast<double>( castlePart ) > army * 1.5 ) {
            castlePart = static_cast<int>( army * 1.25 );
        }
        else if ( army < castlePart ) {
            castlePart = static_cast<int>( army * 0.9 );
        }
        return army + spells + castlePart;
    }

    int armyStrengthOf( const Army & army, const Heroes * hero )
    {
        // FUN_0042e009 for an army on the map (the followers test, third argument 0).
        int creatures[5] = { -1, -1, -1, -1, -1 };
        int counts[5] = { 0, 0, 0, 0, 0 };
        for ( size_t k = 0; k < 5; ++k ) {
            const Troop * troop = army.GetTroop( k );
            if ( troop == nullptr || !troop->isValid() ) {
                continue;
            }
            for ( int c = 0; c < H1::creatureCount(); ++c ) { // H1-INTENDED(D16): 36 in extension games
                if ( H1::creatureRow( c ).monsterId == troop->GetID() ) {
                    creatures[k] = c;
                    counts[k] = static_cast<int>( troop->GetCount() );
                    break;
                }
            }
        }
        // FUN_00405804 (human) and FUN_0040599e (computer) call FUN_0042e009( hero army, hero, 0, 0, 0 ): with the third argument 0 only the
        // creatures' worth x count is summed (no count bonus, no hero part) - note v187.
        H1::StrengthInput in = strengthInput( creatures, counts, hero, 0, 0 );
        return armyStrengthOriginal( in );
    }
}

namespace H1
{
    bool runBattle( Army & attacker, Army & defender, const int32_t tileIndex, Battle::Result & result )
    {
        const uint8_t * tile = getTileBytes( tileIndex );
        if ( tile == nullptr ) {
            return false;
        }
        const int32_t mapX = tileIndex % 72;
        const int32_t mapY = tileIndex / 72;
        const int group = std::min( tile[0] / 20, 6 );

        Fight fight;
        // FUN_0046a5f0: the battle's random numbers start from x * 100 + y.
        fight.random.seed( static_cast<uint32_t>( mapX * 100 + mapY ) );

        // FUN_0046b12f: cells, ground frames, patches, units, obstacles.
        for ( int r = 0; r < 5; ++r ) {
            for ( int c = 0; c < 9; ++c ) {
                Cell & cell = fight.cells[r * 9 + c];
                cell.y = r * 0x50 + 0x8b;
                cell.x = 0x1b + c * 0x4e - ( ( r & 1 ) == 0 ? 0x27 : 0 );
                cell.ground = fight.random.get( 0, 3 ) + 4;
                if ( c == 0 ) {
                    cell.ground = ( r & 1 ) == 0 ? 1 : 0;
                }
                else if ( c == 8 ) {
                    cell.ground = ( r & 1 ) == 0 ? 2 : 3;
                }
            }
        }
        const int patches = fight.random.get( 8, 15 );
        for ( int k = 0; k < patches; ++k ) {
            const int r = fight.random.get( 0, 4 );
            const int c = fight.random.get( 1, 7 );
            fight.cells[r * 9 + c].ground = fight.random.get( 0, 2 ) + 8;
        }
        // FUN_0046a5f0: a battle at a town of the defending player; walls only when it has the Castle building (note v100).
        const Castle * castle = world.getCastleEntrance( Maps::GetPoint( tileIndex ) );
        if ( castle != nullptr && castle->GetColor() == defender.GetColor() ) {
            fight.castleDefender = true;
            fight.defenderCastle = castle;
            fight.siege = castle->isCastle();
            fight.castleRace = std::clamp( H1::h1RaceFromEngine( castle->GetRace() ), 0, 3 );
            fight.castleBits = H1::castleBuildingBits( *castle );
            fight.castleGuild = H1::castleGuildByte( *castle );
        }
        if ( fight.siege ) {
            for ( int r = 0; r < 5; ++r ) {
                fight.cells[r * 9 + 6].ground = 4;
                fight.cells[r * 9 + 7].ground = 4;
                fight.cells[r * 9 + 5].obstacle = 8;
            }
        }
        fillUnits( fight, 1, attacker );
        fillUnits( fight, 0, defender );
        fight.updateCells();
        const int obstacles = fight.siege ? 0 : fight.random.get( 0, 3 );
        for ( int k = 0; k < obstacles; ++k ) {
            int c = fight.random.get( 3, 5 );
            int r = fight.random.get( 0, 4 );
            while ( fight.cells[r * 9 + c].side != -1 ) {
                c = fight.random.get( 3, 5 );
                r = fight.random.get( 0, 4 );
            }
            int frame = fight.random.get( 0, 2 );
            if ( ( group == 0 || group == 4 ) && frame == 2 ) {
                frame = 0;
            }
            fight.cells[r * 9 + c].obstacle = frame;
        }

        Army * armies[2] = { &defender, &attacker };
        for ( int s = 0; s < 2; ++s ) {
            fight.sideArmy[s] = armies[s];
            const Heroes * hero = dynamic_cast<const Heroes *>( armies[s]->GetCommander() );
            fight.heroes[s] = hero;
            fight.hasHero[s] = ( hero != nullptr );
            fight.heroColour[s] = playerColourIndex( *armies[s] );
            fight.human[s] = armies[s]->isControlHuman();
            // FUN_00467754( army +0x2ae, hero +0x2a6, castle +0x29e ) for morale and FUN_0041ca0a( hero ) for luck, as the battle code
            // calls them (0x46c2ed, 0x40a771; note v198). Until v197 the engine's GetMorale / GetLuck stood in.
            fight.morale[s] = H1::heroMorale( hero, *armies[s], s == 0 ? fight.defenderCastle : nullptr );
            fight.luck[s] = H1::heroLuck( hero );
            // Ballista (artifact 17): two catapult stones a round.
            const int ballista = H1::engineArtifactFromH1( 17 );
            fight.catapultShots[s] = ( hero != nullptr && ballista >= 0 && hero->hasArtifact( Artifact( ballista ) ) ) ? 2 : 1;
            fight.catapultLeft[s] = fight.catapultShots[s];
            fight.keepLeft[s] = 1;
        }

        fight.xtl = &getIcn( std::string( groundFiles[group] ) + ".xtl" );
        fight.obj = &getIcn( std::string( groundFiles[group] ) + ".obj" );
        std::string backgroundName = backgroundFile( group, tile, mapX, mapY );
        fight.background = UI::readBmp( backgroundName );
        fight.bar = UI::loadLayout( "cmbtwin.bin" );
        for ( UI::Item & it : fight.bar.items ) {
            if ( it.y < 460 ) {
                it.hidden = true;
            }
        }

        fight.visible = fight.human[0] || fight.human[1];
        // H1-INTENDED(D16): extension mode battles are drawn at 0.8 (not sieges or sea battles; needs h1/ext/TENTW.ICN).
        fight.bigView = fight.visible && H1::isExtension() && !fight.siege && group != 0 && !H1::getIcn( "tentw.icn" ).empty();
        if ( fight.bigView ) {
            fight.buildSurround( group, backgroundName, static_cast<uint32_t>( mapX * 72 + mapY ) );
        }
        fight.speedPercent = 100 / H1::combatSpeedMultiplier();
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::Image saved = fight.visible ? display : fheroes2::Image();
        std::unique_ptr<CursorRestorer> cursorRestorer;
        if ( fight.visible ) {
            cursorRestorer = std::make_unique<CursorRestorer>( true, Cursor::POINTER );
        }

        // FUN_0046a9cb: prebattl.82m as the battlefield is first shown.
        fight.redraw();
        // FUN_0046a9cb: the music stops (FUN_004750f0 -1), prebattl.82m is played and waited for, then one of the battle tunes
        // 0x29, 0x2a, 0x28, 0x35 is chosen with the game's random numbers (FUN_0041c90b 0..3).
        if ( fight.visible ) {
            H1::playMusic( -1 );
        }
        fight.waitSound( fight.sound( "prebattl.82m" ) );
        {
            static const int battleTunes[4] = { 0x29, 0x2a, 0x28, 0x35 };
            const int tune = battleTunes[fight.random.get( 0, 3 )];
            if ( fight.visible ) {
                H1::playMusic( tune );
            }
        }
        // FUN_0046a9cb: side 0 starts the search (so side 1, the attacker, acts first), speed level 4, no morale test.
        fight.current = 0;
        fight.speedLevel = 4;
        fight.selectNext( false );
        fight.ogreWarCry();
        // FUN_0046a9cb ends by seeding the game's random numbers again with the battle's own number (FUN_0046a5f0 argument 10, which
        // FUN_00405c7f takes from the C library rand() 1..1000 when none is given) [the engine's random numbers stand in for rand()].
        fight.random.seed( static_cast<uint32_t>( Rand::Get( 1, 1000 ) ) );
        int winner = -1;
        while ( true ) {
            if ( fight.isOver( winner ) ) {
                break;
            }
            if ( fight.human[fight.current] && fight.autoCombat ) {
                // FUN_00449d70: a key or a mouse button ends Auto Combat.
                LocalEvent & ev = LocalEvent::Get();
                ev.HandleEvents( false );
                if ( ev.MouseClickLeft() || ev.isAnyKeyPressed() ) {
                    fight.autoCombat = false;
                }
            }
            fight.siegeWeapons();
            if ( fight.isOver( winner ) ) {
                break;
            }
            if ( fight.acting().spell == SPELL_BERSERK ) {
                // FUN_00449d70: a unit under Berserk chooses by itself (FUN_0040b85c).
                fight.berserkChoice();
            }
            else if ( fight.human[fight.current] && !fight.autoCombat ) {
                fight.playerChoice();
                if ( fight.action == ACTION_NONE ) {
                    continue; // AUTO pressed: the computer plays this unit
                }
            }
            else {
                // FUN_0044dece: the retreat test first (FUN_0043de80), then [hero spells FUN_00464be0: step P4] the unit's action.
                if ( fight.computerRetreats() ) {
                    fight.action = ACTION_RETREAT;
                }
                else if ( fight.castCast[fight.current] || ( fight.human[fight.current] && ( Settings::Get().h1BattleView & 8 ) != 0 )
                          || !fight.computerSpell() ) {
                    // [added option] Auto Combat Spell Casting off: the player's side does not cast in Auto Combat (on = original).
                    fight.computerChoice();
                }
                fight.delay( 250 );
            }
            const bool turnOver = fight.carryOut();
            fight.action = ACTION_NONE;
            if ( fight.isOver( winner ) ) {
                break;
            }
            if ( turnOver && !fight.selectNext( true ) && !fight.selectWaiting() ) {
                fight.newRound();
                fight.selectNext( true );
            }
            fight.ogreWarCry();
            fight.redraw();
        }

        // FUN_0044e18c action 5: the price goes from the surrendering player to the other one.
        for ( int s = 0; s < 2; ++s ) {
            if ( fight.surrendered[s] && fight.heroes[s] != nullptr && fight.heroes[1 - s] != nullptr ) {
                const Funds price( Resource::GOLD, static_cast<uint32_t>( fight.surrenderCost ) );
                const_cast<Heroes *>( fight.heroes[s] )->GetKingdom().OddFundsResource( price );
                const_cast<Heroes *>( fight.heroes[1 - s] )->GetKingdom().AddFundsResource( price );
            }
        }

        // FUN_0044cd7d: outcome. Experience FUN_0046e26e: killed creatures of the losing side x creature experience, + 500 for a hero;
        // - 500 when the loser fled or surrendered. [Castle battles (+ 500) and the win / loss windows: later steps.]
        const bool loserFled[2] = { fight.fled[0] || fight.surrendered[0], fight.fled[1] || fight.surrendered[1] };
        auto experienceFrom = [&fight]( const int side ) {
            int sum = 0;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = fight.units[side][k];
                if ( u.exists ) {
                    sum += ( u.startCount - u.count ) * H1::creatureExperience( u.creature );
                }
            }
            if ( fight.hasHero[side] ) {
                sum += 500;
            }
            return sum;
        };
        result.attacker = 0;
        result.defender = 0;
        if ( winner == 1 ) {
            result.attacker = Battle::RESULT_WINS;
            result.defender = Battle::RESULT_LOSS | ( fight.fled[0] ? Battle::RESULT_RETREAT : 0 ) | ( fight.surrendered[0] ? Battle::RESULT_SURRENDER : 0 );
            // + 500 for taking a town (FUN_0044cd7d, castle +0x29e).
            const int e = experienceFrom( 0 ) - ( loserFled[0] ? 500 : 0 ) + ( fight.castleDefender ? 500 : 0 );
            result.attackerExperience = static_cast<uint32_t>( std::max( e, 0 ) );
        }
        else if ( winner == 0 ) {
            result.defender = Battle::RESULT_WINS;
            result.attacker = Battle::RESULT_LOSS | ( fight.fled[1] ? Battle::RESULT_RETREAT : 0 ) | ( fight.surrendered[1] ? Battle::RESULT_SURRENDER : 0 );
            const int e = experienceFrom( 1 ) - ( loserFled[1] ? 500 : 0 );
            result.defenderExperience = static_cast<uint32_t>( std::max( e, 0 ) );
        }
        else {
            result.attacker = Battle::RESULT_LOSS;
            result.defender = Battle::RESULT_LOSS;
        }

        // H1-INTENDED(D16) necromancy (owner 13:35 "사령술 넣기", note v247): a winning necromancer hero (class 4, extension games) raises
        // part of the creatures killed on the losing side as skeletons: 10 % at level 1, + 2 % a level, at most 30 % (HoMM2's basic ..
        // expert necromancy 10 .. 30 %) [adapted]. Called after the survivors are written back.
        auto raiseSkeletons = [&fight, &armies, winner]() -> int {
            if ( !H1::isExtension() || ( winner != 0 && winner != 1 ) || fight.heroes[winner] == nullptr || H1::heroClass( *fight.heroes[winner] ) != 4 ) {
                return 0;
            }
            int killed = 0;
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = fight.units[1 - winner][k];
                if ( u.exists ) {
                    killed += std::max( 0, u.startCount - u.count );
                }
            }
            const int percent = std::min( 30, 10 + 2 * ( static_cast<int>( fight.heroes[winner]->GetLevel() ) - 1 ) );
            const int raised = killed * percent / 100;
            if ( raised <= 0 || !armies[winner]->JoinTroop( Monster( H1::creatureRow( 29 ).monsterId ), static_cast<uint32_t>( raised ), false ) ) {
                return 0;
            }
            return raised;
        };

        // Texts of FUN_0044cd7d / FUN_0044d51e (table 0x493c60).
        if ( !fight.visible ) {
            for ( int s = 0; s < 2; ++s ) {
                for ( int k = 0; k < 5; ++k ) {
                    const Unit & u = fight.units[s][k];
                    if ( !u.exists ) {
                        continue;
                    }
                    Troop * troop = armies[s]->GetTroop( static_cast<size_t>( u.armySlot ) );
                    if ( troop != nullptr ) {
                        if ( u.count > 0 ) {
                            troop->SetCount( static_cast<uint32_t>( u.count ) );
                        }
                        else {
                            troop->Reset();
                        }
                    }
                }
            }
            raiseSkeletons();
            return true;
        }
        const int humanSide = fight.human[1] ? 1 : 0;
        const int otherSide = 1 - humanSide;
        const Heroes * humanHero = fight.heroes[humanSide];
        if ( winner == humanSide ) {
            std::string text = fight.surrendered[otherSide] ? "The enemy has surrendered!" : ( fight.fled[otherSide] ? "The enemy has fled!" : "A glorious victory!" );
            if ( humanHero != nullptr ) {
                const uint32_t gained = ( winner == 1 ) ? result.attackerExperience : result.defenderExperience;
                text += "\n\nFor valor in combat, " + humanHero->GetName() + " receives " + std::to_string( gained ) + " experience";
            }
            // FUN_0044cd7d: the loser's artifacts 4 and up except the Magic Book (37), when nobody fled or surrendered and both sides have a hero.
            std::vector<int> captured;
            const Heroes * loserHero = fight.heroes[otherSide];
            if ( humanHero != nullptr && loserHero != nullptr && !fight.fled[otherSide] && !fight.surrendered[otherSide] ) {
                for ( const Artifact & art : loserHero->GetBagArtifacts() ) {
                    const int a = art.isValid() ? H1::h1ArtifactFromEngine( art.GetID() ) : -1;
                    if ( a > 3 && a != 37 ) {
                        captured.push_back( a );
                    }
                }
            }
            showBattleEnd( fight, true, text, captured );
        }
        else {
            std::string text;
            if ( humanHero == nullptr ) {
                text = fight.surrendered[humanSide] ? "Your forces surrender to the enemy, and depart in shame."
                                                    : ( fight.fled[humanSide] ? "Your cowardly forces flee from battle." : "Your forces suffer a bitter defeat." );
            }
            else if ( fight.surrendered[humanSide] ) {
                text = humanHero->GetName() + " surrenders to the enemy, and departs in shame.";
            }
            else if ( fight.fled[humanSide] ) {
                text = "The cowardly " + humanHero->GetName() + " flees from battle.";
            }
            else {
                text = "Your forces suffer a bitter defeat, and " + humanHero->GetName() + " abandons your cause.";
            }
            showBattleEnd( fight, false, text, {} );
        }

        // Write the survivors back into the armies.
        for ( int s = 0; s < 2; ++s ) {
            for ( int k = 0; k < 5; ++k ) {
                const Unit & u = fight.units[s][k];
                if ( !u.exists ) {
                    continue;
                }
                Troop * troop = armies[s]->GetTroop( static_cast<size_t>( u.armySlot ) );
                if ( troop == nullptr ) {
                    continue;
                }
                if ( u.count > 0 ) {
                    troop->SetCount( static_cast<uint32_t>( u.count ) );
                }
                else {
                    troop->Reset();
                }
            }
        }
        if ( const int raised = raiseSkeletons(); raised > 0 && winner == humanSide ) {
            H1::UI::messageBox( std::to_string( raised )
                                    + ( raised == 1 ? " skeleton rises from the battlefield and joins your army."
                                                    : " skeletons rise from the battlefield and join your army." ),
                                H1::UI::MESSAGE_OKAY );
        }
        if ( fight.visible ) {
            // FUN_0046ade4 (battle cleanup): the music stops; the map tune comes back when the visit ends (FUN_00401000).
            H1::playMusic( -1 );
            fheroes2::Copy( saved, display );
            display.render();
        }
        return true;
    }
}
