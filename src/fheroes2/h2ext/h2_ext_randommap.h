/***************************************************************************
 *   H2-EXT: Heroes II random map from the new game window (docs/H2_EXTRAS_CONCEPT_v1.md 1, owner 2026-11-19 16:26).
 *   The engine's map generator (editor only until now) makes a Resurrection-format (.fh2m) map. Owner 2026-11-19
 *   ("원작 규칙으로"): such a generated map is played with the original Succession Wars rules, so its map info is
 *   marked SUCCESSION_WARS. A generated map is recognised by a marker file next to it (<map>.fh2m.h2r).
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <string>

namespace Maps::Map_Format
{
    struct MapFormat;
}

namespace H2Ext::RandomMap
{
    // True while createForGame() runs the generator (the generator then prepares its own empty map instead of the editor's).
    bool isGeneratingForGame();

    // The editor's empty-map step (EditorInterface::generateNewMap) for the given map container, without the editor.
    bool prepareEmptyMap( Maps::Map_Format::MapFormat & map, const int32_t width );

    // Asks the size and the generator settings, generates and saves the map. Returns the saved path, or empty when cancelled or failed.
    std::string createForGame();

    // A map file made by createForGame() (its marker file exists).
    bool isGameRandomMap( const std::string & mapPath );
}
