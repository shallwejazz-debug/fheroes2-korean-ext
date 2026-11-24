#pragma once

// H1-INTENDED(D16) extension mode: random HoMM1 maps (owner 2026-11-18 11:17 / 12:0x, notes v244-v245). The maker writes a HoMM1 .MAP from
// pieces of the original maps (h1/ext/RMGDATA.BIN, made by tools/randommap/export_data.py) and converts it like the original maps.

#include <cstdint>
#include <string>

namespace H1
{
    // File name of the random map (5th..8th letters "1234": listed for 1 to 4 people, FUN_0041eeab pattern).
    constexpr const char * randomMapBase = "RNDM1234";

    // Writes a HoMM1 .MAP made with `seed` to `mapPath`. False (and `err`) if the data file is missing or no castle fitted.
    bool makeRandomMap( const std::string & mapPath, uint32_t seed, std::string & err );

    // New extension game: a fresh random map as maps/RNDM1234.fh2m (converted with convertMap). False if it could not be made.
    bool prepareRandomMap( uint32_t seed );
}
