// Heroes I sound effects (homm1-remaster project). Note v107.
// The original keeps a sound object per .82M file (FUN_0047f6c0): playback rate and sample size from the extension read backwards
// ("82M": 22050 Hz, 8 bit, mono), volume 0x7f (creature sounds 0x40, FUN_00406b67), loop count 1, and a channel group:
// 2 for one-off sounds (FUN_00426ed9), 3 for creature sounds. FUN_00475970 plays it on a free channel of its group (table 0x4a3268:
// group 0 = channel 0, 1 = channel 1, 2 = channels 2..5, 3 = channels 6..15); when the group is full the next channel in turn is stopped
// and reused. The samples are signed 8-bit (AIL_set_sample_type( s, 0, 1 ); the data centre on 0x00 / 0xff).
#pragma once

#include <cstdint>
#include <string>

namespace H1
{
    enum : int
    {
        SOUND_GROUP_EFFECT = 2,
        SOUND_GROUP_CREATURE = 3
    };

    // Plays <name> from HEROES.AGG. volume 0..0x7f as in the original object (+0x26). Returns a handle (-1 when nothing plays).
    int playSound( const std::string & name, int volume = 0x7f, int group = SOUND_GROUP_EFFECT );

    // Endless loop on group 3 (FUN_0046254e: loop count 0). Returns a handle.
    int playLoop( const std::string & name, int volume );

    // FUN_00474ba0: new volume of a playing sound. FUN_00474b80: stops it.
    void setSoundVolume( int handle, int volume );
    void stopSound( int handle );
    bool isSoundPlaying( int handle );

    // FUN_00462208 (note v109): the map's loop sounds around (x, y): up to 8 loops from the tiles within 3 rings, volume by ring
    // (table 0x48d4b8: 64, 48, 32, 16). reset: stop them all first (the original does it when the view jumps; 0 while a hero walks).
    void updateMapLoopSounds( int x, int y, bool reset );
    void stopMapLoopSounds();

    // FUN_0043a199: a hero's step on the map: wsnd<a><b>.82m, a = 0 at speed Walk else 2 (setting DAT_004a9b70), b from the tile's
    // terrain group (table 0x4a7d68: tile / 20) through {0, 3, 5, 3, 4, 5, 6}; volume 0x40, group 2 (FUN_0045440e).
    void playHeroStepSound( int32_t tileIndex );

    // ---- Music (note v111): HEROES%02d.82M from the CD's SOUND folder (here h1/SOUND/), signed 8-bit 22050 Hz mono like the effects.
    // FUN_004757c0 / FUN_004750f0: a track already playing is not started again; tracks 0..6, 0x1d..0x20, 0x28..0x2a, 0x2f..0x31,
    // 0x35..0x36 loop, the others play once. -1 stops the music.
    void playMusic( int track );
    int currentMusic();

    // Engine music track (MUS::) -> HoMM1 track; -2 when HoMM1 has no such moment (the music then stays as it is).
    int musicFromEngineTrack( int engineTrack );

    // FUN_0040339f: the tune for visiting a map object (tile byte 8 & 0x7f, byte 9), -1 none.
    int objectVisitTrack( int objectType, int objectValue );

    // Terrain tune of a map tile: its terrain group (table 0x4a7d68 = tile / 20), as the hero's +0x9a.
    int terrainTrack( int32_t tileIndex );

    // FUN_00426f2a: waits until the sound of the handle ends, at most timeoutMs (the original's -1 = 4000 ms).
    void waitSound( int handle, int timeoutMs = 4000 );
}
