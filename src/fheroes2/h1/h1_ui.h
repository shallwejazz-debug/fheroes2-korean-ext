// HoMM1 dialog layouts (.bin) and fonts (homm1-remaster project). Format: note v51/v52, tools/format/h1bin.py.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fheroes2
{
    class Image;
    class Sprite;
}

namespace H1::UI
{
    enum : int
    {
        ITEM_IMAGE = 1, // .bmp picture or colour fill (FUN_0047d410)
        ITEM_BUTTON = 2, // FUN_0047ebd0
        ITEM_TEXT = 8, // FUN_0047cf50
        ITEM_ICON = 0x10, // FUN_0047bb70
        ITEM_AREA = 0x20, // FUN_0047dcc0: x, y, w, h, id, flags (not drawn by itself)
        ITEM_STRIP = 0x40, // FUN_0047ead0
        ITEM_EDIT = 0x100 // FUN_0047ddd0 (also 0x201 / 0x202 in the file): text entry field
    };

    // Item state bits (+0x16 of every item, FUN_0047f2d0 starts it at 6; message cmd 5 sets bits, cmd 6 clears them).
    enum : int
    {
        STATE_ACTIVE = 2, // takes clicks [meaning from use: the file list clears it on OK when nothing is selected]
        STATE_VISIBLE = 4, // drawn (cmd 2 draws only with this bit)
        STATE_GREYED = 8 // drawn, then darkened with the table at 0x4a3e2c (FUN_00479366)
    };

    struct Item
    {
        int type = 0;
        int32_t x = 0;
        int32_t y = 0;
        int32_t w = 0;
        int32_t h = 0;
        std::string name; // .bmp (image), .icn (button / icon) or .fnt (text)
        int frame = 0; // icon frame, or button released frame
        int pressedFrame = 0;
        int hotkey = 0xffff; // DOS scan code (buttons)
        int colour = 0; // text or fill colour: DOS palette index
        int align = 0; // text: 0 left, 1 centre, 2 right
        std::string text;
        int id = 0xffff;
        int flags = 0;
        bool hidden = false; // STATE_VISIBLE cleared (message cmd 6 with 4; cmd 5 with 4 shows it again)
        bool greyed = false; // STATE_GREYED set
        bool inactive = false; // STATE_ACTIVE cleared
        int maxLength = 0; // edit field
        bool mirror = false; // icon byte +0x26: drawn flipped left-right, ending at x (FUN_0047a360)
        int monoColour = 0; // icon byte +0x27: colour of a one-colour icon (flags 0x80, FUN_0047be00)
        std::shared_ptr<const fheroes2::Image> picture; // H1-INTENDED(D16): ITEM_IMAGE with flags 0x1000 draws this (extension windows)
    };

    struct Layout
    {
        int32_t width = 0;
        int32_t height = 0;
        std::vector<Item> items;
        // Icon (or text) ids that close runLayoutDialog on a left click like a button (the original handlers react to clicks on these icons,
        // e.g. FUN_0042299f for the portraits of rcrthero.bin).
        std::vector<int> clickableIcons;

        Item * find( int id );
    };

    // Reads "<name>.bin" from the HoMM1 AGG (all item types of the 75 files are read, note v54).
    Layout loadLayout( const std::string & name );

    // HoMM1 .bmp: u16 0x21, u16 width, u16 height, raw palette indices.
    fheroes2::Image readBmp( const std::string & name );

    // Text in a box with word wrap and alignment, as FUN_0047ab10 / FUN_0047aa30 do it. colour is a palette index of the DOS palette
    // (DOS FUN_0008df44 passes it straight to the drawing routine; the Windows table 0x492b18 only converts DOS indices to the Windows palette).
    void drawText( fheroes2::Image & dst, const std::string & text, const std::string & fontName, int32_t x, int32_t y, int32_t w, int32_t h, int colour,
                   int align );

    // Draws the pixels of a one-colour (mono) frame in a palette colour.
    void drawMonoSprite( fheroes2::Image & dst, const fheroes2::Sprite & sprite, int32_t x, int32_t y, uint8_t colour );

    // Width in pixels of a text in a font (sum of glyph width + 1, FUN_0047b000).
    int32_t textWidth( const std::string & text, const std::string & fontName );

    // Number of lines a text takes in a width (FUN_0047ade0).
    int countLines( const std::string & text, const std::string & fontName, int32_t width );

    // Message styles of FUN_00422fc2 (second argument).
    enum : int
    {
        MESSAGE_OKAY = 1,
        MESSAGE_YES_NO = 2,
        MESSAGE_OKAY_CANCEL = 3, // both buttons at the same place in evntwin*.bin [as in the original]
        MESSAGE_POPUP = 4, // no button, closes when the mouse button is let go (right-click help)
        MESSAGE_OKAY_ONLY = 5,
        MESSAGE_CANCEL_ONLY = 6
    };

    // HoMM1 message box (FUN_00422fc2 without the picture arguments): evntwin<n>.bin, n from the text height. x / y = -1 for the
    // original default place (menus: x 0xb1). Draws over what the display shows now and puts it back. Returns the button id
    // (0x8001 Cancel, 0x8002 OK, 0x8005 Yes, 0x8006 No; DOS ids) or 0 for a popup.
    int messageBox( const std::string & text, int style, int32_t x = -1, int32_t y = -1 );

    // Pictures of FUN_00422fc2 (arguments 5..8, note v113): kind 0..6 resource (value > 0 "%d", < 0 "%d/day"), 7 artifact, 8 spell,
    // 9 crest (brcrest.icn), 10 luck, 11..13 morale pictures (expmrl.icn kind - 10), 14 experience ("%d"), 15 hero portrait
    // (surrendr.icn 4 + port%04d.icn). kind -1 = none.
    struct MessagePicture
    {
        int kind = -1;
        int value = 0;
    };

    // Same with up to two pictures under the text; orBetween writes "or" between them (argument 9).
    int messageBox( const std::string & text, int style, int32_t x, int32_t y, MessagePicture first, MessagePicture second, bool orBetween = false );

    // Shows a layout at (x, y) of the 640 x 480 screen over what the display shows now, waits for a click on a visible button (or Enter / Esc
    // hotkeys 28 / 1; popup: until the mouse button is let go), puts the screen back and returns the button id (0 for a popup).
    int runLayoutDialog( const Layout & layout, int32_t x, int32_t y, bool popup );

    // Same, and a right click on a visible button shows help( id ) as a popup at (helpX, -1) when help returns a text.
    int runLayoutDialog( const Layout & layout, int32_t x, int32_t y, const std::function<const char *( int )> & help, int32_t helpX );

    // Same, the help popup with a picture under its text (FUN_00422fc2 arguments 5 / 6).
    int runLayoutDialog( const Layout & layout, int32_t x, int32_t y, const std::function<const char *( int )> & help, int32_t helpX,
                         const std::function<MessagePicture( int )> & helpPicture );

    // Same as the non-popup form, and calls tick() on every event loop pass (the dialog handler of the original, e.g. FUN_0041853a);
    // the layout is drawn again when tick returns true (tick may change the layout it captured).
    int runLayoutDialogTicking( const Layout & layout, int32_t x, int32_t y, const std::function<bool()> & tick );

    // After runLayoutDialog returned an edit field's id: the key typed (0..9, or -1 for Backspace).
    int lastEditKey();

    // Applies lastEditKey() to a number field: digits appended up to maxLength, Backspace removes the last one.
    void applyEditKey( Item & field );

    // Typing into a text edit field (FUN_0047ddd0 object, key handling at 0x47e1ed..0x47e5f5, display FUN_0047e8f0; note v172).
    // The field shows the text with '_' put in at the cursor. Start of editing for a field of type 0x100 (all text fields of the
    // game): the text is cleared and the cursor is 0; Esc puts back the text from before and ends, Enter ends; Left / Right move the
    // cursor, Backspace removes the character before it, Delete the one at it; a character is put in at the cursor while the text is
    // shorter than max length - 1 and the shown text still fits one line of the field width.
    struct TextEdit
    {
        std::string text;
        size_t cursor = 0;
        std::string before;

        void start();
        // 0 = editing goes on, 1 = Enter, 2 = Esc.
        int key( int keyValue, int32_t modifiers, const Item & field );
        std::string shown() const;
    };

    // Draws all visible items at (offsetX, offsetY); the button with pressedId is drawn with its pressed frame.
    void drawLayout( fheroes2::Image & dst, const Layout & layout, int32_t offsetX, int32_t offsetY, int pressedId );
}
