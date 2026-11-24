// Heroes I hero rules (homm1-remaster project). See h1_heroes.h.
#include "h1_heroes.h"

#include <algorithm>
#include <cstdio>

#include "army.h"
#include "army_troop.h"
#include "mp2.h"
#include "kingdom.h"
#include "h1_overview.h"
#include "h1_armywin.h"
#include "ground.h"
#include "artifact.h"
#include "h1_rules.h"
#include "h1_gfx.h"
#include "h1_newgame.h"
#include "castle.h"
#include "heroes.h"
#include "monster.h"
#include "race.h"
#include "rand.h"
#include "skill.h"
#include "world.h"

namespace
{
    // Table 0x492a80: class x 9 rows (row = min(new level - 2, 8)) x attack / defense / power / knowledge percent.
    const int levelTable[5][9][4] = { { { 20, 60, 10, 10 }, { 60, 20, 10, 10 }, { 20, 60, 10, 10 }, { 25, 25, 25, 25 }, { 20, 60, 10, 10 }, { 60, 20, 10, 10 }, { 20, 60, 10, 10 }, { 20, 60, 10, 10 }, { 25, 25, 25, 25 } },
                                      { { 70, 20, 5, 5 }, { 20, 70, 5, 5 }, { 70, 20, 5, 5 }, { 40, 40, 10, 10 }, { 70, 20, 5, 5 }, { 20, 70, 5, 5 }, { 70, 20, 5, 5 }, { 70, 20, 5, 5 }, { 40, 40, 10, 10 } },
                                      { { 5, 5, 20, 70 }, { 5, 5, 70, 20 }, { 5, 5, 20, 70 }, { 10, 10, 40, 40 }, { 10, 10, 30, 50 }, { 10, 10, 50, 30 }, { 10, 10, 30, 50 }, { 10, 10, 30, 50 }, { 20, 20, 30, 30 } },
                                      { { 5, 5, 70, 20 }, { 5, 5, 20, 70 }, { 5, 5, 70, 20 }, { 10, 10, 40, 40 }, { 10, 10, 50, 30 }, { 10, 10, 30, 50 }, { 10, 10, 50, 30 }, { 10, 10, 50, 30 }, { 20, 20, 30, 30 } },
                                      // H1-INTENDED(D16) necromancer: HoMM2's necromancer chances (levels 2..9: 15 / 15 / 35 / 35, 10 on: 25 each) [adapted]
                                      { { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 15, 15, 35, 35 }, { 25, 25, 25, 25 } } };

    // The game's own random numbers, state DAT_00490cec (32 bits).
    struct Roll
    {
        uint32_t state = 0;

        // FUN_0041c95d
        void mix( const int a, const int b )
        {
            const uint32_t u = static_cast<uint32_t>( b * 0xd ) & 0xff;
            state += u * 0x20;
            state += ( static_cast<uint32_t>( a * 0xd ) & 0xff ) * 0x33b1;
            state += u;
            state += ( state & 0x3f ) * 0x100;
        }

        // FUN_0041c87a
        uint32_t next()
        {
            state &= 0xfff;
            state = state * 7 + ( ( ( state * 7 ) & 0xff0 ) >> 4 );
            return state;
        }

        // FUN_0041c90b( a, b ): a .. b
        int get( const int a, const int b )
        {
            mix( a, b );
            const uint32_t u = next();
            state += static_cast<uint32_t>( a );
            state += static_cast<uint32_t>( b * 8 );
            return a + static_cast<int>( static_cast<int32_t>( u ) % ( b - a + 1 ) );
        }
    };
}

namespace H1
{
    int GameRandom::get( const int a, const int b )
    {
        Roll roll;
        roll.state = state;
        const int value = roll.get( a, b );
        state = roll.state;
        return value;
    }

    int heroClass( const Heroes & hero )
    {
        switch ( hero.GetRace() ) {
        case Race::KNGT:
            return 0;
        case Race::BARB:
            return 1;
        case Race::SORC:
            return 2;
        case Race::WRLK:
            return 3;
        case Race::NECR:
            return 4; // H1-INTENDED(D16) necromancer class (extension games)
        default:
            return -1;
        }
    }

    int heroNumberOfId( const int id )
    {
        if ( id >= Heroes::LORDKILBURN && id <= Heroes::WRATHMONT ) {
            return id - Heroes::LORDKILBURN;
        }
        if ( id >= Heroes::ZOM && id <= Heroes::CELIA ) {
            return firstNecromancerHero + id - Heroes::ZOM; // H1-INTENDED(D16)
        }
        return -1;
    }

    int heroNumber( const Heroes & hero )
    {
        return heroNumberOfId( hero.GetID() );
    }

    Heroes * heroByNumber( const int number )
    {
        if ( number >= 0 && number < firstNecromancerHero ) {
            return world.GetHeroes( Heroes::LORDKILBURN + number );
        }
        if ( number >= firstNecromancerHero && number < heroCount ) {
            return world.GetHeroes( Heroes::ZOM + number - firstNecromancerHero );
        }
        return nullptr;
    }

    uint32_t maxMovePoints( const Heroes & hero, const bool onWater )
    {
        const auto hasArtifact = [&hero]( const int h1Artifact ) {
            const int id = engineArtifactFromH1( h1Artifact );
            return id >= 0 && hero.GetBagArtifacts().isPresentArtifact( Artifact( id ) );
        };
        int points = 0;
        if ( !onWater ) {
            static const int bySpeed[4] = { 0, 40, 50, 60 }; // locals 0x28, 0x32, 0x3c (index 0 is never reached: speeds are 1..4)
            int slowest = 3;
            const Army & army = hero.GetArmy();
            for ( size_t i = 0; i < army.Size(); ++i ) {
                const Troop * troop = army.GetTroop( i );
                if ( troop == nullptr || !troop->isValid() ) {
                    continue;
                }
                const int creature = creatureFromMonster( troop->GetID() );
                if ( creature >= 0 && creatureRow( creature ).speedLevel < slowest ) {
                    slowest = creatureRow( creature ).speedLevel;
                }
            }
            points = bySpeed[std::clamp( slowest, 1, 3 )];
            if ( hasArtifact( 0x1d ) ) {
                points += 24;
            }
            if ( hasArtifact( 0x1e ) ) {
                points += 12;
            }
        }
        else {
            // +0x14349: owner of mine-list entry 1 (the lighthouse).
            points = ( world.CountCapturedObject( MP2::OBJ_LIGHTHOUSE, hero.GetColor() ) > 0 ) ? 80 : 60;
            if ( hasArtifact( 0x24 ) ) {
                points += 40;
            }
            static const float classFactor[5] = { 1.0f, 1.0f, 2.0f, 1.0f, 1.0f }; // 0x491E50; H1-INTENDED(D16) necromancer 1.0
            const int cls = heroClass( hero );
            if ( cls >= 0 ) {
                points = static_cast<int>( static_cast<float>( points ) * classFactor[cls] );
            }
        }
        if ( hasArtifact( 0x23 ) ) {
            points += 20;
        }
        // DAT_004a9b60[player] == 0 (computer) and player +0x21e > 2.
        if ( hero.GetColor() != PlayerColor::NONE ) {
            const Kingdom & kingdom = hero.GetKingdom();
            if ( kingdom.isControlAI() && computerLevel( kingdom ) > 2 ) {
                points += 3;
            }
        }
        return static_cast<uint32_t>( points ) * movementScale;
    }

    uint32_t straightStepCost( const int ground, const bool barbarian )
    {
        if ( barbarian ) {
            return 4 * movementScale;
        }
        switch ( ground ) {
        case Maps::Ground::SNOW:
        case Maps::Ground::SWAMP:
            return 6 * movementScale;
        case Maps::Ground::DESERT:
            return 8 * movementScale;
        default:
            return 4 * movementScale;
        }
    }

    int levelUpStat( const int cls, const uint16_t seed, const int newLevel )
    {
        // H1-INTENDED(D16): class 4 = necromancer (extension); before v252 it fell out here, so necromancers always raised Attack (owner 18:09).
        if ( cls < 0 || cls > 4 ) {
            return 0;
        }
        int row = newLevel - 2;
        if ( row > 8 ) {
            row = 8;
        }
        if ( row < 0 ) {
            row = 0;
        }
        // FUN_0041c9e6( hero +0x3d + level x 30 ), then a roll 1..100 compared with "<" against each percent in turn.
        Roll roll;
        roll.state = static_cast<uint32_t>( static_cast<int>( static_cast<int16_t>( seed ) ) + newLevel * 0x1e );
        int r = roll.get( 1, 100 );
        const int * p = levelTable[cls][row];
        for ( int k = 0; k < 3; ++k ) {
            if ( r < p[k] ) {
                return k;
            }
            r -= p[k];
        }
        return 3;
    }

    int levelUpSecondChoice( const int cls, const uint16_t seed, const int newLevel )
    {
        const int first = levelUpStat( cls, seed, newLevel );
        int second = levelUpStat( cls, static_cast<uint16_t>( seed ^ 0x5a5a ), newLevel + 0x40 );
        if ( second == first && cls >= 0 && cls <= 4 ) {
            const int * p = levelTable[cls][std::clamp( newLevel - 2, 0, 8 )];
            int best = -1;
            for ( int k = 0; k < 4; ++k ) {
                if ( k != first && ( best < 0 || p[k] > p[best] ) ) {
                    best = k;
                }
            }
            second = best;
        }
        return second;
    }

    void rollHeroArmy( Heroes & hero, const bool recruited )
    {
        const int cls = heroClass( hero );
        if ( cls < 0 ) {
            return;
        }
        // Table built in FUN_0041beec: per class 3 x (creature, min, max); only the first two are used.
        static const int table[5][2][3] = { { { 0, 30, 50 }, { 1, 3, 5 } }, { { 6, 15, 25 }, { 7, 3, 5 } }, { { 12, 10, 20 }, { 13, 2, 4 } }, { { 18, 6, 10 }, { 19, 2, 4 } },
                                            { { 29, 10, 20 }, { 36, 2, 4 } } }; // H1-INTENDED(D16) necromancer: skeletons, zombies [adapted]
        bool use[2] = { true, false };
        use[1] = static_cast<int>( Rand::Get( 0, 99 ) ) < ( recruited ? 0x50 : 0x32 );
        const bool second = static_cast<int>( Rand::Get( 0, 99 ) ) < ( recruited ? 0x41 : 0x19 );
        if ( !second ) {
            use[1] = true;
        }
        Army & army = hero.GetArmy();
        army.Clean();
        const auto & creatures = getCreatureTable();
        for ( int k = 0; k < 2; ++k ) {
            if ( !use[k] ) {
                continue;
            }
            int low = table[cls][k][1] * 10;
            const int high = table[cls][k][2] * 10 + 9;
            if ( recruited ) {
                low = ( low + high ) / 2;
            }
            const int count = static_cast<int>( Rand::Get( static_cast<uint32_t>( low ), static_cast<uint32_t>( high ) ) ) / 10;
            const int creature = table[cls][k][0];
            if ( creature < static_cast<int>( creatures.size() ) && count > 0 ) {
                army.JoinTroop( Monster( creatures[creature].monsterId ), static_cast<uint32_t>( count ), true );
            }
            else if ( creature >= firstExtensionCreature && count > 0 ) {
                army.JoinTroop( Monster( creatureRow( creature ).monsterId ), static_cast<uint32_t>( count ), true ); // H1-INTENDED(D16) necromancer army
            }
        }
    }

    void setUpHeroesForNewGame()
    {
        for ( int id = Heroes::UNKNOWN + 1; id < Heroes::HEROES_COUNT; ++id ) {
            Heroes * hero = world.GetHeroes( id );
            if ( hero == nullptr ) {
                continue;
            }
            // Starting records of origdata.bin (loaded as a saved game for a new game, FUN_004129f2): per class Attack / Defense /
            // Spell Power / Knowledge = Knight 1/2/1/1, Barbarian 2/1/1/1, Sorceress 0/0/2/3, Warlock 0/0/3/2; Sorceresses and Warlocks
            // carry the Magic Book (artifact 37) in slot 0, Knights and Barbarians nothing.
            static const int skills[5][4] = { { 1, 2, 1, 1 }, { 2, 1, 1, 1 }, { 0, 0, 2, 3 }, { 0, 0, 3, 2 }, { 1, 0, 2, 2 } }; // H1-INTENDED(D16): necromancer = HoMM2 start
            const int cls = heroClass( *hero );
            if ( cls >= 0 ) {
                hero->h1SetPrimarySkills( skills[cls][0], skills[cls][1], skills[cls][2], skills[cls][3] );
                if ( cls >= 2 ) {
                    if ( !hero->HaveSpellBook() ) {
                        hero->SpellBookActivate();
                    }
                }
                else if ( hero->HaveSpellBook() ) {
                    hero->SpellBookDeactivate();
                }
            }
            hero->h1BookSpell.fill( 0 );
            hero->h1BookCount.fill( 0 );
            hero->GetSecondarySkills() = Skill::SecSkills();
            // FUN_0041bdea: +0x29 = 0x28 + random 0..50, +0x3d = random 1..16000 (FUN_00476010 = rand()).
            hero->h1SetExperience( 0x28 + Rand::Get( 0, 0x32 ) );
            hero->h1LevelSeed = static_cast<uint16_t>( Rand::Get( 1, 16000 ) );
            rollHeroArmy( *hero, false );
        }
    }

    void placeRecordHeroes()
    {
        const NewGameChoice & choice = lastNewGameChoice();
        const int seats = choice.playerCount;
        for ( const HeroRecord & r : mapHeroRecords() ) {
            const uint8_t * rec = r.bytes.data();
            // Owner = record byte 0, the last playing seat when it is not below the number of seats (game +0x1ff).
            int seat = static_cast<int8_t>( rec[0] );
            if ( seat >= seats ) {
                seat = seats - 1;
            }
            // The seat's colour (FUN_0041fb75; campaigns too since note v169).
            const int colour = ( seat >= 0 && seat < 4 ) ? choice.seatColour[seat] : -1;
            if ( seat < 0 || seat >= 4 || colour < 0 ) {
                continue;
            }
            const int number = rec[0x10];
            Heroes * hero = ( number < 36 ) ? heroByNumber( number ) : nullptr;
            if ( hero == nullptr || hero->GetColor() != PlayerColor::NONE ) {
                continue;
            }
            // Army: record bytes 1..5 creatures, 6.. five 2-byte counts; a count below 1 leaves the slot empty.
            Army & army = hero->GetArmy();
            army.Reset( false );
            for ( int k = 0; k < 5; ++k ) {
                const int count = static_cast<int16_t>( rec[6 + 2 * k] | ( rec[7 + 2 * k] << 8 ) );
                const int creature = rec[1 + k];
                Troop * troop = army.GetTroop( static_cast<size_t>( k ) );
                if ( troop == nullptr ) {
                    continue;
                }
                if ( count < 1 || creature >= 28 ) {
                    troop->Reset();
                }
                else {
                    troop->Set( Monster( creatureRow( creature ).monsterId ), static_cast<uint32_t>( count ) );
                }
            }
            // Artifacts: bytes 0x11..0x14 (FUN_004036bd for each one that is not -1).
            for ( int k = 0; k < 4; ++k ) {
                const int art = static_cast<int8_t>( rec[0x11 + k] );
                if ( art >= 0 ) {
                    const int engineArt = engineArtifactFromH1( art );
                    if ( engineArt >= 0 ) {
                        hero->PickupArtifact( Artifact( engineArt ) );
                    }
                }
            }
            // Experience: hero +0x29 = 0, then the record's value with its level-ups (FUN_004037c5, FUN_00448e23); the hero has no owner
            // yet, so no level-up box.
            hero->h1SetExperience( 0 );
            const uint32_t experience = static_cast<uint32_t>( rec[0x15] | ( rec[0x16] << 8 ) | ( rec[0x17] << 16 ) | ( rec[0x18] << 24 ) );
            hero->IncreaseExperience( experience );
            // Position; a hero just below a castle entrance (0xa8) goes into the castle.
            int x = r.x;
            int y = r.y;
            if ( y > 0 ) {
                const uint8_t * above = getTileBytes( ( y - 1 ) * 72 + x );
                if ( above != nullptr && above[8] == 0xa8 ) {
                    --y;
                }
            }
            hero->Recruit( Color::IndexToColor( colour ), { x, y } );
        }
    }

    std::string levelUpText( const std::string & name, const int levels, const int raised[4] )
    {
        static const char * const skillNames[4] = { "Attack Skill", "Defense Skill", "Spell Power", "Knowledge" };
        char buffer[160];
        std::string text = name + " has gained";
        if ( levels == 1 ) {
            text += " a level.\n";
        }
        else {
            std::snprintf( buffer, sizeof( buffer ), " %d levels.\n", levels );
            text += buffer;
        }
        for ( int k = 0; k < 4; ++k ) {
            if ( raised[k] > 0 ) {
                std::snprintf( buffer, sizeof( buffer ), "\n%s +%d", skillNames[k], raised[k] );
                text += buffer;
            }
        }
        return text;
    }
}
