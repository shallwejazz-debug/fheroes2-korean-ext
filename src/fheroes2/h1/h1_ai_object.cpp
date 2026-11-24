// Heroes I computer player: object values FUN_004307f8 with FUN_0042e76d (hire), FUN_0042f88d (reward fight), FUN_0042dcb0 (castle value) and
// FUN_0042eca6 (own castle visit, evaluation part) (homm1-remaster project, notes v34, v145). Not connected to the game yet (AC6).
#include "h1_ai.h"
#include "h1_visit.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
#include "h1_newgame.h"
#include "h1_rules.h"
#include "h1_spells.h"
#include "heroes.h"
#include "kingdom.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "monster.h"
#include "mp2.h"
#include "resource.h"
#include "world.h"

namespace
{
    const int artifactValue[38] = { 9000, 22000, 18000, 14000, 6000, 4000, 4000, 5600, 1200, 1200, 1200, 1200, -1200, 2000, 1800, 1800, 2000, 1000, 3600,
                                    5600, 4000, 5040, 2700, 3900, 4950, 5850, 7000, 6000, 4000, 4500, 2250, 1200, 1200, 1200, 1200, 3500, 1500, 16200 };
    // Spell tables (note v29): attribute 0x490f98 (bit 0 damage, bit 1 combat), base value 0x490f58, uses weight 0x492330 (index above 20 = 20).
    const int spellFlags[29] = { 3, 3, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
    const int spellWorth[29] = { 500, 350, 300, 400, 550, 900, 400, 500, 300, 350, 250, 0, 100, 150, 1000, 2000, 1700, 700, 700, 0, 0, 0, 0, 0, 0, 0, 0, 1200, 0 };
    const float usesWeight[21] = { 0.0f, 1.0f, 1.7f, 2.2f, 2.6f, 2.95f, 3.27f, 3.56f, 3.81f, 4.04f, 4.25f, 4.45f, 4.64f, 4.83f, 5.01f, 5.19f, 5.36f, 5.53f, 5.68f, 5.82f, 5.96f };

    int ftol( const double v )
    {
        return static_cast<int>( v );
    }

    int engineResource( const int r )
    {
        static const int map[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
        return map[r];
    }

    int32_t engineIndex( const int x, const int y )
    {
        return y * world.w() + x;
    }

    bool isHuman( const PlayerColor c )
    {
        return c != PlayerColor::NONE && world.GetKingdom( c ).isControlHuman();
    }

    int colourIndexOf( const PlayerColor c )
    {
        return ( c == PlayerColor::NONE ) ? -1 : Color::GetIndex( c );
    }

    // FUN_004304a6: uses weight[b] - uses weight[a].
    float usesDifference( const int a, const int b )
    {
        const float wb = usesWeight[std::clamp( b, 0, 20 )];
        const float wa = usesWeight[std::clamp( a, 0, 20 )];
        return wb - wa;
    }

    void fillArmy( Army & army, const int creature, const int perSlot )
    {
        if ( creature < 0 || creature >= H1::creatureCount() || perSlot <= 0 ) { // H1-INTENDED(D16): 36 in extension games
            return;
        }
        const Monster m( H1::creatureRow( creature ).monsterId );
        for ( size_t s = 0; s < 5 && s < army.Size(); ++s ) {
            army.GetTroop( s )->Set( m, static_cast<uint32_t>( perSlot ) );
        }
    }

    // FUN_0042cd5b: value of a resource bundle.
    int bundleValue( const int amounts[7] )
    {
        return H1::Ai::bundleValueOriginal( amounts );
    }

    float b2Of( const H1::Ai::HeroView & hero )
    {
        const int n = hero.number;
        return ( n >= 0 && n < 45 ) ? H1::Ai::state().heroB2[n] : 0.4f; // H1-INTENDED(D16): 45
    }

    // FUN_0042e76d (note v34 2-3): value of hiring "available" creatures (free or paid) at an object; count out.
    int hireValue( const H1::Ai::HeroView & hero, const int creature, const int available, const bool free, int & countOut, int * replaceSlot = nullptr )
    {
        countOut = 0;
        if ( replaceSlot != nullptr ) {
            *replaceSlot = -1;
        }
        if ( hero.hero == nullptr || creature < 0 || creature >= H1::creatureCount() ) { // H1-INTENDED(D16): 36 in extension games
            return 0;
        }
        int gold = 0;
        int extra = -1;
        H1::creaturePrice( creature, gold, extra );
        int n = available;
        if ( !free ) {
            // FUN_0042c6e1: the loop overwrites its result, so only the gold counts.
            const int have = static_cast<int>( hero.hero->GetKingdom().GetFunds().Get( Resource::GOLD ) );
            n = std::min( available, gold > 0 ? have / gold : available );
        }
        if ( n <= 0 ) {
            return 0;
        }
        int gain = H1::creatureWorthOf( creature ) * n;
        const Army & army = hero.hero->GetArmy();
        if ( !army.CanJoinTroop( Monster( H1::creatureRow( creature ).monsterId ) ) ) {
            // 0x42e81d: the slot with the lowest count x worth, where the worth is read from the creature table by the SLOT number
            // (0x491e7a + slot x 0x1f: peasant .. cavalry for slots 0 .. 4), not by the creature in the slot - kept as the original has
            // it (note v201). Start 999999, the first lowest wins; a slot holding this creature ends the search with no slot.
            int weakest = 999999;
            int slot = -1;
            for ( int i = 0; i < 5; ++i ) {
                const Troop * t = army.GetTroop( static_cast<size_t>( i ) );
                const bool valid = t != nullptr && t->isValid();
                if ( valid && H1::creatureFromMonster( t->GetID() ) == creature ) {
                    slot = -1;
                    break;
                }
                const int v = ( valid ? static_cast<int>( t->GetCount() ) : 0 ) * H1::creatureWorthOf( i );
                if ( v < weakest ) {
                    weakest = v;
                    slot = i;
                }
            }
            if ( replaceSlot != nullptr ) {
                *replaceSlot = slot;
            }
            if ( slot != -1 ) {
                gain -= weakest;
            }
        }
        int value = ftol( gain * static_cast<double>( H1::Ai::state().f9 ) );
        if ( !free ) {
            int amounts[7] = { 0, 0, 0, 0, 0, 0, gold };
            if ( extra >= 0 && extra < 6 ) {
                amounts[extra] = 1;
            }
            value -= bundleValue( amounts ) * n;
        }
        if ( value < 0 ) {
            return 0;
        }
        countOut = n;
        return value;
    }

    // FUN_0042f88d: max(0, trunc(reward x p + G)) of a fight against an army without owner.
    int rewardFight( const H1::Ai::HeroView & hero, const Army & enemy, const int reward, float & p )
    {
        const H1::Ai::Forecast f = H1::Ai::forecast( hero, enemy, nullptr, nullptr, false, 0, 0, -1 );
        p = f.p;
        const int v = ftol( reward * static_cast<double>( f.p ) + f.g );
        return v > 0 ? v : 0;
    }

    // FUN_0042dcb0 (note v34 2-8 1).
    int castleValue( const Castle & castle )
    {
        static const int guildValue[4] = { 4000, 6500, 8500, 10500 };
        static const int common[7] = { 0, 1500, 500, 2000, 3000, 0, 12000 };
        // Table 0x491338 by the castle's race number (order of note v12) [race order unconfirmed].
        static const int dwelling[4][6] = { { 858, 2225, 2816, 7385, 13754, 29785 },
                                            { 1684, 2256, 3736, 7213, 15181, 27684 },
                                            { 1802, 2615, 3414, 6967, 12212, 38141 },
                                            { 1956, 2607, 3869, 7510, 16002, 111967 } };
        const int bits = H1::castleBuildingBits( castle );
        int value = 0;
        for ( int b = 0; b <= 12; ++b ) {
            if ( ( bits & ( 1 << b ) ) == 0 ) {
                continue;
            }
            if ( b == 0 ) {
                const int level = H1::castleGuildByte( castle );
                value += ( level < 0 ) ? 0 : guildValue[std::min( level, 3 )];
            }
            else if ( b < 7 ) {
                value += common[b];
            }
            else {
                value += dwelling[std::clamp( H1::buildingRace( castle ), 0, 3 )][b - 7];
            }
        }
        return value + ftol( H1::Ai::state().resourceValue[6] * 1250 * 1.5 ) + 750;
    }

    // FUN_0042eca6 in evaluation mode (note v34 2-13): spells to learn and the army split.
    int ownCastleValue( const H1::Ai::HeroView & hero, const Castle & castle )
    {
        const Heroes * h = hero.hero;
        if ( h == nullptr ) {
            return 0;
        }
        int value = 0;
        const int bits = H1::castleBuildingBits( castle );
        const int knowledge = h->GetKnowledge();
        // Boat castle candidates (v36 4): the nearest own castle with a shipyard (bit 8), else the nearest with the Castle building and water
        // at (x - 1, y + 1). Distance = |dx| + |dy| from the hero, kept as a byte (0x4aa030).
        H1::Ai::State & st = H1::Ai::state();
        const int slot = H1::getCastleSlot( castle.GetIndex(), nullptr );
        const int cx = castle.GetCenter().x;
        const int cy = castle.GetCenter().y;
        const int8_t distance = static_cast<int8_t>( std::abs( cx - hero.x ) + std::abs( cy - hero.y ) );
        if ( ( bits & 8 ) == 0 || slot == st.boatCastle ) {
            if ( ( bits & 0x40 ) != 0 && H1::Path::groupOf( st.world.tile( cx - 1, cy + 1 ) ) == 0 && !st.hasShipyard && slot != st.boatCastle ) {
                if ( !st.shipyardCandidate || distance < st.boatCastleDistance ) {
                    st.boatCastleDistance = distance;
                    st.boatCastle = slot;
                }
                st.shipyardCandidate = true;
            }
        }
        else {
            if ( !st.hasShipyard || distance < st.boatCastleDistance ) {
                st.boatCastleDistance = distance;
                st.boatCastle = slot;
            }
            st.shipyardCandidate = true;
            st.hasShipyard = true;
        }
        // Spells of the guild (no spell book needed in this evaluation).
        if ( ( bits & 1 ) != 0 ) {
            // Spells of the guild: 3, 5, 7, 9 by guild level (table 0x49232c).
            static const int perLevel[4] = { 3, 5, 7, 9 };
            const int level = std::clamp( H1::castleGuildByte( castle ), 0, 3 );
            const std::array<int, 9> & spells = H1::guildSpells( castle );
            for ( int i = 0; i < perLevel[level]; ++i ) {
                const int spell = spells[i];
                if ( spell < 0 || spell >= 29 ) {
                    continue;
                }
                const int added = H1::learnSpell( const_cast<Heroes &>( *h ), spell, knowledge, true );
                const double times = ( spellFlags[spell] & 1 ) ? knowledge : 1;
                value += ftol( usesDifference( knowledge - added, knowledge ) * spellWorth[spell] * times );
            }
        }
        // Army split.
        // 0x42efe2: both strengths are asked with no hero (FUN_0042e009( army, 0, 0, 0, 0 )), note v201.
        const int vh = H1::Ai::strength( h->GetArmy(), nullptr, false, false, 0, 0 );
        const int vg = H1::Ai::strength( castle.GetArmy(), nullptr, false, false, 0, 0 );
        if ( vh + vg > 0 ) {
            // The share, the target and their distance are each stored as a 4-byte float (0x42f033, 0x42f08b, 0x42f0c8).
            const float r = static_cast<float>( static_cast<double>( vg ) / static_cast<double>( vh + vg ) );
            const int s = std::min( h->GetAttack() + h->GetDefense(), 10 );
            const float t = static_cast<float>( ( ( bits & 0x40 ) != 0 ) ? 0.54 - s * 0.02 : 0.33 - s * 0.01 );
            const float d = ( r <= t ) ? static_cast<float>( static_cast<double>( t ) - static_cast<double>( r ) )
                                       : static_cast<float>( static_cast<double>( r ) - static_cast<double>( t ) );
            if ( static_cast<double>( t ) * 0.15 <= static_cast<double>( d ) ) {
                const float m = ( vh < vg ) ? 0.25f : 0.13f;
                // 0x42f143: x = d + 1.0 - 0.22; x times x stored as a float; - 1.0; x (vh + vg); x +0xf9; x m.
                const double x = static_cast<double>( d ) + 1.0 - 0.22;
                const double product = ( ( x * static_cast<double>( static_cast<float>( x ) ) - 1.0 ) * ( vh + vg ) )
                                       * static_cast<double>( H1::Ai::state().f9 ) * static_cast<double>( m );
                int v = ftol( product );
                // The receiving army (hero when r > t, else the garrison) needs a free slot or a creature both have.
                const Army & from = ( r > t ) ? castle.GetArmy() : h->GetArmy();
                const Army & to = ( r > t ) ? h->GetArmy() : castle.GetArmy();
                bool room = to.GetOccupiedSlotCount() < to.Size();
                for ( size_t i = 0; i < from.Size() && !room; ++i ) {
                    const Troop * tr = from.GetTroop( i );
                    if ( tr != nullptr && tr->isValid() && to.HasMonster( Monster( tr->GetID() ) ) ) {
                        room = true;
                    }
                }
                if ( v > 0 && room ) {
                    value += v;
                }
            }
        }
        return value;
    }
}

namespace H1::Ai
{
    int objectValue( const HeroView & hero, const int x, const int y, const int mode, int & k )
    {
        // FUN_004307f8 (note v34).
        State & s = state();
        if ( x < 0 || y < 0 || x >= 72 || y >= 72 ) {
            k = 100;
            return 0;
        }
        const int cacheIndex = x * 72 + y;
        if ( mode == 0 && s.objectCache[cacheIndex] != -32001 ) {
            return s.objectCache[cacheIndex];
        }
        bool scaleAllowed = true; // 0x4aca60
        bool & danger2Allowed = s.danger2Allowed; // 0x4c2050: a global, nested values (teleports) leave theirs (note v199)
        danger2Allowed = true;
        k = 100;
        int value = 0; // 0x4aa0c8
        const Path::World & w = s.world;
        const std::array<uint8_t, 10> & t = w.tile( x, y );
        s.currentX = x;
        s.currentY = y;
        const int32_t index = engineIndex( x, y );
        const Maps::Tile & tile = world.getTile( index );
        // A human player visited the tile (table 0x4c0bf8, any player bit) [engine: the tile was visited by a human kingdom].
        bool humanVisited = false;
        for ( int c = 0; c < 4; ++c ) {
            const PlayerColor col = Color::IndexToColor( c );
            if ( isHuman( col ) && world.GetKingdom( col ).isVisited( index, tile.getMainObjectType( false ) ) ) {
                humanVisited = true;
            }
        }
        const float g = s.resourceValue[6];
        const float wv = b2Of( hero );
        const int b9 = t[9];
        const Heroes * h = hero.hero;
        const PlayerColor myColor = ( h != nullptr ) ? h->GetColor() : PlayerColor::NONE;
        const bool kingOfTheHill = H1::lastNewGameChoice().kingOfTheHill != 0;
        switch ( t[8] & 0x7f ) {
        case 0x01:
        case 0x19:
        case 0x20: {
            const PlayerColor owner = world.ColorCapturedObject( index );
            if ( owner == myColor ) {
                value = 0;
            }
            else if ( !kingOfTheHill || owner == PlayerColor::NONE || isHuman( owner ) ) {
                static const int production[7] = { 2, 1, 2, 1, 1, 1, 1000 };
                const Funds income = Maps::getDailyIncomeObjectResources( tile );
                const int er = income.getFirstValidResource().first;
                for ( int r = 0; r < 7; ++r ) {
                    if ( engineResource( r ) == er ) {
                        value = ftol( production[r] * static_cast<double>( s.resourceValue[r] ) * s.enemyNear[cacheIndex] );
                    }
                }
                if ( owner != PlayerColor::NONE ) {
                    value = ftol( value * static_cast<double>( isHuman( owner ) ? s.x : s.y ) );
                }
            }
            break;
        }
        case 0x02:
        case 0x21:
            value = 0;
            break;
        case 0x03:
        case 0x07:
        case 0x09:
        case 0x1c:
            // +0xae bits 2 / 0x10 / 4 / 8 (used until the next battle; h1_visit bonus bits, note v198).
            if ( h != nullptr && !H1::hasBonusBit( *h, ( t[8] & 0x7f ) == 0x03 ? 2 : ( ( t[8] & 0x7f ) == 0x07 ? 0x10 : ( ( t[8] & 0x7f ) == 0x09 ? 4 : 8 ) ) ) ) {
                value = ftol( 200 * static_cast<double>( wv ) );
            }
            break;
        case 0x24:
            if ( h != nullptr && !H1::hasBonusBit( *h, 0x100 ) ) {
                value = ftol( 400 * static_cast<double>( wv ) );
            }
            break;
        case 0x04:
            value = ( b9 == 1 ) ? 0 : ftol( s.averageArtifact * 0.1 );
            break;
        case 0x05:
            if ( b9 == 1 ) {
                value = 0;
            }
            else {
                value = ftol( 700 * static_cast<double>( wv ) + 1750 * static_cast<double>( g ) + s.averageArtifact );
                if ( b9 == 5 && h != nullptr && h->GetKingdom().GetFunds().Get( Resource::GOLD ) < 2500 ) {
                    value = -100;
                }
            }
            break;
        case 0x06:
            value = ftol( 1500 * static_cast<double>( g ) );
            break;
        case 0x08: {
            double sum = 0;
            for ( int r = 0; r < 6; ++r ) {
                sum += s.resourceValue[r];
            }
            value = ftol( sum / 6 * 5 + 500 * static_cast<double>( g ) );
            break;
        }
        case 0x0a:
            value = ( h != nullptr && h->isVisited( tile ) ) ? 0 : ftol( 1000 * static_cast<double>( wv ) );
            break;
        case 0x0b: {
            int n = 0;
            value = hireValue( hero, 0x1b, b9, false, n );
            scaleAllowed = false;
            break;
        }
        case 0x0c:
        case 0x23: {
            if ( b9 == 1 ) {
                value = 0;
                break;
            }
            int per = 0;
            int gold = 0;
            switch ( b9 ) {
            case 2:
                per = 2;
                gold = 1000;
                break;
            case 3:
                per = 3;
                gold = 2000;
                break;
            case 4:
                per = 5;
                gold = 5000;
                break;
            case 5:
                per = 10;
                gold = 2000;
                break;
            default:
                break;
            }
            Army ghosts;
            fillArmy( ghosts, 0x1a, per );
            float p = 1;
            value = rewardFight( hero, ghosts, ftol( gold * static_cast<double>( g ) + ( b9 == 5 ? s.averageArtifact : 0.0f ) ), p );
            break;
        }
        case 0x0d: {
            int n = 0;
            value = hireValue( hero, 6, b9, true, n );
            break;
        }
        case 0x0e:
        case 0x11: {
            int n = 0;
            value = hireValue( hero, 0, b9, true, n );
            scaleAllowed = false;
            break;
        }
        case 0x0f: {
            int n = 0;
            value = hireValue( hero, 1, b9, true, n );
            scaleAllowed = false;
            break;
        }
        case 0x10: {
            int n = 0;
            value = hireValue( hero, 0xd, b9, true, n );
            scaleAllowed = false;
            break;
        }
        case 0x16: {
            const int reward = ftol( s.enemyNear[cacheIndex] * static_cast<double>( g ) * 1000 * 1.5 );
            Army dragons;
            fillArmy( dragons, 0x17, 1 );
            const PlayerColor owner = world.ColorCapturedObject( index );
            float p = 1;
            if ( owner == myColor ) {
                value = 0;
            }
            else if ( owner == PlayerColor::NONE ) {
                value = rewardFight( hero, dragons, reward, p );
            }
            else {
                value = rewardFight( hero, dragons, ftol( reward * ( 1 + static_cast<double>( s.share ) ) ), p );
            }
            break;
        }
        case 0x17:
            // Game +0x14349: owner of the lighthouse (mine list entry 1) = the owner of this lighthouse tile.
            value = ( world.ColorCapturedObject( index ) == myColor ) ? 0 : 1000;
            break;
        case 0x18:
            value = ftol( b9 * 500 * static_cast<double>( g ) );
            break;
        case 0x1a: {
            // Byte 2 = the creature (0x430d36); a monster of the map file (0x9a) has it there, a random one (0xbf, 0xc3..0xc6) is the
            // engine's choice - the same rule as the visit and the fight (h1_visit, note v199).
            const uint8_t * mapBytes = H1::getTileBytes( index );
            const int creature = ( mapBytes != nullptr && mapBytes[8] == 0x9a ) ? mapBytes[2]
                                                                               : H1::creatureFromMonster( Maps::getMonsterFromTile( tile ).GetID() );
            // Byte 9 & 0x7f = the count (0x430cc5; the H1 count the visit and the fight use, h1_visit). Until v198 the engine's own
            // monster count stood here, which the new game draws separately (note v199).
            const int count = b9 & 0x7f;
            Army monsters;
            if ( count / 5 > 0 ) {
                fillArmy( monsters, creature, count / 5 );
            }
            if ( creature >= 0 ) {
                const Monster m( H1::creatureRow( creature ).monsterId );
                for ( int i = count % 5 - 1; i >= 0; --i ) {
                    Troop * tr = monsters.GetTroop( static_cast<size_t>( i ) );
                    tr->Set( m, tr->GetCount() + 1 );
                }
            }
            const Forecast f = forecast( hero, monsters, nullptr, nullptr, false, 0, 0, -1 );
            int n = 0;
            const int join = hireValue( hero, creature, count, true, n );
            const bool willJoin = ( t[9] & 0x80 ) != 0;
            const int myValue = ( h != nullptr ) ? strength( h->GetArmy(), h, false, false, 0, 0 ) : 0;
            if ( !willJoin || static_cast<double>( myValue ) <= static_cast<double>( H1::creatureWorthOf( creature ) * count ) * 1.75 ) {
                k = ftol( static_cast<double>( f.p ) * 100 );
                value = f.g;
            }
            else {
                k = ftol( static_cast<double>( f.p ) * 60 + 40 );
                const bool canJoin = ( h != nullptr && creature >= 0 ) && h->GetArmy().CanJoinTroop( Monster( H1::creatureRow( creature ).monsterId ) );
                value = ftol( ( canJoin ? join : 0 ) * 0.6 + f.g * 0.4 );
            }
            if ( value < 0 ) {
                scaleAllowed = false;
            }
            break;
        }
        case 0x1b:
            // Obelisk visit bit of this player (game +0x145a5[byte 9]) = the engine's global visit of the tile.
            value = ( h != nullptr && h->GetKingdom().isVisited( index, MP2::OBJ_OBELISK ) ) ? 0 : s.ultimate;
            break;
        case 0x1d: {
            const int r = static_cast<int>( t[2] ) - 0x3d;
            if ( r >= 0 && r < 6 ) {
                value = ftol( b9 * static_cast<double>( s.resourceValue[r] ) );
            }
            else if ( r == 6 ) {
                value = ftol( b9 * 100 * static_cast<double>( g ) );
            }
            break;
        }
        case 0x1e:
        case 0x1f:
        case 0x25:
        case 0x2e:
            value = 0;
            break;
        case 0x22: {
            const int knowledge = ( h != nullptr ) ? h->GetKnowledge() : 0;
            if ( h == nullptr || knowledge < 1 || !h->HaveSpellBook() ) {
                value = 0;
                break;
            }
            const int spell = b9 - 1;
            if ( spell < 0 || spell >= 29 ) {
                break;
            }
            const int added = H1::learnSpell( const_cast<Heroes &>( *h ), spell, knowledge, true );
            value = ftol( usesDifference( knowledge - added, knowledge ) * spellWorth[spell] );
            if ( spellFlags[spell] & 1 ) {
                value = ftol( value * static_cast<double>( H1::attackDefenseMultiplier( std::min( knowledge, 40 ) - 20 ) ) );
            }
            break;
        }
        case 0x27: {
            int n = 0;
            value = hireValue( hero, 0x19, b9, false, n );
            scaleAllowed = false;
            break;
        }
        case 0x28: {
            const Castle * castle = world.getCastleEntrance( { x, y } );
            if ( castle == nullptr ) {
                break;
            }
            const PlayerColor owner = castle->GetColor();
            if ( owner == myColor ) {
                if ( castle->GetHero() == nullptr ) {
                    value = ownCastleValue( hero, *castle );
                    const int n = hero.number;
                    value = ftol( value * static_cast<double>( ( n >= 0 && n < 45 ) ? s.heroFactor[n] : 1.0f ) );
                }
                else {
                    value = 0;
                }
                scaleAllowed = false;
            }
            else if ( !kingOfTheHill || owner == PlayerColor::NONE || isHuman( owner ) ) {
                int c = castleValue( *castle );
                Forecast f;
                f.p = 1.0f;
                f.g = 0;
                if ( const Heroes * guest = castle->GetHero(); guest != nullptr ) {
                    f = forecast( hero, guest->GetArmy(), guest, &castle->GetArmy(), true, x, y, colourIndexOf( owner ) );
                }
                else if ( castle->GetArmy().isValid() ) {
                    // FUN_0043c38b: the castle has a garrison.
                    f = forecast( hero, castle->GetArmy(), nullptr, nullptr, true, x, y, colourIndexOf( owner ) );
                }
                k = ftol( static_cast<double>( f.p ) * 100 );
                if ( owner != PlayerColor::NONE ) {
                    int seats = 0;
                    for ( const int sc : H1::lastNewGameChoice().seatColour ) {
                        seats += ( sc >= 0 ) ? 1 : 0;
                    }
                    // 0x431884: ((5 - seats) x 0.25 + 0.9) x X or Y first, then x the castle value (the order changes the last digit).
                    c = ftol( ( ( ( 5 - seats ) * 0.25 + 0.9 ) * static_cast<double>( isHuman( owner ) ? s.x : s.y ) ) * c );
                }
                value = ftol( c * static_cast<double>( f.p ) + f.g );
                if ( owner != PlayerColor::NONE ) {
                    danger2Allowed = false;
                }
            }
            break;
        }
        case 0x29:
        case 0x2c: {
            // Teleports: the best surroundings among tiles with the same byte 8 farther than 1 (Stone Liths) / 3 (Whirlpool).
            if ( !s.teleportGuard ) {
                value = 0;
                break;
            }
            s.teleportGuard = false;
            int best = -9999;
            for ( int cy = 0; cy < 72; ++cy ) {
                for ( int cx = 0; cx < 72; ++cx ) {
                    // Kind and byte 8 of DAT_004c0b94, which the nested values below move to the last tile they work out (0x431c50 area).
                    const std::array<uint8_t, 10> & current = w.tile( s.currentX, s.currentY );
                    const int minDistance = ( ( current[8] & 0x7f ) == 0x29 ) ? 1 : 3;
                    if ( std::abs( cx - x ) + std::abs( cy - y ) <= minDistance || w.tile( cx, cy )[8] != current[8] ) {
                        continue;
                    }
                    int safety = 100;
                    const int v = ftol( surroundings( hero, cx, cy, false, safety ) * 0.85 );
                    if ( best < v ) {
                        best = v;
                    }
                }
            }
            int safety = 100;
            const int here = surroundings( hero, hero.x, hero.y, false, safety );
            value = ( here + 200 < best ) ? best - here - 200 : -200;
            s.teleportGuard = true;
            scaleAllowed = false;
            break;
        }
        case 0x2a: {
            int n = 0;
            value = hireValue( hero, 0x18, b9, false, n );
            scaleAllowed = false;
            break;
        }
        case 0x2d:
            if ( b9 == 99 ) {
                value = 0;
            }
            else {
                int amounts[7] = { 0, 0, 0, 0, 0, 0, 0 };
                if ( b9 >= 0 && b9 < 7 ) {
                    amounts[b9] = 2;
                }
                value = bundleValue( amounts );
            }
            break;
        case 0x30: {
            const int art = t[2];
            const int b0 = ( art >= 0 && art < 38 ) ? artifactValue[art] : 0;
            Army rogues;
            fillArmy( rogues, 0x18, 10 );
            const Forecast f = forecast( hero, rogues, nullptr, nullptr, false, 0, 0, -1 );
            const int b1 = ftol( b0 * static_cast<double>( f.p ) + f.g );
            int b2 = ftol( b0 - 2000 * static_cast<double>( g ) );
            if ( b2 < 0 ) {
                b2 = 0;
            }
            if ( !humanVisited ) {
                value = ftol( b0 * 0.6 + b2 * 0.2 + b1 * 0.2 );
            }
            else if ( b9 == 1 ) {
                value = b0;
            }
            else if ( b9 == 2 ) {
                value = b1;
            }
            else if ( b9 == 3 ) {
                value = b2;
            }
            break;
        }
        case 0x3d: {
            const Heroes * other = H1::heroByNumber( b9 );
            const PlayerColor owner = ( other != nullptr ) ? other->GetColor() : PlayerColor::NONE;
            if ( owner == myColor ) {
                if ( b9 < 45 ) { // H1-INTENDED(D16): 45 hero numbers
                    s.heroWin[b9] = 100;
                }
                value = ( mode == 0 || s.danger1Set ) ? 0 : -5000;
                k = 0;
            }
            else if ( !kingOfTheHill || isHuman( owner ) ) {
                if ( other == nullptr ) {
                    break;
                }
                int c = 0;
                const Castle * castle = other->inCastle();
                if ( castle != nullptr ) {
                    c = castleValue( *castle );
                    if ( castle->GetColor() != PlayerColor::NONE ) {
                        int seats = 0;
                        for ( const int sc : H1::lastNewGameChoice().seatColour ) {
                            seats += ( sc >= 0 ) ? 1 : 0;
                        }
                        c = ftol( c * ( ( 5 - seats ) * 0.25 * static_cast<double>( isHuman( castle->GetColor() ) ? s.x : s.y ) + 1 ) );
                    }
                }
                const Forecast f = forecast( hero, other->GetArmy(), other, castle != nullptr ? &castle->GetArmy() : nullptr, castle != nullptr,
                                             castle != nullptr ? castle->GetCenter().x : 0, castle != nullptr ? castle->GetCenter().y : 0, colourIndexOf( owner ) );
                value = f.g;
                k = ftol( static_cast<double>( f.p ) * 100 );
                if ( c > 0 ) {
                    value = ftol( c * static_cast<double>( f.p ) + f.g );
                }
                if ( mode != 0 && isHuman( owner ) && value > 200 ) {
                    value = ftol( value * 1.5 );
                }
                const float p = f.p;
                int win = 100;
                if ( p > 0.75f ) {
                    win = 100;
                }
                else if ( p > 0.5f ) {
                    win = ftol( p * 136.0 );
                }
                else if ( p > 0.4f ) {
                    win = ftol( p * 130.0 );
                }
                else if ( p > 0.3f ) {
                    win = ftol( p * 125.0 );
                }
                else if ( p > 0.2f ) {
                    win = ftol( p * 113.0 );
                }
                else {
                    win = ftol( p * 100.0 );
                }
                if ( b9 < 45 ) { // H1-INTENDED(D16): 45 hero numbers
                    s.heroWin[b9] = static_cast<int16_t>( std::min( win, 100 ) );
                }
                if ( mode == 0 && p < 0.4f ) {
                    value = ftol( value * ( 3 - 2 * static_cast<double>( p ) ) );
                }
                if ( mode == 0 && p < 0.2f ) {
                    value = ftol( value * ( 2 - 2 * static_cast<double>( p ) ) );
                }
                if ( value < 0 ) {
                    scaleAllowed = false;
                }
                danger2Allowed = false;
            }
            else {
                value = 0;
                k = 100;
            }
            break;
        }
        case 0x3e:
            s.boatValued = true;
            value = 100;
            break;
        default:
            // v34 2-12: the tile the computer means to dig.
            if ( s.digConfidence >= 0x10 && s.digX == x && s.digY == y ) {
                value = ( s.digConfidence - 0xf ) * 16200 / 100;
            }
            else {
                const int kind = t[8] & 0x7f;
                if ( kind > 0x30 && kind < 0x3d ) {
                    value = 0;
                }
            }
            break;
        }
        if ( s.danger1Set && scaleAllowed ) {
            value = ftol( value * static_cast<double>( s.danger1 ) );
        }
        if ( s.danger2Set && danger2Allowed ) {
            value = ftol( value * static_cast<double>( s.danger2 ) );
        }
        if ( mode == 0 ) {
            // The kind is read through DAT_004c0b94 (0x4327ae, 0x4327db): after a teleport's nested values it is the last tile worked out.
            const int currentKind = w.tile( s.currentX, s.currentY )[8] & 0x7f;
            if ( value > 0 && ( w.monsterArea[y + x * 72] & 0x80 ) != 0 && currentKind != 0x1a ) {
                value = 0;
            }
            if ( value < 0 && currentKind != 0x3d ) {
                value = 0;
            }
            else if ( value > 32000 ) {
                value = 32000;
            }
            else if ( value < -32000 ) {
                value = -32000;
            }
            s.objectCache[cacheIndex] = static_cast<int16_t>( value );
        }
        return value;
    }

    void ownCastleActions( Heroes & hero, Castle & castle )
    {
        State & s = state();
        const int bits = H1::castleBuildingBits( castle );
        const int knowledge = hero.GetKnowledge();
        Kingdom & kingdom = hero.GetKingdom();
        if ( knowledge > 0 && !hero.HaveSpellBook() && ( bits & 1 ) != 0 ) {
            if ( kingdom.GetFunds().Get( Resource::GOLD ) < 500 ) {
                hero.ResetMovePoints();
            }
            else {
                hero.SpellBookActivate();
                kingdom.OddFundsResource( Funds( Resource::GOLD, 500 ) );
            }
        }
        if ( ( bits & 1 ) != 0 && hero.HaveSpellBook() ) {
            static const int perLevel[4] = { 3, 5, 7, 9 };
            const int level = std::clamp( H1::castleGuildByte( castle ), 0, 3 );
            const std::array<int, 9> & spells = H1::guildSpells( castle );
            for ( int i = 0; i < perLevel[level]; ++i ) {
                if ( spells[i] >= 0 && spells[i] < 29 ) {
                    H1::learnSpell( hero, spells[i], knowledge, false );
                }
            }
        }
        // Army split.
        Army & heroArmy = hero.GetArmy();
        Army & garrison = castle.GetArmy();
        const int vh = strength( heroArmy, nullptr, false, false, 0, 0 );
        const int vg = strength( garrison, nullptr, false, false, 0, 0 );
        if ( vh + vg <= 0 ) {
            return;
        }
        const float r = static_cast<float>( vg ) / static_cast<float>( vh + vg );
        const int sk = std::min( hero.GetAttack() + hero.GetDefense(), 10 );
        const float t = ( ( bits & 0x40 ) == 0 ) ? static_cast<float>( 0.33 - sk * 0.01 ) : static_cast<float>( 0.54 - sk * 0.02 );
        const float d = ( t < r ) ? r - t : t - r;
        if ( static_cast<double>( t ) * 0.15 > d ) {
            return;
        }
        const bool toHero = t < r;
        int amount = ftol( ( vh + vg ) * static_cast<double>( toHero ? d + 0.04f : d ) );
        Army & from = toHero ? garrison : heroArmy;
        Army & to = toHero ? heroArmy : garrison;
        const int base = toHero ? vg : vh;
        for ( bool go = true; go; ) {
            int total = 0;
            if ( !toHero ) {
                for ( size_t i = 0; i < heroArmy.Size(); ++i ) {
                    const Troop * tr = heroArmy.GetTroop( i );
                    total += ( tr != nullptr && tr->isValid() ) ? static_cast<int>( tr->GetCount() ) : 0;
                }
                if ( total < 2 ) {
                    return;
                }
            }
            int pick = -1;
            for ( size_t i = 0; i < from.Size() && pick < 0; ++i ) {
                const Troop * tr = from.GetTroop( i );
                if ( tr != nullptr && tr->isValid() && to.HasMonster( Monster( tr->GetID() ) ) ) {
                    pick = static_cast<int>( i );
                }
            }
            if ( pick < 0 ) {
                // Fastest for the hero, slowest for the garrison; equal speed: the bigger value.
                int speed = toHero ? 1 : 3;
                int bestValue = -9999;
                for ( size_t i = 0; i < from.Size(); ++i ) {
                    const Troop * tr = from.GetTroop( i );
                    if ( tr == nullptr || !tr->isValid() ) {
                        continue;
                    }
                    const int c = H1::creatureFromMonster( tr->GetID() );
                    const int sp = ( c >= 0 ) ? H1::creatureRow( c ).speedLevel : 0;
                    const int v = H1::creatureWorthOf( c ) * static_cast<int>( tr->GetCount() );
                    if ( ( toHero && speed < sp ) || ( !toHero && sp < speed ) ) {
                        speed = sp;
                        pick = static_cast<int>( i );
                        bestValue = v;
                    }
                    else if ( sp == speed && bestValue < v ) {
                        pick = static_cast<int>( i );
                        bestValue = v;
                    }
                }
            }
            if ( pick < 0 ) {
                break;
            }
            Troop * tr = from.GetTroop( static_cast<size_t>( pick ) );
            const Monster m( tr->GetID() );
            if ( !to.CanJoinTroop( m ) ) {
                break;
            }
            const int worth = H1::creatureWorthOf( H1::creatureFromMonster( tr->GetID() ) );
            int n = ftol( static_cast<double>( amount ) / worth + 0.5 );
            if ( n < 1 ) {
                break;
            }
            const int have = static_cast<int>( tr->GetCount() );
            if ( have < n ) {
                n = have;
            }
            else {
                go = false;
                if ( have * 0.65 <= n || have - 1 <= n ) {
                    if ( static_cast<double>( worth * ( have - n ) ) < ( base - amount ) * 0.2 ) {
                        n = have;
                    }
                }
            }
            if ( !toHero && total <= n ) {
                n = total - 1;
                go = false;
            }
            if ( worth * n * 1.2 <= amount ) {
                amount -= worth * n;
            }
            else {
                go = false;
            }
            to.JoinTroop( m, static_cast<uint32_t>( n ), false );
            tr->SetCount( static_cast<uint32_t>( have - n ) );
            if ( have - n == 0 ) {
                tr->Reset();
            }
        }
        const int slot = H1::getCastleSlot( castle.GetIndex(), nullptr );
        if ( slot == s.capturedCastle && hero.GetMovePoints() < 0x15 * H1::movementScale ) {
            hero.ResetMovePoints();
        }
    }

    int computerHire( Heroes & hero, const int creature, const int available, const bool free, const int addCount )
    {
        // FUN_004046c3 (LAB_00404e92): FUN_0042e76d decides the count and the slot to replace when the army is full; FUN_0041c6f2 adds them;
        // paid creatures cost their price x count.
        if ( available <= 0 ) {
            return 0;
        }
        const HeroView view = viewOf( hero );
        int count = 0;
        int slot = -1;
        hireValue( view, creature, available, free, count, &slot );
        if ( count <= 0 ) {
            return 0;
        }
        if ( addCount >= 0 ) {
            count = addCount;
        }
        const Monster m( H1::creatureRow( creature ).monsterId );
        Army & army = hero.GetArmy();
        if ( slot >= 0 ) {
            army.GetTroop( static_cast<size_t>( slot ) )->Set( m, static_cast<uint32_t>( count ) );
        }
        else if ( !army.JoinTroop( m, static_cast<uint32_t>( count ), false ) ) {
            return 0;
        }
        if ( !free ) {
            int gold = 0;
            int extra = -1;
            H1::creaturePrice( creature, gold, extra );
            Funds cost( Resource::GOLD, static_cast<uint32_t>( gold * count ) );
            if ( extra >= 0 && extra < 6 ) {
                cost += Funds( engineResource( extra ), static_cast<uint32_t>( count ) );
            }
            hero.GetKingdom().OddFundsResource( cost );
        }
        return count;
    }

    bool computerTakesGold( const Heroes & hero, const int gold, const int experience )
    {
        // FUN_0042f82c: trunc(experience x hero +0xb2) < trunc(gold x g).
        const State & s = state();
        const int n = H1::heroNumberOfId( hero.GetID() );
        const double b2 = ( n >= 0 && n < 45 ) ? s.heroB2[n] : 0.4;
        return ftol( experience * b2 ) < ftol( gold * static_cast<double>( s.resourceValue[6] ) );
    }

    int fightWorth( const Heroes & hero, const Army & enemy, const int reward )
    {
        float p = 1;
        return rewardFight( viewOf( hero ), enemy, reward, p );
    }

    bool computerBuysArtifact( const Heroes & hero, const int h1Artifact, const int price )
    {
        // FUN_0042f954: price <= gold and price x g < artifact value (table 0x491960).
        const int worth = ( h1Artifact >= 0 && h1Artifact < 38 ) ? artifactValue[h1Artifact] : 0;
        return price <= static_cast<int>( hero.GetKingdom().GetFunds().gold )
               && price * static_cast<double>( static_cast<float>( state().resourceValue[6] ) ) < worth;
    }
}
