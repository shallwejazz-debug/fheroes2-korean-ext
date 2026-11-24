// Heroes I hero screen (homm1-remaster project): heroscrn.bmp + herowind.bin, FUN_00447aad / FUN_0044969c / FUN_0044916e. Note v58.
#pragma once

class Heroes;

namespace H1
{
    // Returns a Dialog:: code like Heroes::OpenDialog (Dialog::CANCEL or Dialog::DISMISS).
    int runHeroScreen( Heroes & hero, bool readonly, bool disableDismiss );

    // Artifact description (table 0x492ef8, FUN_004485ff), "" for other numbers.
    const char * artifactInfoText( int h1Artifact );
}
