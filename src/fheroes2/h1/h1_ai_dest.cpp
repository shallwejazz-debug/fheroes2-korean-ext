// Heroes I computer player: destination choice FUN_0042aaf9, destination score FUN_0042cecb, surroundings FUN_0042d407, caches FUN_0042817b
// (homm1-remaster project, notes v32, v36, v142, v143). Not connected to the game yet (AC5): the engine AI still moves the computer.
#include "h1_ai.h"

#include <sstream>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "castle.h"
#include "color.h"
#include "h1_gfx.h"
#include "h1_overview.h"
#include "resource.h"
#include "h1_heroes.h"
#include "heroes.h"
#include "kingdom.h"
#include "rand.h"
#include "world.h"

namespace
{
    H1::Ai::State aiState;

    // __ftol: towards zero.
    int ftol( const double v )
    {
        return static_cast<int>( v );
    }

    int idx( const int x, const int y )
    {
        return x * 72 + y;
    }

    bool isHumanColour( const int colour )
    {
        return colour >= 0 && world.GetKingdom( Color::IndexToColor( colour ) ).isControlHuman();
    }
}

namespace H1::Ai
{
    State & state()
    {
        return aiState;
    }

    void resetState()
    {
        aiState = State();
    }

    std::string savedState()
    {
        std::ostringstream out;
        const State & s = aiState;
        out << "ai 1";
        for ( size_t i = 0; i < 6; ++i ) {
            out << ' ' << s.digConf[i] << ' ' << s.digPosX[i] << ' ' << s.digPosY[i];
        }
        for ( size_t i = 0; i < 36; ++i ) {
            out << ' ' << static_cast<int>( s.destX[i] ) << ' ' << static_cast<int>( s.destY[i] ) << ' ' << s.turnsOwned[i] << ' ' << s.lastOwner[i];
        }
        out << ' ' << s.lastDay;
        // Player +0xd5 (note v199); saves before it end here.
        out << " gain";
        for ( const auto & g : s.dayGain ) {
            for ( const int v : g ) {
                out << ' ' << v;
            }
        }
        return out.str();
    }

    void loadSavedState( const std::string & line )
    {
        resetState();
        std::istringstream in( line );
        std::string tag;
        int version = 0;
        if ( !( in >> tag >> version ) || tag != "ai" || version != 1 ) {
            return;
        }
        State s;
        for ( size_t i = 0; i < 6; ++i ) {
            if ( !( in >> s.digConf[i] >> s.digPosX[i] >> s.digPosY[i] ) ) {
                return;
            }
        }
        for ( size_t i = 0; i < 36; ++i ) {
            int dx = -1;
            int dy = -1;
            if ( !( in >> dx >> dy >> s.turnsOwned[i] >> s.lastOwner[i] ) ) {
                return;
            }
            s.destX[i] = static_cast<int8_t>( dx );
            s.destY[i] = static_cast<int8_t>( dy );
        }
        if ( !( in >> s.lastDay ) ) {
            return;
        }
        std::string gainTag;
        if ( in >> gainTag && gainTag == "gain" ) {
            std::array<std::array<int, 7>, 6> g{};
            bool ok = true;
            for ( auto & row : g ) {
                for ( int & v : row ) {
                    ok = ok && static_cast<bool>( in >> v );
                }
            }
            if ( ok ) {
                s.dayGain = g;
            }
        }
        aiState = s;
    }

    void recordDayGain( const int colourIndex, const int gain[7] )
    {
        if ( colourIndex >= 0 && colourIndex < 6 ) {
            for ( int r = 0; r < 7; ++r ) {
                aiState.dayGain[colourIndex][r] = gain[r];
            }
        }
    }

    const std::array<int, 7> & dayGain( const int colourIndex )
    {
        static const std::array<int, 7> none{};
        return ( colourIndex >= 0 && colourIndex < 6 ) ? aiState.dayGain[colourIndex] : none;
    }

    void clearCaches( const bool all, const int x, const int y )
    {
        State & s = aiState;
        for ( int cx = 0; cx < 72; ++cx ) {
            for ( int cy = 0; cy < 72; ++cy ) {
                if ( all ) {
                    s.surround[idx( cx, cy )] = -32001;
                    s.objectCache[idx( cx, cy )] = -32001;
                }
                else if ( std::abs( x - cx ) + std::abs( y - cy ) < 10 ) {
                    s.surround[idx( cx, cy )] = -32001;
                }
            }
        }
        if ( x >= 0 && x < 72 && y >= 0 && y < 72 ) {
            s.objectCache[idx( x, y )] = -32001;
        }
        for ( int h = 0; h < 45; ++h ) { // H1-INTENDED(D16): 45 hero numbers
            if ( all ) {
                s.heroWin[h] = -32001;
                continue;
            }
            // The original compares y with the hero's x and x with the hero's x again (both read +0x129a3) - kept as written.
            const Heroes * other = H1::heroByNumber( h );
            const int hx = ( other != nullptr ) ? other->GetCenter().x : 0;
            if ( std::abs( y - hx ) + std::abs( x - hx ) < 10 ) {
                s.heroWin[h] = -32001;
            }
        }
    }

    HeroView viewOf( const Heroes & hero )
    {
        HeroView v;
        v.hero = &hero;
        v.number = H1::heroNumberOfId( hero.GetID() );
        v.owner = ( hero.GetColor() != PlayerColor::NONE ) ? Color::GetIndex( hero.GetColor() ) : -1;
        v.x = hero.GetCenter().x;
        v.y = hero.GetCenter().y;
        v.maxMove = static_cast<int>( hero.GetMaxMovePoints() / H1::movementScale );
        v.moveLeft = static_cast<int>( hero.GetMovePoints() / H1::movementScale );
        v.heroClass = std::max( 0, H1::heroClass( hero ) );
        v.onBoat = hero.isShipMaster();
        return v;
    }

    int surroundings( const HeroView & hero, const int x, const int y, const bool wide, int & safetyOut )
    {
        State & s = aiState;
        if ( !wide && s.surround[idx( x, y )] != -32001 ) {
            safetyOut = s.safety[idx( x, y )];
            return s.surround[idx( x, y )];
        }
        safetyOut = 100;
        const Path::World & w = s.world;
        // A hero on a boat searches water, unless the tile is 0x1f.
        const bool boat = hero.onBoat && w.tile( x, y )[8] != 0x1f;
        // H1-INTENDED(D5): fast-processor range 36 (docs/INTENDED_DIFFERENCES.md). Do not change without the owner.
        // Range 36 (24 on a slow processor, 0x4a9ca0 = 0: see note v142 4), wide 60.
        const int range = wide ? 0x3c : 0x24;
        Path::Search grid;
        grid.run( w, x, y, 2, range, boat, false, 999, hero.heroClass, -1, -1, false, false );
        grid.grid[idx( x, y )].flags &= static_cast<uint8_t>( ~1 );
        int total = 0;
        for ( int cx = 0; cx < 72; ++cx ) {
            for ( int cy = 0; cy < 72; ++cy ) {
                const Path::Cell & cell = grid.at( cx, cy );
                if ( !cell.reached() ) {
                    continue;
                }
                const std::array<uint8_t, 10> & t = w.tile( cx, cy );
                const int c = cell.cost();
                // (a) values of the objects around (0x4aa0dc is the shared K).
                if ( ( !wide && ( t[8] & 0x80 ) != 0 ) || ( wide && t[8] == 0xbd ) ) {
                    int k = 100;
                    const int v = objectValue( hero, cx, cy, 0, k );
                    total = ftol( static_cast<double>( total ) + static_cast<double>( v ) / ( static_cast<double>( c ) + 2.0 ) );
                }
                // (b) heroes the hero is not sure to beat lower the safety.
                if ( t[8] == 0xbd ) {
                    const int other = t[9];
                    if ( other < 45 && s.heroWin[other] == -32001 ) {
                        int k = 100;
                        objectValue( hero, cx, cy, 0, k );
                    }
                    if ( other < 45 && s.heroWin[other] != -32001 && s.heroWin[other] < 100 ) {
                        const Heroes * h = H1::heroByNumber( other );
                        const int e = ( h != nullptr ) ? static_cast<int>( h->GetMaxMovePoints() / H1::movementScale ) : 0;
                        const int win = s.heroWin[other];
                        int r = 0;
                        const int ownerColour = ( h != nullptr && h->GetColor() != PlayerColor::NONE ) ? Color::GetIndex( h->GetColor() ) : -1;
                        if ( !isHumanColour( ownerColour ) ) {
                            r = ( ( e + 0x14 ) - c ) * ( 100 - win ) / ( e + 0x14 );
                        }
                        else if ( e < c ) {
                            r = ftol( ( 100 - win ) * 0.2 );
                        }
                        else if ( c < 0xf ) {
                            r = 100 - win;
                        }
                        else {
                            r = ( ( e - c ) + 10 ) * ( 100 - win ) / e;
                        }
                        safetyOut = ( 100 - r ) * safetyOut / 100;
                    }
                }
                // (c) another own hero closer than 32.
                if ( c < 0x20 && t[8] == 0xbd && t[9] != hero.number && w.heroOwner && w.heroOwner( t[9] ) == hero.owner ) {
                    total -= ( ( 0x20 - c ) * 1250 ) >> 5;
                }
            }
        }
        // 2-4: own heroes whose destination is closer than 9 (both on water or both on land).
        const int g0 = Path::groupOf( w.tile( x, y ) );
        if ( hero.hero != nullptr ) {
            for ( const Heroes * own : hero.hero->GetKingdom().GetHeroes() ) {
                if ( own == nullptr || own == hero.hero ) {
                    continue;
                }
                const HeroView ov = viewOf( *own );
                const int dx = state().heroDestX( ov.number );
                const int dy = state().heroDestY( ov.number );
                const int g = std::abs( dy - y ) + std::abs( dx - x );
                if ( g >= 9 ) {
                    continue;
                }
                const int g1 = Path::groupOf( w.tile( dx, dy ) );
                if ( ( g0 == 0 && g1 > 0 ) || ( g0 > 0 && g1 == 0 ) ) {
                    continue;
                }
                total -= ( 9 - g ) * 1250 / 9;
            }
        }
        total = ftol( static_cast<float>( total ) * 1.25f );
        if ( total > 32000 ) {
            total = 32000;
        }
        if ( !wide ) {
            s.surround[idx( x, y )] = static_cast<int16_t>( total );
            s.safety[idx( x, y )] = static_cast<int16_t>( safetyOut );
        }
        return total;
    }

    int destinationScore( const HeroView & hero, const Path::Search & grid, const int x, const int y, const bool guarded, const int guardX, const int guardY,
                          const bool turnBreak, const int breakX, const int breakY, const int mode )
    {
        // FUN_0042cecb.
        const Path::World & w = aiState.world;
        const int object = w.tile( x, y )[8];
        int kGuard = 100; // local_28
        int kHere = 100; // local_1c
        int pTarget = 100; // local_44
        int pBreak = 100; // local_18
        int kMonster = 100; // local_14
        int ignored = 100;
        surroundings( hero, hero.x, hero.y, false, ignored ); // result and safety unused (only the cache is filled)
        int around = surroundings( hero, x, y, false, pTarget );
        if ( ( object & 0x7f ) == 0x3e && around < 0 ) {
            around = 0;
        }
        int total = 0;
        if ( guarded ) {
            total += objectValue( hero, guardX, guardY, 1, kGuard );
        }
        if ( turnBreak ) {
            const int danger = surroundings( hero, breakX, breakY, true, pBreak );
            if ( danger < 0 ) {
                total += danger;
            }
        }
        int mx = 0;
        int my = 0;
        if ( Path::findGuard( w, x, y, mx, my, -1, -1 ) ) {
            switch ( object & 0x7f ) {
            case 2: case 4: case 6: case 8: case 9: case 0xb: case 0x1a: case 0x1b: case 0x1d: case 0x24: case 0x2b: case 0x30:
                break;
            default: {
                const int v = objectValue( hero, mx, my, 1, kMonster );
                if ( v < 0 ) {
                    total += v;
                }
                kGuard = ( kGuard == 100 ) ? kMonster : kGuard * kMonster / 100;
                break;
            }
            }
        }
        int value = 0;
        const State & st = aiState;
        if ( ( object & 0x80 ) != 0 || ( st.digX == x && st.digY == y ) ) {
            value = objectValue( hero, x, y, mode, kHere );
        }
        if ( kHere < 100 ) {
            around = around * kHere / 100;
        }
        if ( pTarget < 100 ) {
            value = pTarget * value / 100;
            around = around * pTarget / 100;
        }
        if ( pBreak < 100 ) {
            value = pBreak * value / 100;
            around = around * pBreak / 100;
        }
        if ( kGuard < 100 ) {
            if ( total < 1 ) {
                total += ( around + value ) * kGuard / 100;
            }
            else {
                total = ( around + total + value ) * kGuard / 100;
            }
        }
        else {
            total += value;
        }
        float d = static_cast<float>( grid.at( x, y ).cost() ) / static_cast<float>( std::max( 1, hero.maxMove ) );
        if ( !hero.onBoat ) {
            if ( d > 5.0f ) {
                d *= 3.0f;
            }
            else if ( d > 4.0f ) {
                d *= 2.5f;
            }
            else if ( d > 3.0f ) {
                d *= 2.0f;
            }
            else if ( d > 2.0f ) {
                d *= 1.7f;
            }
            else if ( d > 1.5f ) {
                d *= 1.4f;
            }
            else if ( d > 1.0f ) {
                d *= 1.2f;
            }
        }
        else {
            d = d * 0.5f + 0.5f;
        }
        int result = ftol( static_cast<double>( total ) / ( static_cast<double>( d ) + 0.2 ) );
        const int aroundPart = ftol( static_cast<double>( around * 2 ) / ( static_cast<double>( d ) + 1.0 ) );
        if ( kGuard == 100 ) {
            result += aroundPart;
        }
        if ( hero.onBoat && object == 0x1f ) {
            result += 0x28;
        }
        return result;
    }

    int chooseDestination( HeroView & hero, int range )
    {
        // FUN_0042aaf9.
        State & st = aiState;
        const Path::World & w = st.world;
        int bestX = -1;
        int bestY = -1;
        int best = -999999;
        st.boatCastle = -1;
        st.shipyardCandidate = false;
        st.hasShipyard = false;
        st.boatValued = false;
        int spacing = hero.maxMove / 6;
        const int group = Path::groupOf( w.tile( hero.x, hero.y ) );
        if ( group == 2 || group == 3 ) {
            spacing -= 1;
            range = ftol( range * 1.25 );
        }
        if ( group == 5 ) {
            spacing -= 2;
            range = ftol( range * 1.5 );
        }
        if ( spacing < 3 ) {
            spacing = 3;
        }
        Path::Search & grid = st.grid;
        grid.run( w, hero.x, hero.y, hero.direction, static_cast<int16_t>( range * 3 ), hero.onBoat, true, hero.moveLeft, hero.heroClass, -1, -1, false, false );
        grid.grid[idx( hero.x, hero.y )].flags &= static_cast<uint8_t>( ~1 );
        const int lastX = st.heroDestX( hero.number );
        const int lastY = st.heroDestY( hero.number );
        for ( int x = 0; x < 72; ++x ) {
            for ( int y = 0; y < 72; ++y ) {
                const Path::Cell & cell = grid.at( x, y );
                if ( !cell.reached() ) {
                    continue;
                }
                const std::array<uint8_t, 10> & t = w.tile( x, y );
                const int c = cell.cost();
                bool candidate = false;
                if ( range < c ) {
                    candidate = !( range * 2 < c ) && ( t[8] == 0xa8 || t[8] == 0xbd || ( t[8] == 0xbe && !hero.onBoat ) );
                }
                else {
                    const bool sample = ( x % spacing == 0 && y % spacing == 0 ) && ( hero.onBoat ? Path::groupOf( t ) == 0 : Path::groupOf( t ) != 0 );
                    candidate = ( t[8] & 0x80 ) != 0 || ( t[8] == 0x1f && hero.onBoat ) || sample || ( st.digX == x && st.digY == y );
                }
                int value = -100;
                bool skipBonus = false;
                if ( candidate ) {
                    if ( t[8] != 0xa8 && t[8] != 0xbd && hero.hero != nullptr ) {
                        for ( const Heroes * own : hero.hero->GetKingdom().GetHeroes() ) {
                            if ( own == nullptr || own == hero.hero ) {
                                continue;
                            }
                            const int n = H1::heroNumberOfId( own->GetID() );
                            if ( st.heroDestX( n ) == x && st.heroDestY( n ) == y ) {
                                value = -2000;
                                skipBonus = true;
                                break;
                            }
                        }
                    }
                    if ( !skipBonus ) {
                        const int score = destinationScore( hero, grid, x, y, ( cell.flags & 4 ) != 0, cell.guardX, cell.guardY, ( cell.flags >> 3 ) != 0,
                                                            cell.breakX, cell.breakY, 2 );
                        value = ( static_cast<int>( Rand::Get( 1, 50 ) ) + 0x4b ) * score / 100;
                    }
                }
                if ( !skipBonus && lastX == x && lastY == y ) {
                    value = ftol( static_cast<float>( value ) * 1.5f );
                    if ( std::abs( y - hero.y ) + std::abs( x - hero.x ) > 3 ) {
                        value += 1;
                    }
                }
                if ( best < value ) {
                    best = value;
                    bestX = x;
                    bestY = y;
                }
                else if ( value == best && value == 0 ) {
                    if ( std::abs( bestX - hero.x ) + std::abs( bestY - hero.y ) < std::abs( y - hero.y ) + std::abs( x - hero.x ) ) {
                        bestX = x;
                        bestY = y;
                    }
                }
            }
        }
        // v36 4: no good destination - get a boat at a coastal castle.
        if ( best < 0x4b && ( st.shipyardCandidate || st.hasShipyard ) && !st.boatValued && st.dayNumber > 3 ) {
            const int p = st.player;
            const Kingdom & kingdom = world.GetKingdom( Color::IndexToColor( p ) );
            // Player +0x7d wood + 6 x +0xd5 wood (0x42aaf9 area, decompiled line 18238; +0xd5 = the last day change's gain, note v199).
            const int wood = static_cast<int>( kingdom.GetFunds().Get( Resource::WOOD ) ) + dayGain( p )[0] * 6;
            if ( ( st.hasShipyard || st.shipyardGoal[p] < 0 || st.shipyardGoal[p] == st.boatCastle ) && ( st.hasShipyard ? 10 : 30 ) <= wood ) {
                if ( !st.hasShipyard ) {
                    st.shipyardGoal[p] = st.boatCastle;
                }
                st.boatGoal[p] = st.boatCastle;
                st.goalDay[p] = st.dayNumber;
                int32_t cx = 0;
                int32_t cy = 0;
                if ( H1::getListPosition( false, st.boatCastle, cx, cy ) ) {
                    best = 0x7b;
                    bestX = cx;
                    bestY = cy;
                    if ( hero.x == cx && hero.y == cy && hero.hero != nullptr ) {
                        const_cast<Heroes *>( hero.hero )->ResetMovePoints();
                        hero.moveLeft = 0;
                    }
                }
            }
            checkBuy();
        }
        hero.destX = bestX;
        hero.destY = bestY;
        if ( hero.number >= 0 && hero.number < 45 ) { // H1-INTENDED(D16)
            st.destX[hero.number] = static_cast<int8_t>( bestX );
            st.destY[hero.number] = static_cast<int8_t>( bestY );
        }
        return best;
    }

    bool enabled()
    {
        // Heroes I maps in Heroes I mode always use the Heroes I computer player (note v148); H1_ENGINE_AI keeps the engine one for tests.
        return std::getenv( "H1_ENGINE_AI" ) == nullptr;
    }

    void dangerFactors( const HeroView & hero )
    {
        State & s = aiState;
        const Path::World & w = s.world;
        // FUN_00428c79.
        s.danger1 = 1.0f;
        s.danger1Set = false;
        if ( hero.hero != nullptr ) {
            int mine = strength( hero.hero->GetArmy(), hero.hero, false, false, 0, 0 );
            if ( mine < 100 ) {
                mine = 100;
            }
            Path::Search grid;
            grid.run( w, hero.x, hero.y, hero.direction, static_cast<int16_t>( hero.maxMove << 2 ), hero.onBoat, false, hero.moveLeft, hero.heroClass, -1, -1,
                      false, false );
            float own = 0.0f;
            float other = 0.0f;
            const int half = mine >> 1;
            for ( int x = 0; x < 72; ++x ) {
                for ( int y = 0; y < 72; ++y ) {
                    const Path::Cell & cell = grid.at( x, y );
                    if ( !cell.reached() ) {
                        continue;
                    }
                    const std::array<uint8_t, 10> & t = w.tile( x, y );
                    const float c = static_cast<float>( static_cast<uint16_t>( cell.cost() ) );
                    if ( t[8] == 0xa8 ) {
                        const Castle * castle = world.getCastleEntrance( { x, y } );
                        if ( castle == nullptr ) {
                            continue;
                        }
                        const int g = strength( castle->GetArmy(), nullptr, false, false, 0, 0 );
                        if ( castle->GetColor() == hero.hero->GetColor() ) {
                            if ( mine * 2 < g ) {
                                own = ( static_cast<float>( hero.maxMove + 10 ) * ( static_cast<float>( g ) / static_cast<float>( mine * 2 ) - 1.0f ) ) / ( c + 10 ) + own;
                            }
                        }
                        else if ( half < g ) {
                            other = ( static_cast<float>( hero.maxMove + 0x1e ) * ( static_cast<float>( g ) / static_cast<float>( half ) - 1.0f ) ) / ( c + 0x1e ) + other;
                        }
                    }
                    else if ( t[8] == 0xbd && w.heroOwner && w.heroOwner( t[9] ) != hero.owner ) {
                        const Heroes * h = H1::heroByNumber( t[9] );
                        const int g = ( h != nullptr ) ? strength( h->GetArmy(), nullptr, false, false, 0, 0 ) : 0;
                        if ( half < g ) {
                            other = ( static_cast<float>( hero.maxMove + 0x1e ) * ( static_cast<float>( g ) / static_cast<float>( half ) - 1.0f ) ) / ( c + 0x1e ) + other;
                        }
                    }
                }
            }
            if ( 1.0f < own && 1.0f < other ) {
                s.danger1 = 3.0f / ( own + other + 2.0f );
                s.danger1Set = true;
            }
        }
        // FUN_0042905e.
        s.danger2 = 1.0f;
        s.danger2Set = false;
        if ( hero.hero != nullptr ) {
            int mine = strength( hero.hero->GetArmy(), hero.hero, true, false, 0, 0 );
            if ( mine < 100 ) {
                mine = 100;
            }
            if ( mine < 30000 ) {
                return;
            }
            int most = -1;
            for ( int x = 0; x < 72; ++x ) {
                for ( int y = 0; y < 72; ++y ) {
                    const std::array<uint8_t, 10> & t = w.tile( x, y );
                    if ( t[8] == 0xa8 ) {
                        const Castle * castle = world.getCastleEntrance( { x, y } );
                        if ( castle == nullptr || castle->GetColor() == hero.hero->GetColor() || castle->GetColor() == PlayerColor::NONE ) {
                            continue;
                        }
                        const int g = strength( castle->GetArmy(), nullptr, true, true, x, y );
                        if ( mine < g ) {
                            return;
                        }
                        most = std::max( most, g );
                    }
                    else if ( t[8] == 0xbd && w.heroOwner && w.heroOwner( t[9] ) != hero.owner ) {
                        const Heroes * h = H1::heroByNumber( t[9] );
                        if ( h == nullptr ) {
                            continue;
                        }
                        const Castle * in = h->inCastle();
                        const int g = strength( h->GetArmy(), nullptr, true, in != nullptr, in != nullptr ? in->GetCenter().x : 0, in != nullptr ? in->GetCenter().y : 0 );
                        if ( mine < g * 2 ) {
                            return;
                        }
                        most = std::max( most, g * 2 );
                    }
                }
            }
            if ( 0 < most ) {
                s.danger2 = static_cast<float>( ( static_cast<float>( most ) * 0.75 ) / static_cast<float>( mine ) );
                s.danger2Set = true;
            }
        }
    }

    namespace
    {
        const int pdx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
        const int pdy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

        // FUN_0043bd45: the hero may step from its tile in direction d (blocked tile, water / land by boat, upper pictures; 0xac passes).
        bool stepAllowed( const HeroView & hero, const int d )
        {
            const Path::World & w = aiState.world;
            const int nx = hero.x + pdx[d];
            const int ny = hero.y + pdy[d];
            const std::array<uint8_t, 10> & t = w.tile( nx, ny );
            if ( ( t[7] & 0x80 ) != 0 ) {
                return false;
            }
            if ( Path::groupOf( t ) == 0 ) {
                if ( !hero.onBoat && t[8] != 0xbe && t[8] != 0xa3 ) {
                    return false;
                }
            }
            else if ( hero.onBoat && t[8] != 0x1f && t[8] != 0xac ) {
                return false;
            }
            const std::array<uint8_t, 10> & here = w.tile( hero.x, hero.y );
            if ( ( ( 1u << d ) & 0x83 ) != 0 && here[2] != 0xff && ( here[6] & 0x80 ) == 0 && here[8] != 0xac ) {
                return false;
            }
            if ( ( ( 1u << d ) & 0x38 ) != 0 && t[2] != 0xff && ( t[6] & 0x80 ) == 0 && t[8] != 0xac ) {
                return false;
            }
            return true;
        }

        // FUN_0043bb3b.
        bool canEnter( const HeroView & hero, const int d )
        {
            const int nx = hero.x + pdx[d];
            const int ny = hero.y + pdy[d];
            if ( nx < 0 || ny < 0 || nx > 0x47 || ny > 0x47 ) {
                return false;
            }
            const std::array<uint8_t, 10> & t = aiState.world.tile( nx, ny );
            switch ( t[8] & 0x7f ) {
            case 3:
                return !hero.onBoat;
            case 0x3d:
                if ( hero.onBoat ) {
                    const Heroes * other = H1::heroByNumber( t[9] );
                    return other != nullptr && other->isShipMaster();
                }
                return true;
            case 2: case 4: case 6: case 8: case 9: case 0xb: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x24: case 0x2b: case 0x30:
                return !hero.onBoat;
            default:
                return stepAllowed( hero, d );
            }
        }
    }

    int sidePickup( const HeroView & hero )
    {
        const Path::World & w = aiState.world;
        if ( ( w.tile( hero.x, hero.y )[8] & 0x7f ) == 0x29 ) {
            return -1;
        }
        int bestDir = -1;
        int best = 100;
        for ( int d = 0; d < 8; ++d ) {
            if ( !canEnter( hero, d ) ) {
                continue;
            }
            const int nx = hero.x + pdx[d];
            const int ny = hero.y + pdy[d];
            const std::array<uint8_t, 10> & t = w.tile( nx, ny );
            if ( ( t[8] & 0x80 ) == 0 || ( w.monsterArea[ny + nx * 72] & 0x80 ) != 0 ) {
                continue;
            }
            const int kind = t[8] & 0x7f;
            if ( kind == 0x29 || kind == 0x2c ) {
                continue;
            }
            int k = 100;
            const int v = objectValue( hero, nx, ny, 2, k );
            if ( k > 0x50 && best < v ) {
                bestDir = d;
                best = v;
            }
        }
        return bestDir;
    }
}
