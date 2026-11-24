/***************************************************************************
 *   H2-KOREAN: HD backgrounds (owner 2026-11-21 01:57). Pictures upscaled by the owner with an AI  *
 *   upscaler (ComfyUI, 4x-UltraSharp) from the original backgrounds, kept in h2/hd/ as .h2hd files *
 *   (tools/h2hd/pack_hd.py). Off when the folder is missing; F9 turns them on and off.             *
 ***************************************************************************/
#pragma once

#include "image.h"
#include "math_base.h"

namespace H2Korean::Hd
{
    enum Slot : int
    {
        BATTLE = 0,
        TOWN = 1
    };

    // Shows the big picture of the ICN's frame 0 in place of 'low' drawn at 'screenPos' (does nothing when there is no file)
    void showBackground( Slot slot, int icnId, const fheroes2::Sprite & low, const fheroes2::Point & screenPos );
    void hide( Slot slot );

    // Hides the slot when it goes out of scope
    class Guard
    {
    public:
        explicit Guard( const Slot slot )
            : _slot( slot )
        {}
        Guard( const Guard & ) = delete;
        Guard & operator=( const Guard & ) = delete;
        ~Guard()
        {
            hide( _slot );
        }

    private:
        Slot _slot;
    };

    // F9: on / off (kept in the configuration file); returns the new state
    bool toggle();
    void applySetting( bool enabled, int blendPercent );
}
