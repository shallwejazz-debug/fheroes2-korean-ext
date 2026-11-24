/***************************************************************************
 *   H2-KOREAN: HD background layer (owner 2026-11-21 01:57 "그렇게 해 보자").                *
 *   A big picture (made by the owner with an AI upscaler, blended with a smooth upscale of the     *
 *   original) is shown in place of a background picture drawn on the screen: wherever a screen     *
 *   pixel still has the background's own colour (and so do its 4 neighbours), the output shows     *
 *   the big picture there. Everything drawn over the background (units, grid, windows, text)       *
 *   keeps the original pixels. Only the final picture sent to the window changes.                  *
 *   License: GPL-2.0 like the rest of the engine.                                                  *
 ***************************************************************************/
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "math_base.h"

namespace fheroes2::HDLayer
{
    // The big pictures are stored 3 times the size of the original.
    constexpr int32_t storedScale = 3;

    struct Layer
    {
        Rect rect; // where the original background is on the screen (screen pixels)
        std::vector<uint8_t> low; // the original background (palette indices), rect.width x rect.height
        std::shared_ptr<const std::vector<uint32_t>> big; // 0x00RRGGBB, rect.width * storedScale x rect.height * storedScale
    };

    // slot 0: battle background, slot 1: town background
    constexpr int slotCount = 2;

    void set( int slot, const Rect & rect, std::vector<uint8_t> low, std::shared_ptr<const std::vector<uint32_t>> big );
    void clear( int slot );

    // The layers in use (empty rect = not in use)
    const Layer & get( int slot );

    // Changes every time a layer is set or cleared, so that the renderer can redraw the whole frame
    uint32_t generation();

    // On / off (F9). Off: the original pictures only.
    bool isEnabled();
    void setEnabled( bool enable );

    bool anyActive();
}
