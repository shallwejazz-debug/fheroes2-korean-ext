// Heroes I "New Game" flow (homm1-remaster project): game type dialog (stpnewgm.bin, FUN_0040cad1) and the new game settings
// screen (newgame.bin, FUN_00413dd1 / FUN_0041384b / FUN_004132ca / FUN_0041f2df). See docs/analysis note v53.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fheroes2
{
    enum class GameMode : int;
}

enum class PlayerColor : uint8_t;

class Castle;

namespace H1
{
    // Called for GameMode::NEW_GAME in Heroes I graphics mode. Returns START_GAME when a map was loaded, MAIN_MENU otherwise.
    fheroes2::GameMode runNewGame();

    // Settings chosen on the new game screen that the engine has no place for yet (kept for later steps: AI, score).
    struct NewGameChoice
    {
        int difficulty = 0; // game +0x1fe: 0 Easy .. 3 Expert
        int levels[4] = { 2, 2, 2, 2 }; // per player record +0x21e (stride 0x105): human seats 1..4, computer 0 None .. 4 Genius
        int colour = 0; // player 0 record +0x21d: 0 blue, 1 green, 2 red, 3 yellow
        int kingOfTheHill = 0; // DAT_004a82ec
        int rating = 0; // FUN_0041f2df result, kept in game +0x0
        int playerCount = 0; // game +0x1ff
        int humans = 1; // human seats 0 .. humans - 1 (DAT_004a9b60 non-zero)
        int seatColour[4] = { -1, -1, -1, -1 }; // player record +0x21d of each seat as an engine colour index (standard game, note v138)
        int extension = 0; // H1-INTENDED(D16): 1 = extension rules ("Extended Game" button), kept with saves
    };

    const NewGameChoice & lastNewGameChoice();

    // H1-INTENDED(D16): the game was started with the "Extended Game" button (owner 2026-11-17 23:32). Every extension rule asks this;
    // false = the original rules.
    bool isExtension();

    // Heroes I campaign (note v126): whether the game is a campaign map, its number 0..8 (-1 otherwise), and the campaign window
    // (campaign.bin, FUN_0041421f; inGame = from the scenario information button).
    bool isCampaignGame();
    // Town names of a HoMM1 map (FUN_00415467 part / FUN_00427056, note v227): each castle gets a random name of table 0x493380 not given
    // to another (36 names, as many as castle slots). Campaign goal towns are named afterwards from table 0x492769 (note v136).
    void assignOriginalCastleNames( const std::vector<Castle *> & castles );
    int campaignScenario();
    // Returns true when RESTART SCENARIO was chosen and confirmed (then call restartCampaignScenario).
    bool showCampaignWindow( bool inGame );
    fheroes2::GameMode restartCampaignScenario();

    // The new game choice and the campaign state are kept beside a saved game ("<save>.h1", plain numbers); a save without it
    // leaves the defaults (standard game).
    // End of a campaign map (note v127): objective 0 none / 1 won / 2 lost (FUN_00424700); won / lost screens and the next map.
    int campaignObjective( PlayerColor human );
    bool campaignHasGoal();
    fheroes2::GameMode campaignScenarioWon();
    fheroes2::GameMode campaignScenarioLost();

    // Player +0x21e of the seat that plays this colour (1 Dumb .. 4 Genius for a computer), -1 when not known (saves before note v139).
    // Campaign: every computer seat of a scenario has the same level (table 0x492769 +0x13..+0x15).
    int seatLevel( PlayerColor color );

    // FUN_00419d62: campaign maps 0 and 4..7, the goal town tile (table 0x492769 x, y): a hero there starts the turn without movement.
    bool isCampaignStopTile( int x, int y );

    // True for a computer colour removed at the start because its seat was "None" (no "vanquished" message).
    bool isDroppedAtStart( PlayerColor color );

    // The file requester (request.bin) for saved games, FUN_00467e1c / FUN_0046878c modes 0 (load, at 0x136, 0xe) and 1 (save, at 0xa0,
    // 0x28), drawn over the current screen (note v160). Returns the chosen save file's full path, empty for Cancel.
    std::string selectSaveFile( bool save );

    // An in-game load (FUN_0040cf57 runs the requester over the map; Cancel goes back to the game): the file chosen there, loaded by the
    // main loop's Game::LoadGame instead of showing the requester again.
    void setPendingLoad( const std::string & path );
    std::string takePendingLoad();

    void saveSideState( const std::string & path );
    void loadSideState( const std::string & path );

    // Difficulty and size bytes of the original map header (offsets 2 and 3) and its description (offset 0x9a), kept by --h1convert at the
    // end of "<map>.h1t" (notes v53, v54). False for older .h1t files that do not have them.
    bool readOriginalMapHeader( const std::string & fh2mFile, int & mapDifficulty, int & mapSize, std::string * description = nullptr );
}
