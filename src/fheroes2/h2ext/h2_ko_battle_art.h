// H2-KOREAN: battle spell marks and the spell list panel art (see h2_ko_battle_art.cpp). 0 = transparent palette index.
#pragma once

#include <cstdint>

namespace H2Korean::BattleArt
{
    // spell mark pictures, order: bloodlust, bless, haste, shield, stoneskin, dragon slayer, steelskin, anti-magic,
    // curse, slow, berserker, hypnotize, blind, paralyze, petrify, defend
    extern const uint8_t icons22[16][22 * 22];
    extern const uint8_t icons26[16][26 * 26];

    // panel frame: 9 slices around corners of 20 pixels
    constexpr int32_t frameWidth = 493;
    constexpr int32_t frameHeight = 180;
    constexpr int32_t frameCorner = 20;
    extern const uint8_t frame[frameWidth * frameHeight];

    // header band, one per side colour: blue, green, red, yellow, orange, purple, neutral, berserk; 3 slices with caps of 13 pixels
    constexpr int32_t bandWidth = 283;
    constexpr int32_t bandHeight = 24;
    constexpr int32_t bandCap = 13;
    extern const uint8_t bands[8][bandWidth * bandHeight];
}
