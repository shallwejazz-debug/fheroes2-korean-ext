// Heroes I castle screen (homm1-remaster project). See h1_castle.h and docs/analysis note v61.
#include "h1_heroes.h"
#include "h1_castle.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "castle.h"
#include "color.h"
#include "cursor.h"
#include "h1_armywin.h"
#include "h1_gfx.h"
#include "h1_heroscreen.h"
#include "h1_rules.h"
#include "h1_spells.h"
#include "h1_ui.h"
#include "h1_sound.h"
#include "heroes.h"
#include "image.h"
#include "heroes_recruits.h"
#include "h1_tavern.h"
#include "kingdom.h"
#include "localevent.h"
#include "maps.h"
#include "mp2.h"
#include "players.h"
#include "settings.h"
#include "maps_tiles.h"
#include "monster.h"
#include "race.h"
#include "resource.h"
#include "screen.h"
#include "world.h"

namespace
{
    // Table 0x48d428: drawing order of the buildings per race (0 knight, 1 sorceress, 2 barbarian, 3 warlock), 0xff ends.
    const int buildingOrder[5][14] = { { 5, 6, 8, 11, 7, 0, 1, 2, 10, 9, 3, 4, 12, -1 },
                                       { 5, 6, 12, 8, 0, 9, 10, 1, 2, 11, 3, 4, 7, -1 },
                                       { 13, 5, 6, 9, 7, 11, 0, 1, 2, 10, 8, 12, 3, 4 },
                                       { 5, 6, 12, 9, 0, 11, 10, 1, 2, 7, 3, 4, 8, -1 },
                                       // H1-INTENDED(D16) 5th race, necromancer (docs/EXTENSION_NECRO_v1.md; town look v3 = concept 1 layout, owner
                                       // 2026-11-18 16:55 / 17:08, tools/extension/make_ext_necro_town_v3.py): back to front as HoMM2 draws them
                                       { 6, 12, 9, 10, 8, 11, 0, 1, 5, 7, 2, 4, 3, -1 } };

    // Tables 0x491168 (buildings 0..4), 0x49117c (race prefix) and 0x491178 (suffix of buildings 5..13), joined with "%s%s".
    const char * const commonNames[5] = { "magegld", "thievesg", "tavern", "dock", "well" };
    // H1-INTENDED(D16): the necromancer's copies of the common buildings in its town colours (make_ext_necro_town_v2.py, h1/ext/).
    const char * const necroCommonNames[5] = { "necrmage", "necrthie", "necrtavn", "necrdock", "necrwell" };
    const char * const racePrefix[5] = { "farm", "frst", "plns", "mtn", "necr" }; // "necr": h1/ext/ (H1-INTENDED(D16))
    const char * const raceSuffix[9] = { "tent", "cast", "_d0", "_d1", "_d2", "_d3", "_d4", "_d5", "_e0" };

    struct Building
    {
        int number = 0; // 0..13
        std::string name;
        int animationFrames = 0; // .tod byte 0 (5 or 0)
        int step = 0; // object +1
        int32_t x = 0; // .tod click area
        int32_t y = 0;
        int32_t w = 0;
        int32_t h = 0;
        int id = -1; // .tod building number; -1 = no click area (always drawn)
        bool shown = true; // object +2
    };

    int u16( const std::vector<uint8_t> & b, const size_t offset )
    {
        return static_cast<int16_t>( b[offset] | ( b[offset + 1] << 8 ) );
    }

    // Castle +0x16 bit of a HoMM1 building number, from the engine castle [mapping of engine buildings: temporary].
    // Castle +0x18 (mage guild byte): 0 = first level ... 3 = fourth level; the engine counts levels 1..4 (note v68).
    int guildByte( const Castle & castle )
    {
        return std::max( 0, castle.GetLevelMageGuild() - 1 );
    }

    bool isBuilt( const Castle & castle, const int number )
    {
        switch ( number ) {
        case 0:
            return castle.GetLevelMageGuild() > 0;
        case 1:
            return castle.isBuild( BUILD_THIEVESGUILD );
        case 2:
            return castle.isBuild( BUILD_TAVERN );
        case 3:
            return castle.isBuild( BUILD_SHIPYARD );
        case 4:
            return castle.isBuild( BUILD_WELL );
        case 5:
            return !castle.isCastle();
        case 6:
            return castle.isCastle();
        default:
            break;
        }
        if ( number >= 7 && number <= 12 ) {
            return castle.isBuild( DWELLING_MONSTER1 << ( number - 7 ) );
        }
        return true;
    }

    void blit( fheroes2::Image & dst, const std::vector<fheroes2::Sprite> & icn, const int frame, const int32_t x, const int32_t y )
    {
        // Frames that the file does not have are skipped (plns_d4: animated in its .tod but one frame in the .icn).
        if ( frame >= 0 && frame < static_cast<int>( icn.size() ) ) {
            const fheroes2::Sprite & s = icn[frame];
            fheroes2::Blit( s, dst, x + s.x(), y + s.y() );
        }
    }

    // FUN_00438302: crest / portrait at (+5, +6) and the five army slots.
    void drawArmyStrip( fheroes2::Image & dst, const int32_t x, const int32_t y, const std::string & icn, const int frame, const Army * army,
                        const int selected )
    {
        blit( dst, H1::getIcn( icn ), frame, x + 5, y + 6 );
        const std::vector<fheroes2::Sprite> & strip = H1::getIcn( "strip.icn" );
        const std::vector<fheroes2::Sprite> & monsters = H1::getIcn( "monsters.icn" );
        for ( int k = 0; k < 5; ++k ) {
            const int32_t sx = x + k * 0x58 + 0x70;
            const Troop * troop = ( army != nullptr ) ? army->GetTroop( static_cast<size_t>( k ) ) : nullptr;
            const int creature = ( troop != nullptr && troop->isValid() ) ? H1::creatureFromMonster( troop->GetID() ) : -1;
            if ( creature < 0 ) {
                blit( dst, strip, 2, sx, y + 6 );
                continue;
            }
            blit( dst, strip, std::min( creature / 6, 4 ) + 3, sx, y + 6 ); // H1-INTENDED(D16): extension creatures on the neutral background
            blit( dst, monsters, creature, sx + 7, y + 0x13 );
            H1::UI::drawText( dst, std::to_string( troop->GetCount() ), "smalfont.fnt", sx, y + 0x56, 0x4d, 0xd, 1, 2 );
        }
        // Selected slot (+0x1b): strip.icn 1 over it.
        if ( army != nullptr && selected >= 0 ) {
            blit( dst, strip, 1, x + selected * 0x58 + 0x70, y + 6 );
        }
    }

    // Table 0x4911b8 (0x4911b1 + building number 7..12): creature of each dwelling, per race.
    const int dwellingCreature[5][6] = { { 0, 1, 2, 3, 4, 5 },     { 12, 13, 14, 15, 16, 17 }, { 6, 7, 8, 9, 10, 11 }, { 18, 19, 20, 21, 22, 23 },
                                         // H1-INTENDED(D16) necromancer: skeleton, zombie, mummy, vampire, power lich, bone dragon (owner 13:35)
                                         { 29, 36, 37, 38, 30, 39 } };

    // Engine building of a HoMM1 building number (0 = the next mage guild level).
    uint32_t engineBuildingOf( const Castle & castle, const int number )
    {
        switch ( number ) {
        case 0:
            return static_cast<uint32_t>( BUILD_MAGEGUILD1 ) << std::min( castle.GetLevelMageGuild(), 4 );
        case 1:
            return BUILD_THIEVESGUILD;
        case 2:
            return BUILD_TAVERN;
        case 3:
            return BUILD_SHIPYARD;
        case 4:
            return BUILD_WELL;
        case 6:
            return BUILD_CASTLE;
        default:
            break;
        }
        return static_cast<uint32_t>( DWELLING_MONSTER1 ) << ( number - 7 );
    }

    // Castle +0x16 bits as the original keeps them (bit = building number), from the engine castle.
    int builtMaskOf( const Castle & castle )
    {
        int mask = 0;
        for ( int number = 0; number < 14; ++number ) {
            if ( isBuilt( castle, number ) ) {
                mask |= 1 << number;
            }
        }
        return mask;
    }

    // FUN_00422bb1: cost (wood, mercury, ore, sulfur, crystal, gems, gold). Tables 0x4911d0 (guild), 0x491240, 0x491398.
    void buildCostOf( const Castle & castle, const int race, const int number, int cost[7] )
    {
        // FUN_00422e94: the guild row is 0 with no guild, else castle +0x18 + 1 (at most 3).
        H1::buildingCostRow( race, number, number == 0 ? ( castle.GetLevelMageGuild() == 0 ? 0 : std::min( guildByte( castle ) + 1, 3 ) ) : 0, cost );
    }

    // FUN_00422d50
    bool canBuildOf( const Castle & castle, const int race, const int number )
    {
        // Built in this castle today already (bit table +0x12981): the engine "allowed to build today" mode stands in.
        if ( !castle.Modes( Castle::ALLOW_TO_BUILD_TODAY ) ) {
            return false;
        }
        const int mask = builtMaskOf( castle );
        if ( number != 6 && ( mask & 0x40 ) == 0 ) {
            return false;
        }
        if ( number == 3 ) {
            // Dock: the tile at (x - 1, y + 1) of the castle must be water (tile < 0x14) [castle point: engine centre, temporary].
            const fheroes2::Point c = castle.GetCenter();
            return Maps::isValidAbsPoint( c.x - 1, c.y + 1 ) && world.getTile( Maps::GetIndexFromAbsPoint( c.x - 1, c.y + 1 ) ).isWater();
        }
        if ( number == 0 ) {
            return guildByte( castle ) < 3;
        }
        if ( number == 5 ) {
            return false;
        }
        if ( number < 7 ) {
            return true;
        }
        // Table 0x491880: buildings each dwelling needs.
        static const int needs[5][6] = { { 0, 0x80, 0x90, 0x84, 0x600, 0x600 },
                                         { 0, 0x84, 0x80, 0x201, 0x400, 0x800 },
                                         { 0, 0x80, 0x80, 0x80, 0x400, 0x800 },
                                         { 0, 0x80, 0x80, 0x100, 0x200, 0xc00 },
                                         // H1-INTENDED(D16) necromancer: the HoMM2 rules (castle_building_info.cpp getBuildingRequirement), as
                                         // the four HoMM1 rows equal the HoMM2 rows of their races [checked]: 2, 3 <- 1; 4 <- 3 + thieves' guild;
                                         // 5 <- 2 + mage guild; 6 <- 5
                                         { 0, 0x80, 0x80, 0x202, 0x101, 0x800 } };
        const int need = needs[race][number - 7];
        return ( mask & need ) == need;
    }


    // Creature names: H1::creatureNameOne (table 0x493068; H1-INTENDED(D16) 28..35 in extension games).

    std::string sformat( const char * fmt, const char * a, const char * b = "" )
    {
        char buffer[160];
        std::snprintf( buffer, sizeof( buffer ), fmt, a, b );
        return buffer;
    }
}

namespace H1
{
    // Recruit window (FUN_00467373 / FUN_00467449 object: recruit0/1.bin at (0xa0, 0x10), FUN_00466b14 / FUN_004669b0 / FUN_00466e67 /
    // FUN_00466f8e) for a creature with "available" waiting; the buyer's funds limit the count. Returns the count chosen with OK (0 none).
    int recruitWindow( const int creature, const int available, const PlayerColor buyer, const bool popup )
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        const CreatureRow & row = creatureRow( creature );
        // FUN_00422c6a: gold from the table; one extra resource for cyclops (crystal), phoenix (mercury), dragon (sulfur), genie (gems).
        int resource = -1;
        switch ( creature ) {
        case 11:
            resource = 4;
            break;
        case 17:
            resource = 1;
            break;
        case 23:
            resource = 3;
            break;
        case 27:
            resource = 5;
            break;
        default:
            break;
        }
        const int resourceCost = ( resource >= 0 ) ? 1 : 0;
        const Monster monster( row.monsterId );

        // recruit0.bin without an extra resource, recruit1.bin with one; at (0xa0, 0x10).
        // Right click on a dwelling (FUN_0046753d): the same picture with "recruiq0/1.bin" (no buttons), shown until release.
        UI::Layout layout = UI::loadLayout( popup ? ( resource < 0 ? "recruiq0.bin" : "recruiq1.bin" ) : ( resource < 0 ? "recruit0.bin" : "recruit1.bin" ) );
        auto setText = [&layout]( const int id, const std::string & text ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->text = text;
            }
        };
        auto setFrame = [&layout]( const int id, const int frame ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->frame = frame;
            }
        };
        std::string title = H1::creatureNameOne( creature );
        title[0] = static_cast<char>( title[0] - 0x20 );
        setText( 0x40, "Recruit " + title );
        setText( 0x49, std::to_string( row.gold ) );
        if ( resource >= 0 ) {
            setText( 0x4b, std::to_string( resourceCost ) );
        }
        setFrame( 0x42, creature );
        if ( resource >= 0 ) {
            setFrame( 0x4a, resource );
            setFrame( 0x4e, resource );
        }

        // Most that can be bought: gold / cost, extra resource / its cost, and what the dwelling has.
        const Funds & funds = world.GetKingdom( buyer ).GetFunds();
        int most = funds.gold / row.gold;
        if ( resource >= 0 ) {
            const int32_t have[6] = { funds.wood, funds.mercury, funds.ore, funds.sulfur, funds.crystal, funds.gems };
            most = std::min( most, have[resource] / resourceCost );
        }
        most = std::min( most, available );
        if ( available == 0 ) {
            // OK greyed and not clickable.
            if ( UI::Item * it = layout.find( 0x8002 ) ) {
                it->greyed = true;
                it->inactive = true;
            }
        }

        if ( popup ) {
            setText( 0x43, "Available: " + std::to_string( available ) );
            UI::runLayoutDialog( layout, 0xa0, 0x10, true );
            return 0;
        }
        int count = 0;
        int answer = 0;
        while ( true ) {
            // FUN_00466e67
            setText( 0x43, "Available: " + std::to_string( available ) );
            setText( 0x44, std::to_string( count ) );
            setText( 0x4d, std::to_string( count * row.gold ) );
            if ( resource >= 0 ) {
                setText( 0x4f, std::to_string( count * resourceCost ) );
            }
            layout.clickableIcons = { 0x42 };
            // Right click on the picture: creature window popup (the help callback shows it and returns no text).
            answer = UI::runLayoutDialog(
                layout, 0xa0, 0x10,
                [&row]( const int id ) -> const char * {
                    if ( id == 0x42 ) {
                        showCreatureWindow( row.monsterId, 0, nullptr, nullptr, true, true, nullptr, 0x20 );
                    }
                    return nullptr;
                },
                -1 );
            if ( answer == 0x44 ) {
                // Typed into the number field: atoi, then limited to 0..most (FUN_00466f8e).
                if ( UI::Item * it = layout.find( 0x44 ) ) {
                    UI::applyEditKey( *it );
                    count = std::clamp( std::atoi( it->text.c_str() ), 0, most );
                }
            }
            else if ( answer == 0x42 ) {
                // Creature picture: creature window at (0x77, 0x20) without Dismiss (count 0 -> no number), over the recruit window.
                UI::drawLayout( display, layout, ox + 0xa0, oy + 0x10, -1 );
                showCreatureWindow( row.monsterId, 0, nullptr, nullptr, true, false, nullptr, 0x20 );
            }
            else if ( answer == 0x45 ) {
                count = std::min( count + 1, most );
            }
            else if ( answer == 0x46 ) {
                count = std::max( count - 1, 0 );
            }
            else if ( answer == 0x47 ) {
                count = most;
            }
            else {
                break;
            }
        }
        if ( answer != 0x8002 ) {
            return 0;
        }
        return count;
    }

    // Price of one creature: gold and the extra resource (number, -1 none) (FUN_00422c6a).
    void creaturePrice( const int creature, int & gold, int & resource )
    {
        gold = creatureRow( creature ).gold;
        switch ( creature ) {
        case 11:
            resource = 4;
            break;
        case 17:
            resource = 1;
            break;
        case 23:
            resource = 3;
            break;
        case 27:
            resource = 5;
            break;
        default:
            resource = -1;
            break;
        }
    }
}

namespace H1
{
    // Thieves' guild window (thiefwin.bin at (0, 0), FUN_0044535b / FUN_00445647 / FUN_00445acb, note v69) for the viewer's kingdom.
    void showThievesGuildWindow( const PlayerColor viewer, const int guildsOverride, const char * title )
    {
        UI::Layout layout = UI::loadLayout( "thiefwin.bin" );
        // Texts from table 0x492650 key 0xf (the file says "Sulphur", the table "Sulfur").
        static const std::pair<int, const char *> texts[] = { { 0, title },
                                                             { 600, "First" },
                                                             { 601, "Second" },
                                                             { 602, "Third" },
                                                             { 603, "Fourth" },
                                                             { 604, "Number of Towns:" },
                                                             { 605, "Number of Castles:" },
                                                             { 606, "Number of Heroes:" },
                                                             { 607, "Gold in Treasury:" },
                                                             { 608, "Wood, Crystal & Ore:" },
                                                             { 609, "Gems, Sulfur & Mercury:" },
                                                             { 610, "Number Obelisks Found:" },
                                                             { 611, "Total Army Strength:" } };
        for ( const auto & [id, text] : texts ) {
            if ( UI::Item * it = layout.find( id ) ) {
                it->text = text;
            }
        }
        // Rows shown: by the viewing player's thieves' guilds (FUN_0041f247): 1 -> 3, 2 -> 5, 3 -> 7, 4 or more -> 8 (guildsOverride: the
        // seer's shrine passes 8, FUN_00401000 case 0x21).
        const Kingdom & own = world.GetKingdom( viewer );
        int guilds = guildsOverride;
        for ( const Castle * c : own.GetCastles() ) {
            if ( guildsOverride > 0 ) {
                break;
            }
            if ( c != nullptr && c->isBuild( BUILD_THIEVESGUILD ) ) {
                ++guilds;
            }
        }
        const int rows = guilds >= 4 ? 8 : ( guilds == 3 ? 7 : ( guilds == 2 ? 5 : 3 ) );

        // Players in their order; a player out of the game counts -1 (and is not drawn).
        std::vector<const Player *> players;
        for ( const Player * p : Settings::Get().GetPlayers().getVector() ) {
            if ( p != nullptr ) {
                players.push_back( p );
            }
        }
        auto armyValue = []( const Army & army ) {
            // FUN_0042e009 (without the battle part): table +0x02 (int) x count for each stack.
            int sum = 0;
            for ( size_t k = 0; k < 5; ++k ) {
                const Troop * t = army.GetTroop( k );
                const int c = ( t != nullptr && t->isValid() ) ? creatureFromMonster( t->GetID() ) : -1;
                if ( c >= 0 ) {
                    sum += creatureWorth( c ) * static_cast<int>( t->GetCount() );
                }
            }
            return sum;
        };
        auto valueOf = [&]( const Player * p, const int category ) -> int {
            const Kingdom & k = world.GetKingdom( p->GetColor() );
            if ( !k.isPlay() ) {
                return -1;
            }
            const Funds & f = k.GetFunds();
            switch ( category ) {
            case 0:
                return static_cast<int>( k.GetCountTown() );
            case 1:
                return static_cast<int>( k.GetCountCastle() );
            case 2:
                return static_cast<int>( k.GetHeroes().size() );
            case 3:
                return f.gold;
            case 4:
                return f.ore + f.crystal + f.wood;
            case 5:
                return f.gems + f.mercury + f.sulfur;
            case 6:
                // Player obelisk bits (player +0x9b, 0x30 bits) - the engine's visited obelisks stand in [temporary].
                return static_cast<int>( k.CountVisitedObjects( MP2::OBJ_OBELISK ) );
            default: {
                int sum = 0;
                for ( const Heroes * h : k.GetHeroes() ) {
                    sum += armyValue( h->GetArmy() );
                }
                // Castles of the player whose garrison has a stack (FUN_0043c38b).
                for ( const Castle * c : k.GetCastles() ) {
                    if ( c != nullptr && c->GetArmy().isValid() ) {
                        sum += armyValue( c->GetArmy() );
                    }
                }
                return sum;
            }
            }
        };
        int outCount = 0;
        for ( const Player * p : players ) {
            if ( !world.GetKingdom( p->GetColor() ).isPlay() ) {
                ++outCount;
            }
        }
        const int count = static_cast<int>( players.size() );
        for ( int row = 0; row < rows; ++row ) {
            std::vector<int> values;
            std::vector<int> order;
            for ( int i = 0; i < count; ++i ) {
                values.push_back( valueOf( players[i], row ) );
                order.push_back( i );
            }
            // FUN_00445acb: swap sort, largest first.
            for ( int a = 0; a + 1 < count; ++a ) {
                for ( int b = a + 1; b < count; ++b ) {
                    if ( values[a] < values[b] ) {
                        std::swap( values[a], values[b] );
                        std::swap( order[a], order[b] );
                    }
                }
            }
            // Up to 4 rank columns; equal values share a column, flags centred on 0x120 + column * 0x61.
            int column = 0;
            int first = 0;
            while ( column < 4 && count - outCount != first ) {
                int same = 1;
                int last = first;
                while ( last + 1 < count && values[last + 1] == values[last] ) {
                    ++last;
                    ++same;
                }
                for ( int i = first; i <= last; ++i ) {
                    UI::Item flag;
                    flag.type = UI::ITEM_ICON;
                    flag.x = ( i - first ) * 0x12 + ( column * 0x61 + 0x120 - ( same * 9 - 9 ) );
                    flag.y = row * 0x1a + 0x1b;
                    flag.w = 0x12;
                    flag.h = 0x16;
                    flag.name = "townwind.icn";
                    flag.frame = std::max( 0, Color::GetIndex( players[order[i]]->GetColor() ) ) + 0xe;
                    flag.flags = 0x10;
                    layout.items.push_back( flag );
                }
                ++column;
                first = last + 1;
            }
        }
        UI::runLayoutDialog( layout, 0, 0, false );
    }
}

namespace H1
{
    CastleScreenResult runCastleScreen( Castle & castle )
    {
        const int race = buildingRace( castle );
        const int colour = std::max( 0, Color::GetIndex( castle.GetColor() ) );
        Heroes * hero = castle.GetHero();

        UI::Layout town = UI::loadLayout( "townwind.bin" );
        if ( UI::Item * it = town.find( 37 ) ) {
            it->text = castle.GetName();
        }
        if ( UI::Item * it = town.find( 902 ) ) {
            it->text = "Town Screen";
        }
        // Items 0x24..0x25 (name plate and name) are drawn again over the buildings (FUN_00477be0( list, 0, 0x24, 0x25 )).
        UI::Layout namePlate;
        for ( const UI::Item & it : town.items ) {
            if ( it.id == 36 || it.id == 37 ) {
                namePlate.items.push_back( it );
            }
        }

        // bankbox.bin at (0x222, 0x100): resources 0..5 in ids 0x1e..0x23 ("%d"), gold in 0x24.
        UI::Layout bank = UI::loadLayout( "bankbox.bin" );
        // FUN_004386e8
        auto updateBank = [&bank, &castle]() {
            const Funds & funds = world.GetKingdom( castle.GetColor() ).GetFunds();
            const int32_t values[7] = { funds.wood, funds.mercury, funds.ore, funds.sulfur, funds.crystal, funds.gems, funds.gold };
            for ( int k = 0; k < 7; ++k ) {
                if ( UI::Item * it = bank.find( 0x1e + k ) ) {
                    it->text = std::to_string( values[k] );
                }
            }
        };
        updateBank();

        std::vector<Building> buildings;
        for ( const int number : buildingOrder[race] ) {
            if ( number < 0 ) {
                break;
            }
            Building b;
            b.number = number;
            b.name = ( number < 5 ) ? ( race == 4 ? necroCommonNames[number] : commonNames[number] ) : std::string( racePrefix[race] ) + raceSuffix[number - 5];
            const std::vector<uint8_t> & tod = getH1AggFile( b.name + ".tod" );
            if ( tod.size() >= 11 ) {
                b.animationFrames = tod[0];
                b.x = u16( tod, 1 );
                b.y = u16( tod, 3 );
                b.w = u16( tod, 5 );
                b.h = u16( tod, 7 );
                b.id = u16( tod, 9 );
            }
            // H1-INTENDED(D16): the necromancer guild (HoMM2 picture, make_ext_necro_town_v3.py) keeps the .tod area.
            if ( b.id == 0 && race != 4 ) {
                // Mage guild: click area from the guild level (castle +0x18): height level * 0x14 + 0x61, top 0x99 - height.
                b.h = guildByte( castle ) * 0x14 + 0x61;
                b.y = 0x99 - b.h;
            }
            if ( b.id != -1 && !isBuilt( castle, number ) ) {
                b.shown = false;
            }
            buildings.push_back( b );
        }

        const fheroes2::Image background = UI::readBmp( "townbkg" + std::to_string( race ) + ".bmp" );

        std::string crestIcn;
        {
            char name[16];
            // No hero: player colour + 0x10; else hero class + colour * 4.
            int crest = 0x10 + colour;
            if ( hero != nullptr ) {
                int cls = 0;
                switch ( hero->GetRace() ) {
                case Race::BARB:
                    cls = 1;
                    break;
                case Race::SORC:
                    cls = 2;
                    break;
                case Race::WRLK:
                case Race::NECR: // H1-INTENDED(D16): warlock crest for now
                    cls = 3;
                    break;
                default:
                    break;
                }
                crest = cls + colour * 4;
            }
            std::snprintf( name, sizeof( name ), "crst%04d.icn", crest );
            crestIcn = name;
        }
        std::string portraitIcn;
        if ( hero != nullptr ) {
            char name[16];
            std::snprintf( name, sizeof( name ), "port%04d.icn", std::max( 0, H1::heroNumberOfId( hero->getPortraitId() ) ) );
            portraitIcn = name;
        }

        fheroes2::Display & display = fheroes2::Display::instance();
        const int32_t ox = ( display.width() - 640 ) / 2;
        const int32_t oy = ( display.height() - 480 ) / 2;
        int pressed = -1;
        // Town manager state: selected strip / slot (+0x8f / +0x93), target (+0x95 / +0x99), action (+0xf1), hovered id (+0xef).
        int selStrip = -1;
        int selSlot = -1;
        int action = -1;
        int hoverId = -1;

        auto draw = [&]() {
            fheroes2::Image frame( 640, 480 );
            frame._disableTransformLayer();
            fheroes2::Fill( frame, 0, 0, 640, 480, 0 );
            UI::drawLayout( frame, town, 0, 0, -1 );
            // FUN_0044340d: background, then every shown building (FUN_00440091).
            fheroes2::Copy( background, 0, 0, frame, 0, 0, std::min( 640, background.width() ), std::min( 480, background.height() ) );
            const int guildLevel = guildByte( castle );
            for ( const Building & b : buildings ) {
                if ( !b.shown ) {
                    continue;
                }
                const std::vector<fheroes2::Sprite> & icn = getIcn( b.name + ".icn" );
                blit( frame, icn, 0, 0, 0 );
                if ( b.id == 0 ) {
                    for ( int k = 0; k < guildLevel; ++k ) {
                        blit( frame, icn, k * 2 + 2, 0, 0 );
                    }
                    blit( frame, icn, guildLevel * 2 + 1, 0, 0 );
                }
                if ( b.animationFrames != 0 ) {
                    blit( frame, icn, b.step + 1, 0, 0 );
                }
            }
            UI::drawLayout( frame, namePlate, 0, 0, -1 );
            // FUN_00437ed0 strips: (0, 0x100) garrison with the crest; (0, 0x163) hero army with the portrait, or strip.icn 8 and empty slots.
            drawArmyStrip( frame, 0, 0x100, crestIcn, 0, &castle.GetArmy(), selStrip == 0 ? selSlot : -1 );
            if ( hero != nullptr ) {
                drawArmyStrip( frame, 0, 0x163, portraitIcn, 0, &hero->GetArmy(), selStrip == 1 ? selSlot : -1 );
            }
            else {
                drawArmyStrip( frame, 0, 0x163, "strip.icn", 8, nullptr, -1 );
            }
            UI::drawLayout( frame, bank, 0x222, 0x100, pressed );
            fheroes2::Copy( frame, 0, 0, display, ox, oy, 640, 480 );
            display.render();
        };

        const CursorRestorer cursorRestorer( true, Cursor::POINTER );
        LocalEvent & le = LocalEvent::Get();
        const UI::Item * exitButton = bank.find( 0x8000 );
        const fheroes2::Image saved = display;

        auto armyOf = [&]( const int strip ) -> Army * {
            if ( strip == 0 ) {
                return &castle.GetArmy();
            }
            return ( strip == 1 && hero != nullptr ) ? &hero->GetArmy() : nullptr;
        };
        auto creatureAt = [&]( const int strip, const int slot ) {
            Army * army = armyOf( strip );
            const Troop * troop = ( army != nullptr && slot >= 0 && slot < 5 ) ? army->GetTroop( static_cast<size_t>( slot ) ) : nullptr;
            return ( troop != nullptr && troop->isValid() ) ? creatureFromMonster( troop->GetID() ) : -1;
        };
        auto troopAt = [&]( const int strip, const int slot ) { return armyOf( strip )->GetTroop( static_cast<size_t>( slot ) ); };
        auto filledSlots = [&]( const int strip ) {
            int n = 0;
            for ( int k = 0; k < 5; ++k ) {
                if ( creatureAt( strip, k ) >= 0 ) {
                    ++n;
                }
            }
            return n;
        };

        // Hit test: strips (their own windows, on top), Exit, then the building areas (later ones are on top).
        auto idAt = [&]( const int32_t x, const int32_t y ) {
            auto inside = [x, y]( const int32_t rx, const int32_t ry, const int32_t rw, const int32_t rh ) {
                return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
            };
            for ( int strip = 0; strip < 2; ++strip ) {
                const int32_t sy = strip == 0 ? 0x100 : 0x163;
                if ( armyOf( strip ) == nullptr ) {
                    continue;
                }
                if ( inside( 5, sy + 6, 0x65, 0x5d ) ) {
                    return strip == 0 ? 0x10 : 0x16;
                }
                for ( int k = 0; k < 5; ++k ) {
                    if ( inside( k * 0x58 + 0x70, sy + 6, 0x52, 0x5d ) ) {
                        return ( strip == 0 ? 0x11 : 0x17 ) + k;
                    }
                }
            }
            if ( exitButton != nullptr && inside( 0x222 + exitButton->x, 0x100 + exitButton->y, exitButton->w, exitButton->h ) ) {
                return 0x7800;
            }
            for ( auto it = buildings.rbegin(); it != buildings.rend(); ++it ) {
                if ( it->shown && it->id >= 0 && inside( it->x, it->y, it->w, it->h ) ) {
                    return it->id;
                }
            }
            return -1;
        };

        // FUN_00440bcc: status and action when a slot is chosen as the target while another one is selected.
        auto targetStatus = [&]( const int tStrip, const int tSlot, const bool shift ) {
            action = -1;
            const char * selName = H1::creatureNameOne( std::max( 0, creatureAt( selStrip, selSlot ) ) );
            // bVar2: the selected army is the hero's, it has one filled slot, and the target is the other strip.
            const bool lastHeroArmy = filledSlots( selStrip ) == 1 && selStrip == 1 && tStrip != selStrip;
            std::string text;
            if ( tStrip == selStrip ) {
                if ( tSlot == selSlot ) {
                    text = sformat( "View %s", selName );
                    action = 1;
                }
            }
            else {
                bool same = false;
                for ( int k = 0; k < 5; ++k ) {
                    if ( creatureAt( selStrip, selSlot ) == creatureAt( tStrip, k ) ) {
                        same = true;
                    }
                }
                if ( same ) {
                    if ( !shift ) {
                        if ( lastHeroArmy ) {
                            return std::string( "Cannot combine Hero's last army" );
                        }
                        text = sformat( "Combine %s armies", selName );
                        action = 2;
                    }
                    else {
                        text = sformat( "Redistribute %s army", selName );
                        action = 5;
                    }
                }
                else if ( shift && creatureAt( tStrip, tSlot ) < 0 ) {
                    text = sformat( "Redistribute %s army", selName );
                    action = 5;
                }
            }
            if ( action == -1 ) {
                if ( creatureAt( tStrip, tSlot ) < 0 ) {
                    if ( lastHeroArmy ) {
                        text = "Cannot move last army to garrison.";
                    }
                    else {
                        text = sformat( "Move %s", selName );
                        action = 3;
                    }
                }
                else {
                    text = sformat( "Exchange %s with %s", selName, H1::creatureNameOne( creatureAt( tStrip, tSlot ) ) );
                    action = 3;
                }
            }
            return text;
        };

        int tgtStrip = -1;
        int tgtSlot = -1;
        int hovStrip = -1;
        int hovSlot = -1;

        // FUN_00440f82: status line (item 902) for the item under the mouse.
        auto hover = [&]( const int id, const bool shift ) {
            action = -1;
            std::string text;
            if ( id == 0x7800 ) {
                text = "Exit town";
            }
            else if ( id >= 0 && id <= 6 ) {
                const char * const names[7] = { "Mage Guild", "Thieves' Guild", "Tavern", "Dock", "Well", "Tent", "Castle" };
                text = names[id];
            }
            else if ( id >= 7 && id <= 12 ) {
                text = sformat( "Recruit %s", H1::creatureNameOne( dwellingCreature[race][id - 7] ) );
            }
            else if ( id == 0x10 ) {
                text = "Kingdom Overview";
                action = 6;
            }
            else if ( id == 0x16 ) {
                text = "View Hero";
                action = 4;
            }
            else if ( ( id >= 0x11 && id <= 0x15 ) || ( id >= 0x17 && id <= 0x1b ) ) {
                const int strip = id >= 0x17 ? 1 : 0;
                const int slot = id - ( strip == 1 ? 0x17 : 0x11 );
                if ( selSlot == -1 ) {
                    hovStrip = strip;
                    hovSlot = slot;
                    if ( creatureAt( strip, slot ) < 0 ) {
                        text = "Empty";
                    }
                    else {
                        text = sformat( "Select %s", H1::creatureNameOne( creatureAt( strip, slot ) ) );
                        action = 0;
                    }
                }
                else {
                    tgtStrip = strip;
                    tgtSlot = slot;
                    text = targetStatus( strip, slot, shift );
                }
            }
            if ( UI::Item * it = town.find( 902 ) ) {
                it->text = text;
            }
        };

        auto deselect = [&]() {
            // FUN_004432cd
            selStrip = -1;
            selSlot = -1;
            tgtStrip = -1;
            tgtSlot = -1;
        };

        // splitwin.bin (FUN_00442e81 / FUN_00446f16): how many to move; 0 .. count - 1; OK with 0 counts as Cancel.
        auto redistribute = [&]() {
            UI::Layout split = UI::loadLayout( "splitwin.bin" );
            Troop * src = troopAt( selStrip, selSlot );
            const int total = static_cast<int>( src->GetCount() );
            int amount = 0;
            if ( UI::Item * it = split.find( 1 ) ) {
                char buffer[160];
                std::snprintf( buffer, sizeof( buffer ), "Move how many %s troops from %s to %s?", H1::creatureNameOne( creatureAt( selStrip, selSlot ) ),
                               selStrip == 1 ? "Hero's Army" : "Garrison", tgtStrip == 1 ? "Hero's Army" : "Garrison" );
                it->text = buffer;
            }
            int answer = 0;
            while ( true ) {
                if ( UI::Item * it = split.find( 0x44 ) ) {
                    it->text = std::to_string( amount );
                }
                answer = UI::runLayoutDialog( split, 0xb1, 0x14, false );
                if ( answer == 0x44 ) {
                    // FUN_00446f16: typed number read with atoi, limited to 0 .. count - 1.
                    if ( UI::Item * it = split.find( 0x44 ) ) {
                        UI::applyEditKey( *it );
                        amount = std::clamp( std::atoi( it->text.c_str() ), 0, total - 1 );
                    }
                }
                else if ( answer == 0x45 ) {
                    amount = std::min( amount + 1, total - 1 );
                }
                else if ( answer == 0x46 ) {
                    amount = std::max( amount - 1, 0 );
                }
                else {
                    break;
                }
            }
            if ( answer != 0x8002 || amount == 0 ) {
                return;
            }
            Army * target = armyOf( tgtStrip );
            int same = -1;
            for ( int k = 0; k < 5; ++k ) {
                if ( creatureAt( selStrip, selSlot ) == creatureAt( tgtStrip, k ) ) {
                    same = k;
                    break;
                }
            }
            if ( same >= 0 ) {
                Troop * t = target->GetTroop( static_cast<size_t>( same ) );
                t->SetCount( t->GetCount() + static_cast<uint32_t>( amount ) );
            }
            else {
                target->GetTroop( static_cast<size_t>( tgtSlot ) )->Set( Monster( src->GetID() ), static_cast<uint32_t>( amount ) );
            }
            src->SetCount( static_cast<uint32_t>( total - amount ) );
        };

        // FUN_0044277d: the action of the last status.
        auto doAction = [&]() {
            switch ( action ) {
            case 0:
                selStrip = hovStrip;
                selSlot = hovSlot;
                break;
            case 1: {
                const bool hideDismiss = selStrip == 1 && filledSlots( 1 ) == 1;
                Troop * t = troopAt( selStrip, selSlot );
                showCreatureWindow( t->GetID(), static_cast<int>( t->GetCount() ), selStrip == 1 ? hero : nullptr, armyOf( selStrip ), hideDismiss, false,
                                    &castle );
                deselect();
                break;
            }
            case 2: {
                Troop * src = troopAt( selStrip, selSlot );
                for ( int k = 0; k < 5; ++k ) {
                    if ( creatureAt( tgtStrip, k ) == creatureAt( selStrip, selSlot ) ) {
                        Troop * t = troopAt( tgtStrip, k );
                        t->SetCount( t->GetCount() + src->GetCount() );
                        break;
                    }
                }
                src->Reset();
                deselect();
                break;
            }
            case 3: {
                Troop * a = troopAt( selStrip, selSlot );
                Troop * b = troopAt( tgtStrip, tgtSlot );
                Army::SwapTroops( *a, *b );
                if ( selStrip != tgtStrip ) {
                    // After an exchange between the two armies, a stack that now has a twin in its army joins it.
                    for ( int k = 0; k < 5; ++k ) {
                        if ( k != tgtSlot && creatureAt( tgtStrip, tgtSlot ) >= 0 && creatureAt( tgtStrip, k ) == creatureAt( tgtStrip, tgtSlot ) ) {
                            Troop * t = troopAt( tgtStrip, k );
                            t->SetCount( t->GetCount() + b->GetCount() );
                            b->Reset();
                        }
                        if ( k != selSlot && creatureAt( selStrip, selSlot ) >= 0 && creatureAt( selStrip, k ) == creatureAt( selStrip, selSlot ) ) {
                            Troop * t = troopAt( selStrip, k );
                            t->SetCount( t->GetCount() + a->GetCount() );
                            a->Reset();
                        }
                    }
                }
                deselect();
                break;
            }
            case 4:
                if ( hero != nullptr ) {
                    // FUN_00447aad( hero, 1 ) [meaning of the 1: unconfirmed; the engine "no dismiss" flag stands in, temporary].
                    runHeroScreen( *hero, false, true );
                }
                break;
            case 5:
                redistribute();
                deselect();
                break;
            case 6:
                // Kingdom overview (FUN_004186f2): the engine window stands in [temporary].
                world.GetKingdom( castle.GetColor() ).openOverviewDialog();
                break;
            default:
                break;
            }
            hoverId = -1;
        };

        // Dwelling: recruit window (FUN_00467449 object, FUN_00466b14 / FUN_004669b0 / FUN_00466e67 / FUN_00466f8e / FUN_00466d96).
        auto recruit = [&]( const int dwelling, const bool popup = false ) {
            const int creature = dwellingCreature[race][dwelling];
            const CreatureRow & row = creatureRow( creature );
            const Monster monster( row.monsterId );
            const int available = static_cast<int>( castle.getMonstersInDwelling( DWELLING_MONSTER1 << dwelling ) );
            const int count = H1::recruitWindow( creature, available, castle.GetColor(), popup );
            if ( popup || count == 0 ) {
                return;
            }
            // FUN_00467a4c: a garrison slot with this creature, else an empty one; none -> message after the window.
            bool room = false;
            Army & garrison = castle.GetArmy();
            for ( size_t k = 0; k < 5; ++k ) {
                const Troop * t = garrison.GetTroop( k );
                if ( t != nullptr && ( !t->isValid() || t->GetID() == row.monsterId ) ) {
                    room = true;
                }
            }
            if ( !room ) {
                UI::messageBox( "There is no room in the garrison for this army.", UI::MESSAGE_OKAY, 0xb1, 100 );
                return;
            }
            // FUN_00467af9 (same creature first, else the first empty slot), payment, dwelling count: the engine call does the same steps.
            castle.RecruitMonster( Troop( monster, static_cast<uint32_t>( count ) ), false );
        };

        // Well: wellwind.bin at (0, 0) (FUN_00444c57), closed by its button (FUN_00424203).
        auto showWell = [&]() {
            // Table 0x4931e0: dwelling names per race (FUN_00422b6a).
            static const char * const dwellingNames[5][6] = { { "Thatched Hut", "Archery Range", "Blacksmith", "Armory", "Jousting Arena", "Cathedral" },
                                                              { "Treehouse", "Cottage", "Archery Range", "Stonehenge", "Fenced Meadow", "Red Tower" },
                                                              { "Hut", "Stick Hut", "Den", "Adobe", "Bridge", "Pyramid" },
                                                              { "Cave", "Crypt", "Nest", "Maze", "Swamp", "Black Tower" },
                                                              { "Excavation", "Graveyard", "Pyramid", "Mansion", "Mausoleum", "Laboratory" } };
            UI::Layout layout = UI::loadLayout( "wellwind.bin" );
            for ( int k = 0; k < 6; ++k ) {
                const int creature = dwellingCreature[race][k];
                if ( UI::Item * it = layout.find( k + 1 ) ) {
                    it->frame = ( race + 1 ) * 7 + k + 1;
                }
                if ( UI::Item * it = layout.find( k + 0xd ) ) {
                    it->frame = creature;
                }
                if ( UI::Item * it = layout.find( k + 7 ) ) {
                    it->text = dwellingNames[race][k];
                }
                if ( UI::Item * it = layout.find( k + 0x13 ) ) {
                    std::string name = H1::creatureNameOne( creature );
                    name[0] = static_cast<char>( name[0] - 0x20 );
                    it->text = name;
                }
                if ( UI::Item * it = layout.find( k + 0x19 ) ) {
                    if ( !isBuilt( castle, k + 7 ) ) {
                        it->text = "Available:\nNONE\nGrowth Rate:\nN/A";
                    }
                    else {
                        // Growth shown = table growth (+7) + 2 (the well's bonus, always added here).
                        char buffer[96];
                        std::snprintf( buffer, sizeof( buffer ), "Available:\n%d\nGrowth Rate:\n%d/week",
                                       static_cast<int>( castle.getMonstersInDwelling( DWELLING_MONSTER1 << k ) ), creatureRow( creature ).growth + 2 );
                        it->text = buffer;
                    }
                }
            }
            UI::runLayoutDialog( layout, 0, 0, false );
        };

        // Tavern: tavwin.bin at (0xa2, 10) (FUN_004466ba); texts from table 0x492650 key 0xe (the file has the same words).
        // FUN_00446565: every 6 ticks (ms) the picture (item 2) shows frame (counter & 7) + 1. The tavern music (0x2f) is not played.
        auto showTavern = [&]() {
            UI::Layout layout = UI::loadLayout( "tavwin.bin" );
            if ( UI::Item * it = layout.find( 0 ) ) {
                it->text = "Tavern";
            }
            if ( UI::Item * it = layout.find( 601 ) ) {
                it->text = "The tavern increases the morale of all garrisoned troops.";
            }
            using TavernClock = std::chrono::steady_clock;
            TavernClock::time_point due = TavernClock::now();
            int counter = 0;
            auto tick = [&]() {
                if ( TavernClock::now() <= due ) {
                    return false;
                }
                ++counter;
                if ( UI::Item * it = layout.find( 2 ) ) {
                    it->frame = ( counter & 7 ) + 1;
                }
                due = TavernClock::now() + std::chrono::milliseconds( 6 );
                return true;
            };
            UI::runLayoutDialogTicking( layout, 0xa2, 10, tick );
        };

        // ---- Mage guild (building 0): FUN_0044145e case 0, magewind.bin (FUN_00444ea4 / FUN_004451d5), buybook.bin (note v68) ----
        auto showMageGuild = [&]() {
            if ( hero != nullptr && !hero->HaveSpellBook() ) {
                // A hero without a book: buy one first. FUN_00449117 == 14 (all artifact slots used) -> message instead.
                if ( hero->GetBagArtifacts().isFull() ) {
                    UI::messageBox( "You must purchase a spell book to use the mage guild, but you currently have no room for a spell book.  Try giving "
                                    "one of your artifacts to another hero.",
                                    UI::MESSAGE_OKAY );
                    return;
                }
                UI::Layout book = UI::loadLayout( "buybook.bin" );
                if ( UI::Item * it = book.find( 0 ) ) {
                    it->text = "Buy Spellbook:"; // table 0x492650 key 0
                }
                if ( UI::Item * it = book.find( 1 ) ) {
                    it->text = "Resource cost:";
                }
                // Gold of the hero's owner < 500: OK greyed (state 8) and not active.
                if ( world.GetKingdom( hero->GetColor() ).GetFunds().gold < 500 ) {
                    if ( UI::Item * it = book.find( 0x8002 ) ) {
                        it->greyed = true;
                        it->inactive = true;
                    }
                }
                if ( UI::runLayoutDialog( book, 0xb1, 0x14, false ) == 0x8002 && hero->SpellBookActivate() ) {
                    // FUN_004036bd( hero, 0x25 ): the book; gold - 500; then FUN_0043c3e0 (spells learned).
                    Funds pay;
                    pay.gold = 500;
                    world.GetKingdom( castle.GetColor() ).OddFundsResource( pay );
                    updateBank();
                    learnGuildSpells( castle, *hero );
                }
                return;
            }
            UI::Layout layout = UI::loadLayout( "magewind.bin" );
            if ( hero == nullptr ) {
                if ( UI::Item * it = layout.find( 0x50 ) ) {
                    it->text = "The above spells are available here.";
                }
            }
            else if ( UI::Item * it = layout.find( 0x50 ) ) {
                it->text = "The above spells have been added to your book."; // table 0x492650 key 6
            }
            const int level = guildByte( castle );
            const std::array<int, 9> & spells = guildSpells( castle );
            auto shown = [level]( const int slot ) {
                // Slots 0..2 need byte >= 0, 3..4 >= 1, 5..6 >= 2, 7..8 >= 3.
                const int need = slot < 3 ? 0 : ( slot < 5 ? 1 : ( slot < 7 ? 2 : 3 ) );
                return level >= need;
            };
            for ( int k = 0; k < 9; ++k ) {
                const bool open = shown( k );
                if ( UI::Item * it = layout.find( k + 1 ) ) {
                    it->frame = open ? 0 : 1; // townwind.icn: scroll / empty space
                }
                if ( UI::Item * it = layout.find( k + 10 ) ) {
                    it->hidden = !open;
                    it->frame = spells[k];
                }
                if ( UI::Item * it = layout.find( k + 0x13 ) ) {
                    it->hidden = !open;
                    it->text = spellName( spells[k] );
                }
            }
            // Guild picture: items 0x1d..0x23 hidden, then floors 0x1e + 2k (k < byte) and roof 0x1d + byte * 2.
            for ( int id = 0x1d; id <= 0x23; ++id ) {
                if ( UI::Item * it = layout.find( id ) ) {
                    it->hidden = true;
                }
            }
            for ( int k = 0; k < level; ++k ) {
                if ( UI::Item * it = layout.find( k * 2 + 0x1e ) ) {
                    it->hidden = false;
                }
            }
            if ( UI::Item * it = layout.find( level * 2 + 0x1d ) ) {
                it->hidden = false;
            }
            if ( hero != nullptr ) {
                learnGuildSpells( castle, *hero );
            }
            // FUN_004451d5: a click on an open slot shows the spell text (left: OK box, right: popup).
            layout.clickableIcons = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
            // FUN_004451d5: FUN_00422fc2( text, 1 (left) / 4 (right), -1, -1, 8, spell ) - with the spell picture (note v171).
            auto slotOf = [&shown]( const int id ) {
                const int slot = ( id >= 1 && id <= 18 ) ? ( id <= 9 ? id - 1 : id - 10 ) : -1;
                return ( slot >= 0 && shown( slot ) ) ? slot : -1;
            };
            auto help = [&]( const int id ) -> const char * {
                const int slot = slotOf( id );
                return slot >= 0 ? spellDescription( spells[slot] ) : nullptr;
            };
            auto helpPicture = [&]( const int id ) {
                UI::MessagePicture p;
                const int slot = slotOf( id );
                if ( slot >= 0 ) {
                    p.kind = 8;
                    p.value = spells[slot];
                }
                return p;
            };
            while ( true ) {
                const int answer = UI::runLayoutDialog( layout, 0, 0, help, -1, helpPicture );
                if ( answer >= 1 && answer <= 18 ) {
                    const int slot = slotOf( answer );
                    if ( slot >= 0 ) {
                        // Over the still open guild window.
                        UI::drawLayout( display, layout, ox, oy, -1 );
                        UI::MessagePicture picture;
                        picture.kind = 8;
                        picture.value = spells[slot];
                        UI::messageBox( spellDescription( spells[slot] ), UI::MESSAGE_OKAY, -1, -1, picture, UI::MessagePicture() );
                    }
                    continue;
                }
                break;
            }
        };

        // ---- Thieves' guild (building 1): thiefwin.bin (0, 0), FUN_0044535b / FUN_00445647 / FUN_00445acb (note v69) ----
        auto showThievesGuild = [&]() { H1::showThievesGuildWindow( castle.GetColor(), 0, "Thieves' Guild: Player Rankings" ); };

        // ---- Dock (building 3): FUN_0044145e case 3, shipwind.bin (0xb1, 0x14) (note v70) ----
        auto showDock = [&]() {
            // Original: fewer than 32 boats (FUN_0041ee53), nothing on the tile (x - 1, y + 1) of the castle and no hero standing there;
            // else "Cannot build another boat." at (0xd0, 0x28). The engine's own boat place check stands in [temporary].
            if ( !castle.AllowBuyBoat( false ) ) {
                UI::messageBox( "Cannot build another boat.", UI::MESSAGE_OKAY, 0xd0, 0x28 );
                return;
            }
            UI::Layout layout = UI::loadLayout( "shipwind.bin" );
            if ( UI::Item * it = layout.find( 0 ) ) {
                it->text = "Build a new ship:"; // table 0x492650 key 0xc
            }
            if ( UI::Item * it = layout.find( 1 ) ) {
                it->text = "Resource cost:";
            }
            const Funds & f = world.GetKingdom( castle.GetColor() ).GetFunds();
            if ( f.gold < 1000 || f.wood < 10 ) {
                if ( UI::Item * it = layout.find( 0x8002 ) ) {
                    it->greyed = true;
                    it->inactive = true;
                }
            }
            if ( UI::runLayoutDialog( layout, 0xb1, 0x14, false ) == 0x8002 ) {
                // FUN_00411d44 puts the boat on (x - 1, y + 1); 1000 gold and 10 wood (the engine boat costs the same) [engine places it].
                castle.BuyBoat();
                H1::playSound( "buildtwn.82m" ); // FUN_0044145e case 3: the boat is made
                updateBank();
            }
        };

        // ---- Castle options (build) window: caslwind.bin, FUN_004447a2 / FUN_004467f0 / FUN_00443505 / FUN_00444402 (note v66) ----

        // Engine building of a HoMM1 building number (0 = the next mage guild level).
        auto engineBuilding = [&castle]( const int number ) -> uint32_t { return engineBuildingOf( castle, number ); };

        // Castle +0x16 bits as the original keeps them (bit = building number), from the engine castle.
        auto builtMask = [&castle]() { return builtMaskOf( castle ); };

        // FUN_00422bb1: cost (wood, mercury, ore, sulfur, crystal, gems, gold). Tables 0x4911d0 (guild), 0x491240, 0x491398.
        auto buildCost = [&castle, race]( const int number, int cost[7] ) { buildCostOf( castle, race, number, cost ); };

        auto fundsArray = [&castle]( int32_t have[7] ) {
            const Funds & f = world.GetKingdom( castle.GetColor() ).GetFunds();
            const int32_t v[7] = { f.wood, f.mercury, f.ore, f.sulfur, f.crystal, f.gems, f.gold };
            for ( int k = 0; k < 7; ++k ) {
                have[k] = v[k];
            }
        };

        // FUN_00422e94
        auto canAfford = [&]( const int number ) {
            int cost[7];
            int32_t have[7];
            buildCost( number, cost );
            fundsArray( have );
            for ( int k = 0; k < 7; ++k ) {
                if ( have[k] < cost[k] ) {
                    return false;
                }
            }
            return true;
        };

        // FUN_00422d50
        auto canBuild = [&]( const int number ) { return canBuildOf( castle, race, number ); };

        // Building names (tables 0x4931c0 / 0x4931e0, FUN_00422b6a).
        auto buildingName = [race]( const int number ) -> std::string {
            static const char * const common[7] = { "Mage Guild", "Thieves' Guild", "Tavern", "Shipyard", "Well", "Tent", "Castle" };
            static const char * const dwellings[5][6] = { { "Thatched Hut", "Archery Range", "Blacksmith", "Armory", "Jousting Arena", "Cathedral" },
                                                          { "Treehouse", "Cottage", "Archery Range", "Stonehenge", "Fenced Meadow", "Red Tower" },
                                                          { "Hut", "Stick Hut", "Den", "Adobe", "Bridge", "Pyramid" },
                                                          { "Cave", "Crypt", "Nest", "Maze", "Swamp", "Black Tower" },
                                                          { "Excavation", "Graveyard", "Pyramid", "Mansion", "Mausoleum", "Laboratory" } };
            return number < 7 ? common[number] : dwellings[race][number - 7];
        };

        // FUN_00443505: the buy window. popup = right click (no buttons, closes on release). Returns true when bought (paid already).
        auto buyWindow = [&]( const int number, const bool cannotAfford, const bool popup ) {
            static const char * const commonText[7] = { "The Mage Guild allows heroes to learn and replenish spells.",
                                                        "The Thieves' Guild provides information on enemy players.  Thieves' Guilds can also provide scouting "
                                                        "information on enemy towns.  Additional Guilds provide more information.",
                                                        "The Tavern increases morale for troops defending the castle.",
                                                        "The Shipyard allows ships to be built.",
                                                        "The Well increases the growth rate of all dwellings by 2 creatures per week.",
                                                        "The Tent provides workers to build a castle.",
                                                        "The Castle improves town defense and income." };
            static const char * const dwellingText[30]
                = { "The Thatched Hut produces Peasants.", "The Archery Range produces Archers.",  "The Blacksmith produces Pikemen.",
                    "The Armory produces Swordsmen.",      "The Jousting Arena produces Cavalries.", "The Cathedral produces Paladins.",
                    "The Treehouse produces Sprites.",     "The Cottage produces Dwarves.",        "The Archery Range produces Elves.",
                    "Stonehenge produces Druids.",         "The Fenced Meadow produces Unicorns.", "The Red Tower produces Phoenix.",
                    "The Hut produces Goblins.",           "The Stick Hut produces Orcs.",         "The Den produces Wolves.",
                    "The Adobe produces Ogres.",           "The Bridge produces Trolls.",          "The Pyramid produces Cyclopes.",
                    "The Cave produces Centaurs.",         "The Crypt produces Gargoyles.",        "The Nest produces Griffins.",
                    "The Maze produces Minotaurs.",        "The Swamp produces Hydras.",           "The Black Tower produces Dragons.",
                    // H1-INTENDED(D16) necromancer
                    "The Excavation produces Skeletons.",  "The Graveyard produces Zombies.",      "The Pyramid produces Mummies.",
                    "The Mansion produces Vampires.",      "The Mausoleum produces Power Liches.", "The Laboratory produces Bone Dragons." };
            int cost[7];
            buildCost( number, cost );
            int kinds[7];
            int amounts[7];
            int used = 0;
            for ( int k = 0; k < 7; ++k ) {
                if ( cost[k] > 0 ) {
                    kinds[used] = k;
                    amounts[used] = cost[k];
                    ++used;
                }
            }
            // Rows: up to 4 in one row; 5 -> 2 + 3, 6 -> 3 + 3, 7 -> 3 + 4.
            int rowCount[2] = { used, 0 };
            if ( used == 5 ) {
                rowCount[0] = 2;
                rowCount[1] = 3;
            }
            else if ( used == 6 ) {
                rowCount[0] = 3;
                rowCount[1] = 3;
            }
            else if ( used == 7 ) {
                rowCount[0] = 3;
                rowCount[1] = 4;
            }

            std::string text = number < 7 ? commonText[number] : dwellingText[race * 6 + number - 7];
            if ( number >= 7 ) {
                static const int needs[5][6] = { { 0, 0x80, 0x90, 0x84, 0x600, 0x600 },
                                                 { 0, 0x84, 0x80, 0x201, 0x400, 0x800 },
                                                 { 0, 0x80, 0x80, 0x80, 0x400, 0x800 },
                                                 { 0, 0x80, 0x80, 0x100, 0x200, 0xc00 },
                                         // H1-INTENDED(D16) necromancer: the HoMM2 rules (castle_building_info.cpp getBuildingRequirement), as
                                         // the four HoMM1 rows equal the HoMM2 rows of their races [checked]: 2, 3 <- 1; 4 <- 3 + thieves' guild;
                                         // 5 <- 2 + mage guild; 6 <- 5
                                         { 0, 0x80, 0x80, 0x202, 0x101, 0x800 } };
                const int need = needs[race][number - 7];
                int listed = 0;
                for ( int b = 0; b < 12; ++b ) {
                    if ( need & ( 1 << b ) ) {
                        if ( listed == 0 ) {
                            text += "\n\nRequires:";
                        }
                        ++listed;
                        text += "\n" + buildingName( b );
                    }
                }
            }
            text += "\n ";
            const int lines = UI::countLines( text, "bigfont.fnt", 0xee );
            int height = lines * 16 + ( used < 5 ? 0xc3 : 0xef );
            if ( !popup ) {
                height += 0x27;
            }
            const int size = std::clamp( ( height - 0x35 ) / 0x2d, 3, 7 );
            UI::Layout layout = UI::loadLayout( "buybuil" + std::to_string( size ) + ".bin" );
            if ( UI::Item * it = layout.find( 0 ) ) {
                it->text = "Build improvement:"; // table 0x492650 key 1
            }
            if ( UI::Item * it = layout.find( 2 ) ) {
                it->frame = number < 5 ? number + 1 : ( race + 1 ) * 7 + number - 6;
            }
            if ( UI::Item * it = layout.find( 3 ) ) {
                if ( number == 0 ) {
                    // local_3c (0 without a guild, else castle +0x18 + 1 capped at 3) + 1
                    const int next = castle.GetLevelMageGuild() == 0 ? 0 : std::min( guildByte( castle ) + 1, 3 );
                    it->text = "Mage Guild, Level " + std::to_string( next + 1 );
                }
                else {
                    it->text = buildingName( number );
                }
            }
            UI::Item description;
            description.type = UI::ITEM_TEXT;
            description.x = 0x18;
            description.y = 0x97;
            description.w = 0xee;
            description.h = lines * 16 + 6;
            description.name = "bigfont.fnt";
            description.colour = 1;
            description.align = 1;
            description.text = text;
            layout.items.push_back( description );
            int index = 0;
            for ( int row = 0; row < 2; ++row ) {
                const int n = rowCount[row];
                if ( n <= 0 ) {
                    continue;
                }
                const int32_t ry = row * 0x2c + lines * 16 + 0x97 + 0xc;
                int total = 0;
                for ( int k = 0; k < n; ++k ) {
                    total += ( kinds[index + k] == 6 ) ? 0x50 : 0x28;
                }
                const int gap = ( 0x10a - total ) / ( n + 1 );
                int32_t rx = gap + 10;
                for ( int k = 0; k < n; ++k, ++index ) {
                    const int32_t w = ( kinds[index] == 6 ) ? 0x50 : 0x28;
                    UI::Item amount;
                    amount.type = UI::ITEM_TEXT;
                    amount.x = rx;
                    amount.y = ry + 0x20;
                    amount.w = w;
                    amount.h = 0xc;
                    amount.name = "smalfont.fnt";
                    amount.colour = 1;
                    amount.align = 1;
                    amount.text = std::to_string( amounts[index] );
                    UI::Item picture;
                    picture.type = UI::ITEM_ICON;
                    picture.x = rx;
                    picture.y = ry;
                    picture.w = w;
                    picture.h = 0xc;
                    picture.name = "resource.icn";
                    picture.frame = kinds[index];
                    picture.flags = 0x10;
                    layout.items.push_back( amount );
                    layout.items.push_back( picture );
                    rx += gap + w;
                }
            }
            if ( popup ) {
                for ( const int id : { 0x8002, 0x8001, 0 } ) {
                    if ( UI::Item * it = layout.find( id ) ) {
                        it->hidden = true;
                        it->inactive = true;
                    }
                }
                UI::runLayoutDialog( layout, 0xb1, 0x10, true );
                return false;
            }
            if ( cannotAfford ) {
                // OK: active bit cleared and state 0x1000 set [meaning of 0x1000 unconfirmed; drawn greyed here].
                if ( UI::Item * it = layout.find( 0x8002 ) ) {
                    it->inactive = true;
                    it->greyed = true;
                }
            }
            if ( UI::runLayoutDialog( layout, 0xb1, 0x10, false ) != 0x8002 ) {
                return false;
            }
            // Paid here, the building itself comes after the castle options window closes (FUN_00444402).
            Funds pay;
            pay.wood = cost[0];
            pay.mercury = cost[1];
            pay.ore = cost[2];
            pay.sulfur = cost[3];
            pay.crystal = cost[4];
            pay.gems = cost[5];
            pay.gold = cost[6];
            world.GetKingdom( castle.GetColor() ).OddFundsResource( pay );
            return true;
        };

        // FUN_00444402: the building appears with "buildtwn.82M" (the fade-in of rectangle table 0x491680 is not ported).
        auto finishBuilding = [&]( const int number ) {
            castle.constructBuilding( engineBuilding( number ) );
            H1::playSound( "buildtwn.82m" );
            if ( number >= 7 ) {
                // Dwelling count = growth of its creature (table +7) [the engine sets its own growth value].
            }
            for ( Building & b : buildings ) {
                if ( b.id != -1 ) {
                    b.shown = isBuilt( castle, b.number );
                }
                if ( b.id == 0 && race != 4 ) {
                    b.h = guildByte( castle ) * 0x14 + 0x61;
                    b.y = 0x99 - b.h;
                }
            }
            updateBank();
        };

        // Recruit hero window: rcrthero.bin at (0xb1, 0x10), FUN_00445be4 / handler FUN_0042299f (note v67).
        // The two candidates are the player's tavern heroes (player +0x1e / +0x1f) = the kingdom's recruits, kept by H1::Tavern (note v169).
        // popup: right click (Recruit 8 / 9 and Cancel hidden, closes on release). Returns true when a hero was recruited.
        auto recruitWindow = [&]( const bool popup ) {
            Kingdom & kingdom = world.GetKingdom( castle.GetColor() );
            const Recruits & recruits = kingdom.GetRecruits();
            Heroes * candidates[2] = { recruits.GetHero1(), recruits.GetHero2() };
            static const char * const classNames[5] = { "Knight", "Barbarian", "Sorceress", "Warlock", "Necromancer" }; // table 0x493058; H1-INTENDED(D16) 5th
            while ( true ) {
                UI::Layout layout = UI::loadLayout( "rcrthero.bin" );
                if ( UI::Item * it = layout.find( 600 ) ) {
                    it->text = "Recruit Hero"; // table 0x492650 key 0xb
                }
                for ( int k = 0; k < 2; ++k ) {
                    Heroes * h = candidates[k];
                    if ( UI::Item * it = layout.find( 2 + k ) ) {
                        char name[16];
                        std::snprintf( name, sizeof( name ), "port%04d.icn", h != nullptr ? std::max( 0, H1::heroNumberOfId( h->getPortraitId() ) ) : 0 );
                        it->name = name;
                        it->hidden = ( h == nullptr );
                    }
                    if ( UI::Item * it = layout.find( 6 + k ) ) {
                        int cls = 0;
                        if ( h != nullptr ) {
                            switch ( h->GetRace() ) {
                            case Race::BARB:
                                cls = 1;
                                break;
                            case Race::SORC:
                                cls = 2;
                                break;
                            case Race::WRLK:
                                cls = 3;
                                break;
                            case Race::NECR:
                                cls = 4; // H1-INTENDED(D16)
                                break;
                            default:
                                break;
                            }
                        }
                        it->text = h != nullptr ? classNames[cls] : "";
                    }
                    if ( h == nullptr ) {
                        if ( UI::Item * it = layout.find( 8 + k ) ) {
                            it->hidden = true;
                            it->inactive = true;
                        }
                    }
                }
                if ( popup ) {
                    for ( const int id : { 8, 9, 0x8001 } ) {
                        if ( UI::Item * it = layout.find( id ) ) {
                            it->hidden = true;
                            it->inactive = true;
                        }
                    }
                    UI::runLayoutDialog( layout, 0xb1, 0x10, true );
                    return false;
                }
                layout.clickableIcons = { 2, 3 };
                const int answer = UI::runLayoutDialog( layout, 0xb1, 0x10, false );
                if ( answer == 2 || answer == 3 ) {
                    // Portrait: hero screen of the candidate (FUN_00447aad( hero, 0 )) [second argument unconfirmed; read-only here].
                    if ( candidates[answer - 2] != nullptr ) {
                        runHeroScreen( *candidates[answer - 2], true, true );
                    }
                    continue;
                }
                if ( answer != 8 && answer != 9 ) {
                    return false;
                }
                // Pay 2500 (DAT_00491648; the engine's recruit price is the same 2500), the hero stands in this castle.
                // FUN_00445be4 then fills the place with a hero of class (hired class + 1..3) & 3 (FUN_00411ef0, note v169).
                Heroes * hired = castle.RecruitHero( candidates[answer - 8] );
                if ( hired == nullptr ) {
                    return false;
                }
                H1::Tavern::afterHire( kingdom, answer - 8, *hired );
                return true;
            }
        };

        // Castle options window. Returns HoMM1 building bought (-1 none); recruitHero set when "Recruit Hero" was chosen.
        auto castleOptions = [&]( bool & recruitHero ) {
            recruitHero = false;
            UI::Layout layout = UI::loadLayout( "caslwind.bin" );
            if ( UI::Item * it = layout.find( 502 ) ) {
                it->text = "Castle Options:  Town Improvements/Recruit Hero";
            }
            const int mask = builtMask();
            int affordMask = 0;
            int buildMask = 0;
            for ( int number = 0; number < 13; ++number ) {
                if ( canAfford( number ) ) {
                    affordMask |= 1 << number;
                }
                if ( canBuild( number ) ) {
                    buildMask |= 1 << number;
                }
            }
            for ( int k = 0; k < 6; ++k ) {
                if ( UI::Item * it = layout.find( k + 7 ) ) {
                    it->frame = ( race + 1 ) * 7 + k + 1;
                }
                if ( UI::Item * it = layout.find( k + 0x17 ) ) {
                    it->text = buildingName( k + 7 );
                }
            }
            auto setMark = [&layout]( const int id, const int frame ) {
                if ( UI::Item * it = layout.find( id ) ) {
                    it->hidden = ( frame < 0 );
                    if ( frame >= 0 ) {
                        it->frame = frame;
                    }
                }
            };
            for ( int number = 0; number < 13; ++number ) {
                if ( number == 5 || number == 6 ) {
                    continue;
                }
                int frame = -1;
                const bool built = ( mask & ( 1 << number ) ) != 0 && !( number == 0 && guildByte( castle ) != 3 );
                if ( built ) {
                    frame = 0xb;
                }
                else if ( ( buildMask & ( 1 << number ) ) == 0 ) {
                    frame = 0xc;
                }
                else if ( ( affordMask & ( 1 << number ) ) == 0 ) {
                    frame = 0xd;
                }
                setMark( number < 5 ? 0x20 + number : 0x27 + number - 7, frame );
            }
            const Kingdom & kingdom = world.GetKingdom( castle.GetColor() );
            const bool heroCostOk = kingdom.GetFunds().gold >= 2500;
            const bool heroLimit = kingdom.GetHeroes().size() >= 8;
            {
                int frame = -1;
                if ( !heroCostOk ) {
                    frame = 0xd;
                }
                else if ( heroLimit || hero != nullptr ) {
                    frame = 0xc;
                }
                setMark( 0x50, frame );
            }

            // FUN_004467f0: status line (item 0x1f6) and clicks.
            auto statusFor = [&]( const int id ) -> std::string {
                char buffer[160];
                if ( id >= 0 && id < 0xd ) {
                    const std::string name = buildingName( id );
                    if ( id == 0 ) {
                        if ( ( buildMask & 1 ) == 0 ) {
                            std::snprintf( buffer, sizeof( buffer ), "Cannot build %s", name.c_str() );
                            return buffer;
                        }
                        if ( ( affordMask & 1 ) == 0 ) {
                            std::snprintf( buffer, sizeof( buffer ), "Cannot afford %s", name.c_str() );
                            return buffer;
                        }
                        if ( ( mask & 1 ) == 0 ) {
                            return "Build Mage Guild";
                        }
                        // Level 3 -> "highest level"; else "Cannot afford next level." / "Add another level" (never reached: checked above).
                        return guildByte( castle ) == 3 ? "Mage Guild is at highest level." : "Add another level to Mage Guild";
                    }
                    if ( id >= 5 && id <= 6 ) {
                        return "Castle Options";
                    }
                    if ( mask & ( 1 << id ) ) {
                        std::snprintf( buffer, sizeof( buffer ), "%s is already built", name.c_str() );
                    }
                    else if ( ( buildMask & ( 1 << id ) ) == 0 ) {
                        std::snprintf( buffer, sizeof( buffer ), "Cannot build %s", name.c_str() );
                    }
                    else if ( ( affordMask & ( 1 << id ) ) == 0 ) {
                        std::snprintf( buffer, sizeof( buffer ), "Cannot afford %s", name.c_str() );
                    }
                    else {
                        std::snprintf( buffer, sizeof( buffer ), "Build %s", name.c_str() );
                    }
                    return buffer;
                }
                if ( id == 0x30 ) {
                    if ( !heroCostOk ) {
                        return "Cannot afford a Hero.";
                    }
                    if ( heroLimit ) {
                        return "Cannot recruit - you already have 8 Heroes.";
                    }
                    if ( hero == nullptr ) {
                        return "Recruit a new Hero";
                    }
                    return "Cannot recruit - you already have a Hero in this town.";
                }
                if ( id == 0x8000 ) {
                    return "Exit Castle";
                }
                return "Castle Options";
            };

            fheroes2::Display & disp = fheroes2::Display::instance();
            const fheroes2::Image before = disp;
            LocalEvent & ev = LocalEvent::Get();
            int bought = -1;
            int lastId = -2;
            int down = -1;
            auto itemAt = [&layout]( const int32_t x, const int32_t y ) {
                for ( auto it = layout.items.rbegin(); it != layout.items.rend(); ++it ) {
                    if ( it->hidden || it->id == 0xffff || ( it->type != UI::ITEM_ICON && it->type != UI::ITEM_BUTTON ) ) {
                        continue;
                    }
                    if ( x >= it->x && y >= it->y && x < it->x + it->w && y < it->y + it->h ) {
                        // Marks (0x20..0x2c, 0x50) are not click targets in the handler; the building picture under them is.
                        if ( ( it->id >= 0x20 && it->id <= 0x2c ) || it->id == 0x50 ) {
                            continue;
                        }
                        return it->id;
                    }
                }
                return -1;
            };
            auto redrawWindow = [&]() {
                fheroes2::Copy( before, disp );
                UI::drawLayout( disp, layout, ox, oy, down );
                disp.render();
            };
            redrawWindow();
            while ( ev.HandleEvents() ) {
                const fheroes2::Point mp = ev.getMouseCursorPos();
                const int id = itemAt( mp.x - ox, mp.y - oy );
                bool again = false;
                if ( id != lastId ) {
                    lastId = id;
                    if ( UI::Item * it = layout.find( 502 ) ) {
                        it->text = statusFor( id );
                    }
                    again = true;
                }
                const int newDown = ( id == 0x8000 && ev.isMouseLeftButtonPressed() ) ? 0x8000 : -1;
                if ( newDown != down ) {
                    down = newDown;
                    again = true;
                }
                if ( again ) {
                    redrawWindow();
                }
                const bool left = ev.MouseClickLeft();
                const bool right = ev.isMouseRightButtonPressed();
                if ( ( id == 0x8000 && left ) || ev.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                    break;
                }
                const bool buildingId = ( id >= 0 && id <= 4 ) || ( id >= 7 && id <= 12 );
                if ( buildingId && ( left || right ) ) {
                    bool open = right;
                    if ( left ) {
                        if ( id == 0 ) {
                            open = guildByte( castle ) != 3 && ( buildMask & 1 ) != 0;
                        }
                        else {
                            open = ( mask & ( 1 << id ) ) == 0 && ( buildMask & ( 1 << id ) ) != 0;
                        }
                    }
                    if ( open && buyWindow( id, ( affordMask & ( 1 << id ) ) == 0, right ) ) {
                        bought = id;
                        break;
                    }
                    redrawWindow();
                }
                if ( id == 0x30 && right ) {
                    recruitWindow( true );
                    redrawWindow();
                }
                else if ( id == 0x30 && left && heroCostOk && !heroLimit && hero == nullptr ) {
                    // FUN_00445be4. On Cancel the castle options window stays open (FUN_004447a2 refreshes it).
                    if ( recruitWindow( false ) ) {
                        recruitHero = true;
                        break;
                    }
                    redrawWindow();
                }
            }
            fheroes2::Copy( before, disp );
            disp.render();
            return bought;
        };

        draw();

        // FUN_0044021c: next animation step 0x96 ms later (GetTickCount).
        using Clock = std::chrono::steady_clock;
        Clock::time_point next = Clock::now() + std::chrono::milliseconds( 0x96 );

        CastleScreenResult result = CastleScreenResult::Exit;
        bool lastShift = false;
        while ( le.HandleEvents() ) {
            const fheroes2::Point m = le.getMouseCursorPos();
            const int32_t mx = m.x - ox;
            const int32_t my = m.y - oy;
            const int id = idAt( mx, my );
            const bool overExit = ( id == 0x7800 );
            const bool shift = ( LocalEvent::getCurrentKeyModifiers() & fheroes2::KEY_MODIFIER_SHIFT ) != 0;
            bool redraw = false;
            if ( id != hoverId || shift != lastShift ) {
                hoverId = id;
                lastShift = shift;
                hover( id, shift );
                redraw = true;
            }
            const int newPressed = ( overExit && le.isMouseLeftButtonPressed() ) ? 0x8000 : -1;
            if ( newPressed != pressed ) {
                pressed = newPressed;
                redraw = true;
            }
            const bool click = le.MouseClickLeft();
            if ( ( overExit && click ) || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                result = CastleScreenResult::Exit;
                break;
            }
            if ( click && id >= 7 && id <= 12 ) {
                recruit( id - 7 );
                updateBank();
                deselect();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 0 ) {
                showMageGuild();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 1 ) {
                showThievesGuild();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 2 ) {
                showTavern();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 4 ) {
                showWell();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 5 ) {
                // Tent (FUN_0044145e case 5): straight to the buy window for the castle (building 6). No "built today" check here
                // (FUN_00443505 only greys OK when it cannot be afforded) - as written in the original.
                if ( buyWindow( 6, !canAfford( 6 ), false ) ) {
                    finishBuilding( 6 );
                }
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 3 ) {
                showDock();
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id == 6 ) {
                // Castle: castle options window (FUN_004447a2); the castle screen is drawn behind it.
                bool recruitHero = false;
                const int bought = castleOptions( recruitHero );
                if ( bought >= 0 ) {
                    finishBuilding( bought );
                }
                if ( recruitHero ) {
                    // FUN_0044145e (+0xf2 set by the recruit): "buildtwn.82M" as the castle screen shows the hero.
                    H1::playSound( "buildtwn.82m" );
                    // A hero now stands in the castle: the strips are made again (FUN_00445be4 rebuilds both strip objects).
                    result = CastleScreenResult::Reopen;
                    break;
                }
                hoverId = -1;
                redraw = true;
            }
            else if ( click && id >= 0 && id <= 6 ) {
                // Building windows (mage guild, thieves' guild, tavern, dock, well, castle / tent) are not ported yet:
                // the engine castle screen opens [temporary].
                result = CastleScreenResult::OpenEngineScreen;
                break;
            }
            if ( click && id >= 0x10 && id <= 0x1b ) {
                doAction();
                redraw = true;
            }
            if ( le.isMouseRightButtonPressed() && id >= 7 && id <= 12 ) {
                recruit( id - 7, true );
                redraw = true;
            }
            if ( le.isMouseRightButtonPressed() && id == 5 ) {
                // Right click on the tent: the castle buy window as a popup.
                buyWindow( 6, !canAfford( 6 ), true );
                redraw = true;
            }
            if ( le.isMouseRightButtonPressed() && ( ( id >= 0x11 && id <= 0x15 ) || ( id >= 0x17 && id <= 0x1b ) ) ) {
                // Right click on a slot: creature window as a popup (hero only for the hero strip).
                const int strip = id >= 0x17 ? 1 : 0;
                const int slot = id - ( strip == 1 ? 0x17 : 0x11 );
                if ( creatureAt( strip, slot ) >= 0 ) {
                    Troop * t = troopAt( strip, slot );
                    showCreatureWindow( t->GetID(), static_cast<int>( t->GetCount() ), strip == 1 ? hero : nullptr, armyOf( strip ), true, true, &castle );
                }
            }
            if ( Clock::now() > next ) {
                for ( Building & b : buildings ) {
                    if ( b.animationFrames != 0 ) {
                        ++b.step;
                        if ( b.step == b.animationFrames ) {
                            b.step = 0;
                        }
                    }
                }
                next = Clock::now() + std::chrono::milliseconds( 0x96 );
                redraw = true;
            }
            if ( redraw ) {
                draw();
            }
        }

        fheroes2::Copy( saved, display );
        display.render();
        return result;
    }

    int buildingRace( const Castle & castle )
    {
        // H1-INTENDED(D16): 4 = necromancer (extension games only; an original game has no necromancer castle).
        return std::clamp( h1RaceFromEngine( castle.GetRace() ), 0, 4 );
    }

    bool isBuildingBuilt( const Castle & castle, const int number )
    {
        return isBuilt( castle, number );
    }

    bool canBuildBuilding( const Castle & castle, const int number )
    {
        return canBuildOf( castle, buildingRace( castle ), number );
    }

    void buildingCost( const Castle & castle, const int number, int cost[7] )
    {
        buildCostOf( castle, buildingRace( castle ), number, cost );
    }

    void constructBuilding( Castle & castle, const int number )
    {
        // Pays (FUN_00422e94 price) and builds; the castle cannot build again today.
        int cost[7];
        buildingCost( castle, number, cost );
        Funds pay;
        pay.wood = cost[0];
        pay.mercury = cost[1];
        pay.ore = cost[2];
        pay.sulfur = cost[3];
        pay.crystal = cost[4];
        pay.gems = cost[5];
        pay.gold = cost[6];
        world.GetKingdom( castle.GetColor() ).OddFundsResource( pay );
        castle.constructBuilding( engineBuildingOf( castle, number ) );
    }

    int dwellingCreatureOf( const Castle & castle, const int number )
    {
        return ( number >= 7 && number <= 12 ) ? dwellingCreature[buildingRace( castle )][number - 7] : -1;
    }

    int castleBuildingBits( const Castle & castle )
    {
        int mask = 0;
        for ( int number = 0; number < 14; ++number ) {
            if ( isBuilt( castle, number ) ) {
                mask |= 1 << number;
            }
        }
        return mask;
    }

    int castleGuildByte( const Castle & castle )
    {
        return guildByte( castle );
    }

    void buildingCostRow( const int race, const int number, const int guildRow, int cost[7] )
    {
        // FUN_00422bb1( race, building, out, guild row ): tables 0x4911d0 (guild rows), 0x491240 (buildings 1..6), 0x491398 (dwellings).
        static const int guild[4][7]
            = { { 5, 0, 5, 0, 0, 0, 2000 }, { 5, 4, 5, 4, 4, 4, 1000 }, { 5, 6, 5, 6, 6, 6, 1000 }, { 5, 10, 5, 10, 10, 10, 1000 } };
        static const int common[7][7] = { { 5, 0, 5, 0, 0, 0, 2000 }, { 5, 0, 0, 0, 0, 0, 750 },   { 5, 0, 0, 0, 0, 0, 500 }, { 20, 0, 0, 0, 0, 0, 2000 },
                                          { 0, 0, 0, 0, 0, 0, 500 },  { 5, 0, 5, 0, 0, 0, 2000 }, { 20, 0, 20, 0, 0, 0, 5000 } };
        static const int dwelling[5][6][7] = { { { 0, 0, 0, 0, 0, 0, 200 },
                                                 { 0, 0, 0, 0, 0, 0, 1000 },
                                                 { 0, 0, 5, 0, 0, 0, 1000 },
                                                 { 10, 0, 10, 0, 0, 0, 2000 },
                                                 { 20, 0, 0, 0, 0, 0, 3000 },
                                                 { 20, 0, 0, 0, 20, 0, 5000 } },
                                               { { 5, 0, 0, 0, 0, 0, 500 },
                                                 { 5, 0, 0, 0, 0, 0, 1000 },
                                                 { 0, 0, 0, 0, 0, 0, 1500 },
                                                 { 0, 10, 10, 0, 0, 0, 2500 },
                                                 { 10, 0, 0, 0, 0, 10, 3000 },
                                                 { 0, 20, 30, 0, 0, 0, 10000 } },
                                               { { 0, 0, 0, 0, 0, 0, 300 },
                                                 { 5, 0, 0, 0, 0, 0, 800 },
                                                 { 0, 0, 0, 0, 0, 0, 1000 },
                                                 { 10, 0, 10, 0, 0, 0, 2000 },
                                                 { 0, 0, 20, 0, 0, 0, 4000 },
                                                 { 0, 0, 20, 0, 20, 0, 6000 } },
                                               { { 0, 0, 0, 0, 0, 0, 500 },
                                                 { 0, 0, 10, 0, 0, 0, 1000 },
                                                 { 0, 0, 0, 0, 0, 0, 2000 },
                                                 { 0, 0, 0, 0, 0, 10, 3000 },
                                                 { 0, 0, 0, 10, 0, 0, 4000 },
                                                 { 0, 0, 30, 20, 0, 0, 15000 } },
                                               // H1-INTENDED(D16) necromancer: HoMM2 values (buildinginfo.cpp), as the four HoMM1 rows equal
                                               // the HoMM2 rows of their races [checked]
                                               { { 0, 0, 0, 0, 0, 0, 400 },
                                                 { 0, 0, 0, 0, 0, 0, 1000 },
                                                 { 0, 0, 10, 0, 0, 0, 1500 },
                                                 { 10, 0, 0, 0, 0, 0, 3000 },
                                                 { 10, 0, 0, 10, 0, 0, 4000 },
                                                 { 10, 5, 10, 5, 5, 5, 10000 } } };
        const int * row = nullptr;
        if ( number == 0 ) {
            row = guild[std::clamp( guildRow, 0, 3 )];
        }
        else if ( number < 7 ) {
            row = common[number];
        }
        else {
            row = dwelling[std::clamp( race, 0, 4 )][number - 7];
        }
        for ( int k = 0; k < 7; ++k ) {
            cost[k] = row[k];
        }
    }
}
