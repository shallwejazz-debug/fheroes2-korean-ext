// Heroes I / Heroes II edition switch (homm1-remaster project). See h1_mode.h.
#include "h1_mode.h"

#include <cstring>
#include <vector>

#if defined( _WIN32 )
#include <process.h>
#else
#include <unistd.h>
#endif

#include "dialog.h"
#include "h1_gfx.h"
#include "logging.h"
#include "ui_dialog.h"

namespace
{
    bool decided = false;
    bool h1Mode = false;
}

namespace H1
{
    void decideEditionFromCommandLine( const int argc, char ** argv )
    {
        for ( int i = 1; i < argc; ++i ) {
            if ( std::strcmp( argv[i], "--h2" ) == 0 ) {
                decided = true;
                h1Mode = false;
            }
            else if ( std::strncmp( argv[i], "--h1", 4 ) == 0 ) {
                // "--h1" itself and every "--h1..." tool command.
                decided = true;
                h1Mode = true;
            }
        }
    }

    bool isEditionDecided()
    {
        return decided;
    }

    bool isH1Mode()
    {
        return h1Mode;
    }

    void askEditionIfNeeded( const int argc, char ** argv )
    {
        if ( decided ) {
            return;
        }
        decided = true;

        if ( !hasH1Data() ) {
            // No HoMM1 data: plain Heroes II.
            return;
        }

        const int answer = fheroes2::showStandardTextMessage( "Heroes of Might and Magic",
                                                              "Which game do you want to play?\n\nYes = Heroes of Might and Magic I\nNo = Heroes of Might and Magic II",
                                                              Dialog::YES | Dialog::NO );
        if ( answer != Dialog::YES ) {
            return;
        }

        // Restart as Heroes I.
        std::vector<char *> args;
        for ( int i = 0; i < argc; ++i ) {
            args.push_back( argv[i] );
        }
        static char h1Arg[] = "--h1";
        args.push_back( h1Arg );
        args.push_back( nullptr );
#if defined( _WIN32 )
        _execv( argv[0], args.data() );
#else
        execv( "/proc/self/exe", args.data() );
        execv( argv[0], args.data() );
#endif
        ERROR_LOG( "Heroes I mode: failed to restart the program." )
    }
}
