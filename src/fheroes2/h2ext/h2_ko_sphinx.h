/***************************************************************************
 *   H2-KOREAN: Sphinx answer chosen from four (docs/H2_EXTRAS_CONCEPT_v1.md 2, owner 2026-11-19 16:26).
 *   The Korean version cannot type the English answers, so it shows the correct answer and three others.
 ***************************************************************************/

#pragma once

#include <string>

struct MapSphinx;

namespace fheroes2
{
    class TextBase;
}

namespace H2Korean
{
    // The answer the player picked (an English answer of the map, so the Sphinx's own check decides). Empty when cancelled.
    std::string chooseSphinxAnswer( const std::string & title, const fheroes2::TextBase & question, const MapSphinx & sphinx );
}
