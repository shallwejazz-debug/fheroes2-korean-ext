// Heroes I computer player: turn values FUN_00429db9, army strength FUN_0042e009, battle forecast FUN_0042b42b, object values FUN_004307f8
// and buying FUN_0042868b (homm1-remaster project, notes v33, v34, v35, v144), quick combat FUN_0042e947 (note v38, v150).
#include "h1_heroes.h"
#include "h1_ai.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "artifact_ultimate.h"
#include "castle.h"
#include "color.h"
#include "h1_armywin.h"
#include "h1_battle.h"
#include "h1_castle.h"
#include "h1_gfx.h"
#include "h1_newgame.h"
#include "mp2.h"
#include "maps_tiles_helper.h"
#include "maps_tiles.h"
#include "h1_sound.h"
#include "h1_map_render.h"
#include "h1_heroscreen.h"
#include "h1_obelisk.h"
#include "h1_overview.h"
#include "h1_rules.h"
#include "h1_ui.h"
#include "heroes.h"
#include "kingdom.h"
#include "rand.h"
#include "resource.h"
#include "world.h"
#include "world_object_uid.h"

namespace
{
    // Table 0x491960: artifact AI values (0..37; 37 = magic book).
    const int artifactValue[38] = { 9000, 22000, 18000, 14000, 6000, 4000, 4000, 5600, 1200, 1200, 1200, 1200, -1200, 2000, 1800, 1800, 2000, 1000, 3600,
                                    5600, 4000, 5040, 2700, 3900, 4950, 5850, 7000, 6000, 4000, 4500, 2250, 1200, 1200, 1200, 1200, 3500, 1500, 16200 };
    // Table 0x4918b0: base value of one unit (wood, mercury, ore, sulfur, crystal, gems, gold).
    const double resourceBase[7] = { 250, 250, 200, 250, 250, 250, 1 };

    int ftol( const double v )
    {
        return static_cast<int>( v );
    }

    int engineResource( const int r )
    {
        static const int map[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
        return map[r];
    }

    int h1Artifact( const Artifact & art )
    {
        if ( !art.isValid() ) {
            return -1;
        }
        return art.GetID() == Artifact::MAGIC_BOOK ? 37 : H1::h1ArtifactFromEngine( art.GetID() );
    }

    // Sum of the AI values of the hero's artifacts 0..36 (14 slots).
    int artifactSum( const Heroes * hero )
    {
        int s = 0;
        if ( hero == nullptr ) {
            return 0;
        }
        for ( const Artifact & a : hero->GetBagArtifacts() ) {
            const int h = h1Artifact( a );
            if ( h >= 0 && h <= 36 ) {
                s += artifactValue[h];
            }
        }
        return s;
    }

    void splitArmy( const Army & army, int creatures[5], int counts[5] )
    {
        for ( int k = 0; k < 5; ++k ) {
            creatures[k] = -1;
            counts[k] = 0;
            const Troop * t = ( static_cast<size_t>( k ) < army.Size() ) ? army.GetTroop( static_cast<size_t>( k ) ) : nullptr;
            if ( t != nullptr && t->isValid() ) {
                creatures[k] = H1::creatureFromMonster( t->GetID() );
                counts[k] = static_cast<int>( t->GetCount() );
            }
        }
    }

    // FUN_0041c7ee: hit points of the army (creature table +0x08 x count) + 500 with a hero (the experience of beating it).
    int experienceOf( const Army & army, const bool withHero )
    {
        // Creature table +0x08 = H1::creatureExperience (H1-INTENDED(D16) 28..35 = hit points).
        int creatures[5];
        int counts[5];
        splitArmy( army, creatures, counts );
        int v = 0;
        for ( int k = 0; k < 5; ++k ) {
            if ( counts[k] > 0 && creatures[k] >= 0 ) {
                v += H1::creatureExperience( creatures[k] ) * counts[k];
            }
        }
        return withHero ? v + 500 : v;
    }

    bool isHuman( const int colour )
    {
        return colour >= 0 && world.GetKingdom( Color::IndexToColor( colour ) ).isControlHuman();
    }

    int seatCount()
    {
        int n = 0;
        for ( const int c : H1::lastNewGameChoice().seatColour ) {
            n += ( c >= 0 ) ? 1 : 0;
        }
        return n;
    }
}

namespace H1::Ai
{
    int strength( const Army & army, const Heroes * hero, const bool corrections, const bool castle, const int castleX, const int castleY )
    {
        int creatures[5];
        int counts[5];
        splitArmy( army, creatures, counts );
        int bits = 0;
        if ( castle ) {
            const Castle * c = world.getCastleEntrance( { castleX, castleY } );
            bits = ( c != nullptr ) ? H1::castleBuildingBits( *c ) : 0;
        }
        // FUN_0042e009 calls FUN_00467754( hero army, hero, 0 ) and FUN_0041ca0a( hero ): no castle part (the engine's army morale would
        // add the castle's tavern when the hero stands in one, note v187).
        const int morale = ( hero != nullptr ) ? H1::heroMorale( hero, army, nullptr ) : 0;
        const int luck = ( hero != nullptr ) ? H1::heroLuck( hero ) : 0;
        return H1::armyStrengthFull( creatures, counts, hero, morale, luck, corrections, castle, bits );
    }

    void prepareTurn( const int colourIndex )
    {
        // FUN_00429db9 (note v33 4).
        State & s = state();
        const PlayerColor color = Color::IndexToColor( colourIndex );
        const Kingdom & kingdom = world.GetKingdom( color );
        s.player = colourIndex;
        s.level = H1::computerLevel( kingdom );
        // 1. Day number 0x4c0b9c.
        s.dayNumber = static_cast<int>( world.CountDay() );
        // 2. Personality (FUN_0042cc6e): fixed every turn, +0xb5 by the day number.
        s.ad = 0.4f;
        s.b1 = 0.3f;
        s.b5 = 0.3f;
        if ( s.dayNumber < 5 ) {
            s.b5 *= 1.6f;
        }
        else if ( s.dayNumber < 10 ) {
            s.b5 *= 1.4f;
        }
        else if ( s.dayNumber < 20 ) {
            s.b5 *= 1.2f;
        }
        else if ( s.dayNumber < 30 ) {
            s.b5 *= 1.0f;
        }
        else {
            s.b5 *= 0.8f;
        }
        // 3. Resource values (FUN_0042dd61).
        // Player +0x7d (resources) and +0xd5 (what the last day change gave, note v199).
        const Funds funds = kingdom.GetFunds();
        const std::array<int, 7> & gain = dayGain( colourIndex );
        int wsum = 0;
        int wv[7];
        for ( int r = 0; r < 7; ++r ) {
            const int have = static_cast<int>( funds.Get( engineResource( r ) ) );
            wv[r] = ftol( ( gain[r] * 5 * 0.7 + have ) * resourceBase[r] );
            wsum += wv[r];
        }
        // The mean is kept as a float and each ratio is stored as a float before it is used (0x42de27, 0x42de56); a mean of 0 divides
        // by zero as the original does.
        const float mean = static_cast<float>( wsum / 7 );
        for ( int r = 0; r < 7; ++r ) {
            const float ratio = static_cast<float>( static_cast<double>( wv[r] ) / static_cast<double>( mean ) );
            s.resourceValue[r] = static_cast<float>( resourceBase[r] / ( ratio / 2.0 + 0.5 ) );
        }
        // The planned digging tile of this player (FUN_0041173d results).
        s.digConfidence = s.digConf[colourIndex];
        s.digX = s.digPosX[colourIndex];
        s.digY = s.digPosY[colourIndex];
        // 6. Boat goals older than 8 days are dropped; 0x4c0ba0 = 0.
        s.heroHired = false;
        if ( s.dayNumber - s.goalDay[colourIndex] > 8 ) {
            s.shipyardGoal[colourIndex] = -1;
            s.boatGoal[colourIndex] = -1;
        }
        // Castle +0x35 "turns owned": + 1 per day (FUN_00419d62); 2 when the castle had no owner before, else 0, when it changes owner
        // (FUN_00416ad0). The engine has no hook here: a change of owner is noticed at the next computer turn [timing within a day].
        for ( int slot = 0; slot < 36; ++slot ) {
            int32_t cx = 0;
            int32_t cy = 0;
            if ( !H1::getListPosition( false, slot, cx, cy ) ) {
                continue;
            }
            const Castle * c = world.getCastleEntrance( { cx, cy } );
            const int owner = ( c != nullptr && c->GetColor() != PlayerColor::NONE ) ? Color::GetIndex( c->GetColor() ) : -1;
            if ( s.lastDay > 0 ) {
                s.turnsOwned[slot] += s.dayNumber - s.lastDay;
            }
            if ( s.lastOwner[slot] != -2 && s.lastOwner[slot] != owner ) {
                s.turnsOwned[slot] = ( s.lastOwner[slot] == -1 ) ? 2 : 0;
            }
            s.lastOwner[slot] = owner;
        }
        s.lastDay = s.dayNumber;
        // 4. Ultimate artifact value (FUN_0042de9a, note v33 6).
        s.ultimate = 0;
        {
            // Game +0x16e58: the ultimate artifact's number, -1 once it is dug up.
            const UltimateArtifact & ua = world.GetUltimateArtifact();
            const int ultimate = ( ua.isFound() || ua.getPosition() < 0 ) ? -1 : h1Artifact( ua.GetArtifact() );
            const int obelisks = H1::obeliskCount();
            if ( ultimate >= 0 && ultimate < 38 && obelisks > 0 ) {
                const int n = static_cast<int>( const_cast<Kingdom &>( kingdom ).PuzzleMaps().count() );
                int u = ( artifactValue[ultimate] / 110 ) * 48 / obelisks;
                u = ftol( u * ( 1.5 - std::abs( 32 - n ) / 48.0 ) );
                u = ftol( u * ( s.b5 + 0.66 ) );
                s.ultimate = u;
            }
        }
        // 7, 8. Hero +0xb2 and the total army value W.
        int total = 0;
        for ( int h = 0; h < 45; ++h ) { // H1-INTENDED(D16): 45 hero numbers
            s.heroB2[h] = 0.4f;
        }
        for ( const Heroes * hero : kingdom.GetHeroes() ) {
            if ( hero == nullptr ) {
                continue;
            }
            const int v = strength( hero->GetArmy(), hero, false, false, 0, 0 );
            const int n = H1::heroNumberOfId( hero->GetID() );
            if ( n >= 0 && n < 45 ) {
                s.heroB2[n] = static_cast<float>( v * 0.00004 + 0.4 );
            }
        }
        for ( const Heroes * hero : kingdom.GetHeroes() ) {
            total += ( hero != nullptr ) ? strength( hero->GetArmy(), hero, false, false, 0, 0 ) : 0;
        }
        for ( const Castle * castle : kingdom.GetCastles() ) {
            total += ( castle != nullptr ) ? strength( castle->GetArmy(), nullptr, false, false, 0, 0 ) : 0;
        }
        // 9. +0xf9 = (+0xed + gold) / (W + 1000) + +0xb1; +0xed = the gold of the last day change (player +0xd5 array, note v199).
        s.f9 = static_cast<float>( static_cast<double>( gain[6] + funds.Get( Resource::GOLD ) ) / ( total + 1000 ) + s.b1 );
        // 10. +0x101 = 1 / (seats + eliminated seats).
        int eliminated = 0;
        for ( const int c : H1::lastNewGameChoice().seatColour ) {
            if ( c >= 0 && !world.GetKingdom( Color::IndexToColor( c ) ).isPlay() ) {
                ++eliminated;
            }
        }
        s.share = 1.0f / static_cast<float>( std::max( 1, seatCount() + eliminated ) );
        // 11. Average artifact value 4..36.
        int sum = 0;
        for ( int a = 4; a <= 36; ++a ) {
            sum += artifactValue[a];
        }
        s.averageArtifact = static_cast<float>( sum ) / 33.0f;
        // 12. Enemy hero nearness 0x4b1b90.
        std::fill( s.enemyNear.begin(), s.enemyNear.end(), static_cast<int8_t>( 7 ) );
        for ( int c = 0; c < 4; ++c ) {
            if ( c == colourIndex ) {
                continue;
            }
            for ( const Heroes * hero : world.GetKingdom( Color::IndexToColor( c ) ).GetHeroes() ) {
                if ( hero == nullptr ) {
                    continue;
                }
                const int hx = hero->GetCenter().x;
                const int hy = hero->GetCenter().y;
                for ( int x = hx - 10; x <= hx + 10; ++x ) {
                    for ( int y = hy - 10; y <= hy + 10; ++y ) {
                        if ( x < 0 || y < 0 || x >= 72 || y >= 72 ) {
                            continue;
                        }
                        const int k = std::abs( ( std::abs( x - hx ) + std::abs( y - hy ) ) - 4 ) >> 2;
                        if ( k < s.enemyNear[x * 72 + y] ) {
                            s.enemyNear[x * 72 + y] = static_cast<int8_t>( k );
                        }
                    }
                }
            }
        }
        // 13. 0x4aa038 = 1.0.
        s.heroFactor.fill( 1.0f );
        // 14. Difficulty factors X, Y.
        if ( s.level == 1 ) {
            s.x = 0.6f;
            s.y = 1.3f;
        }
        else if ( s.level == 2 ) {
            s.x = 1.0f;
            s.y = 1.0f;
        }
        else {
            s.x = static_cast<float>( s.level * 0.07 + 1.0 );
            s.y = static_cast<float>( 1.1 - s.level * 0.12 );
        }
        if ( H1::lastNewGameChoice().kingOfTheHill != 0 ) {
            s.y = 0.1f;
        }
        // 15. Heroes limit 0x4c2048.
        s.maxHeroes = 3;
        const int playing = seatCount() - eliminated;
        if ( playing == 2 || playing == 3 ) {
            ++s.maxHeroes;
        }
        const int castles = static_cast<int>( kingdom.GetCastles().size() );
        if ( castles >= 5 ) {
            ++s.maxHeroes;
        }
        if ( castles >= 10 ) {
            ++s.maxHeroes;
        }
    }

    int bundleValueOriginal( const int amounts[7] )
    {
        const State & s = state();
        auto term = [&s, amounts]( const int r ) { return static_cast<double>( amounts[r] ) * static_cast<float>( s.resourceValue[r] ); };
        const double v = term( 1 ) + term( 5 ) + term( 3 ) + term( 2 ) + term( 6 ) + term( 4 ) + term( 0 );
        return ftol( v );
    }

    Forecast forecastOriginal( const ForecastInput & in )
    {
        // 0x42b42b..0x42ba62: values kept as 4-byte floats where the machine code stores them (fstp dword); x87 at 53-bit precision
        // (= double arithmetic) in between; __ftol cuts towards zero.
        Forecast f;
        float a = static_cast<float>( in.a );
        float b = static_cast<float>( in.b );
        if ( in.extra ) {
            b = static_cast<float>( static_cast<double>( in.bExtra ) + b );
        }
        const float va = static_cast<float>( in.va );
        float vb = static_cast<float>( in.vb );
        if ( in.extra ) {
            vb = static_cast<float>( static_cast<double>( in.vbExtra ) + vb );
        }
        if ( in.castle ) {
            b = static_cast<float>( b * 1.11 );
        }
        float bb = b;
        if ( in.owner == -1 ) {
            a = static_cast<float>( ( in.level * 0.15 + 0.7 ) * a );
        }
        else if ( in.ownerHuman ) {
            bb = static_cast<float>( bb * 1.14 );
            if ( in.level == 1 ) {
                a = static_cast<float>( a * 1.5 );
            }
        }
        if ( a < 1.0f ) {
            a = 1.0f;
        }
        if ( bb < 1.0f ) {
            bb = 1.0f;
        }
        const float k = ( a > 1000000.0f || bb > 1000000.0f ) ? 2.0f : 2.75f;
        const float pa = static_cast<float>( std::pow( static_cast<double>( a ), static_cast<double>( k ) ) );
        const double pb = std::pow( static_cast<double>( bb ), static_cast<double>( k ) );
        float p = static_cast<float>( pa / ( pb + pa ) );
        if ( p < 0.08 ) {
            p = 0;
        }
        else if ( p < 0.12 ) {
            p = static_cast<float>( p - 0.07 );
        }
        else if ( p < 0.2 ) {
            p = static_cast<float>( p - 0.05 );
        }
        else if ( p < 0.3 ) {
            p = static_cast<float>( p - 0.04 );
        }
        else if ( p < 0.4 ) {
            p = static_cast<float>( p - 0.02 );
        }
        f.p = p;
        f.l1 = ftol( ( 1.0 - p ) * va );
        f.l2 = ftol( static_cast<double>( vb ) * p );
        f.l3 = ftol( ( 1.0 - p ) * va + static_cast<double>( f.l1 ) * p );
        f.l4 = ftol( ( 1.0 - p ) * f.l2 + static_cast<double>( vb ) * p );
        const float ff = static_cast<float>( 1.33 - in.b1 );
        int g = ftol( static_cast<double>( -f.l3 ) * ff * ff );
        if ( in.owner >= 0 ) {
            const float gg = static_cast<float>( in.b1 + 0.66 );
            // A human enemy: the x term; a computer enemy: the y term (only one of the two).
            if ( in.ownerHuman ) {
                g = ftol( static_cast<double>( f.l4 ) * gg * gg * in.x + g );
            }
            else {
                g = ftol( static_cast<double>( f.l4 ) * in.y * gg * gg + g );
            }
        }
        g = ftol( static_cast<double>( g ) * in.f9 );
        if ( in.myHero ) {
            g = ftol( g - ( 1.0 - p ) * ( in.myArtifacts + 1400 ) );
            g = ftol( static_cast<double>( in.experience ) * in.b2 * p * 0.8 + g );
        }
        if ( in.enemyHero ) {
            g = ftol( static_cast<double>( in.enemyArtifacts + 1250 ) * ( in.enemyHeroHuman ? in.x : in.y ) * p + g );
        }
        f.g = g;
        return f;
    }

    Forecast forecast( const HeroView & me, const Army & enemy, const Heroes * enemyHero, const Army * extra, const bool castle, const int castleX,
                       const int castleY, const int enemyOwner )
    {
        // FUN_0042b42b (note v33 5, v188).
        const State & s = state();
        const Heroes * myHero = me.hero;
        ForecastInput in;
        in.a = ( myHero != nullptr ) ? strength( myHero->GetArmy(), myHero, true, false, 0, 0 ) : 0;
        in.b = strength( enemy, enemyHero, true, castle, castleX, castleY );
        in.va = ( myHero != nullptr ) ? strength( myHero->GetArmy(), myHero, false, false, 0, 0 ) : 0;
        in.vb = strength( enemy, enemyHero, false, false, 0, 0 );
        if ( extra != nullptr ) {
            in.extra = true;
            in.bExtra = strength( *extra, nullptr, true, false, 0, 0 );
            in.vbExtra = strength( *extra, nullptr, false, false, 0, 0 );
        }
        in.castle = castle;
        in.owner = enemyOwner;
        in.ownerHuman = enemyOwner >= 0 && isHuman( enemyOwner );
        in.level = s.level;
        in.b1 = s.b1;
        in.f9 = s.f9;
        in.x = s.x;
        in.y = s.y;
        if ( myHero != nullptr ) {
            in.myHero = true;
            in.myArtifacts = artifactSum( myHero );
            const int n = H1::heroNumberOfId( myHero->GetID() );
            in.b2 = ( n >= 0 && n < 45 ) ? s.heroB2[n] : 0.4f;
            in.experience = experienceOf( enemy, enemyHero != nullptr );
        }
        if ( enemyHero != nullptr ) {
            in.enemyHero = true;
            in.enemyArtifacts = artifactSum( enemyHero );
            // DAT_004a9b60[ enemy hero + 1 ]: the enemy hero's own player.
            in.enemyHeroHuman = enemyHero->GetColor() != PlayerColor::NONE && isHuman( Color::GetIndex( enemyHero->GetColor() ) );
        }
        return forecastOriginal( in );
    }


    void digGuess( const int colourIndex )
    {
        // FUN_0041173d (note v34 2-14, v148).
        State & s = state();
        const H1::NewGameChoice & choice = H1::lastNewGameChoice();
        if ( colourIndex < 0 || colourIndex >= 6 || choice.seatColour[0] == colourIndex ) {
            return;
        }
        const Kingdom & kingdom = world.GetKingdom( Color::IndexToColor( colourIndex ) );
        const int pieces = static_cast<int>( const_cast<Kingdom &>( kingdom ).PuzzleMaps().count() );
        const UltimateArtifact & ua = world.GetUltimateArtifact();
        if ( pieces < 11 || ua.isFound() || ua.getPosition() < 0 ) {
            s.digConf[colourIndex] = 0;
            s.digPosX[colourIndex] = -1;
            s.digPosY[colourIndex] = -1;
            return;
        }
        const int conf = static_cast<int8_t>( pieces * 4 - 0x2c );
        s.digConf[colourIndex] = conf;
        const int rx = ua.getPosition() % world.w();
        const int ry = ua.getPosition() / world.w();
        if ( static_cast<int>( Rand::Get( 1, 100 ) ) <= conf ) {
            s.digPosX[colourIndex] = rx;
            s.digPosY[colourIndex] = ry;
            return;
        }
        const auto offset = []() {
            int c = 0;
            while ( c == 0 ) {
                const int a = static_cast<int>( Rand::Get( 0, 2 ) );
                const int b = static_cast<int>( Rand::Get( 0, 2 ) );
                const int d = static_cast<int>( Rand::Get( 0, 2 ) );
                c = ( 3 - a ) - b - d;
            }
            return c;
        };
        int x = -1;
        int y = -1;
        for ( int tries = 0; tries < 200; ++tries ) {
            if ( x >= 0 && x < 72 && y >= 0 && y < 72 ) {
                const std::array<uint8_t, 10> & t = s.world.tile( x, y );
                if ( t[8] == 0 && t[2] == 0xff && t[4] == 0xff && t[0] > 0x13 ) {
                    s.digPosX[colourIndex] = x;
                    s.digPosY[colourIndex] = y;
                    return;
                }
            }
            x = rx + offset();
            y = ry + offset();
        }
        s.digPosX[colourIndex] = rx;
        s.digPosY[colourIndex] = ry;
    }

    bool dig( Heroes & hero )
    {
        // FUN_00456943 at the hero's tile (note v154).
        const bool human = hero.GetKingdom().isControlHuman();
        if ( hero.GetMaxMovePoints() != hero.GetMovePoints() ) {
            if ( human ) {
                H1::UI::messageBox( "Digging for artifacts requires a whole day, try again tomorrow.", H1::UI::MESSAGE_OKAY );
            }
            return false;
        }
        const int32_t index = hero.GetIndex();
        const std::vector<std::array<uint8_t, 10>> tiles = H1::gameTileBytes();
        if ( index < 0 || static_cast<size_t>( index ) >= tiles.size() ) {
            return false;
        }
        const std::array<uint8_t, 10> & t = tiles[static_cast<size_t>( index )];
        if ( t[2] != 0xff || t[4] != 0xff ) {
            H1::UI::messageBox( "Try searching on clear ground.", H1::UI::MESSAGE_OKAY );
            return false;
        }
        if ( t[0] < 0x14 ) {
            H1::UI::messageBox( "Try looking on land!!!", H1::UI::MESSAGE_OKAY );
            return false;
        }
        const int sound = human ? H1::playSound( "digsound.82m" ) : -1;
        // The hole (tile bytes 1 = 7, 2 = 1, flat); the engine keeps it as its digging hole (h1_map_render applyDugHole).
        Maps::Tile & tile = world.getTile( index );
        const UltimateArtifact & ultimate = world.GetUltimateArtifact();
        const bool here = ultimate.isPosition( index ) && !ultimate.isFound();
        bool found = false;
        if ( here && hero.IsFullBagArtifacts() ) {
            // FUN_004036bd returns -1: the artifact stays buried (the message shows for any player, as written).
            MP2::ObjectIcnType icnType = MP2::OBJ_ICN_TYPE_UNKNOWN;
            uint8_t imageIndex = 0;
            if ( MP2::getDiggingHoleSprite( tile.GetGround(), icnType, imageIndex ) ) {
                tile.pushGroundObjectPart( Maps::ObjectPart( Maps::TERRAIN_LAYER, Maps::getNewObjectUID(), icnType, imageIndex ) );
            }
            H1::UI::messageBox( "You have no room to carry another artifact!", H1::UI::MESSAGE_OKAY, 0x61, 0x28 );
        }
        else if ( world.DiggingForUltimateArtifact( hero.GetCenter() ) ) {
            const Artifact & art = ultimate.GetArtifact();
            hero.PickupArtifact( art );
            found = true;
            const bool goros = H1::isCampaignGame() && H1::campaignScenario() == 2;
            if ( !human ) {
                if ( goros ) {
                    H1::UI::messageBox( "A great tragedy - the enemy has found the Eye of Goros!!!  The people abandon you, all is lost.", H1::UI::MESSAGE_OKAY,
                                        0xb1, 0x1c );
                }
            }
            else {
                const int tune = H1::objectVisitTrack( 0x3f, 0 );
                if ( tune >= 0 ) {
                    H1::playMusic( tune );
                }
                if ( !goros ) {
                    const int h1Art = H1::h1ArtifactFromEngine( art.GetID() );
                    H1::UI::messageBox( std::string( "Congratulations! After spending many hours digging here, you have uncovered the " ) + H1::artifactName( h1Art ),
                                        H1::UI::MESSAGE_OKAY, 0xb1, 0x1c );
                    // FUN_004485ff: the artifact's description.
                    H1::UI::messageBox( H1::artifactInfoText( h1Art ), H1::UI::MESSAGE_OKAY, -1, 0x1c );
                }
                else {
                    H1::UI::messageBox( "After spending many hours digging here, you have uncovered the Eye of Goros!!!!", H1::UI::MESSAGE_OKAY, 0xb1, 0x1c );
                }
                H1::playMusic( H1::terrainTrack( index ) );
            }
        }
        else if ( human ) {
            H1::UI::messageBox( "Nothing here.\nWhere could it be?", H1::UI::MESSAGE_OKAY, 0x61, 0x28 );
        }
        if ( sound >= 0 ) {
            H1::waitSound( sound );
        }
        // Every playing seat's guess of the digging tile again (FUN_0041173d; it skips seat 0 itself).
        for ( int col = 0; col < 4; ++col ) {
            digGuess( col );
        }
        hero.ResetMovePoints();
        hero.GetPath().Reset();
        return found;
    }

    bool computerDig( Heroes & hero )
    {
        return dig( hero );
    }

    namespace
    {
        // FUN_00467ca8: each creature dies when (0..100 random) < trunc(ratio x 100); the first troop keeps 1 when all of it died and
        // ratio < 0.999; a troop at 0 or a ratio of 1.0 or more empties the slot.
        void applyLosses( Army & army, const float ratio )
        {
            const int k = static_cast<int>( static_cast<double>( ratio ) * 100.0 );
            bool first = true;
            for ( size_t slot = 0; slot < army.Size(); ++slot ) {
                Troop * troop = army.GetTroop( slot );
                if ( troop == nullptr || !troop->isValid() ) {
                    continue;
                }
                const int count = static_cast<int>( troop->GetCount() );
                int dead = 0;
                for ( int i = 0; i < count; ++i ) {
                    if ( static_cast<int>( Rand::Get( 0, 100 ) ) < k ) {
                        ++dead;
                    }
                }
                if ( first && dead == count && ratio < 0.999f ) {
                    --dead;
                }
                const int left = count - dead;
                if ( left < 1 || ratio >= 1.0f ) {
                    troop->Reset();
                }
                else {
                    troop->SetCount( static_cast<uint32_t>( left ) );
                }
                first = false;
            }
        }
    }

    namespace
    {
        // FUN_004041fc( from, to ): for each empty bag place of `to`, the first artifact of `from` that is not the spell book moves there; an
        // ultimate artifact (0..3) vanishes instead and the place stays empty. Both heroes are computers here (no message).
        void takeArtifacts( Heroes & from, Heroes & to )
        {
            BagArtifacts & src = from.GetBagArtifacts();
            BagArtifacts & dst = to.GetBagArtifacts();
            for ( size_t d = 0; d < dst.size(); ++d ) {
                if ( dst[d].isValid() ) {
                    continue;
                }
                for ( size_t k = 0; k < src.size(); ++k ) {
                    const Artifact art = src[k];
                    if ( !art.isValid() || art.GetID() == Artifact::MAGIC_BOOK ) {
                        continue;
                    }
                    const int h1Art = H1::h1ArtifactFromEngine( art.GetID() );
                    if ( h1Art < 0 || h1Art >= 4 ) {
                        dst[d] = art;
                    }
                    src[k] = Artifact( Artifact::UNKNOWN );
                    break;
                }
            }
        }
    }

    bool quickCombat( Heroes & hero, Army & enemy, Castle * castle, Heroes * enemyHero )
    {
        // FUN_0042e947 (note v38 1-2). The enemy owner passed to the forecast comes from the enemy hero (-1 without one).
        const int enemyExperience = experienceOf( enemy, enemyHero != nullptr );
        const fheroes2::Point entrance = ( castle != nullptr ) ? castle->GetCenter() : fheroes2::Point( 0, 0 );
        const int enemyOwner = ( enemyHero != nullptr ) ? Color::GetIndex( enemyHero->GetColor() ) : -1;
        const Forecast f = forecast( viewOf( hero ), enemy, enemyHero, nullptr, castle != nullptr, entrance.x, entrance.y, enemyOwner );
        const float p = f.p;
        const float r = static_cast<float>( static_cast<float>( Rand::Get( 0, 100 ) ) / 100.0 );
        const bool win = r < p;
        const float q = win ? p : 1.0f - p;
        const float d = ( r <= p ) ? p - r : r - p;
        float s = d;
        if ( win && 0.6 < p ) {
            s = static_cast<float>( ( p + 0.65 ) * d );
        }
        float l = static_cast<float>( ( 1.0 - s ) * ( 1.0 - s ) );
        if ( 0.8 < q && 0.2 < l ) {
            l = l * l;
        }
        if ( 0.96 < q && ( 1.0f - q ) / 2.0f < l ) {
            l = ( 1.0f - q ) / 2.0f;
        }
        float myLoss = 1.0f;
        float enemyLoss = 1.0f;
        if ( !win ) {
            hero.ResetMovePoints();
            hero.ActionAfterBattle(); // FUN_00448e05 -> FUN_00448cb6: the bonuses "until the next battle" end (the original clears the hero's
                                      // own bits again when there is an enemy hero, and never the enemy hero's)
            enemyLoss = s * l;
            // My loss is 1.0 (>= 0.99): an enemy hero gets the experience value of its own army (original as written, v38 open question).
            if ( enemyHero != nullptr ) {
                enemyHero->IncreaseExperience( static_cast<uint32_t>( enemyExperience ) );
            }
        }
        else {
            hero.IncreaseExperience( static_cast<uint32_t>( enemyExperience ) );
            hero.ActionAfterBattle();
            myLoss = l;
        }
        if ( std::getenv( "H1_AI_LOG" ) != nullptr ) {
            std::fprintf( stderr, "H1AI quick combat hero %d vs %d castle %d p %.3f r %.2f win %d losses %.3f / %.3f\n", hero.GetID(),
                          enemyHero != nullptr ? enemyHero->GetID() : -1, castle != nullptr ? 1 : 0, static_cast<double>( p ), static_cast<double>( r ),
                          win ? 1 : 0, static_cast<double>( myLoss ), static_cast<double>( enemyLoss ) );
        }
        // Artifacts go to the winner whose opponent lost more than 0.99 (FUN_004041fc).
        if ( enemyHero != nullptr ) {
            if ( myLoss > 0.99f ) {
                takeArtifacts( hero, *enemyHero );
            }
            else if ( enemyLoss > 0.99f ) {
                takeArtifacts( *enemyHero, hero );
            }
        }
        // FUN_00430433 for both sides: a loss of 0.99 or more with a hero = that hero is beaten (FUN_004043fc); else FUN_00467ca8.
        if ( enemyHero != nullptr && enemyLoss >= 0.99f ) {
            enemyHero->Dismiss( 0 );
        }
        else {
            applyLosses( enemy, enemyLoss );
        }
        // A win against a castle takes it (FUN_00416ad0), as the engine's capture does.
        if ( win && castle != nullptr ) {
            castle->GetKingdom().RemoveCastle( castle );
            hero.GetKingdom().AddCastle( castle );
            world.CaptureObject( castle->GetIndex(), hero.GetColor() );
            castle->Scout();
        }
        if ( myLoss >= 0.99f ) {
            hero.Dismiss( 0 );
            return win;
        }
        applyLosses( hero.GetArmy(), myLoss );
        return win;
    }
}
