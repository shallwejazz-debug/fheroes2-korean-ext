// Heroes I tavern heroes (homm1-remaster project, note v169).
#include "h1_heroes.h"
#include "h1_tavern.h"

#include <vector>

#include "color.h"
#include "h1_newgame.h"
#include "heroes.h"
#include "heroes_recruits.h"
#include "kingdom.h"
#include "rand.h"
#include "world.h"

namespace
{
    const PlayerColor seatOrder[4] = { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW };

    int roll( const int low, const int high )
    {
        return static_cast<int>( Rand::Get( static_cast<uint32_t>( low ), static_cast<uint32_t>( high ) ) );
    }

    Heroes * heroOf( const int number )
    {
        return H1::heroByNumber( number ); // H1-INTENDED(D16): 0..44 (necromancers 36..44)
    }

    // State byte 0xff: nobody owns the hero and it is not in a tavern.
    bool isFree( const Heroes * hero )
    {
        return hero != nullptr && hero->isAvailableForHire() && !hero->Modes( Heroes::RECRUIT );
    }

    bool inTavern( const Heroes * hero )
    {
        return hero != nullptr && hero->isAvailableForHire() && hero->Modes( Heroes::RECRUIT );
    }

    // FUN_0042421f: when a player leaves, its '@' heroes become free. The port frees them before the next pick, which is the first moment
    // the state is read again.
    void freeLeftPlayers()
    {
        for ( const PlayerColor color : seatOrder ) {
            Kingdom & kingdom = world.GetKingdom( color );
            if ( kingdom.isPlay() ) {
                continue;
            }
            Recruits & recruits = kingdom.GetCurrentRecruits();
            for ( Heroes * hero : { recruits.GetHero1(), recruits.GetHero2() } ) {
                if ( inTavern( hero ) ) {
                    hero->ResetModes( Heroes::RECRUIT );
                }
            }
            recruits.SetHero1( nullptr );
            recruits.SetHero2( nullptr );
        }
    }

    Heroes * placeHero( const Recruits & recruits, const int place )
    {
        return ( place == 0 ) ? recruits.GetHero1() : recruits.GetHero2();
    }

    // Puts the hero in the place; marked: '@' (engine RECRUIT mode), else the state byte stays as it is.
    void setPlace( Recruits & recruits, const int place, Heroes * hero, const bool marked )
    {
        const bool wasMarked = hero != nullptr && hero->Modes( Heroes::RECRUIT );
        if ( place == 0 ) {
            recruits.SetHero1( hero );
        }
        else {
            recruits.SetHero2( hero );
        }
        // Recruits::SetHero1/2 set RECRUIT; an unmarked pick keeps its old state.
        if ( hero != nullptr && !marked && !wasMarked ) {
            hero->ResetModes( Heroes::RECRUIT );
        }
    }
}

namespace
{
    // H1-INTENDED(D16): 5 hero classes in extension games (necromancers 36..44); the original 4. With 4 the rolls below are the original
    // ones (roll( 1, 3 ) and "& 3" = "% 4").
    int classCount()
    {
        return H1::isExtension() ? 5 : 4;
    }
}

namespace H1::Tavern
{
    Heroes * pickHero( const int heroClass )
    {
        freeLeftPlayers();
        std::vector<int> free;
        for ( int n = heroClass * 9; n < heroClass * 9 + 9; ++n ) {
            if ( isFree( heroOf( n ) ) ) {
                free.push_back( n );
            }
        }
        if ( free.empty() ) {
            for ( int n = 0; n < classCount() * 9; ++n ) {
                if ( isFree( heroOf( n ) ) ) {
                    free.push_back( n );
                }
            }
        }
        if ( !free.empty() ) {
            // FUN_00411e7c draws a number of the range until it hits a free hero: the same as one free hero at random.
            return heroOf( free[static_cast<size_t>( roll( 0, static_cast<int>( free.size() ) - 1 ) )] );
        }
        Heroes * last = nullptr;
        for ( int n = 0; n < classCount() * 9; ++n ) {
            if ( inTavern( heroOf( n ) ) ) {
                last = heroOf( n );
            }
        }
        return last;
    }

    void newGame()
    {
        // The engine may have chosen recruits already: start from empty places.
        for ( const PlayerColor color : seatOrder ) {
            Recruits & recruits = world.GetKingdom( color ).GetCurrentRecruits();
            recruits.SetHero1( nullptr );
            recruits.SetHero2( nullptr );
        }
        for ( int n = 0; n < classCount() * 9; ++n ) {
            if ( Heroes * hero = heroOf( n ) ) {
                hero->ResetModes( Heroes::RECRUIT );
            }
        }
        static const int campaignClass[4] = { 3, 1, 0, 2 }; // 0x492a78 by player colour
        for ( const PlayerColor color : seatOrder ) {
            Kingdom & kingdom = world.GetKingdom( color );
            if ( !kingdom.isPlay() ) {
                continue;
            }
            int heroClass = isCampaignGame() ? campaignClass[Color::GetIndex( color ) & 3] : roll( 0, classCount() - 1 );
            Recruits & recruits = kingdom.GetCurrentRecruits();
            setPlace( recruits, 0, pickHero( heroClass ), true );
            heroClass = ( heroClass + roll( 1, classCount() - 1 ) ) % classCount();
            setPlace( recruits, 1, pickHero( heroClass ), true );
        }
    }

    void replaceOwned()
    {
        for ( const PlayerColor color : seatOrder ) {
            Kingdom & kingdom = world.GetKingdom( color );
            if ( !kingdom.isPlay() ) {
                continue;
            }
            Recruits & recruits = kingdom.GetCurrentRecruits();
            for ( int place = 0; place < 2; ++place ) {
                const Heroes * hero = placeHero( recruits, place );
                if ( hero != nullptr && hero->GetColor() != PlayerColor::NONE ) {
                    setPlace( recruits, place, pickHero( 0 ), true );
                }
            }
        }
    }

    void newWeek()
    {
        int heroClass = 0;
        for ( const PlayerColor color : seatOrder ) {
            Kingdom & kingdom = world.GetKingdom( color );
            if ( !kingdom.isPlay() ) {
                continue;
            }
            Recruits & recruits = kingdom.GetCurrentRecruits();
            for ( int place = 0; place < 2; ++place ) {
                heroClass = ( heroClass + roll( 1, classCount() - 1 ) ) % classCount();
                if ( Heroes * old = placeHero( recruits, place ); inTavern( old ) ) {
                    old->ResetModes( Heroes::RECRUIT );
                }
                setPlace( recruits, place, pickHero( heroClass ), false );
            }
        }
    }

    void afterHire( Kingdom & kingdom, const int place, const Heroes & hired )
    {
        Recruits & recruits = kingdom.GetCurrentRecruits();
        // Heroes::Recruit has let the engine refill the place already: that hero goes back to the pool first.
        if ( Heroes * engineChoice = placeHero( recruits, place ); engineChoice != nullptr && engineChoice != placeHero( recruits, 1 - place ) ) {
            if ( engineChoice->isAvailableForHire() ) {
                engineChoice->ResetModes( Heroes::RECRUIT );
            }
        }
        setPlace( recruits, place, nullptr, false );
        const int hiredClass = ( H1::heroNumberOfId( hired.GetID() ) ) / 9;
        const int heroClass = kingdom.isControlAI() ? roll( 0, classCount() - 1 ) : ( ( hiredClass + roll( 1, classCount() - 1 ) ) % classCount() );
        setPlace( recruits, place, pickHero( heroClass ), true );
    }

    void heroRetreated( Kingdom & kingdom, Heroes & hero )
    {
        Recruits & recruits = kingdom.GetCurrentRecruits();
        const int place = roll( 0, 1 );
        if ( Heroes * old = placeHero( recruits, place ); inTavern( old ) && old != &hero ) {
            old->ResetModes( Heroes::RECRUIT );
        }
        setPlace( recruits, place, &hero, true );
    }
}
