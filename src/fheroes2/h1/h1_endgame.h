// Heroes I end of game screens (homm1-remaster project): victory (congrats.bmp + congspre.bin, FUN_004266c1), high score list
// (hiscore.bin, FUN_00465f7f / FUN_004663ae / FUN_00466145, files STANDARD.HS / CAMPAIGN.HS, FUN_0042587f), the name entry window
// (dataentr.bin, FUN_00426b30) and the loss video (lose1.smk, FUN_0043dde6(7)); note v124.
#pragma once

#include <string>

namespace H1
{
    // FUN_0041f5ef: base score from the days played.
    int baseScore( int days );

    // FUN_004257f1: creature number of a score (standard: score at least the threshold; campaign: days at most the threshold).
    int rankingCreature( int score, bool standard );

    // FUN_00426b30: dataentr.bin text entry; returns the typed text (at most maxLength - 1 characters).
    std::string textEntry( const std::string & prompt, int maxLength );

    // High score screen (from the main menu, or after a game with the new entry highlighted).
    void showHighScores( bool campaign );

    // Standard game won: the victory screen, the high score entry and the list (then the main menu). False when Heroes I graphics are not in use.
    bool runStandardVictory();

    // Game lost: lose1.smk (FUN_0043dde6(7)) when the ANIM files are there. False when Heroes I graphics are not in use.
    bool runLoss();

    // FUN_0042587f( days, 0, ..., lord name ): campaign high score entry (fewer days is better), then the campaign list.
    void addCampaignHighScore( int days, const std::string & lord );
}
