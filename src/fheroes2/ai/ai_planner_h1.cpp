// homm1-remaster: Heroes I computer player turn FUN_0042947e (notes v142, v147, v148), on the original path search (h1_pathfind) and the
// original values (h1_ai_*).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <vector>

#include "ai_hero_action.h"
#include "ai_planner.h" // IWYU pragma: associated
#include "color.h"
#include "direction.h"
#include "game_mode.h"
#include "h1_ai.h"
#include "castle.h"
#include "h1_gfx.h"
#include "h1_heroes.h"
#include "h1_pathfind.h"
#include "h1_spells.h"
#include "heroes.h"
#include "kingdom.h"
#include "maps.h"
#include "route.h"
#include "world.h"

namespace
{
    // Table 0x491660 order -> engine directions.
    const int engineDirection[8] = { Direction::TOP, Direction::TOP_RIGHT, Direction::RIGHT, Direction::BOTTOM_RIGHT,
                                     Direction::BOTTOM, Direction::BOTTOM_LEFT, Direction::LEFT, Direction::TOP_LEFT };
    const int dirX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int dirY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    // FUN_0044751a: the spell is in the hero's book (+0x66, 29 entries).
    bool hasSpell( const Heroes & hero, const int spell )
    {
        for ( const uint8_t entry : hero.h1BookSpell ) {
            if ( entry == spell + 1 ) {
                return true;
            }
        }
        return false;
    }

    bool logging()
    {
        return std::getenv( "H1_AI_LOG" ) != nullptr;
    }

    // FUN_0046e730: directions from the destination back to the start, only the steps whose cost is within the movement left; at most 255.
    std::vector<int> buildPath( const H1::Path::Search & grid, const int sx, const int sy, int tx, int ty, const int moveLeft )
    {
        std::vector<int> steps;
        for ( int guard = 0; guard < 72 * 72; ++guard ) {
            if ( tx == sx && ty == sy ) {
                return steps;
            }
            if ( tx < 0 || ty < 0 || tx >= 72 || ty >= 72 ) {
                return {};
            }
            const H1::Path::Cell & c = grid.at( tx, ty );
            // The original returns 0 only when both coordinates differ from the record (it checks x and y with "and").
            if ( c.x != tx && c.y != ty ) {
                return {};
            }
            if ( c.cost() <= moveLeft ) {
                steps.push_back( c.direction() & 7 );
                if ( steps.size() > 0xff ) {
                    return {};
                }
            }
            const int back = ( c.direction() + 4 ) & 7;
            tx += dirX[back];
            ty += dirY[back];
        }
        return {};
    }
}

fheroes2::GameMode AI::Planner::H1HeroesTurn( VecHeroes & heroes )
{
    if ( heroes.empty() ) {
        return fheroes2::GameMode::END_TURN;
    }
    Kingdom & kingdom = heroes.front()->GetKingdom();
    H1::Ai::State & st = H1::Ai::state();
    // 0x4ac970: tiles heroes started from this turn (at most 30).
    std::vector<std::pair<int, int>> started;
    const int level = st.level;
    for ( int safety = 0; safety < 1000; ++safety ) {
        // FUN_0042a9dd: the hero with the most movement left (> 0) when any hero can make a step (FUN_0041150c).
        Heroes * hero = nullptr;
        bool anyCanMove = false;
        for ( Heroes * h : kingdom.GetHeroes() ) {
            if ( h != nullptr && h->CanMove() ) {
                anyCanMove = true;
            }
        }
        if ( anyCanMove ) {
            uint32_t most = 0;
            for ( Heroes * h : kingdom.GetHeroes() ) {
                if ( h != nullptr && most < h->GetMovePoints() ) {
                    most = h->GetMovePoints();
                    hero = h;
                }
            }
        }
        if ( hero == nullptr ) {
            break;
        }
        st.world = H1::Path::buildWorld( Color::GetIndex( kingdom.GetColor() ) );
        H1::Ai::HeroView view = H1::Ai::viewOf( *hero );
        st.capturedCastle = -1;
        H1::Ai::dangerFactors( view );
        H1::Ai::clearCaches( true, 0, 0 );
        // Step limit L and search limit R (fast processor values, owner decision 2026-11-15).
        const int stepLimit = static_cast<int>( ( view.onBoat ? 15 : 5 ) * ( 1.7 - 0.1 * level ) );
        // H1-INTENDED(D5): fast-processor search values (0x4a9ca0 = 0: k 0.06, no doubled step limit) (docs/INTENDED_DIFFERENCES.md).
        const int range = static_cast<int>( ( view.maxMove + 0x2a ) * ( 0.8 + ( level - 1 ) * 0.06 ) );
        bool finished = false;
        bool halfDone = false;
        while ( !finished && view.moveLeft >= 4 ) {
            // Full movement, guess above 15 and standing on the guessed tile: dig (FUN_00456943).
            if ( view.moveLeft == view.maxMove && st.digConfidence > 0xf && view.x == st.digX && view.y == st.digY ) {
                H1::Ai::computerDig( *hero );
                view = H1::Ai::viewOf( *hero );
                st.digConfidence = st.digConf[st.player];
                st.digX = st.digPosX[st.player];
                st.digY = st.digPosY[st.player];
                if ( view.moveLeft < 4 ) {
                    break;
                }
            }
            H1::Ai::chooseDestination( view, range );
            for ( const auto & [x, y] : started ) {
                if ( x == view.destX && y == view.destY && st.world.tile( x, y )[8] != 0xa8 ) {
                    hero->ResetMovePoints();
                    view.moveLeft = 0;
                }
            }
            if ( started.size() < 0x1e ) {
                started.emplace_back( view.x, view.y );
            }
            if ( view.destX == -1 || view.destY == -1 ) {
                finished = true;
                break;
            }
            // FUN_0046e730 on the grid FUN_0042aaf9 left.
            std::vector<int> path = buildPath( st.grid, view.x, view.y, view.destX, view.destY, view.moveLeft );
            if ( logging() ) {
                std::fprintf( stderr, "H1AI %s at %d,%d -> %d,%d steps %d left %d\n", hero->GetName().c_str(), view.x, view.y, view.destX, view.destY,
                              static_cast<int>( path.size() ), view.moveLeft );
            }
            if ( path.empty() ) {
                finished = true;
                break;
            }
            // FUN_0044751a (Dimension Door 0x1b in the book) and FUN_004292de: with 4 or more movement left, the farthest tile along the path
            // (not the last step) within 7 tiles across and down of the hero, not blocked and without an object, when it saves 5 steps or
            // more: teleport there, movement - 12 (0 below 12), one use of the spell; then choose again.
            if ( hasSpell( *hero, 0x1b ) && view.moveLeft >= 4 ) {
                int px = view.x;
                int py = view.y;
                int bestX = -1;
                int bestY = -1;
                int saved = 0;
                for ( int k = static_cast<int>( path.size() ) - 1; k > 0; --k ) {
                    px += dirX[path[static_cast<size_t>( k )]];
                    py += dirY[path[static_cast<size_t>( k )]];
                    if ( std::abs( px - view.x ) < 8 && std::abs( py - view.y ) < 8 ) {
                        const std::array<uint8_t, 10> & t = st.world.tile( px, py );
                        if ( ( t[8] & 0x80 ) == 0 && ( t[7] & 0x80 ) == 0 ) {
                            saved = static_cast<int>( path.size() ) - k;
                            bestX = px;
                            bestY = py;
                        }
                    }
                }
                if ( bestX != -1 && saved >= 5 ) {
                    const int32_t dst = bestY * world.w() + bestX;
                    hero->Scout( dst );
                    hero->Move2Dest( dst );
                    hero->GetPath().Reset();
                    const uint32_t cost = 12 * H1::movementScale;
                    const uint32_t left = ( hero->GetMovePoints() < cost ) ? 0 : hero->GetMovePoints() - cost;
                    hero->ResetMovePoints();
                    hero->IncreaseMovePoints( left );
                    H1::useSpell( *hero, 0x1b );
                    hero->ActionNewPosition( false );
                    if ( logging() ) {
                        std::fprintf( stderr, "H1AI %s dimension door to %d,%d\n", hero->GetName().c_str(), bestX, bestY );
                    }
                    st.world = H1::Path::buildWorld( Color::GetIndex( kingdom.GetColor() ) );
                    view = H1::Ai::viewOf( *hero );
                    continue;
                }
            }
            // Walk from the end of the list (the first step) at most L steps.
            int walked = 0;
            bool stopped = false;
            int index = static_cast<int>( path.size() ) - 1;
            const int32_t destIndex = view.destY * world.w() + view.destX;
            for ( ; index >= 0 && walked < stepLimit; --index ) {
                // FUN_00428ad0: a worthwhile object next to the hero takes this step (not on the last step of the path). The walk goes
                // on with the next directions of the path afterwards (FUN_0043ab9c's "last step" flag only sets the standing picture).
                if ( index > 0 ) {
                    const H1::Ai::HeroView now = H1::Ai::viewOf( *hero );
                    const int pick = H1::Ai::sidePickup( now );
                    if ( pick >= 0 ) {
                        path[static_cast<size_t>( index )] = pick;
                    }
                }
                const int d = path[static_cast<size_t>( index )];
                const int32_t from = hero->GetIndex();
                const int fx = from % world.w();
                const int fy = from / world.w();
                const int32_t to = Maps::GetDirectionIndex( from, engineDirection[d] );
                const int left = static_cast<int>( hero->GetMovePoints() / H1::movementScale );
                const int cost = H1::Path::stepCost( H1::Path::groupOf( st.world.tile( fx, fy ) ), ( d & 1 ) != 0, left, view.heroClass );
                if ( left < cost ) {
                    stopped = true;
                    break;
                }
                std::list<Route::Step> one;
                one.emplace_back( to, from, engineDirection[d], static_cast<uint32_t>( cost ) * H1::movementScale );
                hero->GetPath().setPath( one );
                const Castle * before = world.getCastleEntrance( Maps::GetPoint( to ) );
                const bool wasOurs = before != nullptr && before->GetColor() == kingdom.GetColor();
                const bool wasHuman = before != nullptr && before->isControlHuman();
                const fheroes2::GameMode state = AI::HeroesMove( *hero );
                // FUN_004305d7: a castle taken from a human in a real battle (FUN_00405c7f) is remembered (DAT_004aa0d8); the quick combat
                // and an empty castle do not set it.
                if ( before != nullptr && !wasOurs && wasHuman && before->GetColor() == kingdom.GetColor() ) {
                    st.capturedCastle = H1::getCastleSlot( before->GetIndex(), nullptr );
                }
                if ( state != fheroes2::GameMode::END_TURN ) {
                    return state;
                }
                ++walked;
                if ( !hero->isActive() || hero->GetColor() != kingdom.GetColor() ) {
                    stopped = true;
                    break;
                }
                // An object or a fight stops the walk (the hero did not move on, or stands on an object tile).
                if ( hero->GetIndex() != to || ( st.world.tile( to % world.w(), to / world.w() )[8] & 0x80 ) != 0 ) {
                    stopped = true;
                    --index;
                    break;
                }
            }
            if ( !hero->isActive() || hero->GetColor() != kingdom.GetColor() ) {
                break;
            }
            st.world = H1::Path::buildWorld( Color::GetIndex( kingdom.GetColor() ) );
            view = H1::Ai::viewOf( *hero );
            if ( view.moveLeft <= view.maxMove / 2 && !halfDone ) {
                halfDone = true; // FUN_0043050c (no effect on decisions)
            }
            // The path is walked to its end on the guessed tile: dig with full movement, else wait there (movement 0).
            if ( index < 0 && st.digConfidence > 0xf && view.x == st.digX && view.y == st.digY ) {
                if ( view.moveLeft == view.maxMove ) {
                    H1::Ai::computerDig( *hero );
                    st.digConfidence = st.digConf[st.player];
                    st.digX = st.digPosX[st.player];
                    st.digY = st.digPosY[st.player];
                }
                else {
                    hero->ResetMovePoints();
                }
                view = H1::Ai::viewOf( *hero );
            }
            const bool reached = hero->GetIndex() == destIndex;
            if ( ( index < 0 && !reached && !stopped ) || view.moveLeft < 4 || ( stopped && hero->GetIndex() != destIndex && walked == 0 ) ) {
                finished = true;
            }
            H1::Ai::clearCaches( false, view.destX, view.destY );
        }
        hero->ResetMovePoints();
        // A hero ending its turn in a castle (the tile byte under it is 0xa8): buy once more.
        if ( hero->isActive() && hero->inCastle() != nullptr ) {
            H1::Ai::checkBuy();
        }
    }
    return fheroes2::GameMode::END_TURN;
}
