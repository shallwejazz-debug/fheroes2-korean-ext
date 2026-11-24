/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2026                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "screen.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iterator>
#include <optional>
#include <ostream>
#include <set>
#include <utility>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_mouse.h>
#include <SDL_pixels.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_stdinc.h>
#include <SDL_surface.h>
#include <SDL_version.h>
#include <SDL_video.h>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#if defined( TARGET_PS_VITA )
#include <vita2d.h>
#endif

#include "h2_hd_layer.h" // H2-KOREAN
#include "image_palette.h"
#include "logging.h"
#include "math_tools.h"
#include "system.h"

namespace
{
    // Returns nearest screen supported resolution
    fheroes2::ResolutionInfo GetNearestResolution( fheroes2::ResolutionInfo resolutionInfo, const std::vector<fheroes2::ResolutionInfo> & resolutions )
    {
        if ( resolutions.empty() ) {
            return resolutionInfo;
        }

        if ( resolutionInfo.gameWidth < 1 ) {
            resolutionInfo.gameWidth = 1;
        }

        if ( resolutionInfo.gameHeight < 1 ) {
            resolutionInfo.gameHeight = 1;
        }

        if ( resolutionInfo.screenWidth < resolutionInfo.gameWidth ) {
            resolutionInfo.screenWidth = resolutionInfo.gameWidth;
        }

        if ( resolutionInfo.screenHeight < resolutionInfo.gameHeight ) {
            resolutionInfo.screenHeight = resolutionInfo.gameHeight;
        }

        const double gameX = resolutionInfo.gameWidth;
        const double gameY = resolutionInfo.gameHeight;
        const double screenX = resolutionInfo.screenWidth;
        const double screenY = resolutionInfo.screenHeight;

        std::vector<double> similarity( resolutions.size(), 0 );
        for ( size_t i = 0; i < resolutions.size(); ++i ) {
            similarity[i] = std::fabs( resolutions[i].gameWidth - gameX ) / gameX + std::fabs( resolutions[i].gameHeight - gameY ) / gameY
                            + std::fabs( resolutions[i].screenWidth - screenX ) / screenX + std::fabs( resolutions[i].screenHeight - screenY ) / screenY;
        }

        const std::vector<double>::difference_type id = std::distance( similarity.begin(), std::min_element( similarity.begin(), similarity.end() ) );

        return resolutions[id];
    }

#if !defined( TARGET_PS_VITA )
    bool IsLowerThanDefaultRes( const fheroes2::ResolutionInfo & value )
    {
        return value.gameWidth < fheroes2::Display::DEFAULT_WIDTH || value.gameHeight < fheroes2::Display::DEFAULT_HEIGHT;
    }

    std::set<fheroes2::ResolutionInfo> FilterResolutions( const std::set<fheroes2::ResolutionInfo> & resolutionSet )
    {
        static_assert( fheroes2::Display::DEFAULT_WIDTH == 640 && fheroes2::Display::DEFAULT_HEIGHT == 480, "Default resolution must be 640 x 480" );

        if ( resolutionSet.empty() ) {
            return { { fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT } };
        }

        std::set<fheroes2::ResolutionInfo> resolutions;

        for ( const fheroes2::ResolutionInfo & resolution : resolutionSet ) {
            if ( IsLowerThanDefaultRes( resolution ) ) {
                continue;
            }

            resolutions.emplace( resolution );
        }

        if ( resolutions.empty() ) {
            return { { fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT } };
        }

        {
            // TODO: add resolutions which are close to the current screen aspect ratio.
            // Some operating systems do not work well with SDL so they return very limited number of high resolutions.
            // Populate missing resolutions into the list.
            const std::set<fheroes2::ResolutionInfo> possibleResolutions
                = { { 640, 480 },   { 800, 600 },  { 1024, 768 },  { 1152, 864 }, { 1280, 600 }, { 1280, 720 },  { 1280, 768 }, { 1280, 960 },
                    { 1280, 1024 }, { 1360, 768 }, { 1400, 1050 }, { 1440, 900 }, { 1600, 900 }, { 1680, 1050 }, { 1920, 1080 } };

            assert( !resolutions.empty() );
            const fheroes2::ResolutionInfo lowestResolution = *( resolutions.begin() );

            for ( const fheroes2::ResolutionInfo & resolution : possibleResolutions ) {
                if ( lowestResolution.gameWidth < resolution.gameWidth || lowestResolution.gameHeight < resolution.gameHeight || lowestResolution == resolution ) {
                    continue;
                }

                resolutions.emplace( resolution );
            }
        }

        std::set<fheroes2::ResolutionInfo> scaledResolutions;

        {
            // Widescreen devices support much higher resolutions but items on such resolutions are too tiny. In order to improve user
            // experience on these devices we are adding a special non-standard resolution with (potentially) non-integer scale.
            assert( !resolutions.empty() );
            const fheroes2::ResolutionInfo biggestResolution = *( std::prev( resolutions.end() ) );
            assert( biggestResolution.gameWidth == biggestResolution.screenWidth && biggestResolution.gameHeight == biggestResolution.screenHeight );

            // This resolution should be really "widescreen" (or at least square), i.e. the width should be not less than the height, otherwise
            // the scaled resolution's in-game width will become less than DEFAULT_WIDTH
            if ( biggestResolution.gameWidth > fheroes2::Display::DEFAULT_WIDTH && biggestResolution.gameWidth >= biggestResolution.gameHeight ) {
                assert( biggestResolution.gameHeight >= fheroes2::Display::DEFAULT_HEIGHT );

                scaledResolutions.emplace( biggestResolution.gameWidth * fheroes2::Display::DEFAULT_HEIGHT / biggestResolution.gameHeight,
                                           fheroes2::Display::DEFAULT_HEIGHT, biggestResolution.gameWidth, biggestResolution.gameHeight );
            }
        }

        // Try to find and add scaled resolutions with integer scale (x2, x3, x4, etc) which correspond to existing "hardware" resolutions
        for ( const fheroes2::ResolutionInfo & resolution : resolutions ) {
            assert( resolution.gameWidth == resolution.screenWidth && resolution.gameHeight == resolution.screenHeight );

            const int32_t maxScale = std::min( resolution.gameWidth / fheroes2::Display::DEFAULT_WIDTH, resolution.gameHeight / fheroes2::Display::DEFAULT_HEIGHT );

            for ( int32_t scale = 2; scale <= maxScale; ++scale ) {
                if ( resolution.gameWidth % scale != 0 || resolution.gameHeight % scale != 0 ) {
                    continue;
                }

                scaledResolutions.emplace( resolution.gameWidth / scale, resolution.gameHeight / scale, resolution.gameWidth, resolution.gameHeight );
            }
        }

        resolutions.merge( scaledResolutions );

        return resolutions;
    }
#endif

    std::vector<uint8_t> StandardPaletteIndexes()
    {
        std::vector<uint8_t> indexes( fheroes2::paletteSize );
        for ( uint32_t i = 0; i < fheroes2::paletteSize; ++i ) {
            indexes[i] = static_cast<uint8_t>( i );
        }
        return indexes;
    }

    const fheroes2::RGB * RGBPalette( const bool forceDefaultPaletteUpdate = false )
    {
        static std::array<fheroes2::RGB, fheroes2::paletteSize> palette;
        static bool paletteInitialized = false;
        if ( forceDefaultPaletteUpdate || !paletteInitialized ) {
            palette = fheroes2::getNormalizedRGBGamePalette();

            paletteInitialized = true;
        }

        return palette.data();
    }

    // Updates `roi` to be only inside {0, 0, width, height} rectangle.
    // Returns false if `roi` is out of such rectangle.
    bool getActiveArea( fheroes2::Rect & roi, const int32_t width, const int32_t height )
    {
        if ( roi.width <= 0 || roi.height <= 0 || roi.x >= width || roi.y >= height ) {
            return false;
        }

        if ( roi.x < 0 ) {
            if ( -roi.x >= roi.width ) {
                return false;
            }

            roi.width += roi.x;
            roi.x = 0;
        }

        if ( roi.y < 0 ) {
            if ( -roi.y >= roi.height ) {
                return false;
            }

            roi.height += roi.y;
            roi.y = 0;
        }

        if ( const int32_t offsetX = roi.x + roi.width - width; offsetX > 0 ) {
            if ( offsetX >= roi.width ) {
                return false;
            }

            roi.width -= offsetX;
        }

        if ( const int32_t offsetY = roi.y + roi.height - height; offsetY > 0 ) {
            if ( offsetY >= roi.height ) {
                return false;
            }

            roi.height -= offsetY;
        }

        return true;
    }

    const fheroes2::RGB * currentRGBPalette = RGBPalette();

// If SDL library is used
#if !defined( TARGET_PS_VITA )
    class BaseSDLRenderer
    {
    protected:
        std::vector<uint32_t> _palette32Bit;
        std::vector<SDL_Color> _palette8Bit;

        void copyImageToSurface( const fheroes2::Image & image, SDL_Surface * surface, const fheroes2::Rect & roi )
        {
            assert( surface != nullptr && !image.empty() );

            if ( SDL_MUSTLOCK( surface ) ) {
                const int returnCode = SDL_LockSurface( surface );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to lock surface. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }
            }

            const int32_t imageWidth = image.width();
            const int32_t imageHeight = image.height();

            const bool fullFrame = ( roi.width == imageWidth ) && ( roi.height == imageHeight );

            const uint8_t * imageIn = image.image();

            if ( fullFrame ) {
                if ( surface->format->BitsPerPixel == 32 ) {
                    uint32_t * out = static_cast<uint32_t *>( surface->pixels );
                    const uint32_t * outEnd = out + imageWidth * imageHeight;
                    const uint8_t * in = imageIn;
                    const uint32_t * transform = _palette32Bit.data();

                    for ( ; out != outEnd; ++out, ++in )
                        *out = *( transform + *in );
                }
                else if ( ( surface->format->BitsPerPixel == 8 ) && ( surface->pixels != imageIn ) ) {
                    if ( imageWidth % 4 != 0 ) {
                        const int32_t screenWidth = ( imageWidth / 4 ) * 4 + 4;
                        for ( int32_t i = 0; i < imageHeight; ++i ) {
                            memcpy( static_cast<uint8_t *>( surface->pixels ) + screenWidth * i, imageIn + imageWidth * i, static_cast<size_t>( imageWidth ) );
                        }
                    }
                    else {
                        memcpy( surface->pixels, imageIn, static_cast<size_t>( imageWidth ) * imageHeight );
                    }
                }
            }
            else {
                if ( surface->format->BitsPerPixel == 32 ) {
                    uint32_t * outY = static_cast<uint32_t *>( surface->pixels );
                    const uint32_t * outYEnd = outY + imageWidth * roi.height;
                    const uint8_t * inY = imageIn + roi.x + roi.y * imageWidth;
                    const uint32_t * transform = _palette32Bit.data();

                    for ( ; outY != outYEnd; outY += imageWidth, inY += imageWidth ) {
                        uint32_t * outX = outY;
                        const uint32_t * outXEnd = outX + roi.width;
                        const uint8_t * inX = inY;

                        for ( ; outX != outXEnd; ++outX, ++inX )
                            *outX = *( transform + *inX );
                    }
                }
                else if ( ( surface->format->BitsPerPixel == 8 ) && ( surface->pixels != imageIn ) ) {
                    const int32_t screenWidth = ( imageWidth / 4 ) * 4 + 4;
                    const int32_t screenOffset = roi.x + roi.y * screenWidth;
                    const int32_t imageOffset = roi.x + roi.y * imageWidth;
                    for ( int32_t i = 0; i < roi.height; ++i ) {
                        memcpy( static_cast<uint8_t *>( surface->pixels ) + screenWidth * i + screenOffset, imageIn + imageOffset + imageWidth * i,
                                static_cast<size_t>( roi.width ) );
                    }
                }
            }

            if ( SDL_MUSTLOCK( surface ) )
                SDL_UnlockSurface( surface );
        }

        void generatePalette( const std::vector<uint8_t> & colorIds, const SDL_Surface * surface )
        {
            assert( surface != nullptr );

            if ( surface->format->BitsPerPixel == 32 ) {
                _palette32Bit.resize( fheroes2::paletteSize );

                if ( surface->format->Amask > 0 ) {
                    if ( surface->format->format == SDL_PIXELFORMAT_RGBA32 ) {
                        for ( size_t i = 0; i < fheroes2::paletteSize; ++i ) {
                            _palette32Bit[i] = currentRGBPalette[colorIds[i]].getRGBA();
                        }
                    }
                    else if ( surface->format->format == SDL_PIXELFORMAT_BGRA32 ) {
                        for ( size_t i = 0; i < fheroes2::paletteSize; ++i ) {
                            _palette32Bit[i] = currentRGBPalette[colorIds[i]].getBGRA();
                        }
                    }
                }
                else {
#if SDL_VERSION_ATLEAST( 2, 30, 0 )
                    if ( surface->format->format == SDL_PIXELFORMAT_RGBX32 ) {
#elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                    if ( surface->format->format == SDL_PIXELFORMAT_RGBX8888 ) {
#else
                    if ( surface->format->format == SDL_PIXELFORMAT_XBGR8888 ) {
#endif
                        for ( size_t i = 0; i < fheroes2::paletteSize; ++i ) {
                            _palette32Bit[i] = currentRGBPalette[colorIds[i]].getRGBX();
                        }
                    }
#if SDL_VERSION_ATLEAST( 2, 30, 0 )
                    else if ( surface->format->format == SDL_PIXELFORMAT_BGRX32 ) {
#elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                    if ( surface->format->format == SDL_PIXELFORMAT_BGRX8888 ) {
#else
                    if ( surface->format->format == SDL_PIXELFORMAT_XRGB8888 ) {
#endif
                        for ( size_t i = 0; i < fheroes2::paletteSize; ++i ) {
                            _palette32Bit[i] = currentRGBPalette[colorIds[i]].getBGRX();
                        }
                    }
                }
            }
            else if ( surface->format->BitsPerPixel == 8 ) {
                _palette8Bit.resize( fheroes2::paletteSize );
                for ( uint32_t i = 0; i < fheroes2::paletteSize; ++i ) {
                    const auto & pal = currentRGBPalette[colorIds[i]];
                    SDL_Color & col = _palette8Bit[i];

                    col.r = pal.r;
                    col.g = pal.g;
                    col.b = pal.b;
                }
            }
            else {
                // This is unsupported format. Please implement it.
                assert( 0 );
            }
        }

        static SDL_Surface * generateIconSurface( const fheroes2::Image & icon )
        {
            if ( icon.empty() || icon.singleLayer() ) {
                // What are you trying to do? Icon should have not empty both image and transform layers.
                assert( 0 );
                return nullptr;
            }

            SDL_Surface * surface = SDL_CreateRGBSurface( 0, icon.width(), icon.height(), 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000 );
            if ( surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << icon.width() << " x " << icon.height() << " size for cursor. The error: " << SDL_GetError() )
                return nullptr;
            }

            const uint32_t width = icon.width();
            const uint32_t height = icon.height();

            uint32_t * out = static_cast<uint32_t *>( surface->pixels );
            const uint32_t * outEnd = out + width * height;
            const uint8_t * in = icon.image();
            const uint8_t * transform = icon.transform();

            if ( surface->format->Amask > 0 ) {
                for ( ; out != outEnd; ++out, ++in, ++transform ) {
                    if ( *transform == 0 ) {
                        *out = currentRGBPalette[*in].getRGBA();
                    }
                }
            }
            else {
                for ( ; out != outEnd; ++out, ++in, ++transform ) {
                    if ( *transform == 0 ) {
                        *out = currentRGBPalette[*in].getRGBX();
                    }
                    else {
                        *out = 0;
                    }
                }
            }

            return surface;
        }
    };

    class RenderCursor final : public fheroes2::Cursor
    {
    public:
        ~RenderCursor() override
        {
            clear();
        }

        void show( const bool enable ) override
        {
            Cursor::show( enable );

            if ( !_emulation ) {
                const int returnCode = SDL_ShowCursor( _show ? SDL_ENABLE : SDL_DISABLE );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to set cursor. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }
            }
        }

        bool isVisible() const override
        {
            if ( _emulation ) {
                return Cursor::isVisible();
            }

            return Cursor::isVisible() && ( SDL_ShowCursor( SDL_QUERY ) == SDL_ENABLE );
        }

        void update( const fheroes2::Image & image, const int32_t offsetX, const int32_t offsetY ) override
        {
            if ( image.empty() || image.singleLayer() ) {
                // What are you trying to do? Set an invisible cursor? Use hide() method!
                assert( 0 );
                return;
            }

            if ( _emulation ) {
                Cursor::update( image, offsetX, offsetY );
                return;
            }

            SDL_Surface * surface = SDL_CreateRGBSurface( 0, image.width(), image.height(), 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000 );
            if ( surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << image.width() << " x " << image.height() << " size for cursor. The error: " << SDL_GetError() )
                return;
            }

            const uint32_t width = image.width();
            const uint32_t height = image.height();

            uint32_t * out = static_cast<uint32_t *>( surface->pixels );
            const uint32_t * outEnd = out + width * height;
            const uint8_t * in = image.image();
            const uint8_t * transform = image.transform();

            if ( surface->format->Amask > 0 ) {
                for ( ; out != outEnd; ++out, ++in, ++transform ) {
                    if ( *transform == 0 ) {
                        *out = currentRGBPalette[*in].getRGBA();
                    }
                    else if ( *transform > 1 && *transform < 6 ) {
                        // SDL2 uses RGBA image on OS level separately from frame rendering.
                        // Here we are trying to simulate cursor's shadow as close as possible to the original game.
                        // Shadow strength in transform layer is: from 5 (light) to 2 (strong).
                        *out = 8U << ( ( 5U - *transform ) + 24U );
                    }
                }
            }
            else {
                for ( ; out != outEnd; ++out, ++in, ++transform ) {
                    if ( *transform == 0 ) {
                        *out = currentRGBPalette[*in].getRGBX();
                    }
                    else {
                        *out = SDL_MapRGB( surface->format, 0, 0, 0 );
                    }
                }
            }

            SDL_Cursor * tempCursor = SDL_CreateColorCursor( surface, offsetX, offsetY );
            if ( tempCursor == nullptr ) {
                ERROR_LOG( "Failed to create a cursor. The error description: " << SDL_GetError() )
            }
            else {
                SDL_SetCursor( tempCursor );
            }

            const int returnCode = SDL_ShowCursor( _show ? SDL_ENABLE : SDL_DISABLE );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to set cursor state. The error value: " << returnCode << ", description: " << SDL_GetError() )
            }
            SDL_FreeSurface( surface );

            if ( tempCursor != nullptr ) {
                clear();
                std::swap( _cursor, tempCursor );
            }
        }

        void enableSoftwareEmulation( const bool enable ) override
        {
            const bool newState = enable || isSoftwareEmulationForced();

            if ( newState == _emulation ) {
                return;
            }

            if ( newState ) {
                clear();

                const int returnCode = SDL_ShowCursor( SDL_DISABLE );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to disable cursor. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }

                _emulation = true;
            }
            else {
                const int returnCode = SDL_ShowCursor( _show ? SDL_ENABLE : SDL_DISABLE );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to set cursor state. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }

                _emulation = false;
            }

            if ( _cursorUpdater != nullptr ) {
                _cursorUpdater();
            }
        }

        static RenderCursor * create()
        {
            return new RenderCursor;
        }

    private:
        SDL_Cursor * _cursor{ nullptr };

        RenderCursor()
        {
            _emulation = false;

            const int returnCode = SDL_ShowCursor( _show ? SDL_ENABLE : SDL_DISABLE );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to set cursor state. The error value: " << returnCode << ", description: " << SDL_GetError() )
            }
        }

        void clear()
        {
            if ( _cursor != nullptr ) {
                SDL_FreeCursor( _cursor );
                _cursor = nullptr;
            }
        }
    };
#endif

#if defined( TARGET_PS_VITA )
    class RenderCursor final : public fheroes2::Cursor
    {
    public:
        static RenderCursor * create()
        {
            auto * cursor = new RenderCursor;
            cursor->forceSoftwareEmulation();

            return cursor;
        }
    };

    class RenderEngine final : public fheroes2::BaseRenderEngine
    {
    public:
        RenderEngine( const RenderEngine & ) = delete;

        ~RenderEngine() override
        {
            clear();
        }

        RenderEngine & operator=( const RenderEngine & ) = delete;

        void toggleFullScreen() override
        {
            BaseRenderEngine::toggleFullScreen();

            const fheroes2::Display & display = fheroes2::Display::instance();
            _calculateScreenScaling( display.width(), display.height(), isFullScreen() );
        }

        static RenderEngine * create()
        {
            return new RenderEngine;
        }

        fheroes2::Rect getActiveWindowROI() const override
        {
            return _destRect;
        }

        fheroes2::Size getCurrentScreenResolution() const override
        {
            return { VITA_FULLSCREEN_WIDTH, VITA_FULLSCREEN_HEIGHT };
        }

        std::vector<fheroes2::ResolutionInfo> getAvailableResolutions() const override
        {
            static const std::vector<fheroes2::ResolutionInfo> filteredResolutions = []() {
                std::set<fheroes2::ResolutionInfo> resolutionSet;
                resolutionSet.emplace( fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT );
                resolutionSet.emplace( VITA_ASPECT_CORRECTED_WIDTH, fheroes2::Display::DEFAULT_HEIGHT );
                resolutionSet.emplace( VITA_FULLSCREEN_WIDTH, VITA_FULLSCREEN_HEIGHT );

                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }();

            return filteredResolutions;
        }

    private:
        // SDL capture touchscreen events only in SDL_Window rectangle. We create SDL_Window on PS Vita only for this purpose.
        SDL_Window * _window{ nullptr };
        vita2d_texture * _texBuffer{ nullptr };
        uint8_t * _palettedTexturePointer{ nullptr };
        fheroes2::Rect _destRect;

        RenderEngine() = default;

        enum : int32_t
        {
            VITA_FULLSCREEN_WIDTH = 960,
            VITA_FULLSCREEN_HEIGHT = 544,
            VITA_ASPECT_CORRECTED_WIDTH = 848
        };

        void clear() override
        {
            if ( _window != nullptr ) {
                SDL_DestroyWindow( _window );
                _window = nullptr;
            }

            vita2d_fini();

            if ( _texBuffer != nullptr ) {
                vita2d_free_texture( _texBuffer );
                _texBuffer = nullptr;
            }
        }

        bool allocate( fheroes2::ResolutionInfo & resolutionInfo, bool isFullScreen ) override
        {
            clear();

            const std::vector<fheroes2::ResolutionInfo> resolutions = getAvailableResolutions();
            assert( !resolutions.empty() );
            if ( !resolutions.empty() ) {
                resolutionInfo = GetNearestResolution( resolutionInfo, resolutions );
            }

            vita2d_init();

            _window = SDL_CreateWindow( "", 0, 0, resolutionInfo.gameWidth, resolutionInfo.gameHeight, 0 );
            if ( _window == nullptr ) {
                ERROR_LOG( "Failed to create an application window of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                        << " size. The error: " << SDL_GetError() )

                clear();
                return false;
            }

            vita2d_texture_set_alloc_memblock_type( SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW );
            _texBuffer = vita2d_create_empty_texture_format( resolutionInfo.gameWidth, resolutionInfo.gameHeight, SCE_GXM_TEXTURE_FORMAT_P8_ABGR );
            _palettedTexturePointer = static_cast<uint8_t *>( vita2d_texture_get_datap( _texBuffer ) );
            memset( _palettedTexturePointer, 0, resolutionInfo.gameWidth * resolutionInfo.gameHeight * sizeof( uint8_t ) );
            _createPalette();

            _calculateScreenScaling( resolutionInfo.gameWidth, resolutionInfo.gameHeight, isFullScreen );

            return true;
        }

        void render( const fheroes2::Display & display, const fheroes2::Rect & roi ) override
        {
            (void)roi;

            if ( _texBuffer == nullptr )
                return;

            const int32_t width = display.width();
            const int32_t height = display.height();

            memcpy( _palettedTexturePointer, display.image(), width * height * sizeof( uint8_t ) );

            vita2d_start_drawing();
            vita2d_draw_rectangle( 0, 0, VITA_FULLSCREEN_WIDTH, VITA_FULLSCREEN_HEIGHT, 0xff000000 );
            vita2d_draw_texture_scale( _texBuffer, _destRect.x, _destRect.y, static_cast<float>( _destRect.width ) / width,
                                       static_cast<float>( _destRect.height ) / height );
            vita2d_end_drawing();
            vita2d_swap_buffers();
        }

        void updatePalette( const std::vector<uint8_t> & colorIds ) override
        {
            if ( colorIds.size() != fheroes2::paletteSize || _texBuffer == nullptr )
                return;

            auto * palette32Bit = reinterpret_cast<uint32_t *>( vita2d_texture_get_palette( _texBuffer ) );

            for ( size_t i = 0; i < fheroes2::paletteSize; ++i ) {
                palette32Bit[i] = currentRGBPalette[colorIds[i]].getRGBA();
            }
        }

        bool isMouseCursorActive() const override
        {
            return true;
        }

        void _createPalette()
        {
            updatePalette( StandardPaletteIndexes() );
        }

        void _calculateScreenScaling( const int32_t width_, const int32_t height_, const bool isFullScreen )
        {
            _destRect.x = 0;
            _destRect.y = 0;
            _destRect.width = width_;
            _destRect.height = height_;

            if ( width_ == VITA_FULLSCREEN_WIDTH && height_ == VITA_FULLSCREEN_HEIGHT ) {
                // Nothing to do more.
                return;
            }

            if ( !isFullScreen ) {
                // Center game area.
                _destRect.x = ( VITA_FULLSCREEN_WIDTH - width_ ) / 2;
                _destRect.y = ( VITA_FULLSCREEN_HEIGHT - height_ ) / 2;
                return;
            }

            const SceGxmTextureFilter textureFilter = isNearestScaling() ? SCE_GXM_TEXTURE_FILTER_POINT : SCE_GXM_TEXTURE_FILTER_LINEAR;

            vita2d_texture_set_filters( _texBuffer, textureFilter, textureFilter );
            if ( ( static_cast<float>( VITA_FULLSCREEN_WIDTH ) / VITA_FULLSCREEN_HEIGHT ) >= ( static_cast<float>( width_ ) / height_ ) ) {
                const float scale = static_cast<float>( VITA_FULLSCREEN_HEIGHT ) / height_;
                _destRect.width = static_cast<int32_t>( static_cast<float>( width_ ) * scale );
                _destRect.height = VITA_FULLSCREEN_HEIGHT;
                _destRect.x = ( VITA_FULLSCREEN_WIDTH - _destRect.width ) / 2;
            }
            else {
                const float scale = static_cast<float>( VITA_FULLSCREEN_WIDTH ) / width_;
                _destRect.width = VITA_FULLSCREEN_WIDTH;
                _destRect.height = static_cast<int32_t>( static_cast<float>( height_ ) * scale );
                _destRect.y = ( VITA_FULLSCREEN_HEIGHT - _destRect.height ) / 2;
            }
        }
    };
#else
    bool shouldUseFullscreenDesktopMode()
    {
        return fheroes2::cursor().isSoftwareEmulation();
    }

    std::optional<bool> isWindowInAnyDisplay( const fheroes2::ResolutionInfo & resolutionInfo, const fheroes2::Point & windowPos )
    {
        const int numDisplays = SDL_GetNumVideoDisplays();
        if ( numDisplays < 1 ) {
            ERROR_LOG( "Failed to get the number of video displays. The error value: " << numDisplays << ", description: " << SDL_GetError() )
            return std::nullopt;
        }

        for ( int displayIndex = 0; displayIndex < numDisplays; ++displayIndex ) {
            SDL_Rect displayBounds;
            if ( SDL_GetDisplayBounds( displayIndex, &displayBounds ) != 0 ) {
                ERROR_LOG( "Failed to get display bounds for display #" << displayIndex << ". The error: " << SDL_GetError() )
                continue;
            }
            const int displayMaxX = displayBounds.x + displayBounds.w;
            const int displayMaxY = displayBounds.y + displayBounds.h;
            const int32_t windowMaxX = windowPos.x + resolutionInfo.screenWidth;
            const int32_t windowMaxY = windowPos.y + resolutionInfo.screenHeight;
            if ( windowPos.x >= displayBounds.x && windowMaxX < displayMaxX && windowPos.y >= displayBounds.y && windowMaxY < displayMaxY ) {
                return true;
            }
        }
        return false;
    }

    class RenderEngine final : public fheroes2::BaseRenderEngine, public BaseSDLRenderer
    {
    public:
        RenderEngine( const RenderEngine & ) = delete;

        ~RenderEngine() override
        {
            clear();
        }

        RenderEngine & operator=( const RenderEngine & ) = delete;

        void toggleFullScreen() override
        {
            if ( _window == nullptr ) {
                BaseRenderEngine::toggleFullScreen();
                return;
            }

            uint32_t flags = SDL_GetWindowFlags( _window );
            if ( flags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP ) ) {
                flags &= ~( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP );
            }
            else {
#if defined( _WIN32 )
                // We force fullscreen at desktop resolution for nearest scaling to disable hardware scaling of game resolution by the monitor.
                if ( isNearestScaling() || fheroes2::cursor().isSoftwareEmulation() ) {
                    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                }
                else {
                    flags |= SDL_WINDOW_FULLSCREEN;
                }
#else
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif

                // Update the window position, in case we get queried for it
                SDL_GetWindowPosition( _window, &_prevWindowPos.x, &_prevWindowPos.y );

                // If the window size has been manually changed to one that does not match any of the resolutions supported by the display, then after switching
                // to full-screen mode, the in-game display area may not occupy the entire screen, and black bars will remain on the sides of the screen. In this
                // case, even if it is specified to use the SDL_WINDOW_FULLSCREEN, the SDL_WINDOW_FULLSCREEN_DESKTOP will still be used under the hood. To avoid
                // this, we need to remember the window size (to restore it later when turning off full-screen mode) and then set the window size to the size of
                // the in-game display area before switching to the full-screen mode.
                SDL_GetWindowSize( _window, &_windowedSize.width, &_windowedSize.height );

                if ( const fheroes2::Display & display = fheroes2::Display::instance(); display.width() != 0 && display.height() != 0 ) {
                    assert( display.screenSize().width >= display.width() && display.screenSize().height >= display.height() );

                    SDL_SetWindowSize( _window, display.screenSize().width, display.screenSize().height );
                }
            }

            if ( const int returnCode = SDL_SetWindowFullscreen( _window, flags ); returnCode < 0 ) {
                ERROR_LOG( "Failed to set fullscreen mode flags. The error value: " << returnCode << ", description: " << SDL_GetError() )
            }

            _syncFullScreen();

            // If the full-screen mode has been turned off, then restore the remembered window size.
            if ( !isFullScreen() && _windowedSize.width != 0 && _windowedSize.height != 0 ) {
                SDL_SetWindowSize( _window, _windowedSize.width, _windowedSize.height );
            }

            _retrieveWindowInfo();
            _toggleMouseCaptureMode();
        }

        bool isFullScreen() const override
        {
            if ( _window == nullptr ) {
                return BaseRenderEngine::isFullScreen();
            }

            const uint32_t flags = SDL_GetWindowFlags( _window );
            return ( flags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP ) );
        }

        std::vector<fheroes2::ResolutionInfo> getAvailableResolutions() const override
        {
            std::set<fheroes2::ResolutionInfo> resolutionSet;

            const int displayCount = SDL_GetNumVideoDisplays();
            if ( displayCount >= 1 ) {
                for ( int displayId = 0; displayId < displayCount; ++displayId ) {
                    const int displayModeCount = SDL_GetNumDisplayModes( displayId );
                    if ( displayModeCount >= 1 ) {
                        for ( int i = 0; i < displayModeCount; ++i ) {
                            SDL_DisplayMode videoMode;
                            const int returnCode = SDL_GetDisplayMode( displayId, i, &videoMode );
                            if ( returnCode != 0 ) {
                                ERROR_LOG( "Failed to get display mode on display " << displayId << ". The error value: " << returnCode
                                                                                    << ", description: " << SDL_GetError() )
                            }
                            else {
                                resolutionSet.emplace( videoMode.w, videoMode.h );
                            }
                        }
                    }
                    else {
                        ERROR_LOG( "Failed to get the number of display modes for display" << displayId << ". The error value: " << displayModeCount
                                                                                           << ", description: " << SDL_GetError() )
                    }
                }
            }
            else {
                ERROR_LOG( "Failed to get the number of displays. The error value: " << displayCount << ", description: " << SDL_GetError() )
            }

#if defined( TARGET_NINTENDO_SWITCH )
            // Nintendo Switch supports arbitrary resolutions via the HW scaler
            // 848x480 is the smallest resolution supported by fheroes2
            resolutionSet.emplace( 848, 480 );
#endif
            resolutionSet = FilterResolutions( resolutionSet );

            if ( displayCount < 1 ) {
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            if ( !shouldUseFullscreenDesktopMode() ) {
                // If software emulation is not enabled it means that we need to use resolutions supported
                // by the hardware.
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            // We should limit all available resolutions to the one which is currently chosen on the system
            // to avoid ending up having application window which is bigger than the screen resolution.
            SDL_DisplayMode maxDisplayMode{};

            for ( int displayId = 0; displayId < displayCount; ++displayId ) {
                SDL_DisplayMode displayMode;

                const int returnValue = SDL_GetCurrentDisplayMode( displayId, &displayMode );
                if ( returnValue < 0 ) {
                    ERROR_LOG( "Failed to retrieve the current display mode for display " << displayId << ". The error value: " << returnValue
                                                                                          << ", description: " << SDL_GetError() )
                    continue;
                }

                // There is no an ideal formula of how to choose the biggest resolution among multiple displays
                // so let's use a simple and well-known approach.
                maxDisplayMode.w = std::max( maxDisplayMode.w, displayMode.w );
                maxDisplayMode.h = std::max( maxDisplayMode.h, displayMode.h );
            }

            if ( maxDisplayMode.w == 0 || maxDisplayMode.h == 0 ) {
                // None of displays returned a value display mode.
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            // If the current display resolution is somehow very small, ignore it.
            // It could happen when a device has smaller than the required by the engine resolution.
            if ( maxDisplayMode.w < fheroes2::Display::DEFAULT_WIDTH || maxDisplayMode.h < fheroes2::Display::DEFAULT_HEIGHT ) {
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            std::vector<fheroes2::ResolutionInfo> temp;
            for ( auto iter = resolutionSet.rbegin(); iter != resolutionSet.rend(); ++iter ) {
                if ( iter->screenWidth <= maxDisplayMode.w && iter->screenHeight <= maxDisplayMode.h ) {
                    temp.emplace_back( *iter );
                }
            }

            return temp;
        }

        void setTitle( const std::string & title ) override
        {
            if ( _window == nullptr ) {
                return;
            }

            SDL_SetWindowTitle( _window, System::encLocalToUTF8( title ).c_str() );
        }

        void setIcon( const fheroes2::Image & icon ) override
        {
            if ( _window == nullptr )
                return;

            SDL_Surface * surface = generateIconSurface( icon );
            if ( surface == nullptr )
                return;

            SDL_SetWindowIcon( _window, surface );

            SDL_FreeSurface( surface );
        }

        fheroes2::Rect getActiveWindowROI() const override
        {
            return _activeWindowROI;
        }

        fheroes2::Size getCurrentScreenResolution() const override
        {
            return _currentScreenResolution;
        }

        static RenderEngine * create()
        {
            return new RenderEngine;
        }

        void setVSync( const bool enable ) override
        {
            _isVSyncEnabled = enable;

            if ( _renderer == nullptr ) {
                return;
            }

            _toggleVSync();
        }

        fheroes2::Point getWindowPos() const override
        {
            if ( isFullScreen() ) {
                return _prevWindowPos;
            }
            if ( _window == nullptr ) {
                return { -1, -1 };
            }
            fheroes2::Point result;
            SDL_GetWindowPosition( _window, &result.x, &result.y );
            return result;
        }

        void setWindowPos( const fheroes2::Point pos ) override
        {
            if ( pos == fheroes2::Point{ -1, -1 } ) {
                _prevWindowPos = { SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
            }
            else {
                _prevWindowPos = pos;
            }
        }

    private:
        SDL_Window * _window{ nullptr };
        SDL_Surface * _surface{ nullptr };
        SDL_Renderer * _renderer{ nullptr };
        SDL_Texture * _texture{ nullptr };
        int _driverIndex{ -1 };

        // H2-KOREAN: HD background output (see h2_hd_layer.h)
        SDL_Texture * _hdTexture{ nullptr };
        int32_t _hdScale{ 0 };
        std::vector<uint32_t> _hdPixels;
        uint32_t _hdGeneration{ 0 };
        bool _hdPaletteChanged{ true };
        std::array<uint32_t, 256> _hdPalette{};
        struct HdScaledLayer
        {
            const void * source{ nullptr };
            int32_t scale{ 0 };
            std::vector<uint32_t> pixels;
        };
        std::array<HdScaledLayer, fheroes2::HDLayer::slotCount> _hdScaled;

        std::string _previousWindowTitle;
        fheroes2::Point _prevWindowPos{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
        fheroes2::Size _currentScreenResolution;
        fheroes2::Rect _activeWindowROI;

        fheroes2::Size _windowedSize;

        bool _isVSyncEnabled{ false };

        RenderEngine() = default;

        void clear() override
        {
            if ( _hdTexture != nullptr ) {
                SDL_DestroyTexture( _hdTexture );
                _hdTexture = nullptr;
            }
            _hdScale = 0;
            _hdGeneration = 0;

            if ( _texture != nullptr ) {
                SDL_DestroyTexture( _texture );
                _texture = nullptr;
            }

            if ( _renderer != nullptr ) {
                SDL_DestroyRenderer( _renderer );
                _renderer = nullptr;
            }

            if ( _window != nullptr ) {
                // Let's collect needed info about previous setup
                if ( !isFullScreen() ) {
                    SDL_GetWindowPosition( _window, &_prevWindowPos.x, &_prevWindowPos.y );
                }
                _previousWindowTitle = System::encUTF8ToLocal( SDL_GetWindowTitle( _window ) );

                SDL_DestroyWindow( _window );
                _window = nullptr;
            }

            if ( _surface != nullptr ) {
                SDL_FreeSurface( _surface );
                _surface = nullptr;
            }

            _driverIndex = -1;
            _windowedSize = {};
        }

        void render( const fheroes2::Display & display, const fheroes2::Rect & roi ) override
        {
            if ( _surface == nullptr ) {
                return;
            }

            assert( _renderer != nullptr && _texture != nullptr );

            copyImageToSurface( display, _surface, roi );

            const bool fullFrame = ( roi.width == display.width() ) && ( roi.height == display.height() );
            if ( fullFrame ) {
                const int returnCode = SDL_UpdateTexture( _texture, nullptr, _surface->pixels, _surface->pitch );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to update texture. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }
            }
            else {
                SDL_Rect area;
                area.x = roi.x;
                area.y = roi.y;
                area.w = roi.width;
                area.h = roi.height;

                const int returnCode = SDL_UpdateTexture( _texture, &area, _surface->pixels, _surface->pitch );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to update texture. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }
            }

            // H2-KOREAN: with an HD background on the screen the frame is sent through the big texture
            SDL_Texture * outputTexture = _texture;
            if ( fheroes2::HDLayer::anyActive() && _prepareHdTexture( display ) ) {
                const bool fullHdFrame = _hdPaletteChanged || _hdGeneration != fheroes2::HDLayer::generation();
                _renderHd( display, fullHdFrame ? fheroes2::Rect( 0, 0, display.width(), display.height() ) : roi );
                _hdPaletteChanged = false;
                _hdGeneration = fheroes2::HDLayer::generation();
                outputTexture = _hdTexture;
            }
            else {
                _hdGeneration = 0;
            }

            // We should clear the renderer to avoid graphical artifacts with most window compositors (in example, Android; Linux with X11 or Wayland)
            // when the aspect ratio of the texture is different from that of the game window.
            int returnCode = SDL_RenderClear( _renderer );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to clear renderer. The error value: " << returnCode << ", description: " << SDL_GetError() )
                return;
            }

            returnCode = SDL_RenderCopy( _renderer, outputTexture, nullptr, nullptr );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to copy render.The error value: " << returnCode << ", description: " << SDL_GetError() )
                return;
            }

            SDL_RenderPresent( _renderer );
        }

        bool allocate( fheroes2::ResolutionInfo & resolutionInfo, bool isFullScreen ) override
        {
            clear();

            const std::vector<fheroes2::ResolutionInfo> resolutions = getAvailableResolutions();
            assert( !resolutions.empty() );
            if ( !resolutions.empty() ) {
                resolutionInfo = GetNearestResolution( resolutionInfo, resolutions );
            }

#if defined( ANDROID ) || defined( __IPHONEOS__ )
            // Same as ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            if ( SDL_SetHint( SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight" ) == SDL_FALSE ) {
                ERROR_LOG( "Failed to set the " SDL_HINT_ORIENTATIONS " hint." )
            }
#endif

            uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
            if ( isFullScreen ) {
#if defined( _WIN32 )
                // We force fullscreen at desktop resolution for nearest scaling to disable hardware scaling of game resolution by the monitor.
                if ( isNearestScaling() || fheroes2::cursor().isSoftwareEmulation() ) {
                    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                }
                else {
                    flags |= SDL_WINDOW_FULLSCREEN;
                }
#else
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
            }

            std::optional<bool> isInDisplay = isWindowInAnyDisplay( resolutionInfo, _prevWindowPos );

            // check if the previous window position is within display or error occured
            if ( !isInDisplay || !*isInDisplay ) {
                // reset position if it is not within the bounds of any display
                _prevWindowPos = { SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
            }

            _window = SDL_CreateWindow( System::encLocalToUTF8( _previousWindowTitle ).c_str(), _prevWindowPos.x, _prevWindowPos.y, resolutionInfo.screenWidth,
                                        resolutionInfo.screenHeight, flags );
            if ( _window == nullptr ) {
                ERROR_LOG( "Failed to create an application window of " << resolutionInfo.screenWidth << " x " << resolutionInfo.screenHeight
                                                                        << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            _syncFullScreen();

            bool isPaletteModeSupported = false;

            SDL_RendererInfo rendererInfo;
            _driverIndex = -1;

            if ( const int driverCount = SDL_GetNumRenderDrivers(); driverCount >= 0 ) {
                const uint32_t requiredFlags = SDL_RENDERER_ACCELERATED;

                for ( int driverId = 0; driverId < driverCount; ++driverId ) {
                    const int returnCode = SDL_GetRenderDriverInfo( driverId, &rendererInfo );
                    if ( returnCode < 0 ) {
                        ERROR_LOG( "Failed to get renderer driver info. The error value: " << returnCode << ", description: " << SDL_GetError() )
                        continue;
                    }

                    if ( ( rendererInfo.flags & requiredFlags ) != requiredFlags ) {
                        continue;
                    }

                    for ( uint32_t i = 0; i < rendererInfo.num_texture_formats; ++i ) {
                        if ( rendererInfo.texture_formats[i] == SDL_PIXELFORMAT_INDEX8 ) {
                            // Bingo! This is the best driver and format.
                            isPaletteModeSupported = true;
                            _driverIndex = driverId;
                            break;
                        }
                    }

                    if ( isPaletteModeSupported ) {
                        break;
                    }

                    if ( _driverIndex < 0 ) {
                        _driverIndex = driverId;
                    }
                }
            }
            else {
                ERROR_LOG( "Failed to get the number of render drivers. The error value: " << driverCount << ", description: " << SDL_GetError() )
            }

            _surface = SDL_CreateRGBSurface( 0, resolutionInfo.gameWidth, resolutionInfo.gameHeight, isPaletteModeSupported ? 8 : 32, 0, 0, 0, 0 );
            if ( _surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            if ( _surface->w <= 0 || _surface->h <= 0 || _surface->w != resolutionInfo.gameWidth || _surface->h != resolutionInfo.gameHeight ) {
                clear();
                return false;
            }

            _createPalette();

            _renderer = SDL_CreateRenderer( _window, _driverIndex, SDL_RENDERER_ACCELERATED );
            if ( _renderer == nullptr ) {
                ERROR_LOG( "Failed to create a window renderer of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                    << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            if ( const int returnCode = SDL_SetRenderDrawColor( _renderer, 0, 0, 0, SDL_ALPHA_OPAQUE ); returnCode < 0 ) {
                ERROR_LOG( "Failed to set default color for renderer. The error value: " << returnCode << ", description: " << SDL_GetError() )
            }

            if ( const int returnCode = SDL_SetRenderTarget( _renderer, nullptr ); returnCode < 0 ) {
                ERROR_LOG( "Failed to set render target to window. The error value: " << returnCode << ", description: " << SDL_GetError() )
            }

            if ( SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, ( isNearestScaling() ? "nearest" : "linear" ) ) == SDL_FALSE ) {
                ERROR_LOG( "Failed to set the " SDL_HINT_RENDER_SCALE_QUALITY " hint." )
            }

            // Setting this hint prevents the window to regain focus after losing it in fullscreen mode.
            // It also fixes issues when SDL_UpdateTexture() calls fail because of refocusing.
            if ( SDL_SetHint( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0" ) == SDL_FALSE ) {
                ERROR_LOG( "Failed to set the " SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS " hint." )
            }

            if ( const int returnCode = SDL_RenderSetLogicalSize( _renderer, resolutionInfo.gameWidth, resolutionInfo.gameHeight ); returnCode < 0 ) {
                ERROR_LOG( "Failed to create logical size of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                               << " size. The error value: " << returnCode << ", description: " << SDL_GetError() )
                clear();
                return false;
            }

            _texture = SDL_CreateTextureFromSurface( _renderer, _surface );
            if ( _texture == nullptr ) {
                ERROR_LOG( "Failed to create a texture from a surface of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                           << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            if ( !_retrieveWindowInfo() ) {
                clear();
                return false;
            }

            _toggleMouseCaptureMode();
            _toggleVSync();

            return true;
        }

        void updatePalette( const std::vector<uint8_t> & colorIds ) override
        {
            if ( _surface == nullptr || colorIds.size() != 256 )
                return;

            // H2-KOREAN: colours for the HD output texture (ARGB8888)
            for ( size_t i = 0; i < 256; ++i ) {
                const fheroes2::RGB & c = currentRGBPalette[colorIds[i]];
                _hdPalette[i] = 0xFF000000U | ( static_cast<uint32_t>( c.r ) << 16 ) | ( static_cast<uint32_t>( c.g ) << 8 ) | c.b;
            }
            _hdPaletteChanged = true;

            generatePalette( colorIds, _surface );
            if ( _surface->format->BitsPerPixel == 8 ) {
                const int returnCode = SDL_SetPaletteColors( _surface->format->palette, _palette8Bit.data(), 0, 256 );
                if ( returnCode < 0 ) {
                    ERROR_LOG( "Failed to set palette color. The error value: " << returnCode << ", description: " << SDL_GetError() )
                }
            }
        }

        bool isMouseCursorActive() const override
        {
            return ( _window != nullptr ) && ( ( SDL_GetWindowFlags( _window ) & SDL_WINDOW_MOUSE_FOCUS ) == SDL_WINDOW_MOUSE_FOCUS );
        }

        void _createPalette()
        {
            if ( _surface == nullptr )
                return;

            updatePalette( StandardPaletteIndexes() );

            if ( _surface->format->BitsPerPixel == 8 ) {
                if ( !SDL_MUSTLOCK( _surface ) ) {
                    // copy the image from display buffer to SDL surface
                    const fheroes2::Display & display = fheroes2::Display::instance();
                    if ( _surface->w == display.width() && _surface->h == display.height() ) {
                        memcpy( _surface->pixels, display.image(), static_cast<size_t>( display.width() * display.height() ) );
                    }

                    // Display class doesn't have support for image pitch so we mustn't link display to surface if width is not divisible by 4.
                    if ( _surface->w % 4 == 0 ) {
                        linkRenderSurface( static_cast<uint8_t *>( _surface->pixels ) );
                    }
                }
            }
        }

        // H2-KOREAN: makes the big output texture (game size x 2 or 3, by the window size). False: not worth it (window not bigger).
        bool _prepareHdTexture( const fheroes2::Display & display )
        {
            int outputWidth = 0;
            int outputHeight = 0;
            if ( SDL_GetRendererOutputSize( _renderer, &outputWidth, &outputHeight ) < 0 || display.height() <= 0 ) {
                return false;
            }
            const int32_t scale = std::min( fheroes2::HDLayer::storedScale, std::max( 1, outputHeight / display.height() ) );
            if ( scale < 2 ) {
                return false;
            }
            if ( _hdTexture == nullptr || scale != _hdScale ) {
                if ( _hdTexture != nullptr ) {
                    SDL_DestroyTexture( _hdTexture );
                }
                _hdTexture = SDL_CreateTexture( _renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, display.width() * scale, display.height() * scale );
                if ( _hdTexture == nullptr ) {
                    ERROR_LOG( "Failed to create the HD texture. The error: " << SDL_GetError() )
                    _hdScale = 0;
                    return false;
                }
                _hdScale = scale;
                _hdPixels.assign( static_cast<size_t>( display.width() * scale ) * static_cast<size_t>( display.height() * scale ), 0xFF000000U );
                _hdGeneration = 0;
            }
            return true;
        }

        const std::vector<uint32_t> & _hdLayerPixels( const int slot )
        {
            const fheroes2::HDLayer::Layer & layer = fheroes2::HDLayer::get( slot );
            HdScaledLayer & cache = _hdScaled[slot];
            if ( _hdScale == fheroes2::HDLayer::storedScale ) {
                return *layer.big;
            }
            if ( cache.source != layer.big.get() || cache.scale != _hdScale ) {
                // smaller copy by averaging (stored 3x -> 2x)
                const int32_t srcW = layer.rect.width * fheroes2::HDLayer::storedScale;
                const int32_t srcH = layer.rect.height * fheroes2::HDLayer::storedScale;
                const int32_t dstW = layer.rect.width * _hdScale;
                const int32_t dstH = layer.rect.height * _hdScale;
                cache.pixels.assign( static_cast<size_t>( dstW ) * dstH, 0 );
                const std::vector<uint32_t> & src = *layer.big;
                for ( int32_t y = 0; y < dstH; ++y ) {
                    const int32_t sy0 = y * srcH / dstH;
                    const int32_t sy1 = std::max( sy0 + 1, ( y + 1 ) * srcH / dstH );
                    for ( int32_t x = 0; x < dstW; ++x ) {
                        const int32_t sx0 = x * srcW / dstW;
                        const int32_t sx1 = std::max( sx0 + 1, ( x + 1 ) * srcW / dstW );
                        uint32_t r = 0;
                        uint32_t g = 0;
                        uint32_t b = 0;
                        uint32_t n = 0;
                        for ( int32_t sy = sy0; sy < sy1; ++sy ) {
                            for ( int32_t sx = sx0; sx < sx1; ++sx ) {
                                const uint32_t c = src[static_cast<size_t>( sy ) * srcW + sx];
                                r += ( c >> 16 ) & 0xFF;
                                g += ( c >> 8 ) & 0xFF;
                                b += c & 0xFF;
                                ++n;
                            }
                        }
                        cache.pixels[static_cast<size_t>( y ) * dstW + x] = ( ( r / n ) << 16 ) | ( ( g / n ) << 8 ) | ( b / n );
                    }
                }
                cache.source = layer.big.get();
                cache.scale = _hdScale;
            }
            return cache.pixels;
        }

        void _renderHd( const fheroes2::Display & display, const fheroes2::Rect & roi )
        {
            const int32_t width = display.width();
            const int32_t height = display.height();
            const int32_t k = _hdScale;
            const int32_t outWidth = width * k;
            const uint8_t * image = display.image();

            struct ActiveLayer
            {
                const fheroes2::HDLayer::Layer * layer;
                const std::vector<uint32_t> * pixels;
            };
            std::array<ActiveLayer, fheroes2::HDLayer::slotCount> active{};
            int activeCount = 0;
            for ( int slot = 0; slot < fheroes2::HDLayer::slotCount; ++slot ) {
                const fheroes2::HDLayer::Layer & layer = fheroes2::HDLayer::get( slot );
                if ( layer.big != nullptr && layer.rect.width > 0 ) {
                    active[activeCount++] = { &layer, &_hdLayerPixels( slot ) };
                }
            }

            // one pixel more around the changed area: with smooth scaling a pixel also mixes in its neighbours
            const int32_t x0 = std::max( 0, roi.x - 1 );
            const int32_t y0 = std::max( 0, roi.y - 1 );
            const int32_t x1 = std::min( width, roi.x + roi.width + 1 );
            const int32_t y1 = std::min( height, roi.y + roi.height + 1 );

            // Pixels that are not the HD background are enlarged like the rest of the game: smoothly (bilinear) with the "linear"
            // screen scaling type, as blocks with "nearest" (owner 02:18: HD on looked sharper than off)
            const bool smooth = !isNearestScaling();
            const uint32_t * palette = _hdPalette.data();
            const auto mix = []( const uint32_t a, const uint32_t b, const uint32_t c, const uint32_t d, const uint32_t wa, const uint32_t wb, const uint32_t wc,
                                 const uint32_t wd ) {
                // weights add up to 256
                const uint32_t r = ( ( ( a >> 16 ) & 0xFF ) * wa + ( ( b >> 16 ) & 0xFF ) * wb + ( ( c >> 16 ) & 0xFF ) * wc + ( ( d >> 16 ) & 0xFF ) * wd ) >> 8;
                const uint32_t g = ( ( ( a >> 8 ) & 0xFF ) * wa + ( ( b >> 8 ) & 0xFF ) * wb + ( ( c >> 8 ) & 0xFF ) * wc + ( ( d >> 8 ) & 0xFF ) * wd ) >> 8;
                const uint32_t bl = ( ( a & 0xFF ) * wa + ( b & 0xFF ) * wb + ( c & 0xFF ) * wc + ( d & 0xFF ) * wd ) >> 8;
                return 0xFF000000U | ( r << 16 ) | ( g << 8 ) | bl;
            };

            for ( int32_t y = y0; y < y1; ++y ) {
                for ( int32_t x = x0; x < x1; ++x ) {
                    const uint8_t index = image[y * width + x];
                    uint32_t * out = _hdPixels.data() + static_cast<size_t>( y * k ) * outWidth + x * k;
                    bool drawn = false;
                    // The colour-cycling colours (lava, water) keep moving: not replaced
                    if ( index < 214 || index > 241 ) {
                        for ( int i = 0; i < activeCount && !drawn; ++i ) {
                            const fheroes2::HDLayer::Layer & layer = *active[i].layer;
                            const fheroes2::Rect & r = layer.rect;
                            const int32_t lx = x - r.x;
                            const int32_t ly = y - r.y;
                            if ( lx < 0 || ly < 0 || lx >= r.width || ly >= r.height ) {
                                continue;
                            }
                            const uint8_t * low = layer.low.data();
                            if ( low[ly * r.width + lx] != index ) {
                                continue;
                            }
                            // the 4 neighbours inside the background must match too (no single matching pixels inside other pictures)
                            bool neighbours = true;
                            static constexpr int32_t dx[4] = { -1, 1, 0, 0 };
                            static constexpr int32_t dy[4] = { 0, 0, -1, 1 };
                            for ( int n = 0; n < 4 && neighbours; ++n ) {
                                const int32_t nx = lx + dx[n];
                                const int32_t ny = ly + dy[n];
                                if ( nx < 0 || ny < 0 || nx >= r.width || ny >= r.height || x + dx[n] < 0 || x + dx[n] >= width || y + dy[n] < 0
                                     || y + dy[n] >= height ) {
                                    continue;
                                }
                                neighbours = ( image[( y + dy[n] ) * width + x + dx[n]] == low[ny * r.width + nx] );
                            }
                            if ( !neighbours ) {
                                continue;
                            }
                            const std::vector<uint32_t> & big = *active[i].pixels;
                            const int32_t bigWidth = r.width * k;
                            const uint32_t * src = big.data() + static_cast<size_t>( ly * k ) * bigWidth + lx * k;
                            for ( int32_t sy = 0; sy < k; ++sy ) {
                                for ( int32_t sx = 0; sx < k; ++sx ) {
                                    out[sy * outWidth + sx] = 0xFF000000U | src[sy * bigWidth + sx];
                                }
                            }
                            drawn = true;
                        }
                    }
                    if ( !drawn && !smooth ) {
                        const uint32_t color = palette[index];
                        for ( int32_t sy = 0; sy < k; ++sy ) {
                            for ( int32_t sx = 0; sx < k; ++sx ) {
                                out[sy * outWidth + sx] = color;
                            }
                        }
                    }
                    else if ( !drawn ) {
                        for ( int32_t sy = 0; sy < k; ++sy ) {
                            // sample position in game pixels: y + (sy + 0.5) / k - 0.5, in 1/256 steps
                            const int32_t fy = y * 256 + ( ( 2 * sy + 1 ) * 128 ) / k - 128;
                            const int32_t yA = std::clamp( fy >> 8, 0, height - 1 );
                            const int32_t yB = std::min( yA + 1, height - 1 );
                            const uint32_t ty = static_cast<uint32_t>( fy < 0 ? 0 : fy & 0xFF );
                            for ( int32_t sx = 0; sx < k; ++sx ) {
                                const int32_t fx = x * 256 + ( ( 2 * sx + 1 ) * 128 ) / k - 128;
                                const int32_t xA = std::clamp( fx >> 8, 0, width - 1 );
                                const int32_t xB = std::min( xA + 1, width - 1 );
                                const uint32_t tx = static_cast<uint32_t>( fx < 0 ? 0 : fx & 0xFF );
                                const uint32_t c00 = palette[image[yA * width + xA]];
                                const uint32_t c10 = palette[image[yA * width + xB]];
                                const uint32_t c01 = palette[image[yB * width + xA]];
                                const uint32_t c11 = palette[image[yB * width + xB]];
                                const uint32_t w00 = ( 256 - tx ) * ( 256 - ty ) >> 8;
                                const uint32_t w10 = tx * ( 256 - ty ) >> 8;
                                const uint32_t w01 = ( 256 - tx ) * ty >> 8;
                                out[sy * outWidth + sx] = mix( c00, c10, c01, c11, w00, w10, w01, 256 - w00 - w10 - w01 );
                            }
                        }
                    }
                }
            }

            if ( x1 <= x0 || y1 <= y0 ) {
                return;
            }
            SDL_Rect area{ x0 * k, y0 * k, ( x1 - x0 ) * k, ( y1 - y0 ) * k };
            const int returnCode
                = SDL_UpdateTexture( _hdTexture, &area, _hdPixels.data() + static_cast<size_t>( y0 * k ) * outWidth + x0 * k, outWidth * static_cast<int>( sizeof( uint32_t ) ) );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to update the HD texture. The error: " << SDL_GetError() )
            }
        }

        bool _retrieveWindowInfo()
        {
            assert( _window != nullptr );

            const int displayIndex = SDL_GetWindowDisplayIndex( _window );
            if ( displayIndex < 0 ) {
                ERROR_LOG( "Failed to get window display index. The error value: " << displayIndex << ", description: " << SDL_GetError() )
                return false;
            }

            SDL_DisplayMode displayMode;

            const int returnCode = SDL_GetCurrentDisplayMode( displayIndex, &displayMode );
            if ( returnCode < 0 ) {
                ERROR_LOG( "Failed to retrieve current display mode. The error value: " << returnCode << ", description: " << SDL_GetError() )
                return false;
            }

            _currentScreenResolution.width = displayMode.w;
            _currentScreenResolution.height = displayMode.h;

#if defined( TARGET_NINTENDO_SWITCH )
            // On a Nintendo Switch the game is always fullscreen
            _activeWindowROI = { 0, 0, _currentScreenResolution.width, _currentScreenResolution.height };
#else
            SDL_GetWindowPosition( _window, &_activeWindowROI.x, &_activeWindowROI.y );
            SDL_GetWindowSize( _window, &_activeWindowROI.width, &_activeWindowROI.height );
#endif

            return true;
        }

        void _toggleVSync()
        {
            assert( _renderer != nullptr );

#if !SDL_VERSION_ATLEAST( 2, 0, 18 )
#error SDL_RenderSetVSync() is only supported since SDL 2.0.18
#endif

            if ( const int returnCode = SDL_RenderSetVSync( _renderer, _isVSyncEnabled ? SDL_ENABLE : SDL_DISABLE ); returnCode != 0 ) {
                ERROR_LOG( "Failed to " << ( _isVSyncEnabled ? "enable" : "disable" ) << " vsync mode for renderer. The error value: " << returnCode
                                        << ", description: " << SDL_GetError() )
            }
        }

        void _toggleMouseCaptureMode()
        {
            assert( _window != nullptr );

            // To properly support fullscreen mode on devices with multiple displays or devices with notch,
            // it is important to lock the mouse in the application window area.
            SDL_SetWindowGrab( _window, isFullScreen() ? SDL_TRUE : SDL_FALSE );
        }

        void _syncFullScreen()
        {
            if ( isFullScreen() == BaseRenderEngine::isFullScreen() ) {
                return;
            }

            BaseRenderEngine::toggleFullScreen();

            assert( isFullScreen() == BaseRenderEngine::isFullScreen() );
        }
    };
#endif
}

namespace fheroes2
{
    void BaseRenderEngine::linkRenderSurface( uint8_t * surface ) const
    {
        Display::instance().linkRenderSurface( surface );
    }

    Display::Display()
        : _engine( RenderEngine::create() )
        , _cursor( RenderCursor::create() )
    {
        _disableTransformLayer();
    }

    void Display::resize( int32_t width_, int32_t height_ )
    {
        assert( width_ == width() && height_ == height() );

#ifdef NDEBUG
        (void)width_;
        (void)height_;
#endif
    }

    void Display::setResolution( ResolutionInfo info )
    {
        if ( width() > 0 && height() > 0 && info.gameWidth == width() && info.gameHeight == height() && info.screenWidth == _screenSize.width
             && info.screenHeight == _screenSize.height ) // nothing to resize
            return;

        const bool isFullScreen = _engine->isFullScreen();

        // deallocate engine resources
        _engine->clear();

        _prevRoi = {};

        // allocate engine resources
        if ( !_engine->allocate( info, isFullScreen ) ) {
            clear();
        }

        Image::resize( info.gameWidth, info.gameHeight );
        Image::reset();

        _screenSize = { info.screenWidth, info.screenHeight };
    }

    void Display::resetRenderer()
    {
        const bool isFullScreen = _engine->isFullScreen();

        // deallocate engine resources
        _engine->clear();

        _prevRoi = {};

        ResolutionInfo res( width(), height(), _screenSize.width, _screenSize.height );

        // allocate engine resources
        if ( !_engine->allocate( res, isFullScreen ) ) {
            clear();
        }
    }

    void Display::setWindowPos( const Point point )
    {
        _engine->setWindowPos( point );
    }

    Display & Display::instance()
    {
        static Display display;
        return display;
    }

    void Display::render( const Rect & roi )
    {
        Rect temp( roi );
        if ( !getActiveArea( temp, width(), height() ) ) {
            return;
        }

        if ( _cursor->isVisible() && _cursor->isSoftwareEmulation() && !_cursor->_image.empty() ) {
            const Sprite & cursorImage = _cursor->_image;
            Rect cursorROI( cursorImage.x(), cursorImage.y(), cursorImage.width(), cursorImage.height() );

            if ( _cursor->_keepInScreenArea ) {
                cursorROI.x = std::clamp<int32_t>( cursorROI.x, 0, width() - cursorROI.width );
                cursorROI.y = std::clamp<int32_t>( cursorROI.y, 0, height() - cursorROI.height );
            }

            const Sprite backup = Crop( *this, cursorROI.x, cursorROI.y, cursorROI.width, cursorROI.height );
            Blit( cursorImage, 0, 0, *this, cursorROI.x, cursorROI.y, cursorROI.width, cursorROI.height );

            // ROI must include cursor's area as well, otherwise cursor won't be rendered.
            if ( !backup.empty() && getActiveArea( cursorROI, width(), height() ) ) {
                temp = getBoundaryRect( temp, cursorROI );
            }

            _renderFrame( temp );

            if ( _postprocessing ) {
                _postprocessing();
            }

            Copy( backup, 0, 0, *this, backup.x(), backup.y(), backup.width(), backup.height() );
        }
        else {
            _renderFrame( temp );

            if ( _postprocessing ) {
                _postprocessing();
            }
        }

        _prevRoi = temp;
    }

    void Display::updateNextRenderRoi( const Rect & roi )
    {
        _prevRoi = getBoundaryRect( _prevRoi, roi );
        getActiveArea( _prevRoi, width(), height() );
    }

    void Display::_renderFrame( const Rect & roi ) const
    {
        bool updateImage = true;
        if ( _preprocessing ) {
            std::vector<uint8_t> palette;
            if ( _preprocessing( palette ) ) {
                _engine->updatePalette( palette );
                // when we change a palette for 8-bit image we unwillingly call render so we don't need to re-render the same frame again
                updateImage = ( _renderSurface == nullptr );
                if ( updateImage ) {
                    // Pre-processing step is applied to the whole image so we forcefully render the full frame.
                    _engine->render( *this, { 0, 0, width(), height() } );
                    return;
                }
            }
        }

        if ( updateImage ) {
            // Make sure that we update the previously rendered area to avoid any ghost effect artefacts.
            _engine->render( *this, getBoundaryRect( roi, _prevRoi ) );
        }
    }

    uint8_t * Display::image()
    {
        return _renderSurface != nullptr ? _renderSurface : Image::image();
    }

    const uint8_t * Display::image() const
    {
        return _renderSurface != nullptr ? _renderSurface : Image::image();
    }

    void Display::release()
    {
        _preprocessing = nullptr;
        _postprocessing = nullptr;

        _cursor.reset();
        _engine->clear();
        _engine.reset();
        clear();

        _renderSurface = nullptr;
        _prevRoi = {};
    }

    void Display::changePalette( const RGB * palette, const bool forceDefaultPaletteUpdate ) const
    {
        if ( currentRGBPalette == palette || ( palette == nullptr && !forceDefaultPaletteUpdate && currentRGBPalette == RGBPalette() ) )
            return;

        currentRGBPalette = ( palette == nullptr ) ? RGBPalette( forceDefaultPaletteUpdate ) : palette;

        _engine->updatePalette( StandardPaletteIndexes() );
    }

    bool Cursor::isFocusActive()
    {
        return engine().isMouseCursorActive();
    }

    BaseRenderEngine & engine()
    {
        const fheroes2::Display & display = Display::instance();

        assert( display._engine );

        return *( display._engine );
    }

    Cursor & cursor()
    {
        const fheroes2::Display & display = Display::instance();

        assert( display._cursor );

        return *( display._cursor );
    }
}
