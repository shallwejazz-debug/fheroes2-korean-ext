// Heroes I sound effects (homm1-remaster project). See h1_sound.h.
#include "h1_sound.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <cstdint>
#include <map>
#include <vector>

#include "audio.h"
#include "h1_gfx.h"
#include "localevent.h"
#include "mus.h"
#include "settings.h"
#include "timing.h"

namespace
{
    struct Group
    {
        int first;
        int end;
        int next;
    };

    // Table 0x4a3268 (start, end, next channel), groups 0..4; group 4 is empty, so its sounds never play.
    std::array<Group, 5> groups{ { { 0, 1, 0 }, { 1, 2, 1 }, { 2, 6, 2 }, { 6, 16, 6 }, { 0, 0, 0 } } };

    // Original channel number -> engine mixer channel playing there (-1 none).
    std::array<int, 16> mixerChannel = [] {
        std::array<int, 16> a{};
        a.fill( -1 );
        return a;
    }();

    std::map<std::pair<std::string, int>, std::vector<uint8_t>> cache;

    // Object volume 1..0x7f -> SDL distance attenuation 0..254 (the loop keeps playing while its volume changes).
    uint8_t distanceFor( const int volume )
    {
        const int v = std::clamp( volume, 1, 0x7f );
        return static_cast<uint8_t>( 254 - 254 * v / 0x7f );
    }

    int freeChannel( const int group )
    {
        if ( group < 0 || group >= static_cast<int>( groups.size() ) ) {
            return -1;
        }
        Group & g = groups[group];
        int ch = g.first;
        while ( ch < g.end && mixerChannel[ch] >= 0 && Mixer::isPlaying( mixerChannel[ch] ) ) {
            ++ch;
        }
        if ( ch == g.end ) {
            if ( g.first == g.end ) {
                return -1;
            }
            ch = g.next;
            g.next = ( g.next + 1 < g.end ) ? g.next + 1 : g.first;
            if ( mixerChannel[ch] >= 0 ) {
                Mixer::Stop( mixerChannel[ch] );
                mixerChannel[ch] = -1;
            }
        }
        return ch;
    }

    struct LoopSlot
    {
        int id = -1;
        int distance = 0x7f;
        int handle = -1;
    };

    std::array<LoopSlot, 8> loopSlots;
    uint32_t loopMask = 0;

    // Table 0x48d4b8.
    const int loopVolume[5] = { 64, 48, 32, 16, 10 };

    const std::vector<uint8_t> & wave( const std::string & name, const int volume )
    {
        std::vector<uint8_t> & v = cache[{ name, volume }];
        if ( !v.empty() ) {
            return v;
        }
        const std::vector<uint8_t> & body = H1::getH1AggFile( name );
        if ( body.empty() ) {
            return v;
        }
        const auto le32 = [&v]( const uint32_t x ) {
            for ( int i = 0; i < 4; ++i ) {
                v.push_back( static_cast<uint8_t>( x >> ( i * 8 ) ) );
            }
        };
        const auto le16 = [&v]( const uint32_t x ) {
            v.push_back( static_cast<uint8_t>( x ) );
            v.push_back( static_cast<uint8_t>( x >> 8 ) );
        };
        const uint32_t size = static_cast<uint32_t>( body.size() );
        v.reserve( size + 44 );
        le32( 0x46464952 );
        le32( size + 0x24 );
        le32( 0x45564157 );
        le32( 0x20746D66 );
        le32( 0x10 );
        le16( 1 );
        le16( 1 );
        le32( 22050 );
        le32( 22050 );
        le16( 1 );
        le16( 8 );
        le32( 0x61746164 );
        le32( size );
        // Signed 8-bit samples (the original) -> unsigned 8-bit WAV, scaled by the object volume / 0x7f [the engine's sound effect volume
        // stands in for the original volume setting (11 - setting) / 10].
        for ( const uint8_t b : body ) {
            const int s = static_cast<int8_t>( b ) * volume / 0x7f;
            v.push_back( static_cast<uint8_t>( s + 0x80 ) );
        }
        return v;
    }
}

namespace H1
{
    int playSound( const std::string & name, const int volume, const int group )
    {
        if ( volume <= 0 ) {
            return -1;
        }
        const int ch = freeChannel( group );
        if ( ch < 0 ) {
            return -1;
        }
        const std::vector<uint8_t> & v = wave( name, std::min( volume, 0x7f ) );
        if ( v.empty() ) {
            return -1;
        }
        mixerChannel[ch] = Mixer::Play( v.data(), static_cast<uint32_t>( v.size() ), false );
        return mixerChannel[ch] >= 0 ? ch : -1;
    }

    int playLoop( const std::string & name, const int volume )
    {
        const int ch = freeChannel( SOUND_GROUP_CREATURE );
        if ( ch < 0 ) {
            return -1;
        }
        const std::vector<uint8_t> & v = wave( name, 0x7f );
        if ( v.empty() ) {
            return -1;
        }
        mixerChannel[ch] = Mixer::Play( v.data(), static_cast<uint32_t>( v.size() ), true, std::pair<int16_t, uint8_t>{ 0, distanceFor( volume ) } );
        return mixerChannel[ch] >= 0 ? ch : -1;
    }

    void setSoundVolume( const int handle, const int volume )
    {
        if ( handle >= 0 && handle < static_cast<int>( mixerChannel.size() ) && mixerChannel[handle] >= 0 ) {
            Mixer::setPosition( mixerChannel[handle], 0, distanceFor( volume ) );
        }
    }

    void stopSound( const int handle )
    {
        if ( handle >= 0 && handle < static_cast<int>( mixerChannel.size() ) && mixerChannel[handle] >= 0 ) {
            Mixer::Stop( mixerChannel[handle] );
            mixerChannel[handle] = -1;
        }
    }

    bool isSoundPlaying( const int handle )
    {
        return handle >= 0 && handle < static_cast<int>( mixerChannel.size() ) && mixerChannel[handle] >= 0 && Mixer::isPlaying( mixerChannel[handle] );
    }

    void stopMapLoopSounds()
    {
        for ( LoopSlot & slot : loopSlots ) {
            if ( slot.id != -1 ) {
                stopSound( slot.handle );
            }
            slot = LoopSlot();
        }
    }

    void updateMapLoopSounds( const int x, const int y, const bool reset )
    {
        // A loop the engine stopped (battle, audio reset) is taken as gone, so it starts again below.
        for ( LoopSlot & slot : loopSlots ) {
            if ( slot.id != -1 && !isSoundPlaying( slot.handle ) ) {
                slot = LoopSlot();
            }
        }
        for ( LoopSlot & slot : loopSlots ) {
            if ( slot.id != -1 ) {
                if ( reset ) {
                    stopSound( slot.handle );
                    slot = LoopSlot();
                }
                else {
                    slot.distance = 0x7f;
                }
            }
        }
        loopMask = 0;
        // FUN_0046254e for one tile at ring distance d; pass 1 only updates loops already playing, pass 2 may start new ones.
        const auto visit = [&]( const int tx, const int ty, const int d, const int pass ) {
            if ( tx < 0 || ty < 0 || tx >= 72 || ty >= 72 ) {
                return;
            }
            const int id = getLoopSound( ty * 72 + tx );
            if ( id < 0 ) {
                return;
            }
            for ( LoopSlot & slot : loopSlots ) {
                if ( slot.id == id ) {
                    if ( d < slot.distance ) {
                        slot.distance = d;
                        loopMask |= 1u << ( id & 0x1f );
                    }
                    return;
                }
            }
            if ( pass == 1 ) {
                return;
            }
            int farthest = d;
            int chosen = -1;
            for ( int k = 0; k < 8; ++k ) {
                if ( farthest < loopSlots[k].distance ) {
                    farthest = loopSlots[k].distance;
                    chosen = k;
                }
            }
            if ( chosen == -1 ) {
                return;
            }
            LoopSlot & slot = loopSlots[chosen];
            if ( slot.id != -1 ) {
                stopSound( slot.handle );
            }
            slot.id = id;
            slot.distance = d;
            char name[32];
            std::snprintf( name, sizeof( name ), "loop%04d.82m", id );
            slot.handle = playLoop( name, loopVolume[std::min( d, 4 )] );
            loopMask ^= 1u << ( id & 0x1f );
        };
        for ( int pass = 1; pass < 3; ++pass ) {
            visit( x, y, 0, pass );
            for ( int d = 0; d < 4; ++d ) {
                for ( int k = 0; k < d * 2; ++k ) {
                    visit( x - d + k, y - d, d, pass );
                    visit( x + d, y - d + k, d, pass );
                    visit( x + d - k, y + d, d, pass );
                    visit( x - d, y + d - k, d, pass );
                }
            }
        }
        for ( LoopSlot & slot : loopSlots ) {
            if ( slot.id != -1 && slot.distance > 5 ) {
                stopSound( slot.handle );
                slot = LoopSlot();
            }
            if ( slot.id != -1 && ( loopMask & ( 1u << ( slot.id & 0x1f ) ) ) != 0 ) {
                setSoundVolume( slot.handle, loopVolume[std::min( slot.distance, 4 )] );
            }
        }
    }

    void playHeroStepSound( const int32_t tileIndex )
    {
        const uint8_t * tile = getTileBytes( tileIndex );
        if ( tile == nullptr ) {
            return;
        }
        static const int digit[7] = { 0, 3, 5, 3, 4, 5, 6 };
        const int group = std::min( tile[0] / 20, 6 );
        // Engine hero speed 1..10 -> HoMM1 speed 0..4 as in the options window (h1_cpanel.cpp) [the engine setting stands in].
        const int h1Speed = std::clamp( ( Settings::Get().HeroesMoveSpeed() - 1 ) / 2, 0, 4 );
        char name[32];
        std::snprintf( name, sizeof( name ), "wsnd%d%d.82m", h1Speed > 0 ? 2 : 0, digit[group] );
        playSound( name, 0x40, SOUND_GROUP_EFFECT );
    }

    namespace
    {
        int musicTrack = -1;
    }

    int currentMusic()
    {
        return musicTrack;
    }

    void playMusic( const int track )
    {
        if ( track < 0 ) {
            Music::Stop();
            musicTrack = -1;
            return;
        }
        if ( track == musicTrack && Music::isPlaying() ) {
            return;
        }
        // The engine streams a music track from memory through an SDL reader it frees at once (fine for its MIDI, not for a streamed
        // WAV), so each track is converted once to a WAV file next to the original and played from the file.
        char wavName[48];
        std::snprintf( wavName, sizeof( wavName ), "h1/SOUND/HEROES%02d.wav", track );
        std::ifstream test( wavName, std::ios::binary );
        if ( !test ) {
            char name[48];
            std::snprintf( name, sizeof( name ), "h1/SOUND/HEROES%02d.82M", track );
            std::ifstream f( name, std::ios::binary );
            if ( !f ) {
                std::snprintf( name, sizeof( name ), "h1/sound/heroes%02d.82m", track );
                f.open( name, std::ios::binary );
            }
            if ( !f ) {
                Music::Stop();
                musicTrack = -1;
                return;
            }
            const std::vector<uint8_t> body( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
            std::vector<uint8_t> v;
            v.reserve( body.size() + 44 );
            const auto le32 = [&v]( const uint32_t x ) {
                for ( int i = 0; i < 4; ++i ) {
                    v.push_back( static_cast<uint8_t>( x >> ( i * 8 ) ) );
                }
            };
            const auto le16 = [&v]( const uint32_t x ) {
                v.push_back( static_cast<uint8_t>( x ) );
                v.push_back( static_cast<uint8_t>( x >> 8 ) );
            };
            const uint32_t size = static_cast<uint32_t>( body.size() );
            le32( 0x46464952 );
            le32( size + 0x24 );
            le32( 0x45564157 );
            le32( 0x20746D66 );
            le32( 0x10 );
            le16( 1 );
            le16( 1 );
            le32( 22050 );
            le32( 22050 );
            le16( 1 );
            le16( 8 );
            le32( 0x61746164 );
            le32( size );
            // Signed 8-bit samples (the original) -> unsigned 8-bit WAV.
            for ( const uint8_t b : body ) {
                v.push_back( static_cast<uint8_t>( b ^ 0x80 ) );
            }
            std::ofstream out( wavName, std::ios::binary );
            out.write( reinterpret_cast<const char *>( v.data() ), static_cast<std::streamsize>( v.size() ) );
        }
        const bool loop = ( track <= 6 ) || ( track >= 0x1d && track <= 0x20 ) || ( track >= 0x28 && track <= 0x2a ) || ( track >= 0x2f && track <= 0x31 )
                          || ( track >= 0x35 && track <= 0x36 );
        musicTrack = track;
        // A key of our own for the engine's music list (HoMM1 tracks never mix with the engine's).
        Music::Play( 0x4831000000000000ULL + static_cast<uint64_t>( track ), std::string( wavName ),
                     loop ? Music::PlaybackMode::REWIND_AND_PLAY_INFINITE : Music::PlaybackMode::PLAY_ONCE );
    }

    int terrainTrack( const int32_t tileIndex )
    {
        const uint8_t * tile = getTileBytes( tileIndex );
        if ( tile == nullptr ) {
            return -1;
        }
        return std::min( tile[0] / 20, 6 );
    }

    int objectVisitTrack( const int objectType, const int objectValue )
    {
        switch ( objectType ) {
        case 1:
        case 0x19:
        case 0x20:
            return 0x17;
        case 3:
        case 0x1c:
            return 0x14;
        case 4:
        case 6:
        case 8:
        case 0x18:
        case 0x2d:
            return 0x1c;
        case 5:
            return 7;
        case 7:
            return 8;
        case 9:
            return 0x18;
        case 10:
            return 9;
        case 0xb:
            return 10;
        case 0xc:
            return 0xb;
        case 0xd:
        case 0xe:
        case 0xf:
        case 0x10:
        case 0x11:
            return 0x16;
        case 0x16:
            return 0xc;
        case 0x17:
            return 0x1a;
        case 0x1b:
            return 0x15;
        case 0x22:
            return 0x1b;
        case 0x24:
            return 0xe;
        case 0x27:
            return 0xf;
        case 0x29:
            return 0x10;
        case 0x2a:
            return 0x11;
        case 0x2c:
            return 0x19;
        case 0x30:
            return objectValue == 1 ? 0x1c : -1;
        case 0x3f:
            return 0x2e;
        default:
            return -1;
        }
    }

    int musicFromEngineTrack( const int engineTrack )
    {
        switch ( engineTrack ) {
        case MUS::OCEAN:
            return 0;
        case MUS::GRASS:
            return 1;
        case MUS::SNOW:
            return 2;
        case MUS::SWAMP:
            return 3;
        case MUS::LAVA:
            return 4;
        case MUS::DESERT:
            return 5;
        case MUS::DIRT:
        case MUS::WASTELAND:
            return 6;
        // Castle screen (FUN_0044021c): 0x1d + table 0x492760 {3, 0, 2, 1} by castle kind (knight, barbarian, sorceress, warlock).
        case MUS::KNIGHT_CASTLE:
            return 0x1d + 3;
        case MUS::BARBARIAN_CASTLE:
            return 0x1d + 0;
        case MUS::SORCERESS_CASTLE:
            return 0x1d + 2;
        case MUS::WARLOCK_CASTLE:
            return 0x1d + 1;
        case MUS::MAINMENU:
            return 0x30;
        case MUS::VICTORY:
            return 0x36;
        case MUS::LOSTGAME:
            return 0x31;
        case MUS::NEW_WEEK:
            return 0x32;
        case MUS::NEW_MONTH:
            return 0x33;
        case MUS::PUZZLE:
            return 0xd;
        case MUS::ULTIMATE_ARTIFACT:
            return 0x2e;
        case MUS::EXPERIENCE:
            return 0x34;
        case MUS::BATTLEWIN:
            return 0x2c;
        case MUS::BATTLELOSE:
            return 0x2b;
        default:
            return -2;
        }
    }

    void waitSound( const int handle, const int timeoutMs )
    {
        if ( handle < 0 || handle >= static_cast<int>( mixerChannel.size() ) || mixerChannel[handle] < 0 ) {
            return;
        }
        const int limit = timeoutMs < 0 ? 4000 : timeoutMs;
        fheroes2::Time timer;
        LocalEvent & le = LocalEvent::Get();
        while ( Mixer::isPlaying( mixerChannel[handle] ) && timer.getMs() < static_cast<uint64_t>( limit ) ) {
            le.HandleEvents( false );
            fheroes2::delayforMs( 10 );
        }
    }
}
