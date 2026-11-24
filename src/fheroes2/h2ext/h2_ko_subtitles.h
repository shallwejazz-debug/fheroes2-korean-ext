/***************************************************************************
 *   H2-KOREAN: Korean subtitles of the Heroes II cutscenes (owner 2026-11-20 18:04-18:22). The voice script comes from the
 *   Age of Heroes "Campaign Story" page the owner sent; the times from the sound of each video (h2korean/subs/make_subs.py).
 *   Active only with the Korean patch and h2/KOREAN_SUBS.TXT (video<TAB>start ms<TAB>end ms<TAB>text).
 *   Every video has its own palette, so the subtitles are not drawn with fixed colour numbers: for each frame the whitest and the
 *   blackest colour of the current video palette are looked up, the text is drawn white with a black outline over a half-dark band
 *   (owner 18:39: the colour changed between videos and the text was hard to read on a bright picture).
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

#include "game_video.h"
#include "image.h"
#include "math_base.h"

namespace H2Korean
{
    class VideoSubtitles
    {
    public:
        // The subtitles of the videos of one playback (none when the patch or the file is missing).
        explicit VideoSubtitles( const std::vector<Video::VideoInfo> & infos );

        bool empty() const
        {
            return _items.empty();
        }

        // Draws the subtitles of this moment over the frame, in the colours of the given video palette (256 x RGB).
        void render( fheroes2::Image & output, const fheroes2::Rect & frameRoi, const uint32_t timeMs, const std::vector<uint8_t> & palette ) const;

    private:
        struct Item
        {
            uint32_t startMs;
            uint32_t endMs;
            int32_t x; // top-left in the 640 x 480 picture
            int32_t y;
            int32_t width;
            int32_t height;
            std::vector<uint8_t> mask; // 0 nothing, 1 text, 2 outline
        };
        std::vector<Item> _items;
    };
}
