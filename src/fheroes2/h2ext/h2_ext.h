// H2-EXT (owner 2026-11-18 19:52 "확장 게임 1편처럼 따로", 20:15 "시나리오 창 토글"): the Heroes II extension mode, design
// docs/H2_EXTENSION_DESIGN_v5.md. Every extension rule runs only when H2Ext::isOn(); original Heroes II games are untouched.
#pragma once

#include <string>

namespace H2Ext
{
    // True in a Heroes II standard game started (or loaded) with the extension box ticked. Never in campaigns or in Heroes I mode.
    bool isOn();

    // The box on the scenario window.
    bool isChosen();
    void setChosen( bool on );

    // "<save>.h2x" next to a save file: written when the game is an extension game, removed otherwise; read back on load.
    void saveSideState( const std::string & path );
    void loadSideState( const std::string & path );
}
