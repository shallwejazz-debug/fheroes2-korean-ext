// HoMM1 graphics layer (homm1-remaster project).
// Reads the original DOS HEROES.AGG (HoMM1 v1.3) and gives its palette and images to the engine.
// Turned on when the HoMM1 AGG file is found: environment variable H1_AGG, else "h1/HEROES.AGG" next to the executable's working directory.
#pragma once

#include <array>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fheroes2
{
    class Image;
    class Sprite;
}

namespace H1
{
    // HoMM1 AGG found (whatever the edition).
    bool hasH1Data();

    // Heroes I mode and HoMM1 AGG found.
    bool isGraphicsMode();

    // Raw file from the HoMM1 AGG (name is case-insensitive). Empty if absent.
    const std::vector<uint8_t> & getH1AggFile( const std::string & name );

    // A HoMM1 mouse set (.mse, note v159): a count, the ICN name in 13 bytes, then per frame width, height, hotspot x, y (2 bytes each).
    // Sets the cursor to that frame of the named ICN with its hotspot; false when the frame does not exist.
    bool setMouseCursor( const std::string & mseName, int frame );

    // HoMM1 kb.pal (768 bytes, 6-bit values like the HoMM2 KB.PAL).
    const std::vector<uint8_t> & getPalette();

    // HoMM2 resources still used in HoMM1 mode are drawn with the HoMM1 palette: each HoMM2 palette index is moved to the
    // nearest HoMM1 colour (temporary stand-in until every screen uses HoMM1 images). h2Palette = HoMM2 KB.PAL bytes.
    void buildRemapFromHoMM2Palette( const std::vector<uint8_t> & h2Palette );
    void remapHoMM2Image( fheroes2::Image & image );

    // The original darkening table (Windows 0x4a3e2c, DOS file offset 0x126068 for the DOS palette): shadows (FUN_0047da2c) and greyed items.
    const std::vector<uint8_t> & darkenTable();

    // Terrain of a converted HoMM1 map: original tile bytes kept next to the .fh2m (written by --h1convert as "<map>.h1t").
    // Returns nullptr if the current map has no HoMM1 tile data.
    const fheroes2::Image * getTerrainImage( int32_t tileIndex );

    // The 10 original bytes of a tile of the current converted HoMM1 map, or nullptr (note v39 section 1).
    const uint8_t * getTileBytes( int32_t tileIndex );

    // New game tile flags of FUN_00415a33 (with FUN_0041584f): byte 7 bit 0x80 (blocked) where two mountain layers overlap, on
    // mountain parts 0x35 / 0x37 / 0x39 / 0x3b by the right neighbour and on mountains and trees at the map edge; byte 6 bit 0x80 for
    // kind 0x32; 0x2c becomes 0xac; active kinds the game does not visit lose bit 0x80 (note v192). `tiles`: 72 x 72 records of 10
    // bytes, row-major (index = y * 72 + x). keepLazyRandom: leave 0xc0..0xc6 (the port converts random castles, mines and monsters
    // later; the original has converted them before this pass) and only apply what the original does to the rest.
    void applyNewGameTileFlags( uint8_t * tiles, bool keepLazyRandom );

    // FUN_0041584f for one tile (also run by FUN_00402f1a when an object is taken off).
    void mountainEdgeFlags( uint8_t * tiles, int32_t x, int32_t y );
    void mountainEdgeFlagsOne( uint8_t * tile, int rightKind ); // rightKind: byte 8 of the tile at x + 1, -1 at the last column

    // FUN_00402f1a: the bytes of a tile whose object was taken off (copy of the tile's bytes, changed in place; neighbours read from
    // getTileBytes).
    void takeOffObject( uint8_t * tile, int32_t tileIndex );

    // True if the converter placed a game object on this tile (older .h1t files: always true).
    bool wasPlaced( int32_t tileIndex );

    // Tile bytes the game changed after the map was read (new month monsters, FUN_0041aa04; note v168). They are written over the map's
    // bytes, kept for the save (side file) and dropped when a map is read again.
    void patchTileBytes( int32_t tileIndex, const std::array<uint8_t, 10> & bytes );
    const std::map<int32_t, std::array<uint8_t, 10>> & tilePatches();
    void resetTilePatches();

    // H1-INTENDED(D16): the random map of an extension game is made again for every new extension game (maps/RNDM1234.fh2m and its
    // .h1t), so a save keeps its own copy of the HoMM1 tile bytes ("<save>.h1t"). saveTileBytesCopy writes it (or removes an old one
    // when the game is not on the random map); useTileBytesCopy makes the drawing read it instead of the map's file.
    void saveTileBytesCopy( const std::string & path );
    void useTileBytesCopy( const std::string & path );

    // FUN_0041d268 at new game (note v195): every tile whose terrain byte 0 has n % 20 > 3 gets (n & 0xfc) + random 0..3 (rows y,
    // columns x). The chosen 0..3 are kept here (one per tile, row-major; empty = none) and saved with the game.
    void newGameTerrainVariants();
    const std::vector<uint8_t> & terrainVariants();
    void setTerrainVariants( std::vector<uint8_t> variants );

    // Position of entry `slot` of the original castle list (mines = false) or mine list (mines = true, v59 .h1t files). False if empty/absent.
    bool getListPosition( bool mines, int slot, int32_t & x, int32_t & y );

    // Kind byte (resource number 0 wood .. 6 gold) of the mine-list entry at this tile, -1 if none (v59 .h1t files).
    int getMineKind( int32_t tileIndex );

    // loop%04d.82m number of the map tile (world +0x145d6 from the .MAP, note v109), -1 none.
    int getLoopSound( int32_t tileIndex );

    // Hero setup records of the map (tile byte 8 = 0xc7), from the "HERO" part of the .h1t (note v157): x, y and the record's
    // first 0x19 bytes. Empty for a map without them or an older .h1t.
    struct HeroRecord
    {
        int x = 0;
        int y = 0;
        std::array<uint8_t, 0x19> bytes{};
    };
    std::vector<HeroRecord> mapHeroRecords();

    // Slot (0..35) of the castle whose entrance is this tile in the original castle list, -1 if none. type = original type byte.
    int getCastleSlot( int32_t tileIndex, int * type );

    // Heroes I graphics mode: replaces the first frames of a HoMM2 ICN by the HoMM1 file of the same name when that file is in the
    // checked list (same frame count/layout: hero, boat, shadow, flag sprites; note v48). No-op otherwise.
    void replaceWithH1Frames( const char * icnFileName, std::vector<fheroes2::Sprite> & frames );

    // Images of a HoMM1 .til file (count, width, height, then raw pixels), decoded (cached). Empty if absent.
    const std::vector<fheroes2::Image> & getTil( const std::string & name );

    // Frames of a HoMM1 ICN file, decoded (cached). Empty if absent.
    const std::vector<fheroes2::Sprite> & getIcn( const std::string & name );

    // Same, every frame read as a one-colour (mono) frame: pixels drawn have image 0 / transform 0. Used for font glyphs.
    const std::vector<fheroes2::Sprite> & getIcnMono( const std::string & name );
    const std::vector<fheroes2::Sprite> & getIcnFrames( const std::string & name, bool forceMono );
}
