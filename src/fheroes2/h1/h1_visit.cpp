// Heroes I map object visits (homm1-remaster project). See h1_visit.h.
#include "h1_visit.h"
#include "h1_ext_objects.h"
#include "h1_ext_objects_internal.h"
#include "h1_newgame.h"
#include "map_object_info.h"
#include "h1_ai.h"

#include <sstream>
#include <map>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#include "artifact.h"
#include "castle.h"
#include "h1_battle.h"
#include "h1_castle.h"
#include "h1_rules.h"
#include "h1_sound.h"
#include "h1_gfx.h"
#include "h1_obelisk.h"
#include "game_interface.h"
#include "interface_radar.h"
#include "maps.h"
#include "maps_tiles_helper.h"
#include "puzzle.h"
#include "h1_rules.h"
#include "army.h"
#include "battle.h"
#include "army_troop.h"
#include "kingdom.h"
#include "monster.h"
#include "rand.h"
#include "resource.h"
#include "h1_ui.h"
#include "h1_map_render.h"
#include "heroes.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "settings.h"
#include "world.h"

namespace
{
    const char * const eventTexts[0x4c] = {
        "Alchemist\n\nYou have taken control of the local Alchemist shop. It will provide you with one unit of Mercury per day.",
        "Signpost\n\nA signpost reads:\n\n%s is near.",
        "Buoy\n\nYour men spot a navigational buoy, confirming that you are on course.",
        "Buoy\n\nYour men spot a navigational buoy, confirming that you are on course and increasing their morale.",
        "Moisture congeals on the walls and trickles slowly down to the ground.  Except for the evidence of a battle, the cave is empty.",
        "A large daemon emerges from the shadows and you attack. After an exhausting battle, you emerge victorious and receive 1000 experience points.",
        "A large daemon emerges from the shadows and you attack. After an exhausting battle, you emerge victorious and receive 1000 experience points and an artifact.",
        "A large daemon emerges from the shadows and you attack. After an exhausting battle, you emerge victorious and receive 1000 experience points and 2500 gold.",
        "You are captured by a large daemon.  He offers to let you free for 2500 gold, otherwise he will devour you.  Do you pay?",
        "Seeing that you do not have 2500 gold, the daemon slashes you with its claws, and the last thing you see is a red haze.",
        "Daemon Cave\n\nThe cave is dank and musty.  Two large red eyes glow eerily within the blackness.  Do you wish to enter?",
        "Chest\n\nAfter scouring the area, you fall upon a hidden treasure cache.  You may take the gold or distribute the gold to the peasants for experience.  Do you wish to keep the gold?",
        "Faerie Ring\n\nYou enter the faerie ring, but nothing happens.",
        "Faerie Ring\n\nUpon entering the mystical faerie ring, your army gains luck for its next battle.",
        "Campfire\n\nRansacking an enemy camp, you discover a hidden cache of treasures.",
        "Fountain\n\nYou drink from the enchanted fountain, but nothing happens.",
        "Fountain\n\nAs you drink the sweet water, you gain luck for your next battle.",
        "Gazebo\n\nAn old knight appears on the steps of the gazebo. \"I am sorry, my liege, I have taught you all I can.\"",
        "Gazebo\n\nAn old knight appears on the steps of the gazebo. \"My liege, I will teach you all that I know to aid you in your travels.\"",
        "Genie Lamp\n\nYou stumble upon a dented and tarnished lamp lodged deep in the earth. Do you wish to rub the lamp?",
        "Graveyard\n\nYou tentatively approach the burial ground of ancient warriors.  Do you want to search the graves?",
        "Upon defeating the ghosts you spend several hours searching the graves and find nothing.  Such a despicable act reduces your army's morale.",
        "Upon defeating the ghosts you search the graves and find something!",
        "Hut\n\nA group of goblins with a desire for greater glory wish to join you. Do you accept?",
        "You are unable to recruit at this time, your ranks are full.",
        "Hut\n\nAs you approach the goblin dwelling, you notice that there is no one here.",
        "Thatched Hut\n\nA group of peasants with a desire for greater glory wish to join you. Do you accept? ",
        "You are unable to recruit at this time, your ranks are full.",
        "Thatched Hut\n\nAs you approach the peasant dwelling, you notice that there is no one here.",
        "Cottage\n\nA group of archers with a desire for greater glory wish to join you. Do you accept? ",
        "You are unable to recruit at this time, your ranks are full.",
        "Cottage\n\nAs you approach the archer dwelling, you notice that there is no one here.",
        "Cottage\n\nA group of dwarves with a desire for greater glory wish to join you. Do you accept? ",
        "You are unable to recruit at this time, your ranks are full.",
        "Cottage\n\nAs you approach the dwarves' dwelling, you notice that there is no one here.",
        "Thatched Hut\n\nA group of peasants with a desire for greater glory wish to join you. Do you accept? ",
        "You are unable to recruit at this time, your ranks are full.",
        "Thatched Hut\n\nAs you approach the peasant dwelling you notice that there is no one here.",
        "Dragon City\n\nYou have reached Dragon City, famous for its wealth and danger. Do you wish to attack?",
        "You have conquered the mighty dragons. In homage to you, they offer 1000 gold a day to your cause, and will defend the city for you in case of attack.",
        "Lighthouse\n\nThe lighthouse is now under your control, and all of your ships will now move further each turn.",
        "Mill\n\nThe keeper of the mill announces: \"Milord, I am sorry, there is no gold currently available.  Please try again next week.\"",
        "Mill\n\nThe keeper of the mill announces: \"Milord, I have been working very hard to provide you with this gold, come back next week for more.\"",
        "Ore Mine\n\nYou gain control of an ore mine. It will provide you with two units of ore per day.",
        "Sulfur Mine\n\nYou gain control of a sulfur mine. It will provide you with one unit of sulfur per day.",
        "Crystal Mine\n\nYou gain control of a crystal mine. It will provide you with one unit of crystal per day.",
        "Gem Mine\n\nYou gain control of a gem mine. It will provide you with one unit of gems per day.",
        "Gold Mine\n\nYou gain control of a gold mine. It will provide you with 1000 gold per day.",
        "Followers\n\nA group of %s with a desire for greater glory wish to join you. Do you accept? ",
        "Insulted by your refusal of their offer, the monsters attack!",
        "Obelisk\n\nYou come upon an obelisk made from a type of stone you have never seen before.  Staring at it intensely, the smooth surface suddenly changes to an inscription.  The inscription is a piece of a lost ancient map.  Quickly you copy down the piece and the inscription vanishes as abruptly as it had appeared.",
        "Obelisk\n\nYou have already been to this obelisk.",
        "Oasis\n\nYou spot an oasis, but the well is dry and you depart empty-handed.",
        "Oasis\n\nA nomad merchant, traveling on foot, hails you and says his horse had spooked and left him stranded. For safe passage he takes your army to an oasis, raising your morale for one battle.",
        "You find a small quantity of %s.",
        "Sawmill\n\nYou gain control of a sawmill. It will provide you with two units of wood per day.",
        "Shrine\n\nWithin the ornate shrine sits a blind seer. After explaining the intent of your journey, the seer activates his crystal ball, allowing you to see the strengths and weaknesses of your opponents.",
        "Shrine\n\nNestled in a small hidden shrine is an ancient wooden altar.  Upon the altar, a golden plaque bears an inscription with the secret to the ancient magical spell ",
        "Shipwreck\n\nThe rotting hulk of a great pirate ship creaks eerily as it is pushed against the rocks.  Do you wish to search the shipwreck? ",
        "Upon defeating the ghosts you spend several hours sifting through the debris and find nothing.  Such a despicable act reduces your army's morale.",
        "Upon defeating the ghosts you sift through the debris and find something!",
        "Statue\n\nA large statue of an angel towers above you. Abruptly, the angel's eyes open and your troops celebrate a great increase in their morale.",
        "Statue\n\nA large statue of an angel towers above you. Your army encircles the statue, but there appears to be nothing special about it.",
        "Tents\n\nA group of tattered tents, billowing in the sandy wind, beckons you.  The tents are unoccupied.  Perhaps more nomads will be here later.",
        "Tents\n\nA group of tattered tents, billowing in the sandy wind, beckons you.  Do you wish to have any nomads join you during your travels?",
        "Wagon\n\nA colorful rogues' wagon stands empty here.  Perhaps more rogues will be here later.",
        "Wagon\n\nDistant sounds of music and laughter draw you to a colorful wagon housing rogues.  Do you wish to have any rogues join your army?",
        "Whirlpool\n\nA whirlpool engulfs your ship.  Some of your army has fallen overboard.",
        "Windmill\n\nThe keeper of the mill announces: \"Milord, I am sorry, there are no resources currently available. Please try again next week.\"",
        "Windmill\n\nThe keeper of the mill announces: \"Milord, I have been working very hard to provide you with these resources, come back next week for more.\"",
        "Artifact\n\nYou come upon an ancient artifact.  As you reach for it, a pack of Rogues leap out of the brush to guard their stolen loot.",
        "Artifact\n\nA leprechaun offers you the %s for the small price of 2000 gold.  Do you wish to buy this artifact?",
        "Insulted by your refusal of his generous offer, the leprechaun stamps his foot ferociously and vanishes.",
        "You try to pay the leprechaun, but realize that you don't have 2000 gold.  The leprechaun stamps his foot and ignores you.",
        "Upon defeating the Rogues, you search their corpses and discover the %s.",
        "Skeleton\n\nYou come upon the remains of an unfortunate adventurer.  Searching through the tattered clothing, you find nothing." };

    // Artifact pickup texts, table 0x492f90 by HoMM1 artifact number (FUN_00401000 case 0x30 with value 1).
    const char * const artifactTexts[38] = {
        "",
        "",
        "",
        "",
        "After rescuing a sorceress from a cursed tomb, she rewards your heroism with an exquisite jeweled necklace.",
        "While searching through the rubble of a caved in mine, you free a group of trapped dwarves.  Grateful, the leader gives you a golden bracelet.",
        "A cry of pain leads you to a centaur, caught in a trap.  Upon setting the creature free, he hands you a small pouch.  Emptying the contents, you find a dazzling jeweled ring.",
        "Alongside the remains of a burnt witch lies a beautiful broach, intricately designed.  Approaching the corpse with caution, you add the broach to your inventory.",
        "Freeing a virtuous maiden from the clutches of an evil overlord, you are granted a Medal of Valor by the King's herald.",
        "After saving a young boy from a vicious pack of wolves, you return him to his father's manor.  The grateful nobleman awards you with a Medal of Courage.",
        "After freeing a princess of a neighboring kingdom from the evil clutches of despicable slavers, she awards you with a Medal of Honor.",
        "Ridding the countryside of the hideous minotaur who made a sport of eating noblemen's knights, you are honored with the Medal of Distinction.",
        "You stumble upon a medal lying alongside the empty road.  Adding the medal to your inventory, you become aware that you have acquired the undesirable Fizbin of Misfortune, greatly decreasing your army's morale.",
        "During a sudden storm, a bolt of lightning strikes a tree, splitting it.  Inside the tree you find a mysterious mace.",
        "You encounter the infamous Black Knight!  After a grueling duel ending in a draw, the knight, out of respect, offers you a pair of armored gauntlets.",
        "A glint of golden light catches your eye.  Upon further investigation, you find a golden helm hidden under a bush.",
        "A clumsy Giant has killed himself with his own flail.  Knowing your superior skill with this weapon, you confidently remove the spectacular flail from the fallen giant.",
        "Walking through the ruins of an ancient walled city, you find the instrument of the city's destruction, an elaborately crafted ballista.",
        "A stone statue of a warrior holds a silver shield.  As you remove the shield, the statue crumbles into dust.",
        "As you are walking along a narrow path, a nearby bush suddenly bursts into flames.  Before your eyes the flames become the image of a beautiful woman.  She holds out a magnificent sword to you.",
        "You see a silver axe embedded deeply in the ground.  After several unsuccessful attempts by your army to remove the axe, you tightly grip the handle of the axe and effortlessly pull it free.",
        "A gang of rogues is sifting through the possessions of dead warriors.  Scaring off the scavengers, you note the rogues had overlooked a beautiful breastplate.",
        "Before you appears a levitating glass case with a scroll, perched upon a bed of crimson velvet.  At your touch, the lid opens and the scroll floats into your awaiting hands.",
        "Visiting a local wiseman, you explain the intent of your journey.  He reaches into a sack and withdraws a yellowed scroll and hands it to you.",
        "You come across the remains of an ancient Druid.  Bones, yellowed with age, peer from the ragged folds of her robe.  Searching the robe, you discover a scroll hidden in the folds.",
        "Mangled bones, yellowed with age, peer from the ragged folds of a dead Druid's robe.  Searching the robe, you discover a scroll hidden within.",
        "A little leprechaun dances gleefully around a magic sack.  Seeing you approach, he stops in mid-stride.  The little man screams and stamps his foot ferociously, vanishing into thin air.  Remembering the old leprechaun saying 'Finders Keepers', you grab the sack and leave.",
        "A noblewoman, separated from her traveling companions, asks for your help.  After escorting her home, she rewards you with a bag filled with gold.",
        "In your travels, you find a leather purse filled with gold that once belonged to a great warrior king who had the ability to transform any inanimate object into gold.",
        "A nomad trader seeks protection from a tribe of goblins.  For your assistance, he gives you a finely crafted pair of boots made from the softest leather.  Looking closely, you see fascinating ancient carvings engraved on the leather.",
        "Discovering a pair of beautifully beaded boots made from the finest and softest leather, you thank the anonymous donor and add the boots to your inventory.",
        "A traveling merchant offers you a rabbit's foot, made of gleaming silver fur, for safe passage.  The merchant explains the charm will increase your luck in combat.",
        "An ensnared unicorn whinnies in fright.  Murmuring soothing words, you set her free.  Snorting and stamping her front hoof once, she gallops off.  Looking down you see a golden horseshoe.",
        "You have captured a mischievous imp who has been terrorizing the region.  In exchange for his release, he rewards you with a magical coin.",
        "In the middle of a patch of dead and dry vegetation, to your surprise you find a healthy green four-leaf clover.",
        "An old man claiming to be an inventor asks you to try his latest invention.  He then hands you a compass.",
        "An old sea captain is being tortured by ogres.  You save him, and in return he rewards you with a wondrous instrument to measure the distance of a star.",
        "The Magic Book  ??????" };

    // FUN_004035e2: event text (or own text) in the HoMM1 message box at x 0x61 with up to two pictures.
    // Computer visit (FUN_004046c3, note v131): no windows; questions take the computer's answer.
    bool computerVisit = false;

    int showEvent( const std::string & text, const int style, const H1::UI::MessagePicture first = H1::UI::MessagePicture(),
                   const H1::UI::MessagePicture second = H1::UI::MessagePicture() )
    {
        if ( computerVisit ) {
            // FUN_004046c3 shows no window; the kinds handled here ask no question.
            return 0x8002;
        }
        return H1::UI::messageBox( text, style, 0x61, -1, first, second, false );
    }

    // Tile byte 9 of the original, kept in the engine tile's third metadata value with a mark (0 = not set by us).
    constexpr uint32_t valueMark = 0x48310000;

    bool hasValue( const Maps::Tile & tile )
    {
        return ( tile.metadata()[2] & 0xffff0000 ) == valueMark;
    }

    int objectValue( const Maps::Tile & tile )
    {
        return hasValue( tile ) ? static_cast<int>( tile.metadata()[2] & 0xff ) : -1;
    }

    void setObjectValue( Maps::Tile & tile, const int value )
    {
        tile.metadata()[2] = valueMark | static_cast<uint32_t>( value & 0xff );
    }

    // HoMM1 resource number (0 wood, 1 mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems, 6 gold) -> engine resource.
    int engineResource( const int r )
    {
        static const int t[7] = { Resource::WOOD, Resource::MERCURY, Resource::ORE, Resource::SULFUR, Resource::CRYSTAL, Resource::GEMS, Resource::GOLD };
        return ( r >= 0 && r < 7 ) ? t[r] : Resource::UNKNOWN;
    }

    // HoMM1 resource number of the engine resource on a tile (random resources), -1 none.
    int h1ResourceOfTile( const Maps::Tile & tile )
    {
        const int er = Maps::getFundsFromTile( tile ).getFirstValidResource().first;
        for ( int k = 0; k < 7; ++k ) {
            if ( engineResource( k ) == er ) {
                return k;
            }
        }
        return -1;
    }

    void giveResource( Heroes & hero, const int r, const int amount )
    {
        // FUN_00403875.
        hero.GetKingdom().AddFundsResource( Funds( engineResource( r ), static_cast<uint32_t>( amount ) ) );
    }

    const char * const resourceNames[7] = { "Wood", "Mercury", "Ore", "Sulfur", "Crystal", "Gems", "Gold" };

    // FUN_004038cf: the recruit window for the creature waiting on the tile, then the tile keeps what is left. Returns what is left.
    int recruitFromTile( Heroes & hero, const int creature, const int waiting )
    {
        const int count = H1::recruitWindow( creature, waiting, hero.GetColor(), false );
        if ( count <= 0 ) {
            return waiting;
        }
        const Monster monster( H1::creatureRow( creature ).monsterId );
        if ( !hero.GetArmy().CanJoinTroop( monster ) ) {
            // FUN_00467a4c: the same text as in a castle.
            H1::UI::messageBox( "There is no room in the garrison for this army.", H1::UI::MESSAGE_OKAY, 0xb1, 100 );
            return waiting;
        }
        int gold = 0;
        int resource = -1;
        H1::creaturePrice( creature, gold, resource );
        Funds cost( Resource::GOLD, static_cast<uint32_t>( gold * count ) );
        if ( resource >= 0 ) {
            cost += Funds( engineResource( resource ), static_cast<uint32_t>( count ) );
        }
        hero.GetKingdom().OddFundsResource( cost );
        hero.GetArmy().JoinTroop( monster, static_cast<uint32_t>( count ), false );
        return waiting - count;
    }

    // FUN_00403db9: a battle against "total" of a creature (HoMM1 number) spread over 5 stacks (total / 5 each, the rest one more in
    // the first stacks). Won: the experience of the battle. Lost: the hero is gone (FUN_004043fc: killfade). Returns true when won.
    // `creature2` / `total2` (extension mode only, H1-INTENDED(D16)): mixed guards, the first creature in troops 1, 3, 5 and the second in
    // troops 2, 4.
    bool guardedBattle( Heroes & hero, const int32_t tileIndex, const int creature, const int total, const int creature2 = -1, const int total2 = 0 )
    {
        Army guards;
        const Monster monster( H1::creatureRow( creature ).monsterId );
        if ( creature2 < 0 ) {
            for ( int k = 0; k < 5; ++k ) {
                const int count = total / 5 + ( k < total % 5 ? 1 : 0 );
                if ( count > 0 ) {
                    if ( Troop * troop = guards.GetTroop( static_cast<size_t>( k ) ) ) {
                        troop->Set( monster, static_cast<uint32_t>( count ) );
                    }
                }
            }
        }
        else {
            const Monster monster2( H1::creatureRow( creature2 ).monsterId );
            for ( int k = 0; k < 5; ++k ) {
                const bool first = ( k % 2 == 0 );
                const int count = first ? total / 3 + ( k / 2 < total % 3 ? 1 : 0 ) : total2 / 2 + ( k / 2 < total2 % 2 ? 1 : 0 );
                if ( count > 0 ) {
                    if ( Troop * troop = guards.GetTroop( static_cast<size_t>( k ) ) ) {
                        troop->Set( first ? monster : monster2, static_cast<uint32_t>( count ) );
                    }
                }
            }
        }
        const Battle::Result result = Battle::Loader( hero.GetArmy(), guards, tileIndex );
        if ( result.isAttackerWin() ) {
            hero.IncreaseExperience( result.getAttackerExperience() );
            return true;
        }
        H1::playSound( "killfade.82m" );
        // FUN_004043fc: the hero goes and the centre of the view dissolves (note v174).
        if ( H1::isGraphicsMode() ) {
            Interface::AdventureMap & adventureMap = Interface::AdventureMap::Get();
            H1::dissolveCentre( [&hero, &adventureMap]() {
                hero.setAlphaValue( 0 );
                adventureMap.redraw( Interface::REDRAW_GAMEAREA );
            } );
        }
        hero.Dismiss( static_cast<int>( result.getAttackerResult() ) );
        return false;
    }

    // FUN_00430052 for a computer hero: the guards split into 5 troops (count / 5 each, the rest one more in the first troops), then the quick
    // combat FUN_0042e947. Returns true when the hero won; left = the guards' count after the fight.
    int armyCount( const Army & army );

    bool computerFight( Heroes & hero, const int creature, const int total, int * left = nullptr )
    {
        Army guards;
        const Monster monster( H1::creatureRow( creature ).monsterId );
        for ( int k = 0; k < 5; ++k ) {
            const int count = total / 5 + ( k < total % 5 ? 1 : 0 );
            if ( count > 0 ) {
                if ( Troop * troop = guards.GetTroop( static_cast<size_t>( k ) ) ) {
                    troop->Set( monster, static_cast<uint32_t>( count ) );
                }
            }
        }
        const bool won = H1::Ai::quickCombat( hero, guards );
        if ( left != nullptr ) {
            *left = armyCount( guards );
        }
        return won;
    }

    // The guards left after a computer fight (FUN_00430052 returns their count).
    int armyCount( const Army & army )
    {
        int n = 0;
        for ( size_t k = 0; k < army.Size(); ++k ) {
            const Troop * troop = army.GetTroop( k );
            if ( troop != nullptr && troop->isValid() ) {
                n += static_cast<int>( troop->GetCount() );
            }
        }
        return n;
    }

    // FUN_00403766: a random artifact into the bag (none when it is full), or 1000 gold when no artifact is left to give.
    void randomArtifactOrGold( Heroes & hero )
    {
        const Artifact art( Artifact::Rand( Artifact::ART_LEVEL_ALL_NORMAL ) );
        if ( !art.isValid() ) {
            hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, 1000 ) );
            return;
        }
        hero.PickupArtifact( art );
    }

    // HoMM1 creature number of an engine monster, -1 none.
    int h1Creature( const Monster & monster )
    {
        for ( int c = 0; c < H1::creatureCount(); ++c ) { // H1-INTENDED(D16): 36 in extension games
            if ( H1::creatureRow( c ).monsterId == monster.GetID() ) {
                return c;
            }
        }
        return -1;
    }

    // FUN_004192e7: a wandering monster's count by creature.
    int monsterCount( const int creature )
    {
        static const int low[28] = { 30, 20, 20, 12, 8, 6, 25, 15, 20, 10, 7, 5, 20, 10, 15, 10, 8, 7, 20, 15, 10, 10, 6, 3, 20, 12, 10, 4 };
        static const int high[28] = { 80, 30, 30, 25, 16, 12, 40, 30, 35, 20, 10, 7, 40, 25, 30, 25, 15, 12, 50, 30, 25, 16, 8, 7, 40, 25, 20, 9 };
        if ( creature >= 28 && creature < 40 ) {
            // H1-INTENDED(D16): neutral creatures, like HoMM1 creatures of about the same strength [adapted]; 36..39 = v241.
            static const int extLow[12] = { 6, 20, 4, 25, 7, 7, 7, 7, 20, 12, 8, 2 };
            static const int extHigh[12] = { 12, 40, 8, 50, 14, 14, 14, 14, 35, 25, 16, 5 };
            return static_cast<int>( Rand::Get( extLow[creature - 28], extHigh[creature - 28] ) );
        }
        return ( creature >= 0 && creature < 28 ) ? static_cast<int>( Rand::Get( low[creature], high[creature] ) ) : 0;
    }

    H1::UI::MessagePicture picture( const int kind, const int value )
    {
        H1::UI::MessagePicture p;
        p.kind = kind;
        p.value = value;
        return p;
    }
}

namespace H1
{
    const char * eventText( const int id )
    {
        return ( id >= 0 && id < 0x4c ) ? eventTexts[id] : "";
    }

    void initObjectValues()
    {
        // FUN_00415a33 rows y, columns x (the same order of random numbers as the original; its rand stands as the engine's).
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                const int32_t index = y * 72 + x;
                const uint8_t * t = getTileBytes( index );
                if ( t == nullptr ) {
                    return;
                }
                Maps::Tile & tile = world.getTile( index );
                switch ( t[8] ) {
                case 0x84: // skeleton: 1 in 10 an artifact (2), else nothing (1)
                    setObjectValue( tile, Rand::Get( 0, 9 ) == 3 ? 2 : 1 );
                    break;
                case 0x86: // treasure chest: 2..4 (gold x 500 or experience (x 4 - 4) x 125)
                    setObjectValue( tile, static_cast<int>( Rand::Get( 2, 4 ) ) );
                    break;
                case 0x88: { // campfire: (4..6) << 4 | resource 0..5
                    const int high = static_cast<int>( Rand::Get( 4, 6 ) ) << 4;
                    setObjectValue( tile, high | static_cast<int>( Rand::Get( 0, 5 ) ) );
                    break;
                }
                case 0x98: // water wheel: 1 (500 gold) at the start
                    setObjectValue( tile, 1 );
                    break;
                case 0xbe: // random resource (the engine has chosen the kind): as a resource of that kind
                case 0x9d: { // resource: kind from the picture (byte 2, - 0x3d when above 4), amount wood / ore 8..16, gold 5..10, others 3..7
                    int r = t[2];
                    if ( r > 4 ) {
                        r -= 0x3d;
                    }
                    if ( t[8] == 0xbe ) {
                        r = h1ResourceOfTile( tile );
                    }
                    const int amount = ( r == 0 || r == 2 ) ? static_cast<int>( Rand::Get( 8, 16 ) )
                                                            : ( r == 6 ? static_cast<int>( Rand::Get( 5, 10 ) ) : static_cast<int>( Rand::Get( 3, 7 ) ) );
                    setObjectValue( tile, amount );
                    break;
                }
                case 0xad: // windmill: resource 1..5
                    setObjectValue( tile, static_cast<int>( Rand::Get( 1, 5 ) ) );
                    break;
                case 0x85: // daemon cave: rand 0..99 % 10 -> 0..2: 2, 3: 3, 4..6: 4, 7..9: 5
                case 0x8c: // graveyard: rand 0..99 % 10 -> 0..2: 2, 3..5: 3, 6..8: 4, 9: 5
                case 0xa3: { // shipwreck: as the graveyard when the tile to the left is a shipwreck part (0x23), else not active
                    if ( t[8] == 0xa3 ) {
                        const uint8_t * left = ( x > 0 ) ? getTileBytes( index - 1 ) : nullptr;
                        if ( left == nullptr || ( left[8] & 0x7f ) != 0x23 ) {
                            break;
                        }
                    }
                    const int r = static_cast<int>( Rand::Get( 0, 99 ) ) % 10;
                    if ( t[8] == 0x85 ) {
                        setObjectValue( tile, r <= 2 ? 2 : ( r == 3 ? 3 : ( r <= 6 ? 4 : 5 ) ) );
                    }
                    else {
                        setObjectValue( tile, r <= 2 ? 2 : ( r <= 5 ? 3 : ( r <= 8 ? 4 : 5 ) ) );
                    }
                    break;
                }
                case 0x9a: // wandering monster (creature = byte 2, or the engine's choice for random monsters)
                case 0xbf:
                case 0xc3:
                case 0xc4:
                case 0xc5:
                case 0xc6: {
                    // FUN_00415a33: with no count in the map, FUN_004192e7's count, and 26 in 100 are willing to join (bit 0x80) - not
                    // ghosts (26).
                    const int creature = ( t[8] == 0x9a ) ? t[2] : h1Creature( Maps::getMonsterFromTile( tile ) );
                    if ( creature < 0 ) {
                        break;
                    }
                    int v = t[9];
                    if ( t[8] != 0x9a || v == 0 ) {
                        v = monsterCount( creature );
                        if ( static_cast<int>( Rand::Get( 0, 99 ) ) < 0x1a && creature != 0x1a ) {
                            v |= 0x80;
                        }
                    }
                    setObjectValue( tile, v );
                    break;
                }
                case 0xbd: // random artifact (the engine has chosen the artifact): as an artifact
                case 0xb0: { // artifact: 0..99 % 10 -> 0..5: 1 (lying there), 6..7: 2 (rogues guard it), 8..9: 3 (a leprechaun sells it)
                    const int r = static_cast<int>( Rand::Get( 0, 99 ) ) % 10;
                    setObjectValue( tile, r <= 5 ? 1 : ( r <= 7 ? 2 : 3 ) );
                    break;
                }
                case 0xa7: // desert tents (nomads) 10..20
                    setObjectValue( tile, static_cast<int>( Rand::Get( 10, 20 ) ) );
                    break;
                case 0x8b: // genie lamp 2..5
                    setObjectValue( tile, static_cast<int>( Rand::Get( 0, 3 ) ) + 2 );
                    break;
                case 0xaa: { // rogues' wagon: 30..50 when the tiles left and right are wagon parts (0x2a), else not active
                    const uint8_t * left = ( x > 0 ) ? getTileBytes( index - 1 ) : nullptr;
                    const uint8_t * right = ( x < 71 ) ? getTileBytes( index + 1 ) : nullptr;
                    if ( left != nullptr && right != nullptr && ( left[8] & 0x7f ) == 0x2a && ( right[8] & 0x7f ) == 0x2a ) {
                        setObjectValue( tile, static_cast<int>( Rand::Get( 30, 50 ) ) );
                    }
                    break;
                }
                case 0x8d: // goblin hut 10..30
                    setObjectValue( tile, static_cast<int>( Rand::Get( 10, 30 ) ) );
                    break;
                case 0x8e: // thatched hut (peasants) 20..50
                case 0x91: // thatched hut (peasants) 20..50
                    setObjectValue( tile, static_cast<int>( Rand::Get( 20, 50 ) ) );
                    break;
                case 0x8f: // cottage (archers) 1..4 (rand 0..127 & 3, + 1)
                    setObjectValue( tile, ( static_cast<int>( Rand::Get( 0, 127 ) ) & 3 ) + 1 );
                    break;
                case 0x90: // cottage (dwarves) 1..3 (rand 0..98 % 3, + 1)
                    setObjectValue( tile, static_cast<int>( Rand::Get( 0, 98 ) ) % 3 + 1 );
                    break;
                default:
                    break;
                }
            }
        }
    }

    void newWeekObjectValues()
    {
        for ( int32_t y = 0; y < 72; ++y ) {
            for ( int32_t x = 0; x < 72; ++x ) {
                const int32_t index = y * 72 + x;
                const uint8_t * t = getTileBytes( index );
                if ( t == nullptr ) {
                    return;
                }
                Maps::Tile & tile = world.getTile( index );
                if ( !hasValue( tile ) ) {
                    continue;
                }
                if ( t[8] == 0x98 && objectValue( tile ) != 0xff ) {
                    setObjectValue( tile, 2 );
                }
                else if ( t[8] == 0xad ) {
                    setObjectValue( tile, static_cast<int>( Rand::Get( 1, 5 ) ) );
                }
                else if ( t[8] == 0xa7 && objectValue( tile ) < 100 ) {
                    setObjectValue( tile, objectValue( tile ) + static_cast<int>( Rand::Get( 1, 3 ) ) );
                }
                else if ( t[8] == 0xaa && objectValue( tile ) < 100 ) {
                    setObjectValue( tile, objectValue( tile ) + static_cast<int>( Rand::Get( 3, 6 ) ) );
                }
                else if ( t[8] >= 0x8d && t[8] <= 0x91 && objectValue( tile ) < 100 ) {
                    // Dwellings grow while below 100: goblins + 3..6, peasants + 5..10, archers / dwarves + 2..4.
                    static const int low[5] = { 3, 5, 2, 2, 5 };
                    static const int high[5] = { 6, 10, 4, 4, 10 };
                    const int k = t[8] - 0x8d;
                    setObjectValue( tile, objectValue( tile ) + static_cast<int>( Rand::Get( low[k], high[k] ) ) );
                }
            }
        }
    }

    void spawnMonthMonsters( const int creature )
    {
        if ( getTileBytes( 0 ) == nullptr || creature < 0 || creature >= H1::creatureCount() ) { // H1-INTENDED(D16): 28..35 too
            return;
        }
        const Monster monster( creatureRow( creature ).monsterId );
        bool placed = false;
        // FUN_0045932d(x, y): x outer, y inner, as the original.
        for ( int32_t x = 0; x < 72; ++x ) {
            for ( int32_t y = 0; y < 72; ++y ) {
                const int32_t index = y * 72 + x;
                const uint8_t * t = getTileBytes( index );
                if ( t == nullptr ) {
                    return;
                }
                Maps::Tile & tile = world.getTile( index );
                // Byte 8 = 0: no object (an object taken off the map counts as none, as the original clears byte 8); ground tile / 20 above
                // 0: not water (DAT_004a7d68).
                const bool noObject = ( t[8] == 0 ) || objectGoneOnTile( index );
                if ( !noObject || t[0] / 20 == 0 ) {
                    continue;
                }
                if ( Rand::Get( 0, 0x168 ) != 10 ) {
                    continue;
                }
                // [port] The engine keeps one object per tile: a tile where a hero or a boat stands now is left out.
                if ( tile.getMainObjectType( true ) != MP2::OBJ_NONE && tile.getMainObjectType( true ) != MP2::OBJ_COAST ) {
                    continue;
                }
                std::array<uint8_t, 10> bytes{};
                std::copy( t, t + 10, bytes.begin() );
                bytes[8] = 0x9a;
                bytes[1] = 0xc;
                bytes[2] = static_cast<uint8_t>( creature );
                bytes[9] = static_cast<uint8_t>( monsterCount( creature ) );
                patchTileBytes( index, bytes );
                Maps::setMonsterOnTile( tile, monster, bytes[9] );
                setObjectValue( tile, bytes[9] );
                placed = true;
            }
        }
        if ( placed ) {
            world.resetPathfinder();
        }
    }


    // ---- H1-INTENDED(D16) extension objects (h1_ext_objects.h, note v240) ----
    namespace
    {
        int extensionCreatureOfKind( const int kind )
        {
            switch ( kind ) {
            case EXT_KIND_RUINS:
                return 28;
            case EXT_KIND_EXCAVATION:
                return 29;
            case EXT_KIND_CITY_OF_DEAD:
                return 30;
            case EXT_KIND_HALFLING_HOLE:
                return 31;
            default:
                return -1;
            }
        }

        // A land tile with no object. `coastOk`: also a shore tile (byte 8 = 0x1f, the editor's landing mark, note v39), which keeps
        // its mark; used for the side parts of an object, never for its entrance.
        bool freeTile( const int32_t x, const int32_t y, const bool forObject, const bool coastOk = false )
        {
            if ( x < 1 || y < 1 || x > 70 || y > 70 ) {
                return false;
            }
            const int32_t index = y * 72 + x;
            const uint8_t * t = getTileBytes( index );
            if ( t == nullptr || t[0] / 20 == 0 || ( t[8] != 0 && !( coastOk && t[8] == 0x1f ) ) || ( t[7] & 0x80 ) != 0 ) {
                return false;
            }
            if ( forObject && ( t[2] != 0xff || t[4] != 0xff ) ) {
                return false;
            }
            const Maps::Tile & tile = world.getTile( index );
            return tile.getMainObjectType( true ) == MP2::OBJ_NONE && !tile.isWater();
        }

        bool placeOne( const ExtObjectDef & def, const int32_t x, const int32_t y, std::vector<int32_t> & placed )
        {
            const Maps::ObjectInfo & info = *def.info;
            for ( const int32_t other : placed ) {
                if ( std::abs( other % 72 - x ) < 5 && std::abs( other / 72 - y ) < 4 ) {
                    return false;
                }
            }
            for ( const auto & part : info.groundLevelParts ) {
                const bool entrance = ( part.tileOffset.x == 0 && part.tileOffset.y == 0 );
                if ( !freeTile( x + part.tileOffset.x, y + part.tileOffset.y, true, !entrance ) ) {
                    return false;
                }
            }
            for ( const auto & part : info.topLevelParts ) {
                const int32_t tx = x + part.tileOffset.x;
                const int32_t ty = y + part.tileOffset.y;
                const uint8_t * t = ( tx >= 0 && ty >= 0 && tx < 72 && ty < 72 ) ? getTileBytes( ty * 72 + tx ) : nullptr;
                if ( t == nullptr || !freeTile( tx, ty, true, true ) ) {
                    return false;
                }
            }
            // A free tile in front (below) of the entrance, so it can be reached.
            if ( !freeTile( x, y + 1, false, true ) ) {
                return false;
            }
            std::map<int32_t, std::array<uint8_t, 10>> bytes;
            const auto bytesOf = [&bytes]( const int32_t index ) -> std::array<uint8_t, 10> & {
                auto it = bytes.find( index );
                if ( it == bytes.end() ) {
                    std::array<uint8_t, 10> b{};
                    const uint8_t * t = getTileBytes( index );
                    std::copy( t, t + 10, b.begin() );
                    it = bytes.emplace( index, b ).first;
                }
                return it->second;
            };
            for ( size_t k = 0; k < info.groundLevelParts.size(); ++k ) {
                const auto & part = info.groundLevelParts[k];
                const int32_t index = ( y + part.tileOffset.y ) * 72 + x + part.tileOffset.x;
                std::array<uint8_t, 10> & b = bytesOf( index );
                const bool flat = ( part.layerType == Maps::SHADOW_LAYER || part.layerType == Maps::TERRAIN_LAYER );
                if ( b[2] == 0xff ) {
                    b[1] = static_cast<uint8_t>( ( b[1] & 0xf0 ) | extensionObjectSet );
                    b[2] = static_cast<uint8_t>( def.groundPart[k] );
                    if ( flat ) {
                        b[6] = static_cast<uint8_t>( b[6] | 0x80 );
                    }
                }
                else {
                    b[3] = static_cast<uint8_t>( ( b[3] & 0xf0 ) | extensionObjectSet );
                    b[4] = static_cast<uint8_t>( def.groundPart[k] );
                }
                if ( k == 0 ) {
                    b[8] = static_cast<uint8_t>( 0x80 | def.kind );
                }
                else if ( !flat ) {
                    b[7] = static_cast<uint8_t>( b[7] | 0x80 );
                }
            }
            for ( size_t k = 0; k < info.topLevelParts.size(); ++k ) {
                const auto & part = info.topLevelParts[k];
                std::array<uint8_t, 10> & b = bytesOf( ( y + part.tileOffset.y ) * 72 + x + part.tileOffset.x );
                b[3] = static_cast<uint8_t>( ( b[3] & 0xf0 ) | extensionObjectSet );
                b[4] = static_cast<uint8_t>( def.topPart[k] );
            }
            const int32_t action = y * 72 + x;
            Maps::Tile & tile = world.getTile( action );
            if ( !Maps::setObjectOnTile( tile, info, true ) ) {
                return false;
            }
            for ( const auto & [index, b] : bytes ) {
                patchTileBytes( index, b );
            }
            const int creature = extensionCreatureOfKind( def.kind );
            if ( creature >= 0 ) {
                setObjectValue( tile, H1::creatureRow( creature ).growth );
            }
            else if ( def.kind == EXT_KIND_MARKET ) {
                std::array<uint8_t, 10> b = bytesOf( action );
                const int c = H1::firstExtensionCreature + static_cast<int>( Rand::Get( 0, H1::extensionCreatureCount - 1 ) );
                b[5] = static_cast<uint8_t>( c );
                patchTileBytes( action, b );
                setObjectValue( tile, H1::creatureRow( c ).growth );
            }
            else {
                setObjectValue( tile, 1 ); // guarded site: 1 = not yet taken
            }
            placed.push_back( action );
            return true;
        }
    }

    void placeExtensionObjects()
    {
        if ( getTileBytes( 0 ) == nullptr || !H1::isExtension() ) {
            return;
        }
        // A quarter of the map's monsters become neutral creatures (owner 2026-11-18 08:34, plan "가").
        for ( int32_t index = 0; index < 72 * 72; ++index ) {
            const uint8_t * t = getTileBytes( index );
            if ( t == nullptr || ( t[8] != 0x9a && !( t[8] == 0xbf || ( t[8] >= 0xc3 && t[8] <= 0xc6 ) ) ) || objectGoneOnTile( index ) ) {
                continue;
            }
            Maps::Tile & tile = world.getTile( index );
            if ( tile.getMainObjectType( false ) != MP2::OBJ_MONSTER || Rand::Get( 0, 3 ) != 0 ) {
                continue;
            }
            const int creature = H1::firstExtensionCreature + static_cast<int>( Rand::Get( 0, H1::extensionCreatureCount - 1 ) );
            std::array<uint8_t, 10> bytes{};
            std::copy( t, t + 10, bytes.begin() );
            bytes[8] = 0x9a;
            bytes[1] = static_cast<uint8_t>( ( bytes[1] & 0xf0 ) | 0xc );
            bytes[2] = static_cast<uint8_t>( creature );
            bytes[9] = static_cast<uint8_t>( monsterCount( creature ) );
            patchTileBytes( index, bytes );
            Maps::setMonsterOnTile( tile, Monster( H1::creatureRow( creature ).monsterId ), bytes[9] );
            setObjectValue( tile, bytes[9] );
        }
        // New objects on free land: 2 of each dwelling, 2 markets, 2 pyramids (fewer when no room is found).
        std::vector<int32_t> placed;
        // Biggest objects first, while there is still room for them.
        std::vector<const ExtObjectDef *> order;
        for ( const ExtObjectDef & def : extensionObjectDefinitions() ) {
            order.push_back( &def );
        }
        std::stable_sort( order.begin(), order.end(), []( const ExtObjectDef * a, const ExtObjectDef * b ) {
            return a->groundPart.size() + a->topPart.size() > b->groundPart.size() + b->topPart.size();
        } );
        for ( const ExtObjectDef * defp : order ) {
            const ExtObjectDef & def = *defp;
            if ( def.info == nullptr ) {
                continue;
            }
            // Every free land tile in a random order (random tries miss on maps that are mostly water).
            std::vector<int32_t> candidates;
            for ( int32_t i = 0; i < 72 * 72; ++i ) {
                if ( freeTile( i % 72, i / 72, true ) ) {
                    candidates.push_back( i );
                }
            }
            for ( size_t i = candidates.size(); i > 1; --i ) {
                std::swap( candidates[i - 1], candidates[Rand::Get( 0, static_cast<uint32_t>( i - 1 ) )] );
            }
            int done = 0;
            for ( size_t i = 0; i < candidates.size() && done < 2; ++i ) {
                if ( placeOne( def, candidates[i] % 72, candidates[i] / 72, placed ) ) {
                    ++done;
                }
            }
        }
        world.resetPathfinder();
    }

    void extensionObjectsNewWeek( const int weekCreature )
    {
        if ( !H1::isExtension() ) {
            return;
        }
        for ( const auto & [index, b] : tilePatches() ) {
            const int kind = b[8] & 0x7f;
            if ( ( b[8] & 0x80 ) == 0 ) {
                continue;
            }
            Maps::Tile & tile = world.getTile( index );
            const int creature = extensionCreatureOfKind( kind );
            if ( creature >= 0 ) {
                // HoMM2 dwellings on the map gather the creature's growth every week (+5 in that creature's week) [adapted].
                const int grow = H1::creatureRow( creature ).growth + ( weekCreature == creature ? 5 : 0 );
                setObjectValue( tile, std::min( 255, std::max( 0, objectValue( tile ) ) + grow ) );
            }
            else if ( kind == EXT_KIND_MARKET ) {
                std::array<uint8_t, 10> nb = b;
                const int c = H1::firstExtensionCreature + static_cast<int>( Rand::Get( 0, H1::extensionCreatureCount - 1 ) );
                nb[5] = static_cast<uint8_t>( c );
                patchTileBytes( index, nb );
                setObjectValue( tile, H1::creatureRow( c ).growth + ( weekCreature == c ? 5 : 0 ) );
            }
        }
    }

    int currentByte9( const int32_t tileIndex )
    {
        if ( !Maps::isValidAbsIndex( tileIndex ) ) {
            return -1;
        }
        return objectValue( world.getTile( tileIndex ) );
    }

    // FUN_00459928: the text of the map quick info window (qwikinfo.bin item 1) for a tile.
    std::string tileQuickInfoText( const int32_t tileIndex )
    {
        if ( tileIndex < 0 || tileIndex >= world.w() * world.h() ) {
            return "\n\nBorder";
        }
        const Maps::Tile & mapTile = world.getTile( tileIndex );
        const PlayerColor viewer = Settings::Get().CurrentColor();
        if ( viewer != PlayerColor::NONE && mapTile.isFog( viewer ) ) {
            return "\n\nUncharted territory";
        }
        const std::vector<std::array<uint8_t, 10>> tiles = gameTileBytes();
        if ( tiles.empty() ) {
            return "";
        }
        const std::array<uint8_t, 10> & t = tiles[static_cast<size_t>( tileIndex )];
        // Tables 0x493240 (terrain, through 0x4a7d68 = ground tile / 20), 0x493280 (object kind names).
        static const char * const terrainNames[7] = { "Ocean", "Grass", "Snow", "Swamp", "Lava", "Desert", "Dirt" };
        static const char * const objectNames[64]
            = { "",          "Alchemist Lab", "Signpost",   "Buoy",        "Skeleton",    "Daemon Cave", "Treasure Chest", "Faerie Ring",
                "Campfire",  "Fountain",      "Gazebo",     "Ancient Lamp", "Graveyard",  "Straw Hut",   "House",          "Cabin",
                "Log Cabin", "Log Cabin",     "Inn 1",      "Inn 2",       "Inn 3",       "Inn 4",       "Dragon City",    "Lighthouse",
                "Waterwheel", "Mine",         "Army Camp",  "Obelisk",     "Oasis",       "Resource",    "Rosebush",       "Sandpit",
                "Sawmill",   "Shrine",        "Shrine",     "Shipwreck",   "Statue",      "Tree Stump",  "Swan Pond",      "Desert Tent",
                "Town",      "Stone Liths",   "Wagon Camp", "Well",        "Whirlpool",   "Windmill",    "Oak Tree",       "Megalith",
                "Artifact",  "Nothing here",  "",           "",            "Mountains",   "Mountains",   "Mountains",      "Mountains",
                "Trees",     "Trees",         "Trees",      "Trees",       "Trees",       "",            "Ship",           "" };
        // FUN_0045cd58 with 1: count words.
        auto countWords = []( const int count ) {
            return count < 5 ? "A few" : ( count < 10 ? "Several" : ( count < 20 ? "A pack of" : ( count < 50 ? "Lots of" : ( count < 100 ? "A Horde of" : "Zounds..." ) ) ) );
        };
        const int kind = ( t[8] == 0xbf || ( t[8] >= 0xc3 && t[8] <= 0xc6 ) ) ? 0x1a : ( t[8] & 0x7f );
        switch ( kind ) {
        case 0:
        case 0x1f:
        case 0x32:
            return std::string( "\n\n" ) + terrainNames[std::min( t[0] / 20, 6 )];
        case 0x19: {
            // Mine: resource of the mine record (world +0x14343 + tile byte 9 x 7) [the engine mine's resource stands in for the record].
            const int32_t mainIndex = Maps::Tile::getIndexOfMainTile( mapTile );
            const Maps::Tile & mineTile = world.getTile( mainIndex >= 0 ? mainIndex : tileIndex );
            const int er = Maps::getDailyIncomeObjectResources( mineTile ).getFirstValidResource().first;
            for ( int k = 0; k < 7; ++k ) {
                if ( engineResource( k ) == er ) {
                    return std::string( "\n\n" ) + resourceNames[k] + " Mine";
                }
            }
            return "\n\nMine";
        }
        case 0x1a: {
            int creature = t[2];
            if ( t[8] != 0x9a ) {
                creature = h1Creature( Maps::getMonsterFromTile( mapTile ) );
            }
            const int value = objectValue( mapTile );
            return std::string( "\n\n" ) + countWords( ( value < 0 ? 0 : value ) & 0x7f ) + " " + H1::creatureNameMany( creature );
        }
        case 0x1d: {
            // Table 0x49316c indexed by the picture number (0x3d + resource) = resource names 0x493260.
            const int r = static_cast<int>( t[2] ) - 0x3d;
            return std::string( "\n\n" ) + ( ( r >= 0 && r < 7 ) ? resourceNames[r] : "" );
        }
        case 0x30:
            return "\n\nArtifact";
        case EXT_KIND_RUINS: // H1-INTENDED(D16) extension objects
        case EXT_KIND_EXCAVATION:
        case EXT_KIND_CITY_OF_DEAD:
        case EXT_KIND_HALFLING_HOLE:
        case EXT_KIND_MARKET:
        case EXT_KIND_PYRAMID:
        case EXT_KIND_BARROW:
            return std::string( "\n\n" ) + H1::extensionObjectName( kind );
        case 0x33: {
            // [Kind 0x33 has no name in the table; table 0x493268 (resource names from ore) by tile byte 2: meaning unconfirmed.]
            static const char * const fromOre[6] = { "Ore", "Sulfur", "Crystal", "Gems", "Gold", "" };
            return std::string( "\n\n" ) + ( t[2] < 6 ? fromOre[t[2]] : "" );
        }
        default:
            return std::string( "\n\n" ) + ( kind < 64 ? objectNames[kind] : "" );
        }
    }

    // FUN_004046c3 case 0x3d: an own hero -> nothing; a human's hero -> the real battle FUN_00405c7f (the engine's); else the quick combat
    // against the hero, and when it won and the beaten hero was in a castle, a second quick combat against the garrison that takes the castle.
    VisitResult computerMeetsHero( Heroes & hero, Heroes & other )
    {
        if ( other.GetColor() == hero.GetColor() ) {
            return VisitResult::Handled;
        }
        if ( other.isControlHuman() ) {
            return VisitResult::NotHandled;
        }
        Castle * castle = other.inCastleMutable();
        const bool won = H1::Ai::quickCombat( hero, other.GetArmy(), nullptr, &other );
        if ( won && castle != nullptr && hero.isActive() ) {
            H1::Ai::quickCombat( hero, castle->GetArmy(), castle );
        }
        return VisitResult::Handled;
    }

    namespace
    {
        std::map<int, int> heroBonusBits; // engine hero id -> hero +0xae bonus bits
    }

    bool hasBonusBit( const Heroes & hero, const int bit )
    {
        const auto it = heroBonusBits.find( hero.GetID() );
        return it != heroBonusBits.end() && ( it->second & bit ) != 0;
    }

    void setBonusBit( const Heroes & hero, const int bit )
    {
        heroBonusBits[hero.GetID()] |= bit;
    }

    void clearBonusBits( const Heroes & hero )
    {
        heroBonusBits.erase( hero.GetID() );
    }

    int bonusMorale( const Heroes & hero )
    {
        const auto it = heroBonusBits.find( hero.GetID() );
        if ( it == heroBonusBits.end() ) {
            return 0;
        }
        const int b = it->second;
        return ( ( b & 2 ) ? 1 : 0 ) + ( ( b & 8 ) ? 1 : 0 ) + ( ( b & 0x100 ) ? 2 : 0 ) - ( ( b & 0x20 ) ? 1 : 0 ) - ( ( b & 0x40 ) ? 1 : 0 );
    }

    int bonusLuck( const Heroes & hero )
    {
        const auto it = heroBonusBits.find( hero.GetID() );
        if ( it == heroBonusBits.end() ) {
            return 0;
        }
        return ( ( it->second & 0x10 ) ? 1 : 0 ) + ( ( it->second & 4 ) ? 1 : 0 );
    }

    std::string bonusSavedState()
    {
        std::ostringstream out;
        out << "herobonus " << heroBonusBits.size();
        for ( const auto & [id, bits] : heroBonusBits ) {
            out << ' ' << id << ' ' << bits;
        }
        return out.str();
    }

    void loadBonusSavedState( const std::string & line )
    {
        heroBonusBits.clear();
        std::istringstream in( line );
        std::string tag;
        size_t n = 0;
        if ( !( in >> tag >> n ) || tag != "herobonus" ) {
            return;
        }
        for ( size_t i = 0; i < n; ++i ) {
            int id = 0;
            int bits = 0;
            if ( !( in >> id >> bits ) ) {
                break;
            }
            heroBonusBits[id] = bits;
        }
    }

    void resetBonusBits()
    {
        heroBonusBits.clear();
    }

    MP2::MapObjectType effectiveObjectType( const int32_t tileIndex, const MP2::MapObjectType engineType )
    {
        if ( MP2::isInGameActionObject( engineType ) || !H1::isHoMM1Map() ) {
            return engineType;
        }
        switch ( activeObjectKind( tileIndex ) ) {
        case 0x2c:
            return MP2::OBJ_WHIRLPOOL;
        case 0x29:
            return MP2::OBJ_STONE_LITHS;
        case 0x1c:
            return MP2::OBJ_OASIS;
        default:
            return engineType;
        }
    }

    int32_t teleportTarget( const int32_t from )
    {
        const int kind = activeObjectKind( from );
        if ( kind != 0x29 && kind != 0x2c ) {
            return from;
        }
        const int fx = from % 72;
        const int fy = from / 72;
        const int minDistance = ( kind == 0x29 ) ? 1 : 3;
        std::vector<int32_t> candidates;
        for ( int y = 0; y < 72; ++y ) {
            for ( int x = 0; x < 72; ++x ) {
                const int32_t index = y * 72 + x;
                // The original compares tile byte 8 with kind | 0x80: a tile with a hero on it (0xbd) does not count.
                if ( activeObjectKind( index ) != kind || world.getTile( index ).getHero() != nullptr ) {
                    continue;
                }
                if ( std::abs( x - fx ) + std::abs( y - fy ) > minDistance ) {
                    candidates.push_back( index );
                }
            }
        }
        if ( candidates.empty() ) {
            return from;
        }
        const size_t pick = ( candidates.size() > 1 ) ? static_cast<size_t>( Rand::Get( 1, static_cast<uint32_t>( candidates.size() ) ) ) - 1 : 0;
        return candidates[pick];
    }

    void whirlpoolLoss( Heroes & hero )
    {
        if ( !hero.GetKingdom().isControlHuman() || Rand::Get( 1, 3 ) != 1 ) {
            return;
        }
        Army & army = hero.GetArmy();
        int weakest = -1;
        int smallest = 99999999;
        int troops = 0;
        for ( size_t k = 0; k < army.Size(); ++k ) {
            const Troop * troop = army.GetTroop( k );
            if ( troop == nullptr || !troop->isValid() ) {
                continue;
            }
            ++troops;
            const int creature = h1Creature( troop->GetMonster() );
            const int worth = ( creature >= 0 ? H1::creatureWorthOf( creature ) : 0 ) * static_cast<int>( troop->GetCount() );
            if ( worth < smallest ) {
                smallest = worth;
                weakest = static_cast<int>( k );
            }
        }
        if ( weakest < 0 ) {
            return;
        }
        Troop * troop = army.GetTroop( static_cast<size_t>( weakest ) );
        const uint32_t count = troop->GetCount();
        if ( troops < 2 ) {
            if ( count > 1 ) {
                troop->SetCount( count / 2 );
            }
        }
        else if ( count / 2 == 0 ) {
            troop->Reset();
        }
        else {
            troop->SetCount( count / 2 );
        }
    }

    VisitResult visitObjectComputer( Heroes & hero, const int32_t tileIndex )
    {
        // FUN_004046c3 for the kinds ported so far: no window; chest (FUN_0042f82c) and hiring (FUN_0042e76d) decide by the computer's values
        // (note v149); fights without a human are the quick combat FUN_0042e947 (note v150), castles without a hero (note v151), other heroes (note v152). Boats and
        // teleports stay with the engine's computer visit [AC8].
        const uint8_t * tile = getTileBytes( tileIndex );
        if ( tile == nullptr ) {
            return VisitResult::NotHandled;
        }
        // Another hero on the tile (the original's object 0x3d).
        if ( Heroes * other = world.getTile( tileIndex ).getHero(); other != nullptr && other != &hero ) {
            return computerMeetsHero( hero, *other );
        }
        // Only active tiles (byte 8 bit 0x80) are visited in the original (FUN_004046c3 is called for them); the engine also passed the
        // other parts of an object here (for example the right half of a mine, which then was taken as a second mine; note v194).
        if ( ( tile[8] & 0x80 ) == 0 || objectGoneOnTile( tileIndex ) ) {
            return VisitResult::NotHandled;
        }
        const int kind = ( tile[8] == 0xbe ) ? 0x1d : ( tile[8] & 0x7f );
        static const int allowed[] = { 0x03, 0x1c, 0x07, 0x09, 0x24, 0x08, 0x0a, 0x18, 0x2d, 0x1d, 0x04, 0x1b, 0x22, 0x06, 0x0b, 0x0d, 0x0e, 0x0f,
                                       0x10, 0x11, 0x27, 0x2a, 0x01, 0x19, 0x20, 0x17, 0x02, 0x21, 0x05, 0x0c, 0x23, 0x16, 0x1a, 0x30, 0x28 };
        if ( std::find( std::begin( allowed ), std::end( allowed ), kind ) == std::end( allowed ) ) {
            return VisitResult::NotHandled;
        }
        computerVisit = true;
        const VisitResult result = visitObject( hero, tileIndex );
        computerVisit = false;
        if ( std::getenv( "H1_AI_LOG" ) != nullptr ) {
            std::fprintf( stderr, "H1AI visit kind 0x%x at %d result %d\n", kind, tileIndex, static_cast<int>( result ) );
        }
        return result;
    }

    VisitResult visitObject( Heroes & hero, const int32_t tileIndex )
    {
        const uint8_t * tile = getTileBytes( tileIndex );
        if ( tile == nullptr ) {
            return VisitResult::NotHandled;
        }
        Maps::Tile & mapTile = world.getTile( tileIndex );
        const int value = objectValue( mapTile );
        // Random placeholders (0xbd artifact, 0xbe resource) became real objects at new game: the engine's choice stands for the kind.
        const int kind = ( tile[8] == 0xbd ) ? 0x30
                         : ( tile[8] == 0xbe ? 0x1d : ( ( tile[8] == 0xbf || ( tile[8] >= 0xc3 && tile[8] <= 0xc6 ) ) ? 0x1a : ( tile[8] & 0x7f ) ) );
        const MP2::MapObjectType objectType = effectiveObjectType( tileIndex, world.getTile( tileIndex ).getMainObjectType( false ) );
        switch ( kind ) {
        case 0x03: // Buoy: morale +1 until the next battle (hero +0xae bit 2), once.
        case 0x1c: // Oasis: the same with bit 8 (no movement bonus in HoMM1).
        case 0x07: // Faerie ring: luck +1 (bit 0x10).
        case 0x09: // Fountain: luck +1 (bit 4).
        case 0x24: { // Statue: morale +2 (bit 0x100).
            const int bit = ( kind == 0x03 ) ? 2 : ( kind == 0x1c ? 8 : ( kind == 0x07 ? 0x10 : ( kind == 0x09 ? 4 : 0x100 ) ) );
            const bool visited = hasBonusBit( hero, bit );
            int textId = 0;
            H1::UI::MessagePicture first;
            H1::UI::MessagePicture second;
            if ( kind == 0x03 ) {
                textId = visited ? 2 : 3;
                first = picture( 0xc, 0 );
            }
            else if ( kind == 0x1c ) {
                textId = visited ? 0x34 : 0x35;
                first = picture( 0xc, 0 );
            }
            else if ( kind == 0x07 ) {
                textId = visited ? 0xc : 0xd;
                first = picture( 10, 0 );
            }
            else if ( kind == 0x09 ) {
                textId = visited ? 0xf : 0x10;
                first = picture( 10, 0 );
            }
            else {
                textId = visited ? 0x3e : 0x3d;
                first = picture( 0xc, 0 );
                second = picture( 0xc, 0 );
            }
            if ( visited ) {
                showEvent( eventText( textId ), H1::UI::MESSAGE_OKAY );
            }
            else {
                setBonusBit( hero, bit );
                showEvent( eventText( textId ), H1::UI::MESSAGE_OKAY, first, second );
            }
            return VisitResult::Handled;
        }
        case 0x0a: { // Gazebo: 1000 experience once per hero and gazebo (hero +0x39 bit = gazebo number, byte 9), then FUN_00448e23.
            if ( hero.isVisited( world.getTile( tileIndex ) ) ) {
                showEvent( eventText( 0x11 ), H1::UI::MESSAGE_OKAY );
            }
            else {
                showEvent( eventText( 0x12 ), H1::UI::MESSAGE_OKAY, picture( 0xe, 1000 ) );
                hero.SetVisited( tileIndex );
                hero.IncreaseExperience( 1000 );
            }
            return VisitResult::Handled;
        }
        case 0x04: { // Skeleton (value 1 nothing, 2 an artifact; then 1).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( value == 2 ) {
                if ( hero.IsFullBagArtifacts() ) {
                    // Bag full (14): "Treasure." with a 1000 gold picture - the original gives no gold here (as written).
                    showEvent( "Treasure.", H1::UI::MESSAGE_OKAY, picture( 6, 1000 ) );
                }
                else {
                    // FUN_00403766: a random artifact [the engine's random artifact stands in for the original pick].
                    const Artifact art( Artifact::Rand( Artifact::ART_LEVEL_ALL_NORMAL ) );
                    hero.PickupArtifact( art );
                    const int h1Art = H1::h1ArtifactFromEngine( art.GetID() );
                    // "%s %s" (0x49009c) with text 0x493540 and the artifact name.
                    showEvent( std::string( "Skeleton\n\nYou come upon the remains of an unfortunate adventurer.  Searching through the tattered "
                                            "clothing, you find" )
                                   + " " + art.GetName(),
                               H1::UI::MESSAGE_OKAY, picture( 7, h1Art ) );
                }
                setObjectValue( mapTile, 1 );
            }
            else {
                showEvent( eventText( 0x4b ), H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case 0x06: { // Treasure chest: keep gold (value x 500) or give it to the peasants for experience ((value x 4 - 4) x 125).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int gold = value * 500;
            const int exp = ( value * 4 - 4 ) * 125;
            H1::UI::MessagePicture g = picture( 6, gold );
            H1::UI::MessagePicture e = picture( 0xe, exp );
            if ( computerVisit ) {
                // FUN_004046c3 case 6: FUN_0042f82c decides.
                if ( H1::Ai::computerTakesGold( hero, gold, exp ) ) {
                    giveResource( hero, 6, gold );
                }
                else {
                    hero.IncreaseExperience( static_cast<uint32_t>( exp ) );
                }
            }
            else if ( H1::UI::messageBox( eventText( 0xb ), H1::UI::MESSAGE_YES_NO, 0x61, -1, g, e, true ) == 0x8005 ) {
                giveResource( hero, 6, gold );
            }
            else {
                hero.IncreaseExperience( static_cast<uint32_t>( exp ) );
            }
            return VisitResult::HandledRemove;
        }
        case 0x08: { // Campfire: gold (value >> 4) x 100 and (value >> 4) of resource (value & 15).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int count = value >> 4;
            const int r = value & 0xf;
            showEvent( eventText( 0xe ), H1::UI::MESSAGE_OKAY, picture( 6, count * 100 ), picture( r, count ) );
            giveResource( hero, 6, count * 100 );
            giveResource( hero, r, count );
            return VisitResult::HandledRemove;
        }
        case 0x18: { // Water wheel: value 0 nothing, else value x 500 gold, then 0.
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( value == 0 ) {
                showEvent( eventText( 0x29 ), H1::UI::MESSAGE_OKAY );
            }
            else {
                showEvent( eventText( 0x2a ), H1::UI::MESSAGE_OKAY, picture( 6, value * 500 ) );
                giveResource( hero, 6, value * 500 );
                setObjectValue( mapTile, 0 );
            }
            return VisitResult::Handled;
        }
        case 0x2d: { // Windmill: value < 7 = 2 of resource (value), then 99.
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( value < 7 ) {
                showEvent( eventText( 0x45 ), H1::UI::MESSAGE_OKAY, picture( value, 2 ) );
                giveResource( hero, value, 2 );
                setObjectValue( mapTile, 99 );
            }
            else {
                showEvent( eventText( 0x44 ), H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case 0x0b: { // Genie lamp: rub it? then genies (27) to recruit; the lamp goes when none are left.
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( computerVisit ) {
                // FUN_004046c3: no question; FUN_0042e76d decides how many (paid); the lamp goes when none are left.
                const int left = value - ( value != 0 ? H1::Ai::computerHire( hero, 27, value, false ) : 0 );
                setObjectValue( mapTile, left );
                return left == 0 ? VisitResult::HandledRemove : VisitResult::Handled;
            }
            if ( showEvent( eventText( 0x13 ), H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
                return VisitResult::Handled;
            }
            const int left = recruitFromTile( hero, 27, value );
            setObjectValue( mapTile, left );
            return left == 0 ? VisitResult::HandledRemove : VisitResult::Handled;
        }
        case 0x27: // Desert tents: nomads (25). 0x2a Rogues' wagon: rogues (24).
        case 0x2a: {
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const bool tents = ( kind == 0x27 );
            if ( computerVisit ) {
                if ( value != 0 ) {
                    setObjectValue( mapTile, value - H1::Ai::computerHire( hero, tents ? 25 : 24, value, false ) );
                }
                return VisitResult::Handled;
            }
            if ( value == 0 ) {
                showEvent( eventText( tents ? 0x3f : 0x41 ), H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            if ( showEvent( eventText( tents ? 0x40 : 0x42 ), H1::UI::MESSAGE_YES_NO ) == 0x8005 ) {
                setObjectValue( mapTile, recruitFromTile( hero, tents ? 25 : 24, value ) );
            }
            return VisitResult::Handled;
        }
        case 0x0c: // Graveyard and 0x23 shipwreck: search? value 1 = nothing and morale -1 until the next battle, else ghosts (26).
        case 0x23: {
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const bool graveyard = ( kind == 0x0c );
            if ( computerVisit ) {
                // FUN_004301c9: value 2..5 -> 5 troops of 2 / 3 / 5 / 10 ghosts; fight when FUN_0042f88d gives more than 0 for the reward
                // trunc(gold x g) (value 5: + the average artifact value); won: the gold (value 5 also FUN_00403766), then value 1.
                // Value 1 does nothing for the computer (no morale loss).
                if ( value >= 2 && value <= 5 ) {
                    static const int per[4] = { 2, 3, 5, 10 };
                    static const int gold[4] = { 1000, 2000, 5000, 2000 };
                    const H1::Ai::State & st = H1::Ai::state();
                    const float g = static_cast<float>( st.resourceValue[6] );
                    const int reward = static_cast<int>( gold[value - 2] * static_cast<double>( g ) + ( value == 5 ? st.averageArtifact : 0.0f ) );
                    Army ghosts;
                    const Monster ghost( H1::creatureRow( 26 ).monsterId );
                    for ( int k = 0; k < 5; ++k ) {
                        ghosts.GetTroop( static_cast<size_t>( k ) )->Set( ghost, static_cast<uint32_t>( per[value - 2] ) );
                    }
                    if ( H1::Ai::fightWorth( hero, ghosts, reward ) != 0 && H1::Ai::quickCombat( hero, ghosts ) ) {
                        giveResource( hero, 6, gold[value - 2] );
                        if ( value == 5 && hero.isActive() ) {
                            randomArtifactOrGold( hero );
                        }
                        setObjectValue( mapTile, 1 );
                    }
                }
                return VisitResult::Handled;
            }
            if ( showEvent( eventText( graveyard ? 0x14 : 0x3a ), H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
                return VisitResult::Handled;
            }
            if ( value == 1 ) {
                showEvent( eventText( graveyard ? 0x15 : 0x3b ), H1::UI::MESSAGE_OKAY, picture( 0xd, 0 ) );
                // Hero +0xae bit 0x20 (graveyard) / 0x40 (shipwreck): morale -1 until the next battle, once.
                setBonusBit( hero, graveyard ? 0x20 : 0x40 );
                return VisitResult::Handled;
            }
            // FUN_004039bb: 2 -> 10 ghosts, 1000 gold; 3 -> 15, 2000; 4 -> 25, 5000; else 50 ghosts, 2000 gold and an artifact.
            const int ghosts = ( value == 2 ) ? 10 : ( value == 3 ? 15 : ( value == 4 ? 25 : 50 ) );
            if ( !guardedBattle( hero, tileIndex, 26, ghosts ) ) {
                return VisitResult::Handled;
            }
            const char * text = eventText( graveyard ? 0x16 : 0x3c );
            if ( value >= 2 && value <= 4 ) {
                const int gold = ( value == 2 ) ? 1000 : ( value == 3 ? 2000 : 5000 );
                showEvent( text, H1::UI::MESSAGE_OKAY, picture( 6, gold ) );
                giveResource( hero, 6, gold );
            }
            else {
                H1::UI::MessagePicture second;
                if ( !hero.IsFullBagArtifacts() ) {
                    const Artifact art( Artifact::Rand( Artifact::ART_LEVEL_ALL_NORMAL ) );
                    hero.PickupArtifact( art );
                    second = picture( 7, H1::h1ArtifactFromEngine( art.GetID() ) );
                }
                showEvent( text, H1::UI::MESSAGE_OKAY, picture( 6, 2000 ), second );
                giveResource( hero, 6, 2000 );
            }
            setObjectValue( mapTile, 1 );
            return VisitResult::Handled;
        }
        case 0x05: { // Daemon cave (case 5 of FUN_00401000).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( computerVisit ) {
                // FUN_004046c3 case 5: no question. 2: 1000 experience; 3: also FUN_00403766; 4: also 2500 gold; 5: under 2500 gold the
                // hero is devoured (FUN_004043fc), else it pays (FUN_0042f9bd always says yes). Then value 1 in every case.
                switch ( value ) {
                case 2:
                    hero.IncreaseExperience( 1000 );
                    break;
                case 3:
                    hero.IncreaseExperience( 1000 );
                    randomArtifactOrGold( hero );
                    break;
                case 4:
                    hero.IncreaseExperience( 1000 );
                    giveResource( hero, 6, 2500 );
                    break;
                case 5:
                    if ( hero.GetKingdom().GetFunds().gold < 2500 ) {
                        setObjectValue( mapTile, 1 );
                        hero.Dismiss( 0 );
                        return VisitResult::Handled;
                    }
                    hero.GetKingdom().OddFundsResource( Funds( Resource::GOLD, 2500 ) );
                    break;
                default:
                    break;
                }
                setObjectValue( mapTile, 1 );
                return VisitResult::Handled;
            }
            if ( showEvent( eventText( 0x0a ), H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
                return VisitResult::Handled;
            }
            int v = value;
            if ( v == 3 && hero.IsFullBagArtifacts() ) {
                v = 4; // no room for the artifact: the 2500 gold case
            }
            switch ( v ) {
            case 1:
                showEvent( eventText( 4 ), H1::UI::MESSAGE_OKAY );
                break;
            case 2:
                hero.IncreaseExperience( 1000 );
                showEvent( eventText( 5 ), H1::UI::MESSAGE_OKAY, picture( 0xe, 1000 ) );
                break;
            case 3: {
                hero.IncreaseExperience( 1000 );
                const Artifact art( Artifact::Rand( Artifact::ART_LEVEL_ALL_NORMAL ) );
                hero.PickupArtifact( art );
                showEvent( eventText( 6 ), H1::UI::MESSAGE_OKAY, picture( 7, H1::h1ArtifactFromEngine( art.GetID() ) ), picture( 0xe, 1000 ) );
                break;
            }
            case 4:
                showEvent( eventText( 7 ), H1::UI::MESSAGE_OKAY, picture( 6, 2500 ), picture( 0xe, 1000 ) );
                hero.IncreaseExperience( 1000 );
                giveResource( hero, 6, 2500 );
                break;
            default: {
                // Captured: pay 2500 or be devoured (FUN_004043fc); the daemon stays (the value is not changed).
                const bool pay = showEvent( eventText( 8 ), H1::UI::MESSAGE_YES_NO ) == 0x8005;
                if ( pay && hero.GetKingdom().GetFunds().gold >= 2500 ) {
                    hero.GetKingdom().OddFundsResource( Funds( Resource::GOLD, 2500 ) );
                }
                else {
                    if ( pay ) {
                        showEvent( eventText( 9 ), H1::UI::MESSAGE_OKAY );
                    }
                    H1::playSound( "killfade.82m" );
                    hero.Dismiss( 0 );
                }
                return VisitResult::Handled;
            }
            }
            setObjectValue( mapTile, 1 );
            return VisitResult::Handled;
        }
        case 0x30: { // Artifact (byte 2 = artifact number).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int h1Art = ( tile[8] == 0xbd ) ? H1::h1ArtifactFromEngine( Maps::getArtifactFromTile( mapTile ).GetID() ) : tile[2];
            const int engineArt = H1::engineArtifactFromH1( h1Art );
            if ( engineArt < 0 ) {
                return VisitResult::NotHandled;
            }
            const Artifact art( engineArt );
            if ( computerVisit ) {
                // FUN_004046c3 case 0x30: 2 = 50 rogues (24) by FUN_00430052, lost -> the artifact stays; 3 = FUN_0042f954 buys for 2000,
                // else the leprechaun goes with it. FUN_004036bd puts it into the first free bag place (nothing when the bag is full).
                if ( value == 2 && !computerFight( hero, 24, 50 ) ) {
                    return VisitResult::Handled;
                }
                if ( value == 3 ) {
                    if ( !H1::Ai::computerBuysArtifact( hero, h1Art, 2000 ) ) {
                        return VisitResult::HandledRemove;
                    }
                    hero.GetKingdom().OddFundsResource( Funds( Resource::GOLD, 2000 ) );
                }
                if ( hero.isActive() ) {
                    hero.PickupArtifact( art );
                }
                return VisitResult::HandledRemove;
            }
            if ( hero.IsFullBagArtifacts() ) {
                // FUN_00449117 == 14.
                H1::UI::messageBox( "You cannot pick up this artifact.", H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            const std::string name = ( h1Art >= 0 && h1Art < 37 ) ? std::string( art.GetName() ) : std::string();
            const auto take = [&]( const std::string & text ) {
                // FUN_004036bd: into the first free bag place.
                showEvent( text, H1::UI::MESSAGE_OKAY, picture( 7, h1Art ) );
                hero.PickupArtifact( art );
            };
            if ( value == 1 ) {
                // H1-INTENDED(D16): extension items 38..63 have HoMM2's find text.
                take( ( h1Art >= 0 && h1Art < 38 ) ? artifactTexts[h1Art] : H1::extensionArtifactFindText( h1Art ) );
                return VisitResult::HandledRemove;
            }
            if ( value == 2 ) {
                // Rogues (24), 50 of them (FUN_00403db9 with 0x18, 0x32).
                showEvent( eventText( 0x46 ), H1::UI::MESSAGE_OKAY );
                if ( !guardedBattle( hero, tileIndex, 24, 50 ) ) {
                    return VisitResult::Handled;
                }
                std::string text = eventText( 0x4a );
                text.replace( text.find( "%s" ), 2, name );
                take( text );
                return VisitResult::HandledRemove;
            }
            // value 3: the leprechaun's offer for 2000 gold.
            std::string offer = eventText( 0x47 );
            offer.replace( offer.find( "%s" ), 2, name );
            if ( showEvent( offer, H1::UI::MESSAGE_YES_NO, picture( 7, h1Art ) ) == 0x8005 ) {
                if ( hero.GetKingdom().GetFunds().gold >= 2000 ) {
                    hero.GetKingdom().OddFundsResource( Funds( Resource::GOLD, 2000 ) );
                    hero.PickupArtifact( art );
                    return VisitResult::HandledRemove;
                }
                showEvent( eventText( 0x49 ), H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            // Refused: the leprechaun vanishes; the artifact is taken off with killfade (FUN_004045b0 with 0).
            showEvent( eventText( 0x48 ), H1::UI::MESSAGE_OKAY );
            return VisitResult::HandledRemoveKillfade;
        }
        case 0x1a: { // Wandering monster (FUN_00405804).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int creature = ( tile[8] == 0x9a ) ? tile[2] : h1Creature( Maps::getMonsterFromTile( mapTile ) );
            if ( creature < 0 ) {
                return VisitResult::NotHandled;
            }
            const int count = value & 0x7f;
            const Monster monster( H1::creatureRow( creature ).monsterId );
            if ( computerVisit ) {
                // FUN_0040599e: willing and the army (FUN_0042e009 without corrections) stronger than worth x count x 1.75: FUN_0042e76d
                // decides, and when it says yes the whole group joins (nothing happens when it says no). Else the fight FUN_00430052: the
                // group keeps the count left; won -> removed.
                if ( ( value & 0x80 ) != 0
                     && static_cast<double>( H1::creatureWorthOf( creature ) * count ) * 1.75 < H1::armyStrengthOf( hero.GetArmy(), &hero ) ) {
                    if ( H1::Ai::computerHire( hero, creature, count, true, count ) > 0 ) {
                        return VisitResult::HandledRemoveKillfade;
                    }
                    return VisitResult::Handled;
                }
                int left = 0;
                if ( computerFight( hero, creature, count, &left ) ) {
                    return VisitResult::HandledRemoveKillfade;
                }
                setObjectValue( mapTile, ( value & 0x80 ) | left );
                return VisitResult::Handled;
            }
            // Willing (bit 0x80), the hero's army stronger than worth x count x 1.75 (0x48d000), and room: they offer to join.
            if ( ( value & 0x80 ) != 0 && static_cast<double>( H1::creatureWorthOf( creature ) * count ) * 1.75 < H1::armyStrengthOf( hero.GetArmy(), &hero )
                 && hero.GetArmy().CanJoinTroop( monster ) ) {
                std::string text = "Followers\n\nA group of %s with a desire for greater glory wish to join you. Do you accept? ";
                text.replace( text.find( "%s" ), 2, H1::creatureNameMany( creature ) );
                if ( showEvent( text, H1::UI::MESSAGE_YES_NO ) == 0x8005 ) {
                    hero.GetArmy().JoinTroop( monster, static_cast<uint32_t>( count ), false );
                    return VisitResult::HandledRemoveKillfade;
                }
                showEvent( eventText( 0x31 ), H1::UI::MESSAGE_OKAY );
            }
            if ( guardedBattle( hero, tileIndex, creature, count ) ) {
                return VisitResult::HandledRemoveKillfade;
            }
            return VisitResult::Handled;
        }
        case 0x16: { // Dragon city (case 0x16): not ours -> attack? dragons (23), 5 of them (20 under a world condition, see note v120);
            // won: ours (mine list slot 0), 1000 gold a day.
            if ( hero.isFriends( Maps::getColorFromTile( mapTile ) ) ) {
                return VisitResult::Handled;
            }
            if ( computerVisit ) {
                // FUN_004046c3 case 0x16: 5 troops of 1 dragon; FUN_0042f88d with the reward 500 must give more than 0, then FUN_00430052
                // with 5 dragons; won -> ours (FUN_00416df1).
                Army dragons;
                const Monster dragon( H1::creatureRow( 23 ).monsterId );
                for ( int k = 0; k < 5; ++k ) {
                    dragons.GetTroop( static_cast<size_t>( k ) )->Set( dragon, 1 );
                }
                if ( H1::Ai::fightWorth( hero, dragons, 500 ) != 0 && computerFight( hero, 23, 5 ) ) {
                    Maps::setColorOnTile( mapTile, hero.GetColor() );
                }
                return VisitResult::Handled;
            }
            if ( showEvent( eventText( 0x26 ), H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
                return VisitResult::Handled;
            }
            if ( guardedBattle( hero, tileIndex, 23, 5 ) ) {
                Maps::setColorOnTile( mapTile, hero.GetColor() );
                Interface::AdventureMap::Get().redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );
                showEvent( eventText( 0x27 ), H1::UI::MESSAGE_OKAY, picture( 6, -1000 ) );
            }
            return VisitResult::Handled;
        }
        case EXT_KIND_RUINS: // H1-INTENDED(D16) extension objects (h1_ext_objects.h)
        case EXT_KIND_EXCAVATION:
        case EXT_KIND_CITY_OF_DEAD:
        case EXT_KIND_HALFLING_HOLE:
        case EXT_KIND_MARKET: {
            if ( computerVisit || value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int creature = ( kind == EXT_KIND_MARKET ) ? tile[5] : extensionCreatureOfKind( kind );
            if ( creature < H1::firstExtensionCreature || creature >= H1::firstExtensionCreature + H1::extensionCreatureCount ) {
                return VisitResult::Handled;
            }
            const std::string name = H1::extensionObjectName( kind );
            if ( value == 0 ) {
                showEvent( name + "\n\nNo creatures are left here this week.", H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            const std::string text = ( kind == EXT_KIND_MARKET )
                                         ? name + "\n\n" + H1::creatureNameMany( creature ) + " are for sale this week. Do you want to recruit them?"
                                         : name + "\n\nA group of " + H1::creatureNameMany( creature ) + " lives here. Do you want to recruit them?";
            if ( showEvent( text, H1::UI::MESSAGE_YES_NO ) == 0x8005 ) {
                setObjectValue( mapTile, recruitFromTile( hero, creature, value ) );
            }
            return VisitResult::Handled;
        }
        case EXT_KIND_BARROW: { // H1-INTENDED(D16), note v243: 20 zombies and 10 mummies; 1500 gold and an unused item, then empty [made here].
            if ( computerVisit || value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( value == 0 ) {
                showEvent( "Barrow Mounds\n\nThe barrow mounds have been plundered.", H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            if ( showEvent( "Barrow Mounds\n\nZombies and mummies guard these old graves. Do you want to search them?", H1::UI::MESSAGE_YES_NO )
                 != 0x8005 ) {
                return VisitResult::Handled;
            }
            if ( guardedBattle( hero, tileIndex, 36, 20, 37, 10 ) ) {
                setObjectValue( mapTile, 0 );
                hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, 1500 ) );
                const int art = H1::chooseRandomArtifact();
                std::string reward = "In the graves you find 1500 gold";
                if ( art >= 0 && hero.GetBagArtifacts().PushArtifact( Artifact( H1::engineArtifactFromH1( art ) ) ) ) {
                    reward += std::string( " and the " ) + H1::artifactName( art );
                }
                showEvent( reward + ".", H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case EXT_KIND_PYRAMID: {
            if ( computerVisit || value < 0 ) {
                return VisitResult::NotHandled;
            }
            if ( value == 0 ) {
                showEvent( "Pyramid\n\nThe pyramid is empty.", H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            if ( showEvent( "Pyramid\n\nPower liches and skeletons guard this ancient pyramid. Do you want to enter?", H1::UI::MESSAGE_YES_NO )
                 != 0x8005 ) {
                return VisitResult::Handled;
            }
            // Guards: 30 skeletons and 10 power liches (HoMM2's pyramid has mummies, which HoMM1 lacks) [made here]; reward 2000 gold and an
            // unused item.
            if ( guardedBattle( hero, tileIndex, 29, 30, 30, 10 ) ) {
                setObjectValue( mapTile, 0 );
                hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, 2000 ) );
                const int art = H1::chooseRandomArtifact();
                std::string reward = "In the burial chamber you find 2000 gold";
                if ( art >= 0 && hero.GetBagArtifacts().PushArtifact( Artifact( H1::engineArtifactFromH1( art ) ) ) ) {
                    reward += std::string( " and the " ) + H1::artifactName( art );
                }
                showEvent( reward + ".", H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case 0x28: { // Castle: the computer's visit FUN_004305d7 (people use the castle window of the engine).
            if ( !computerVisit ) {
                return VisitResult::NotHandled;
            }
            Castle * castle = world.getCastleEntrance( Maps::GetPoint( tileIndex ) );
            if ( castle == nullptr || castle->GetHero() != nullptr ) {
                return VisitResult::NotHandled; // a hero in the castle is the hero visit (0x3d) [AC8]
            }
            if ( castle->GetColor() != hero.GetColor() ) {
                // No defenders (FUN_0043c38b): taken (FUN_00416ad0) - the engine's capture does the same. A human owner: the real battle
                // FUN_00405c7f - the engine's battle. Else the quick combat FUN_0042e947 against the garrison.
                if ( !castle->GetArmy().isValid() || castle->isControlHuman() ) {
                    return VisitResult::NotHandled;
                }
                H1::Ai::quickCombat( hero, castle->GetArmy(), castle );
                return VisitResult::Handled;
            }
            // Ours and the hero at the entrance: FUN_0042eca6 (spell book, guild spells, army split).
            if ( hero.GetIndex() == tileIndex ) {
                H1::Ai::ownCastleActions( hero, *castle );
            }
            return VisitResult::Handled;
        }
        case 0x0d: // Dwellings (FUN_00403c9b): goblin hut, thatched hut, cottage (archers), cottage (dwarves), thatched hut.
        case 0x0e:
        case 0x0f:
        case 0x10:
        case 0x11: {
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            const int k = kind - 0xd;
            if ( computerVisit ) {
                // FUN_004046c3: FUN_0042e76d (free) decides how many join.
                static const int h1Creatures[5] = { 6, 0, 1, 0xd, 0 };
                if ( value != 0 ) {
                    setObjectValue( mapTile, value - H1::Ai::computerHire( hero, h1Creatures[k], value, true ) );
                }
                return VisitResult::Handled;
            }
            // Creatures 6 goblin, 0 peasant, 1 archer, 13 dwarf, 0 peasant.
            static const int creatures[5] = { Monster::GOBLIN, Monster::PEASANT, Monster::ARCHER, Monster::DWARF, Monster::PEASANT };
            if ( value == 0 ) {
                showEvent( eventText( k * 3 + 0x19 ), H1::UI::MESSAGE_OKAY );
                return VisitResult::Handled;
            }
            if ( showEvent( eventText( k * 3 + 0x17 ), H1::UI::MESSAGE_YES_NO ) != 0x8005 ) {
                return VisitResult::Handled;
            }
            // All of them join (FUN_00467af9 with the whole count), or "ranks full" (FUN_00467a4c).
            const Troop troop( Monster( creatures[k] ), static_cast<uint32_t>( value ) );
            if ( !hero.GetArmy().CanJoinTroop( troop.GetMonster() ) ) {
                showEvent( eventText( k * 3 + 0x18 ), H1::UI::MESSAGE_OKAY );
            }
            else {
                hero.GetArmy().JoinTroop( troop );
                setObjectValue( mapTile, 0 );
            }
            return VisitResult::Handled;
        }
        case 0x01: // Alchemist (mercury 1 a day), 0x19 mine, 0x20 sawmill (wood 2 a day): taken when not ours (FUN_00416df1), no guards.
        case 0x19:
        case 0x20: {
            if ( hero.isFriends( Maps::getColorFromTile( mapTile ) ) ) {
                return VisitResult::Handled;
            }
            Maps::setColorOnTile( mapTile, hero.GetColor() );
            Interface::AdventureMap & adventureMap = Interface::AdventureMap::Get();
            adventureMap.getRadar().SetRenderArea( { Maps::GetPoint( tileIndex ).x - 2, Maps::GetPoint( tileIndex ).y - 1, 5, 2 } );
            adventureMap.redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );
            if ( kind == 0x01 ) {
                showEvent( eventText( 0 ), H1::UI::MESSAGE_OKAY, picture( 1, -1 ) );
            }
            else if ( kind == 0x20 ) {
                showEvent( eventText( 0x37 ), H1::UI::MESSAGE_OKAY, picture( 0, -2 ) );
            }
            else {
                // Mine kind (mine list +2): text kind + 0x29, picture kind with -(1000 gold / 2 ore / 1 other) a day.
                int r = H1::getMineKind( tileIndex );
                if ( r < 0 ) {
                    // The entrance is not the listed tile: the engine's income resource stands in.
                    const int er = Maps::getDailyIncomeObjectResources( mapTile ).getFirstValidResource().first;
                    for ( int k = 0; k < 7; ++k ) {
                        if ( engineResource( k ) == er ) {
                            r = k;
                        }
                    }
                }
                if ( r >= 2 && r <= 6 ) {
                    const int perDay = ( r == 6 ) ? 1000 : ( r == 2 ? 2 : 1 );
                    showEvent( eventText( r + 0x29 ), H1::UI::MESSAGE_OKAY, picture( r, -perDay ) );
                }
            }
            return VisitResult::Handled;
        }
        case 0x17: { // Lighthouse: one owner for all lighthouses (world +0x1443e), taken when not ours.
            if ( hero.isFriends( Maps::getColorFromTile( mapTile ) ) ) {
                return VisitResult::Handled;
            }
            Maps::setColorOnTile( mapTile, hero.GetColor() );
            Interface::AdventureMap::Get().redraw( Interface::REDRAW_GAMEAREA | Interface::REDRAW_RADAR );
            showEvent( eventText( 0x28 ), H1::UI::MESSAGE_OKAY );
            return VisitResult::Handled;
        }
        case 0x1b: { // Obelisk: once per player (bit per player of world +0x145a5[byte 9]), opens pieces and shows the puzzle (FUN_0045e89c).
            if ( H1::obeliskCount() <= 0 ) {
                return VisitResult::NotHandled;
            }
            Kingdom & kingdom = hero.GetKingdom();
            if ( !hero.isVisited( mapTile, Visit::GLOBAL ) ) {
                hero.SetVisited( tileIndex, Visit::GLOBAL );
                H1::openObeliskPieces( kingdom.PuzzleMaps() );
                // FUN_00411ac3 ends with FUN_0041173d: the computer's guess of the digging tile for this player.
                H1::Ai::digGuess( Color::GetIndex( hero.GetColor() ) );
                showEvent( eventText( 0x32 ), H1::UI::MESSAGE_OKAY );
                if ( !computerVisit ) {
                    kingdom.PuzzleMaps().ShowMapsDialog();
                }
            }
            else {
                showEvent( eventText( 0x33 ), H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case 0x21: // Seer's shrine: text 0x38, then the thieves' guild window with 8 guilds titled "Shrine - Player Rankings".
            if ( computerVisit ) {
                return VisitResult::Handled; // FUN_004046c3 case 0x21: nothing
            }
            showEvent( eventText( 0x38 ), H1::UI::MESSAGE_OKAY );
            H1::showThievesGuildWindow( hero.GetColor(), 8, "Shrine - Player Rankings" );
            return VisitResult::Handled;
        case 0x02: { // Signpost: "%s is near." with the nearest castle's name (FUN_0046e4f0 finds it) [straight distance stands in].
            if ( computerVisit ) {
                return VisitResult::Handled; // FUN_004046c3 case 2: nothing
            }
            const Castle * nearest = nullptr;
            int32_t best = 0;
            const fheroes2::Point at = Maps::GetPoint( tileIndex );
            for ( int slot = 0; slot < 36; ++slot ) {
                int32_t cx = 0;
                int32_t cy = 0;
                if ( !H1::getListPosition( false, slot, cx, cy ) ) {
                    continue;
                }
                const Castle * c = world.getCastleEntrance( { cx, cy } );
                if ( c == nullptr ) {
                    c = world.getCastle( { cx, cy } );
                }
                if ( c == nullptr ) {
                    continue;
                }
                const fheroes2::Point p = c->GetCenter();
                const int32_t d = ( p.x - at.x ) * ( p.x - at.x ) + ( p.y - at.y ) * ( p.y - at.y );
                if ( nearest == nullptr || d < best ) {
                    nearest = c;
                    best = d;
                }
            }
            if ( nearest != nullptr ) {
                showEvent( std::string( "Signpost\n\nA signpost reads:\n\n" ) + nearest->GetName() + " is near.", H1::UI::MESSAGE_OKAY );
            }
            return VisitResult::Handled;
        }
        case 0x1d: { // Resource: kind = picture - 0x3d, amount = value (gold x 100); the status window shows it (FUN_00425c5a).
            if ( value < 0 ) {
                return VisitResult::NotHandled;
            }
            int r = tile[2];
            if ( r > 4 ) {
                r -= 0x3d;
            }
            if ( tile[8] == 0xbe ) {
                r = h1ResourceOfTile( mapTile );
            }
            if ( r < 0 ) {
                return VisitResult::NotHandled;
            }
            const int amount = ( r == 6 ) ? value * 100 : value;
            giveResource( hero, r, amount );
            // [The status window message "You find a small quantity of <name>." for 5 seconds: the engine status panel stands in.]
            ( void )resourceNames;
            return VisitResult::HandledRemove;
        }
        default:
            return VisitResult::NotHandled;
        }
    }
}
