// Heroes I / Heroes II edition switch (homm1-remaster project, note v47).
// Every HoMM1 change in the engine runs only in Heroes I mode; Heroes II mode is plain fheroes2.
#pragma once

namespace H1
{
    // Reads the command line before anything is loaded: "--h1" = Heroes I mode, "--h2" = Heroes II mode, "--h1..." tool commands = Heroes I.
    void decideEditionFromCommandLine( int argc, char ** argv );

    // True when the edition was given on the command line.
    bool isEditionDecided();

    bool isH1Mode();

    // Called after the engine is ready and before the main menu when no edition was given: asks the player.
    // Choosing Heroes I restarts the program with "--h1" (the palette and every image depend on the edition).
    void askEditionIfNeeded( int argc, char ** argv );
}
