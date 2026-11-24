// Heroes I castle screen (homm1-remaster project): townwind.bin, FUN_0044021c / FUN_0044340d / FUN_00440091 / FUN_00437ed0 / FUN_004385a7.
// See docs/analysis note v61.
#pragma once

#include <cstdint>

class Castle;
enum class PlayerColor : uint8_t;

namespace H1
{
    // Thieves' guild window for the viewer (guildsOverride > 0 sets the guild count, the seer's shrine uses 8).
    void showThievesGuildWindow( PlayerColor viewer, int guildsOverride, const char * title );

    // HoMM1 recruit window for a creature (HoMM1 number) with "available" waiting; returns the count chosen (0 none). popup: right click view.
    int recruitWindow( int creature, int available, PlayerColor buyer, bool popup );

    // Price of one creature: gold and its extra resource (HoMM1 resource number, -1 none).
    void creaturePrice( int creature, int & gold, int & resource );

    enum class CastleScreenResult : int
    {
        Exit, // Exit button / Esc
        Reopen, // the screen must be built again (a hero was recruited)
        OpenEngineScreen // a click the HoMM1 screen does not handle yet: the caller opens the engine castle screen [temporary]
    };

    CastleScreenResult runCastleScreen( Castle & castle );

    // Castle +0x16: bit = HoMM1 building number (0 Mage Guild .. 4 Well, 5 Tent, 6 Castle, 7..12 dwellings) [engine buildings mapped].
    int castleBuildingBits( const Castle & castle );

    // HoMM1 building numbers (0 mage guild .. 6 castle, 7..12 dwellings): built, may be built now (FUN_00422d50), cost (FUN_00422bb1,
    // wood, mercury, ore, sulfur, crystal, gems, gold), build (pays), and the creature of a dwelling (-1 for other numbers).
    bool isBuildingBuilt( const Castle & castle, int number );
    bool canBuildBuilding( const Castle & castle, int number );
    void buildingCost( const Castle & castle, int number, int cost[7] );
    void constructBuilding( Castle & castle, int number );
    int dwellingCreatureOf( const Castle & castle, int number );
    int buildingRace( const Castle & castle );
    // FUN_00422bb1( race, building, out, guild row ): the cost row (guild row 0..3 = the guild byte as the caller gives it).
    void buildingCostRow( int race, int number, int guildRow, int cost[7] );
    // Castle +0x18: mage guild byte (0 = first level .. 3 = fourth).
    int castleGuildByte( const Castle & castle );
}
