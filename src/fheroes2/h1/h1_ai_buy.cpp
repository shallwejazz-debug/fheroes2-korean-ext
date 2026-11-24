// Heroes I computer player: buying FUN_0042868b "CheckBuy" with FUN_0042a40d "BestBHC", building score FUN_0042bff5 / FUN_0042ba83, creature
// score FUN_0042c445 / FUN_0042c164, hero score FUN_0042c924 / FUN_0042c77c, check FUN_0042feca (homm1-remaster project, notes v35, v146).
// Not connected to the game yet (AC6).
#include "h1_ai.h"

#include <algorithm>
#include <cmath>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "castle.h"
#include "color.h"
#include "h1_armywin.h"
#include "h1_battle.h"
#include "h1_castle.h"
#include "h1_gfx.h"
#include "h1_heroes.h"
#include "h1_overview.h"
#include "h1_rules.h"
#include "heroes.h"
#include "heroes_recruits.h"
#include "h1_tavern.h"
#include "kingdom.h"
#include "monster.h"
#include "rand.h"
#include "resource.h"
#include "world.h"

namespace
{
    const int artifactValue[38] = { 9000, 22000, 18000, 14000, 6000, 4000, 4000, 5600, 1200, 1200, 1200, 1200, -1200, 2000, 1800, 1800, 2000, 1000, 3600,
                                    5600, 4000, 5040, 2700, 3900, 4950, 5850, 7000, 6000, 4000, 4500, 2250, 1200, 1200, 1200, 1200, 3500, 1500, 16200 };
    // Creature table +0x06 (Windows values; dragon 0x7f).
    // H1-INTENDED(D1): Windows baseline, dragon value 0x7f (DOS 0x8b) (docs/INTENDED_DIFFERENCES.md).
    const int creatureRankTable[28] = { 9, 17, 20, 31, 42, 60, 14, 17, 24, 30, 44, 57, 15, 19, 23, 29, 37, 64, 16, 19, 25, 31, 43, 127, 22, 27, 59, 43 };
    // H1-INTENDED(D16): 28..35 (extension creatures) ranked like HoMM1 creatures of the same strength [adapted].
    int creatureRank( const int creature )
    {
        static const int extension[12] = { 37, 15, 44, 14, 31, 29, 31, 31, 18, 27, 36, 70 }; // 36..39: v241
        if ( creature >= 0 && creature < 28 ) {
            return creatureRankTable[creature];
        }
        return ( creature >= 28 && creature < 40 ) ? extension[creature - 28] : 0;
    }
    // Building AI values (note v34 2-8, races by v35 0).
    const int guildValue[4] = { 4000, 6500, 8500, 10500 };
    const int commonValue[7] = { 0, 1500, 500, 2000, 3000, 0, 12000 };
    const int dwellingValue[5][6] = { { 858, 2225, 2816, 7385, 13754, 29785 },
                                      { 1684, 2256, 3736, 7213, 15181, 27684 },
                                      { 1802, 2615, 3414, 6967, 12212, 38141 },
                                      { 1956, 2607, 3869, 7510, 16002, 111967 },
                                      // H1-INTENDED(D16) necromancer (owner 13:35: values only): the warlock row times (necromancer creature worth /
                                      // warlock creature worth) per level [adapted]
                                      { 2373, 1981, 3251, 6818, 12768, 35643 } };
    // Table 0x492b10: class a castle race likes (race 0 knight, 1 sorceress, 2 barbarian, 3 warlock -> class 0 knight, 1 barbarian,
    // 2 sorceress, 3 warlock).
    const int likedClass[5] = { 0, 2, 1, 3, 4 }; // H1-INTENDED(D16): necromancer castle -> necromancer class (4)

    int ftol( const double v )
    {
        return static_cast<int>( v );
    }

    int engineResource( const int r )
    {
        static const int map[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
        return map[r];
    }

    // The player's resources as the purchase sees them (player +0x7d..; reservations are taken off them while buying).
    struct Purse
    {
        int have[7] = {};
    };

    Purse purse;

    double bundleValue( const int amounts[7] )
    {
        double v = 0;
        for ( int r = 0; r < 7; ++r ) {
            v += amounts[r] * static_cast<double>( H1::Ai::state().resourceValue[r] );
        }
        return v;
    }

    // FUN_0042ce01 / FUN_0042dfb8: W = 1 - 0.15 x D (at least 0), D = for each missing resource (missing / day income + 1), the largest;
    // 99 without income; 0 when affordable.
    double waitFactor( const Kingdom & kingdom, const int cost[7] )
    {
        // Player +0xd5: what the last day change gave (note v199).
        const std::array<int, 7> & income = H1::Ai::dayGain( Color::GetIndex( kingdom.GetColor() ) );
        int days = 0;
        for ( int r = 0; r < 7; ++r ) {
            const int missing = cost[r] - purse.have[r];
            if ( missing > 0 ) {
                const int per = income[r];
                days = std::max( days, per > 0 ? missing / per + 1 : 99 );
            }
        }
        // FUN_0042dfb8: 1.0f - D x 0.15f (the float at 0x48d07c), kept as a 4-byte float; below 0 -> 0.
        const float w = static_cast<float>( 1.0f - days * static_cast<double>( 0.15f ) );
        return w < 0.0f ? 0.0 : w;
    }

    bool affordable( const int cost[7] )
    {
        for ( int r = 0; r < 7; ++r ) {
            if ( purse.have[r] < cost[r] ) {
                return false;
            }
        }
        return true;
    }

    int guardValue( const Army & army )
    {
        return H1::Ai::strength( army, nullptr, false, false, 0, 0 );
    }

    int flag8Count( const Army & army )
    {
        int c = 0;
        for ( size_t i = 0; i < army.Size(); ++i ) {
            const Troop * t = army.GetTroop( i );
            if ( t != nullptr && t->isValid() ) {
                const int creature = H1::creatureFromMonster( t->GetID() );
                if ( creature >= 0 && ( H1::creatureRow( creature ).flags & 8 ) != 0 ) {
                    ++c;
                }
            }
        }
        return c;
    }

    // Building AI value; the mage guild counts "this level - the level below" (v35 4-1).
    double buildingValue( const Castle & castle, const int number )
    {
        if ( number == 0 ) {
            const int level = std::clamp( H1::castleGuildByte( castle ), 0, 3 );
            return guildValue[level] - ( level > 0 ? guildValue[level - 1] : 0 );
        }
        if ( number < 7 ) {
            return commonValue[number];
        }
        return dwellingValue[H1::buildingRace( castle )][number - 7];
    }

    // FUN_0042ba83 (note v35 4, v189) for an engine castle: the record the original reads, then H1::Ai::buildingScoreOriginal.
    double buildingScore( const Kingdom & kingdom, const Castle & castle, const int number )
    {
        const H1::Ai::State & s = H1::Ai::state();
        H1::Ai::BuildingInput in;
        in.number = number;
        in.race = H1::buildingRace( castle );
        in.bits = H1::castleBuildingBits( castle );
        in.guildByte = H1::castleGuildByte( castle );
        const Army & garrison = castle.GetArmy();
        for ( size_t i = 0; i < 5; ++i ) {
            const Troop * t = garrison.GetTroop( i );
            if ( t != nullptr && t->isValid() ) {
                in.garrison[i] = H1::creatureFromMonster( t->GetID() );
                in.garrisonCounts[i] = static_cast<int>( t->GetCount() );
            }
        }
        for ( int d = 0; d < 6; ++d ) {
            in.dwellingCounts[d] = static_cast<int>( castle.getMonstersInDwelling( DWELLING_MONSTER1 << d ) );
        }
        in.ad = s.ad;
        in.b1 = s.b1;
        for ( int r = 0; r < 7; ++r ) {
            in.have[r] = purse.have[r];
        }
        // Player +0xd5 (note v199).
        for ( int r = 0; r < 7; ++r ) {
            in.income[r] = H1::Ai::dayGain( Color::GetIndex( kingdom.GetColor() ) )[r];
        }
        // FUN_0042ba83 asks FUN_00422bb1 for the guild row of the castle's guild byte itself (not byte + 1 as the price shown when building).
        H1::buildingCostRow( in.race, number, number == 0 ? in.guildByte : 0, in.cost );
        for ( const Castle * c : kingdom.GetCastles() ) {
            if ( c != nullptr && in.castles < 36 ) {
                in.castleRace[in.castles] = H1::buildingRace( *c );
                in.castleBits[in.castles] = H1::castleBuildingBits( *c );
                in.castleGuild[in.castles] = H1::castleGuildByte( *c );
                ++in.castles;
            }
        }
        return H1::Ai::buildingScoreOriginal( in );
    }

    struct Choice
    {
        Castle * castle = nullptr;
        int kind = -1; // 0 building, 1 hero, 2 creatures
        int number = -1; // building / hero slot 0..1 / dwelling 7..12
        int count = 0;
    };

    // FUN_0042bff5: the best building of a castle; returns the unrandomised score.
    double bestBuilding( const Kingdom & kingdom, Castle & castle, Choice & c )
    {
        float best = -99.0f;
        double bestRaw = 0;
        for ( int number = 0; number <= 12; ++number ) {
            const bool guildUp = number == 0 && H1::castleGuildByte( castle ) < 3;
            if ( ( number != 0 && H1::isBuildingBuilt( castle, number ) ) || ( number == 0 && H1::isBuildingBuilt( castle, 0 ) && !guildUp )
                 || !H1::canBuildBuilding( castle, number ) ) {
                continue;
            }
            const double score = buildingScore( kingdom, castle, number );
            // FUN_0042bff5: r = (1..5 + 95) x the score / 100.0f; the unrounded r is compared with the best (a float, starting at -99), then
            // stored as a float (fst / fcomp).
            const double r = static_cast<double>( static_cast<int>( Rand::Get( 1, 5 ) ) + 0x5f ) * static_cast<float>( score ) / 100.0f;
            if ( r > best ) {
                best = static_cast<float>( r );
                bestRaw = score;
                c.castle = &castle;
                c.kind = 0;
                c.number = number;
            }
        }
        return bestRaw;
    }

    // FUN_0042c682: how many to buy. FUN_0042c6e1 overwrites its count for every resource, so only gold counts: price 0 -> 9999,
    // no gold -> 0, else gold / price. More than 1 -> half (>> 1); at most the waiting ones; below 2 none (note v190).
    int buyCount( const int creature, const int available )
    {
        int gold = 0;
        int extra = -1;
        H1::creaturePrice( creature, gold, extra );
        int count = ( gold == 0 ) ? 9999 : ( purse.have[6] < 1 ? 0 : purse.have[6] / gold );
        if ( count > 1 ) {
            count >>= 1;
        }
        count = std::min( count, available );
        return ( count < 2 ) ? 0 : count;
    }

    // FUN_0042c445 / FUN_0042c164 (note v35 5).
    double bestCreatures( const Kingdom & kingdom, Castle & castle, Choice & c )
    {
        const H1::Ai::State & s = H1::Ai::state();
        float best = -99.0f;
        double bestRaw = 0;
        for ( int number = 7; number <= 12; ++number ) {
            const int creature = H1::dwellingCreatureOf( castle, number );
            const int available = static_cast<int>( castle.getMonstersInDwelling( DWELLING_MONSTER1 << ( number - 7 ) ) );
            if ( !H1::isBuildingBuilt( castle, number ) || creature < 0 || available <= 0 ) {
                continue;
            }
            const Monster m( H1::creatureRow( creature ).monsterId );
            if ( !castle.GetArmy().CanJoinTroop( m ) ) {
                continue;
            }
            const int n = buyCount( creature, available );
            if ( n <= 0 ) {
                continue;
            }
            // FUN_0042c164 (h1_ai_buy.cpp H1::Ai::creatureScoreOriginal).
            H1::Ai::CreatureInput in;
            in.creature = creature;
            in.count = n;
            in.f9 = s.f9;
            in.b1 = s.b1;
            auto fill = []( const Army & army, int out[5] ) {
                for ( size_t i = 0; i < 5; ++i ) {
                    const Troop * t = army.GetTroop( i );
                    out[i] = ( t != nullptr && t->isValid() ) ? H1::creatureFromMonster( t->GetID() ) : -1;
                }
            };
            if ( const Heroes * guest = castle.GetHero(); guest != nullptr ) {
                in.hero = true;
                in.heroClass = H1::heroClass( *guest );
                fill( guest->GetArmy(), in.heroArmy );
            }
            fill( castle.GetArmy(), in.garrison );
            for ( int r = 0; r < 7; ++r ) {
                in.have[r] = purse.have[r];
            }
            // Player +0xd5 (note v199).
            for ( int r = 0; r < 7; ++r ) {
                in.income[r] = H1::Ai::dayGain( Color::GetIndex( kingdom.GetColor() ) )[r];
            }
            const double score = H1::Ai::creatureScoreOriginal( in ).score;
            // FUN_0042c445: (1..10 + 0x5a) x score / 100.0 (double); compared unrounded with the best (a float from -99), then stored.
            const double r = static_cast<double>( static_cast<int>( Rand::Get( 1, 10 ) ) + 0x5a ) * static_cast<float>( score ) / 100.0;
            if ( r > best ) {
                best = static_cast<float>( r );
                bestRaw = score;
                c.castle = &castle;
                c.kind = 2;
                c.number = number;
                c.count = n;
            }
        }
        return bestRaw;
    }

    // FUN_0042c924 / FUN_0042c77c (note v35 6).
    double bestHero( Kingdom & kingdom, Castle & castle, Choice & c )
    {
        const H1::Ai::State & s = H1::Ai::state();
        const Recruits & recruits = kingdom.GetRecruits();
        float best = -99.0f;
        double bestRaw = -99;
        int slot = -1;
        const Heroes * candidates[2] = { recruits.GetHero1(), recruits.GetHero2() };
        for ( int i = 0; i < 2; ++i ) {
            const Heroes * h = candidates[i];
            if ( h == nullptr ) {
                continue;
            }
            const int exp = static_cast<int>( h->GetExperience() );
            int arts = 0;
            for ( const Artifact & a : h->GetBagArtifacts() ) {
                const int n = ( a.GetID() == Artifact::MAGIC_BOOK ) ? 37 : H1::h1ArtifactFromEngine( a.GetID() );
                // FUN_0042c77c counts artifacts 0..0x24 only (not the magic book 0x25).
                if ( n >= 0 && n < 0x25 ) {
                    arts += artifactValue[n];
                }
            }
            int v = exp + 2000 + arts + exp / 2;
            v = ftol( ( s.b5 + 1.0 - s.b1 ) * v );
            if ( likedClass[H1::buildingRace( castle )] == H1::heroClass( *h ) ) {
                v = ftol( v * static_cast<double>( 1.12f ) );
            }
            // Surroundings of the candidate's own place (hero +0x1e/+0x1f) [unconfirmed: where a hero in the tavern stands].
            int safety = 100;
            const H1::Ai::HeroView hv = H1::Ai::viewOf( *h );
            v += H1::Ai::surroundings( hv, std::clamp( hv.x, 0, 71 ), std::clamp( hv.y, 0, 71 ), false, safety ) - 200;
            const int cost[7] = { 0, 0, 0, 0, 0, 0, 2500 };
            v = ftol( waitFactor( kingdom, cost ) * v );
            // FUN_0042cd5b of the price (2500 gold) as a whole number; score = v / price as a float.
            const double score = static_cast<float>( static_cast<double>( v ) / H1::Ai::bundleValueOriginal( cost ) );
            // FUN_0042c924: (1..10 + 90.0) x score / 100.0, compared unrounded with the best (a float from -99), then stored.
            const double r = ( static_cast<int>( Rand::Get( 1, 10 ) ) + 90.0 ) * static_cast<float>( score ) / 100.0;
            if ( best < r ) {
                best = static_cast<float>( r );
                bestRaw = score;
                slot = i;
            }
        }
        c.castle = &castle;
        c.kind = 1;
        c.number = slot;
        // A hero already at the castle entrance: - 200.
        if ( const Heroes * there = world.getTile( castle.GetIndex() ).getHero(); there != nullptr ) {
            bestRaw -= 200;
        }
        return bestRaw;
    }
}

namespace H1::Ai
{
    void checkBuy()
    {
        // FUN_0042868b.
        State & s = state();
        Kingdom & kingdom = world.GetKingdom( Color::IndexToColor( s.player ) );
        const Funds funds = kingdom.GetFunds();
        for ( int r = 0; r < 7; ++r ) {
            purse.have[r] = static_cast<int>( funds.Get( engineResource( r ) ) );
        }
        if ( purse.have[6] < 200 ) {
            return;
        }
        const int p = s.player;
        // Boat goals (v35 2-1).
        auto castleOfSlot = []( const int slot ) -> Castle * {
            int32_t x = 0;
            int32_t y = 0;
            return ( slot >= 0 && H1::getListPosition( false, slot, x, y ) ) ? world.getCastleEntrance( { x, y } ) : nullptr;
        };
        Castle * goal = ( s.shipyardGoal[p] >= 0 ) ? castleOfSlot( s.shipyardGoal[p] ) : ( s.boatGoal[p] >= 0 ? castleOfSlot( s.boatGoal[p] ) : nullptr );
        if ( goal != nullptr && goal->GetColor() != kingdom.GetColor() ) {
            s.shipyardGoal[p] = -1;
            s.boatGoal[p] = -1;
            goal = nullptr;
        }
        if ( s.shipyardGoal[p] >= 0 && goal != nullptr ) {
            int cost[7];
            H1::buildingCost( *goal, 3, cost );
            if ( !affordable( cost ) || !H1::canBuildBuilding( *goal, 3 ) ) {
                purse.have[6] -= 2000;
                purse.have[0] -= 20;
            }
            else {
                H1::constructBuilding( *goal, 3 );
                for ( int r = 0; r < 7; ++r ) {
                    purse.have[r] -= cost[r];
                }
                s.shipyardGoal[p] = -1;
            }
        }
        if ( s.boatGoal[p] >= 0 && goal != nullptr ) {
            if ( !H1::isBuildingBuilt( *goal, 3 ) || purse.have[6] < 1000 || purse.have[0] < 10 ) {
                purse.have[6] -= 1000;
                purse.have[0] -= 10;
            }
            else {
                if ( goal->BuyBoat() ) {
                    purse.have[6] -= 1000;
                    purse.have[0] -= 10;
                }
                s.boatGoal[p] = -1;
            }
        }
        // FUN_004285db: heroes in own castles (spell book, guild spells, army).
        for ( Castle * castle : kingdom.GetCastles() ) {
            if ( castle != nullptr && castle->GetHero() != nullptr ) {
                H1::Ai::ownCastleActions( *castle->GetHero(), *castle );
            }
        }
        for ( int r = 0; r < 7; ++r ) {
            purse.have[r] = static_cast<int>( kingdom.GetFunds().Get( engineResource( r ) ) ) - ( ( s.shipyardGoal[p] >= 0 ) ? ( r == 6 ? 2000 : ( r == 0 ? 20 : 0 ) ) : 0 )
                            - ( ( s.boatGoal[p] >= 0 ) ? ( r == 6 ? 1000 : ( r == 0 ? 10 : 0 ) ) : 0 );
        }
        for ( bool done = false; !done; ) {
            // FUN_0042a40d.
            const auto castles = kingdom.GetCastles();
            const int count = static_cast<int>( castles.size() );
            std::vector<int> vc( count );
            std::vector<int> tc( count );
            int sum = 0;
            int weight = 0;
            for ( int i = 0; i < count; ++i ) {
                vc[i] = guardValue( castles[i]->GetArmy() ) + 400;
                sum += vc[i];
                weight += castles[i]->isCastle() ? 10 : 7;
            }
            const int average = sum / std::max( weight, 1 );
            for ( int i = 0; i < count; ++i ) {
                tc[i] = ( castles[i]->isCastle() ? 10 : 7 ) * average + 400;
            }
            float bestScore = -99.0f;
            Choice best;
            for ( int i = 0; i < count; ++i ) {
                Castle & castle = *castles[i];
                const int slot = H1::getCastleSlot( castle.GetIndex(), nullptr );
                const int owned = ( slot >= 0 && slot < 36 ) ? s.turnsOwned[slot] : 10;
                if ( !( s.dayNumber < 4 || owned > 2 ) ) {
                    continue;
                }
                Choice c;
                float score = static_cast<float>( bestBuilding( kingdom, castle, c ) );
                score = static_cast<float>( ( 100 - static_cast<int>( Rand::Get( 0, 10 ) ) ) / 100.0 ) * score;
                if ( bestScore < score ) {
                    bestScore = score;
                    best = c;
                }
                Choice c2;
                score = static_cast<float>( bestCreatures( kingdom, castle, c2 ) );
                score = static_cast<float>( ( static_cast<float>( tc[i] ) / static_cast<float>( vc[i] ) ) / 3.0f + 0.66 ) * score;
                score = static_cast<float>( ( 100 - static_cast<int>( Rand::Get( 0, 10 ) ) ) / 100.0 ) * score;
                if ( bestScore < score && c2.kind >= 0 ) {
                    bestScore = score;
                    best = c2;
                }
                const int heroes = static_cast<int>( kingdom.GetHeroes().size() );
                if ( heroes < s.maxHeroes && castle.isCastle() ) {
                    Choice c3;
                    score = static_cast<float>( bestHero( kingdom, castle, c3 ) );
                    score = static_cast<float>( ( 100 - static_cast<int>( Rand::Get( 0, 10 ) ) ) / 100.0 ) * score;
                    // +0xed is never written (note v33): 0.
                    const int ed = 0;
                    if ( s.heroHired || s.dayNumber < 6 || score <= 0.0f ) {
                        if ( heroes == 0 ) {
                            score += 500.0f;
                        }
                    }
                    else if ( ( ed < 0x4e2 || s.maxHeroes - 2 <= heroes ) && heroes > 1 ) {
                        if ( ed > 0x5db && heroes < s.maxHeroes - 1 ) {
                            score = static_cast<float>( score * 1.3 );
                        }
                    }
                    else {
                        score += 500.0f;
                    }
                    if ( bestScore < score ) {
                        bestScore = score;
                        best = c3;
                    }
                }
            }
            if ( bestScore < 0.02f || best.castle == nullptr || best.kind < 0 || best.number < 0 ) {
                break;
            }
            // FUN_0042feca, then do it.
            Castle & castle = *best.castle;
            if ( best.kind == 0 ) {
                int cost[7];
                H1::buildingCost( castle, best.number, cost );
                if ( !affordable( cost ) ) {
                    break;
                }
                H1::constructBuilding( castle, best.number );
                for ( int r = 0; r < 7; ++r ) {
                    purse.have[r] -= cost[r];
                }
            }
            else if ( best.kind == 1 ) {
                if ( purse.have[6] < 2500 || castle.GetHero() != nullptr || s.heroHired ) {
                    break;
                }
                const Recruits & recruits = kingdom.GetRecruits();
                Heroes * hero = ( best.number == 0 ) ? recruits.GetHero1() : recruits.GetHero2();
                Heroes * hired = ( hero == nullptr ) ? nullptr : castle.RecruitHero( hero );
                if ( hired == nullptr ) {
                    break;
                }
                H1::Tavern::afterHire( kingdom, best.number == 0 ? 0 : 1, *hired ); // FUN_0042fb6e (note v169)
                purse.have[6] -= 2500;
                s.heroHired = true;
            }
            else {
                const int creature = H1::dwellingCreatureOf( castle, best.number );
                const int available = static_cast<int>( castle.getMonstersInDwelling( DWELLING_MONSTER1 << ( best.number - 7 ) ) );
                int gold = 0;
                int extra = -1;
                H1::creaturePrice( creature, gold, extra );
                if ( available < best.count || !castle.GetArmy().CanJoinTroop( Monster( H1::creatureRow( creature ).monsterId ) )
                     || purse.have[6] < gold * best.count || ( extra >= 0 && extra < 6 && purse.have[extra] < best.count ) ) {
                    break;
                }
                if ( !castle.RecruitMonster( Troop( Monster( H1::creatureRow( creature ).monsterId ), static_cast<uint32_t>( best.count ) ), false ) ) {
                    break;
                }
                purse.have[6] -= gold * best.count;
                if ( extra >= 0 && extra < 6 ) {
                    purse.have[extra] -= best.count;
                }
            }
            done = false;
        }
        // FUN_004285db again after buying.
        for ( Castle * castle : kingdom.GetCastles() ) {
            if ( castle != nullptr && castle->GetHero() != nullptr ) {
                H1::Ai::ownCastleActions( *castle->GetHero(), *castle );
            }
        }
    }
}

namespace H1::Ai
{
    float buildingScoreOriginal( const BuildingInput & in )
    {
        // 0x42ba83..0x42bff2: the value is stored as a 4-byte float after every step (fstp dword); x87 at 53-bit precision in between.
        static const int dwellingCreature[5][6] = { { 0, 1, 2, 3, 4, 5 }, { 12, 13, 14, 15, 16, 17 }, { 6, 7, 8, 9, 10, 11 }, { 18, 19, 20, 21, 22, 23 },
                                                    { 29, 36, 37, 38, 30, 39 } }; // H1-INTENDED(D16) necromancer
        const int number = in.number;
        const int race = std::clamp( in.race, 0, 4 ); // H1-INTENDED(D16): 4 = necromancer (the original masks with 3)
        const double ad = in.ad;
        const double b1 = in.b1;
        int n = 0;
        int highest = -1; // 0..5
        for ( int d = 0; d < 6; ++d ) {
            if ( in.bits & ( 1 << ( d + 7 ) ) ) {
                ++n;
                highest = d;
            }
        }
        int filled = 0;
        for ( int k = 0; k < 5; ++k ) {
            if ( in.garrisonCounts[k] > 0 ) {
                ++filled;
            }
        }
        // FUN_00422f62: tables 0x491308 (guild by its byte), 0x491318 (buildings 1..6), 0x491338 (dwellings by race).
        auto value = [race]( const int building, const int guild ) {
            if ( building == 0 ) {
                return guildValue[std::clamp( guild, 0, 3 )];
            }
            if ( building < 7 ) {
                return commonValue[building];
            }
            return dwellingValue[race][building - 7];
        };
        // FUN_004115d3( race, building, 0 ).
        auto count = [&in, race]( const int building ) {
            int c = 0;
            for ( int k = 0; k < in.castles; ++k ) {
                if ( building >= 7 && std::clamp( in.castleRace[k], 0, 4 ) != race ) {
                    continue;
                }
                if ( building == 0 ) {
                    if ( ( in.castleBits[k] & 1 ) && in.castleGuild[k] == 0 ) {
                        ++c;
                    }
                }
                else if ( in.castleBits[k] & ( 1 << building ) ) {
                    ++c;
                }
            }
            return c;
        };
        float b = static_cast<float>( value( number, number == 0 ? in.guildByte : 0 ) );
        if ( number == 0 && in.guildByte > 0 ) {
            b = static_cast<float>( b - static_cast<double>( value( 0, in.guildByte - 1 ) ) );
        }
        switch ( number ) {
        case 0:
            b = static_cast<float>( ( ad * 2.0f + 0.33 ) * b );
            b = static_cast<float>( ( 1.33 - count( 0 ) * 0.33 ) * b );
            break;
        case 1:
        case 5:
            break;
        case 2: {
            H1::StrengthInput s;
            for ( int k = 0; k < 5; ++k ) {
                s.creatures[k] = in.garrison[k];
                s.counts[k] = in.garrisonCounts[k];
            }
            b = static_cast<float>( static_cast<double>( H1::armyStrengthOriginal( s ) ) / 3000.0f * b );
            break;
        }
        case 3:
            b = 0;
            break;
        case 4:
            b = static_cast<float>( ( ad + 0.66 ) * b );
            b = static_cast<float>( ( b1 * 2.0f + 0.33 ) * b );
            b = static_cast<float>( ( n * 0.33 + 0.66 ) * b );
            break;
        case 6:
            b = static_cast<float>( ( ad * 2.0f + 0.33 ) * b );
            b = static_cast<float>( ( 1.6 - n * 0.2 ) * b );
            break;
        default: {
            const int creature = dwellingCreature[race][number - 7];
            if ( filled == 5 ) {
                bool has = false;
                for ( int k = 0; k < 5; ++k ) {
                    has = has || in.garrison[k] == creature;
                }
                if ( !has ) {
                    break;
                }
            }
            b = static_cast<float>( ( ad + 0.66 ) * b );
            b = static_cast<float>( ( b1 * 2.0f + 0.33 ) * b );
            b = static_cast<float>( ( 1.0 - count( number ) * 0.05 ) * b );
            if ( number - 7 < highest ) {
                b = static_cast<float>( ( 1.66 - n * 0.33 ) * b );
            }
            if ( in.bits & 0x10 ) {
                b = static_cast<float>( b * 1.1 );
            }
            for ( int d = 0; d < 6; ++d ) {
                const int other = dwellingCreature[race][d];
                if ( ( in.bits & ( 1 << ( d + 7 ) ) ) && in.dwellingCounts[d] > 0 && creatureRank( creature ) < creatureRank( other ) * 1.2 ) {
                    b = 0;
                    break;
                }
            }
            break;
        }
        }
        // FUN_0042caa8: 0.6f x 0.15f; x (1 - that x 3.0).
        const float danger = 0.6f * 0.15f;
        b = static_cast<float>( ( 1.0 - danger * 3.0 ) * b );
        if ( b < 0.0f ) {
            b = 0;
        }
        // FUN_0042dfb8 / FUN_0042ce01.
        float days = 0;
        for ( int r = 0; r < 7; ++r ) {
            if ( in.have[r] < in.cost[r] ) {
                const float d = ( in.income[r] < 1 ) ? 99.0f : static_cast<float>( ( in.cost[r] - in.have[r] ) / in.income[r] + 1 );
                days = std::max( days, d );
            }
        }
        float w = static_cast<float>( 1.0f - days * static_cast<double>( 0.15f ) );
        if ( w < 0.0f ) {
            w = 0;
        }
        b = static_cast<float>( static_cast<double>( w ) * b );
        const int price = bundleValueOriginal( in.cost );
        return static_cast<float>( b / static_cast<double>( price ) );
    }
}

namespace H1::Ai
{
    CreatureScore creatureScoreOriginal( const CreatureInput & in )
    {
        // 0x42c164..0x42c442: the value is cut to a whole number after every step.
        auto ftolOf = []( const double v ) { return static_cast<int>( v ); };
        const int creature = in.creature;
        const int n = in.count;
        // FUN_00422c6a: one creature's price (gold, and one of its extra resource).
        int gold = 0;
        int extra = -1;
        H1::creaturePrice( creature, gold, extra );
        int one[7] = { 0, 0, 0, 0, 0, 0, gold };
        if ( extra >= 0 && extra < 6 ) {
            one[extra] = 1;
        }
        const int price = bundleValueOriginal( one ) * n;
        const bool hitsBehind = ( H1::creatureRow( creature ).flags & 8 ) != 0;
        auto flagged = []( const int army[5] ) {
            int c = 0;
            for ( int k = 0; k < 5; ++k ) {
                if ( army[k] != -1 && ( H1::creatureRow( army[k] ).flags & 8 ) != 0 ) {
                    ++c;
                }
            }
            return c;
        };
        int v = ftolOf( static_cast<double>( H1::creatureWorthOf( creature ) * n ) * in.f9 );
        int count8 = 0;
        if ( in.hero ) {
            v = ftolOf( v * 1.1 );
            if ( in.heroClass == creature / 6 ) {
                v = ftolOf( v * static_cast<double>( 1.1f ) );
            }
            if ( hitsBehind ) {
                count8 = flagged( in.heroArmy );
                v = ftolOf( ( 1.18 - count8 * 0.06 ) * v );
            }
            v = ftolOf( ( in.b1 + 0.66 ) * v );
        }
        if ( hitsBehind ) {
            count8 += flagged( in.garrison );
            v = ftolOf( ( 1.18 - count8 * 0.06 ) * v );
        }
        // fld danger; fadd 0.96; fst t (float); fmul t; fmul t: the first factor stays unrounded, the other two are the stored float.
        const double u = static_cast<double>( 0.6f * 0.15f ) + 0.96;
        const float t = static_cast<float>( u );
        v = ftolOf( u * t * t * v );
        // FUN_0042dfb8 on the buffer FUN_00422c6a filled: the price of ONE creature (0x42c3f1: lea eax, [ebp - 0x24]), not of the count.
        float days = 0;
        for ( int r = 0; r < 7; ++r ) {
            const int cost = one[r];
            if ( in.have[r] < cost ) {
                const float d = ( in.income[r] < 1 ) ? 99.0f : static_cast<float>( ( cost - in.have[r] ) / in.income[r] + 1 );
                days = std::max( days, d );
            }
        }
        float w = static_cast<float>( 1.0f - days * static_cast<double>( 0.15f ) );
        if ( w < 0.0f ) {
            w = 0;
        }
        v = ftolOf( static_cast<double>( w ) * v );
        CreatureScore out;
        out.value = v;
        out.score = static_cast<float>( static_cast<double>( v ) / price );
        return out;
    }
}
