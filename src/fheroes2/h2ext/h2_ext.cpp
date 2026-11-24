// H2-EXT: see h2_ext.h.
#include "h2_ext.h"

#include <cstdio>
#include <fstream>

#include "game.h"
#include "h1_mode.h"
#include "settings.h"

namespace
{
    bool chosen = false;
}

namespace H2Ext
{
    bool isOn()
    {
        // The Battle Only mode always uses the extension rules (owner 2026-11-20 16:06 "늘 확장 규칙").
        const bool battleOnly = Settings::Get().IsGameType( Game::TYPE_BATTLEONLY );
        return ( chosen || battleOnly ) && !H1::isH1Mode() && !Settings::Get().isCampaignGameType();
    }

    bool isChosen()
    {
        return chosen;
    }

    void setChosen( const bool on )
    {
        chosen = on;
    }

    void saveSideState( const std::string & path )
    {
        if ( !isOn() ) {
            std::remove( path.c_str() );
            return;
        }
        std::ofstream out( path, std::ios::trunc );
        out << "h2ext 1\n";
    }

    void loadSideState( const std::string & path )
    {
        std::ifstream in( path );
        std::string tag;
        int version = 0;
        chosen = !H1::isH1Mode() && static_cast<bool>( in >> tag >> version ) && tag == "h2ext";
    }
}
