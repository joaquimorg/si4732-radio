#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

#include "global.h"
#include "icons.h"
#include "images/welcome.h"

#include "fonts/font_16_tf.h"
#include "fonts/font_20_mn.h"
#include "fonts/font_20_tf.h"
#include "fonts/font_B20_tf.h"
#include "fonts/font_32_tf.h"
#include "fonts/font_B32_tf.h"
#include "fonts/font_32_nf.h"
#include "fonts/font_56_nf.h"


enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

enum class Font {
    FONT_18_TF,
    FONT_20_MN,
    FONT_20_TF,
    FONT_B20_TF,
    FONT_32_TF,
    FONT_B32_TF,
    FONT_32_NF,
    FONT_56_NF
};

#define BLACK 0
#define WHITE 1

#ifndef UI_H
#define UI_H

#define W 400
#define H 240

// Menu panel (right side of the main area, kept above the bottom panel)
#define MENU_X          161
#define MENU_Y           28
#define MENU_W          236
#define MENU_LINE_H      24
#define MENU_LIST_Y     (MENU_Y + 46)
#define MENU_MAX_LINES    5

// Bottom panel: band ruler
#define BOTTOM_Y        180
#define RULER_X           1
#define RULER_W         398
#define RULER_Y         210
#define RULER_H          14

// Band plan segment, drawn on the band ruler with a fill pattern per type
enum SegmentType : uint8_t {
    SEG_CW,         // Solid
    SEG_DIGI,       // Checkered
    SEG_PHONE,      // Empty
    SEG_OTHER       // Dotted (satellite, FM)
};

struct BandSegment {
    uint16_t startKHz;
    uint16_t endKHz;
    SegmentType type;
    const char* name;
};

// Returns the value shown at the right of a list line, or nullptr for none
typedef const char* (*ListValueFn)(uint8_t idx);

class UI {
public:
    UI() :
        u8g2(U8G2_R0, /* cs=*/5, /* dc=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE) // on/*CLK*/7, /*SDA*/11, /*CS*/5, /*DC        
    {
        lcd()->begin();
        lcd()->setBusClock(2800000);

        lcd()->setColorIndex(WHITE);
        lcd()->clearBuffer();
    };

    U8G2_LS027B7DH01_400X240_F_4W_HW_SPI* lcd() { return &u8g2; };

    uint8_t message_result = 0;

    uint8_t menu_pos = 1;

    void clearDisplay() {
        lcd()->setColorIndex(WHITE);
        lcd()->drawBox(0, 0, W, H);
        sendFullBuffer();
    };

    // Sends only the 8-line rows that changed since the last update. The SPI traffic to the display
    // radiates harmonics into the SW bands, so an unchanged screen costs no bus activity at all.
    // The memory LCD driver always writes full-width lines, so rows are sent with their full width.
    void updateDisplay() {
        uint8_t* buf = lcd()->getBufferPtr();
        uint8_t tileWidth = lcd()->getBufferTileWidth();
        uint8_t tileHeight = lcd()->getBufferTileHeight();
        size_t rowBytes = (size_t)tileWidth * 8;

        if (!lastFrameValid) {
            sendFullBuffer();
            return;
        }

        int8_t runStart = -1;
        for (uint8_t row = 0; row <= tileHeight; row++) {
            bool changed = (row < tileHeight) && memcmp(buf + row * rowBytes, lastFrame + row * rowBytes, rowBytes) != 0;
            if (changed && runStart < 0) {
                runStart = row;
            }
            else if (!changed && runStart >= 0) {
                lcd()->updateDisplayArea(0, runStart, tileWidth, row - runStart);
                memcpy(lastFrame + runStart * rowBytes, buf + runStart * rowBytes, (row - runStart) * rowBytes);
                runStart = -1;
            }
        }
    };

    void sendFullBuffer() {
        lcd()->sendBuffer();
        memcpy(lastFrame, lcd()->getBufferPtr(), sizeof(lastFrame));
        lastFrameValid = true;
    };

    void setFont(Font font) {
        switch (font) {
        case Font::FONT_18_TF:
            lcd()->setFont(u8g2_font_16_tf);
            break;
        case Font::FONT_20_MN:
            lcd()->setFont(u8g2_font_20_mn);
            break;
        case Font::FONT_20_TF:
            lcd()->setFont(u8g2_font_20_tf);
            break;
        case Font::FONT_B20_TF:
            lcd()->setFont(u8g2_font_B20_tf);
            break;
        case Font::FONT_32_TF:
            lcd()->setFont(u8g2_font_32_tf);
            break;
        case Font::FONT_B32_TF:
            lcd()->setFont(u8g2_font_B32_tf);
            break;
        case Font::FONT_32_NF:
            lcd()->setFont(u8g2_font_32_nf);
            break;
        case Font::FONT_56_NF:
            lcd()->setFont(u8g2_font_56_nf);
            break;
        }
    };

    void setBlackColor() {
        lcd()->setColorIndex(BLACK);
    };

    void setWhiteColor() {
        lcd()->setColorIndex(WHITE);
    };

    void drawStrf(u8g2_uint_t x, u8g2_uint_t y, const char* str, ...) {
        char text[52] = { 0 };

        va_list va;
        va_start(va, str);
        vsnprintf(text, sizeof(text), str, va);
        va_end(va);

        lcd()->drawStr(x, y, text);
    };

    void drawString(TextAlign tAlign, u8g2_uint_t xstart, u8g2_uint_t xend, u8g2_uint_t y, bool isBlack, bool isFill, bool isBox, const char* str) {

        u8g2_uint_t startX = xstart;
        u8g2_uint_t endX = xend;
        u8g2_uint_t stringWidth = lcd()->getStrWidth(str);

        u8g2_uint_t xx, yy, ww, hh;

        u8g2_uint_t border_width = 1;

        u8g2_uint_t padding_h = 2;
        u8g2_uint_t padding_v = 2;

        u8g2_uint_t h = lcd()->getAscent();//lcd()->getMaxCharHeight() + lcd()->getDescent();

        if (endX > startX) {
            if (tAlign == TextAlign::CENTER) {
                if (stringWidth < (endX - startX)) {
                    startX = ((startX + endX) / 2) - (stringWidth / 2);
                    endX = stringWidth;
                }
            }
            else if (tAlign == TextAlign::RIGHT) {
                startX = endX - stringWidth;
            }
        }

        xx = startX;
        xx -= padding_h;
        xx -= border_width;
        ww = (endX > startX ? (endX - startX) : stringWidth) + (2 * padding_h) + (2 * border_width);

        yy = y;
        yy -= h;
        yy -= padding_v;
        yy -= border_width;
        hh = h + (2 * padding_v) + (2 * border_width);

        /*lcd()->setColorIndex(isBlack ? WHITE : BLACK);
        lcd()->drawBox(xx, yy, ww, hh);*/

        lcd()->setColorIndex(isBlack ? BLACK : WHITE);
        if (isFill) {
            lcd()->drawRBox(xx, yy, ww, hh, 5);
            lcd()->setColorIndex(isBlack ? WHITE : BLACK);
        }
        else if (isBox) {
            lcd()->drawRFrame(xx, yy, ww, hh, 5);
        }

        lcd()->drawStr(startX, y, str);

    };

    void drawStringf(TextAlign tAlign, u8g2_uint_t xstart, u8g2_uint_t xend, u8g2_uint_t y, bool isBlack, bool isFill, bool isBox, const char* str, ...) {
        char text[52] = { 0 };

        va_list va;
        va_start(va, str);
        vsnprintf(text, sizeof(text), str, va);
        va_end(va);

        drawString(tAlign, xstart, xend, y, isBlack, isFill, isBox, text);
    };

    void clearScreenAnimationCircleFromRight() {
        int curr = 0;
        float targ = 80;
        while (targ < W) {
            targ *= 1.6;
            lcd()->setColorIndex(WHITE);
            for (; curr < targ; curr += 1)
            {
                lcd()->drawCircle(/*x0*/ 400, /*y0*/ H / 2, /*rad*/ curr, /*opt*/ U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT); // drawDisc     U8G2_DRAW_ALL
            }
            lcd()->setColorIndex(BLACK);
            lcd()->drawCircle(/*x0*/ 400, /*y0*/ H / 2, /*rad*/ curr + 2, /*opt*/ U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT); // drawDisc     U8G2_DRAW_ALL
            sendFullBuffer();
        }
    };

#define LINE_SPACING_MULTILINE_TEXT_PX 1

    uint32_t draw_string_multi_line(const char* str, uint8_t max_chr_per_line, int32_t x, int32_t y) {
        uint32_t string_length_bytes = strlen(str);
        char partial_string[60 + 1] = { '\0' };

        uint8_t line_start = 0;
        uint8_t line_end = 0;
        uint8_t max_chr_h = lcd()->getMaxCharHeight();
        uint8_t string_length = lcd()->getStrWidth(str);

        while (line_start < string_length_bytes) {
            memset(partial_string, 0, max_chr_per_line + 1);
            line_end = line_start + max_chr_per_line - 1;

            uint8_t i;
            uint8_t last_space_idx = line_start; /* used to keep track of the last space */
            /* for strings with multi-byte utf8 characters, these two values will be different */
            uint8_t partial_str_len = 0;
            uint8_t partial_str_len_bytes = 0;

            /**
             * Parse the string until a new line is found or we process the max number of characters
             * that can fit in a single line. Save the index of the last space/new line in order not to
             * break the word in the middle in case it cannot fit in the line completely.
             */
            for (i = line_start; (partial_str_len <= max_chr_per_line) && (str[i] != '\0'); i++) {
                if (str[i] == '\n') {
                    last_space_idx = i;
                    break;
                }
                else if (str[i] == ' ') {
                    last_space_idx = i;
                    partial_str_len++;
                }
                else {
                    if ((str[i] & 0xc0) != 0x80) {
                        /**
                         * Count only the first byte of each glyph for the length in characters.
                         * UTF8 glyphs can be multi-byte and continuation bytes will have
                         * 0x10xxxxxx (0x80) as leftmost bits
                         */
                        partial_str_len++;
                    }
                    partial_str_len_bytes++;
                }
            }

            /**
             * If new line was found, or a word would be partially cut off, force the last known
             * space/new line as the end of the first line to be drawn;
             * otherwise, take the calculated length of the string that can fit the single line and
             * discard the beginning of the word that cannot fully fit.
             */
            if ((str[i] == '\n') || (partial_str_len > max_chr_per_line)) {
                line_end = last_space_idx;
            }
            else {
                line_end = partial_str_len + line_start;
                while (line_end < (string_length - 1) && !isspace((unsigned char)str[line_end]) &&
                    (line_end > line_start)) {
                    line_end--;
                }
            }

            /**
             * If a word is longer than the max num of characters that can be shown,
             * we can't rely on spaces and new lines so we split the word at the line limit
             */
            if (line_end == line_start) {
                line_end = line_start + max_chr_per_line;
            }
            else if (line_end == (string_length - 1) &&
                !isspace((unsigned char)str[line_end])) {
                line_end = line_start + max_chr_per_line - 1;
            }

            strncpy(partial_string, &str[line_start], line_end - line_start);
            lcd()->drawUTF8(x, y, partial_string);

            /* Prepare the coordinates and indices for the next line */
            y += max_chr_h + LINE_SPACING_MULTILINE_TEXT_PX;

            line_start = line_end;
            while (line_start < string_length_bytes && isspace((unsigned char)str[line_start])) {
                line_start++;
            }
        }

        /* Remove the spacing that was added to the last line */
        return y - max_chr_h - LINE_SPACING_MULTILINE_TEXT_PX;
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

    void draw_ic24_battery75(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawBitmap(x, y, 3, 24, ic24_battery75); };

    void draw_start(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, start_icon_width, start_icon_height, start_icon_bits); };
    void draw_finish(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, finish_icon_width, finish_icon_height, finish_icon_bits); };
    void draw_stereo(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, stereo_icon_width, stereo_icon_height, stereo_icon_bits); };
    void draw_ic24_sound_on(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, sound_width, sound_height, sound_bits); };

    void draw_ic24_save(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, save_icon_width, save_icon_height, save_icon_bits); };

    void draw_ic24_step(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, step_icon_width, step_icon_height, step_icon_bits); };
    void draw_ic24_bandwidth(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, bandwidth_icon_width, bandwidth_icon_height, bandwidth_icon_bits); };
    void draw_ic24_agc(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, agc_icon_width, agc_icon_height, agc_icon_bits); };

    void draw_ic_mode(int x, int y, bool color) { lcd()->setColorIndex(color);  lcd()->drawXBM(x, y, mode_icon_width, mode_icon_height, mode_icon_bits); };
    

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

    void showStatusScreen(const char* title, const char* message ) {
        int popupHigh = 80;

        setFont(Font::FONT_B20_TF);

        u8g2_uint_t string_length = lcd()->getStrWidth(message);

        int popupWidth = 40 + string_length;

        setWhiteColor();
        lcd()->drawRBox((W / 2) - (popupWidth / 2) - 3, (H / 2) - (popupHigh / 2) - 3, popupWidth + 6, popupHigh + 6, 8);

        setBlackColor();
        lcd()->drawRFrame((W / 2) - (popupWidth / 2), (H / 2) - (popupHigh / 2), popupWidth, popupHigh, 8);
        lcd()->drawRFrame((W / 2) - (popupWidth / 2) - 1, (H / 2) - (popupHigh / 2) - 1, popupWidth + 2, popupHigh + 2, 8);
        lcd()->drawRFrame((W / 2) - (popupWidth / 2) + 2, (H / 2) - (popupHigh / 2) + 2, popupWidth - 4, popupHigh - 4, 8);

        setBlackColor();
        //setFont(Font::FONT_B32_TF);
        //drawString(TextAlign::CENTER, 10, 390, 110, true, false, false, title);
        
        drawString(TextAlign::CENTER, (W / 2) - (popupWidth / 2), (W / 2) - (popupWidth / 2) + popupWidth, (H / 2) - (popupHigh / 2) + (popupHigh / 2) + 4, true, false, false, message);
        
    }

    void drawLoading() {
        showStatusScreen("", "Loading SSB ...");
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

    void drawStatus(uint8_t volume, bool itIsTimeToSave) {
        setWhiteColor();
        lcd()->drawBox(0, 0, W, 24);

        setFont(Font::FONT_20_TF);
        //lcd()->drawStr(35, 18, "75%  8.7V");
        //lcd()->drawStr(35, 18, "joaquim.org");

        if(itIsTimeToSave) {
            draw_ic24_save(5, 1, BLACK);
        }        

        draw_ic24_sound_on(300, 3, BLACK);
        drawStringf(TextAlign::RIGHT, 0, 390, 18, true, false, false, "%02u %%", map(volume, 0, 63, 0, 100));

        lcd()->drawHLine(0, 24, W);
    };

    void clearMain() {
        setWhiteColor();
        lcd()->drawBox(0, 25, W, H);
    };

    void drawFrequencyBig(uint32_t freq, int16_t bfo, uint8_t currentBandType, uint8_t currentMode, u8g2_uint_t xend, u8g2_uint_t y) {

        setFont(Font::FONT_56_NF);

        if (currentBandType == FM_BAND_TYPE) {
            drawStringf(TextAlign::RIGHT, 0, xend, y, true, false, false, "%02u.%02u", (freq / 100), (freq % 100));
        }
        else {

            if (currentMode == LSB || currentMode == USB || currentMode == CW) {

                uint32_t cFrequency  = (uint32_t(freq) * 1000) + bfo;

                drawStringf(TextAlign::RIGHT, 0, xend - 55, y, true, false, false, "%2u.%3.3u", (cFrequency / 1000000), (cFrequency % 1000000) / 1000);

                setBlackColor();

                setFont(Font::FONT_32_NF);
                drawStrf(xend - 50, y - 2, "%3.3d", (cFrequency % 1000));                

            }
            else {
                drawStringf(TextAlign::RIGHT, 0, xend, y, true, false, false, "%2u.%03u", (freq / 1000), (freq % 1000));
            }

        }
    };

    // Draws the frequency being edited digit by digit, with the selected digit in reverse video.
    // freq is in 10 kHz units for FM and kHz for AM/SSB. editDigit is the power of ten being edited
    // (0 = least significant) and numDigits is the number of editable digits.
    void drawFrequencyEdit(uint32_t freq, uint8_t editDigit, uint8_t numDigits, uint8_t currentBandType, uint8_t currentMode, u8g2_uint_t xend, u8g2_uint_t y) {
        bool isFM = (currentBandType == FM_BAND_TYPE);
        bool isSSB = (currentMode == LSB || currentMode == USB || currentMode == CW);
        uint8_t decimals = isFM ? 2 : 3;
        uint8_t totalDigits = max(numDigits, (uint8_t)(decimals + 1));

        // Build the string from the most significant digit, remembering where the edited digit lands
        char text[12];
        uint8_t len = 0;
        int8_t editPos = -1;
        bool leading = true;
        uint32_t div = 1;
        for (uint8_t i = 1; i < totalDigits; i++) div *= 10;

        for (int8_t k = totalDigits - 1; k >= 0; k--) {
            uint8_t d = (freq / div) % 10;
            div /= 10;
            if (d != 0 || k <= decimals || k == editDigit) leading = false;
            if (k == editDigit) editPos = len;
            text[len++] = leading ? ' ' : ('0' + d);
            if (k == decimals) text[len++] = '.';
        }
        text[len] = '\0';

        setFont(Font::FONT_56_NF);
        u8g2_uint_t right = isSSB ? xend - 55 : xend;
        u8g2_uint_t x = right - lcd()->getStrWidth(text);
        u8g2_uint_t ascent = lcd()->getAscent();

        char glyph[2] = { 0, 0 };
        for (uint8_t i = 0; i < len; i++) {
            glyph[0] = text[i];
            u8g2_uint_t w = u8g2_GetGlyphWidth(lcd()->getU8g2(), text[i]);
            setBlackColor();
            if (i == editPos) {
                lcd()->drawBox(x - 1, y - ascent - 3, w + 2, ascent + 6);
                setWhiteColor();
            }
            x += lcd()->drawStr(x, y, glyph);
        }

        if (isSSB) {
            setBlackColor();
            setFont(Font::FONT_32_NF);
            drawStrf(xend - 50, y - 2, "000");
        }
    };

    void drawFrequency(uint32_t freq, u8g2_uint_t x, u8g2_uint_t y) {

        setFont(Font::FONT_20_TF);

        drawStringf(TextAlign::LEFT, x, 0, y, false, false, false, "%2u.%03u MHz", (freq / 1000), (freq % 1000));

    };

    long map(long x, long in_min, long in_max, long out_min, long out_max) {
        const long run = in_max - in_min;
        if (run == 0) {
            return -1; // AVR returns -1, SAM returns 0
        }
        const long rise = out_max - out_min;
        const long delta = x - in_min;
        return (delta * rise) / run + out_min;
    }

    void drawRSSI(int rssi, int vu, int snr, u8g2_uint_t x, u8g2_uint_t y) {

        setBlackColor();
        setFont(Font::FONT_32_TF);
        lcd()->drawStr(x, y + 24, "S");

        setFont(Font::FONT_20_MN);
        lcd()->drawStr(x + 20, y + 10, "1.3.5.7.9.20.40.60");

        uint16_t sMultiplier;
        uint8_t sWidth;
        for (int i = 0; i < 15; i++) {
            sMultiplier = (i * 12);
            sWidth = 8;
            if (i > 9) {
                sMultiplier += (i - 9) * 6;
                sWidth = 12;
            }
            lcd()->drawVLine(x + 26 + sMultiplier, y + 14, 16);
            if (i + 1 < vu) {
                if (i > 9) {
                    sMultiplier -= 2;
                }
                lcd()->drawBox(x + 23 + sMultiplier, y + 14, sWidth, 16);
            }
        }

        setFont(Font::FONT_18_TF);
        drawStringf(TextAlign::LEFT, x + 20, 0, y + 50, true, false, false, "%2u dBuV", rssi);
        drawStringf(TextAlign::LEFT, x + 120, 0, y + 50, true, false, false, "%2u dB SNR", snr);

    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

    // Draws one list line: the label on the left and, when the list has a value callback, the
    // current value right aligned. The cursor line is drawn in reverse video.
    u8g2_uint_t drawSelectionListLine(u8sl_t* u8sl, u8g2_uint_t y, uint8_t idx, const char* s) {
        const u8g2_uint_t x0 = MENU_X + 6;
        const u8g2_uint_t x1 = MENU_X + MENU_W - 14;      // Leaves room for the scrollbar
        bool selected = (idx == u8sl->current_pos);

        s = u8x8_GetStringLineStart(idx, s);
        if (s == NULL)
            s = "";

        setBlackColor();
        if (selected) {
            lcd()->drawRBox(x0, y - 18, x1 - x0, MENU_LINE_H - 1, 4);    // Caps (14 px) centered, descenders inside
            setWhiteColor();
        }

        // Value first, so the label can be cut to the room that is left
        u8g2_uint_t labelEnd = x1 - 6;
        if (listValueFn != nullptr) {
            const char* value = listValueFn(idx);
            if (value != nullptr) {
                setFont(Font::FONT_18_TF);
                u8g2_uint_t vw = lcd()->getStrWidth(value);
                lcd()->drawStr(x1 - 6 - vw, y, value);
                labelEnd = x1 - 6 - vw - 8;
            }
        }

        char label[48];
        size_t n = 0;
        while (n < sizeof(label) - 1 && s[n] != '\0' && s[n] != '\n') {
            label[n] = s[n];
            n++;
        }
        label[n] = '\0';
        setFont(selected ? Font::FONT_B20_TF : Font::FONT_20_TF);
        while (n > 0 && x0 + 6 + lcd()->getStrWidth(label) > labelEnd) {
            label[--n] = '\0';
        }
        lcd()->drawStr(x0 + 6, y, label);

        return MENU_LINE_H;
    }

    void drawList(u8sl_t* u8sl, u8g2_uint_t y, const char* s) {
        uint8_t i;
        for (i = 0; i < u8sl->visible; i++) {
            y += drawSelectionListLine(u8sl, y, i + u8sl->first_pos, s);
        }
    }

    // Scrollbar at the right edge of the menu panel, only when the list does not fit
    void drawScrollBar(u8sl_t* u8sl, u8g2_uint_t y) {
        if (u8sl->total <= u8sl->visible) return;

        const u8g2_uint_t x = MENU_X + MENU_W - 11;
        u8g2_uint_t top = y - 18;
        u8g2_uint_t trackH = u8sl->visible * MENU_LINE_H - 1;
        u8g2_uint_t thumbH = max((int)(trackH * u8sl->visible / u8sl->total), 8);
        u8g2_uint_t thumbY = top + (trackH - thumbH) * u8sl->first_pos / (u8sl->total - u8sl->visible);

        setBlackColor();
        for (u8g2_uint_t yy = top; yy < top + trackH; yy += 2) {
            lcd()->drawPixel(x + 2, yy);
        }
        lcd()->drawRBox(x, thumbY, 5, thumbH, 2);
    }

    u8sl_t u8sl;
    const char* slines;
    ListValueFn listValueFn = nullptr;

    void listNext() {
        u8sl.current_pos++;
        if (u8sl.current_pos >= u8sl.total) {
            u8sl.current_pos = 0;
            u8sl.first_pos = 0;
        }
        else {
            uint8_t middle = u8sl.visible / 2;
            if (u8sl.current_pos >= middle && u8sl.current_pos < u8sl.total - middle) {
                u8sl.first_pos = u8sl.current_pos - middle;
            }
            else if (u8sl.current_pos >= u8sl.total - middle) {
                u8sl.first_pos = u8sl.total - u8sl.visible;
            }
        }
    }

    void listPrev() {
        if (u8sl.current_pos == 0) {
            u8sl.current_pos = u8sl.total - 1;
            u8sl.first_pos = (u8sl.total > u8sl.visible) ? (u8sl.total - u8sl.visible) : 0;
        }
        else {
            u8sl.current_pos--;
            uint8_t middle = u8sl.visible / 2;
            if (u8sl.current_pos >= middle && u8sl.current_pos < u8sl.total - middle) {
                u8sl.first_pos = u8sl.current_pos - middle;
            }
            else if (u8sl.current_pos < middle) {
                u8sl.first_pos = 0;
            }
        }
    }

    void drawSelectionList(uint8_t startPos, uint8_t displayLines, const char* sl) {

        u8sl.visible = displayLines;

        u8sl.total = u8x8_GetStringLineCnt(sl);
        if (u8sl.total <= u8sl.visible)
            u8sl.visible = u8sl.total;

        // Calculate the middle position
        uint8_t middlePos = u8sl.visible / 2;

        // Set the current position
        u8sl.current_pos = startPos;

        // Adjust first_pos to center the current_pos if possible
        if (u8sl.current_pos >= middlePos) {
            u8sl.first_pos = u8sl.current_pos - middlePos;
        }
        else {
            u8sl.first_pos = 0;
        }

        // Ensure first_pos does not exceed the total lines
        if (u8sl.first_pos + u8sl.visible > u8sl.total) {
            u8sl.first_pos = u8sl.total - u8sl.visible;
        }

        // Ensure current_pos is within the valid range
        if (u8sl.current_pos >= u8sl.total) {
            u8sl.current_pos = u8sl.total - 1;
        }

        slines = sl;
    }

    void setMenu(uint8_t startPos, const char* sl, ListValueFn valueFn = nullptr) {
        listValueFn = valueFn;
        drawSelectionList(startPos, MENU_MAX_LINES, sl);
    }

    void drawMenu() {
        lcd()->setFontPosBaseline();
        drawList(&u8sl, MENU_LIST_Y, slines);
        drawScrollBar(&u8sl, MENU_LIST_Y);
    }

    // Height of the menu panel for the current list
    uint8_t getMenuHeight() {
        return u8sl.visible * MENU_LINE_H + 30;
    }

    // Menu panel frame with the title bar. When showPosition is set, the title bar also shows
    // the cursor position in the list (e.g. "3/15").
    void drawMenuPanel(const char* title, uint8_t h, bool showPosition) {
        setWhiteColor();
        lcd()->drawRBox(MENU_X - 3, MENU_Y - 3, MENU_W + 6, h + 6, 8);

        setBlackColor();
        lcd()->drawRBox(MENU_X, MENU_Y, MENU_W, 24, 8);

        setWhiteColor();
        lcd()->drawBox(MENU_X, MENU_Y + 21, MENU_W, h - 21);

        setBlackColor();
        lcd()->drawRFrame(MENU_X, MENU_Y, MENU_W, h, 8);
        lcd()->drawRFrame(MENU_X + 2, MENU_Y, MENU_W - 4, h - 2, 8);

        setWhiteColor();
        setFont(Font::FONT_B20_TF);
        drawString(TextAlign::CENTER, MENU_X + 1, MENU_X + MENU_W - 2, MENU_Y + 18, false, false, false, title);

        if (showPosition && u8sl.total > u8sl.visible) {
            setWhiteColor();
            setFont(Font::FONT_18_TF);
            drawStringf(TextAlign::RIGHT, 0, MENU_X + MENU_W - 10, MENU_Y + 17, false, false, false, "%u/%u", u8sl.current_pos + 1, u8sl.total);
        }
    }

    // Value panel: big value with its unit and a level gauge below it. With centerZero the gauge
    // fills from the middle (for signed values such as the calibration offset).
    void drawValuePanel(const char* title, Font valueFont, const char* value, const char* unit,
                        int32_t v, int32_t vMin, int32_t vMax, bool centerZero) {
        const uint8_t h = 104;
        drawMenuPanel(title, h, false);

        setBlackColor();
        setFont(valueFont);
        u8g2_uint_t valueW = lcd()->getStrWidth(value);
        setFont(Font::FONT_20_TF);
        u8g2_uint_t unitW = (unit != nullptr && unit[0] != '\0') ? lcd()->getStrWidth(unit) + 6 : 0;

        u8g2_uint_t x = MENU_X + (MENU_W - valueW - unitW) / 2;
        setFont(valueFont);
        lcd()->drawStr(x, MENU_Y + 70, value);
        if (unitW > 0) {
            setFont(Font::FONT_20_TF);
            lcd()->drawStr(x + valueW + 6, MENU_Y + 70, unit);
        }

        if (vMax <= vMin) return;

        const u8g2_uint_t gx = MENU_X + 14;
        const u8g2_uint_t gw = MENU_W - 28;
        const u8g2_uint_t gy = MENU_Y + 80;
        const u8g2_uint_t gh = 9;

        v = constrain(v, vMin, vMax);
        lcd()->drawFrame(gx, gy, gw, gh);
        u8g2_uint_t inner = gw - 4;
        u8g2_uint_t pos = (uint32_t)(v - vMin) * inner / (vMax - vMin);
        if (centerZero) {
            u8g2_uint_t mid = inner / 2;
            if (pos >= mid) lcd()->drawBox(gx + 2 + mid, gy + 2, pos - mid + 1, gh - 4);
            else            lcd()->drawBox(gx + 2 + pos, gy + 2, mid - pos + 1, gh - 4);
            lcd()->drawVLine(gx + gw / 2, gy - 3, gh + 6);
        }
        else {
            lcd()->drawBox(gx + 2, gy + 2, pos, gh - 4);
        }
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

    // Band ruler: slide-rule style overview of the band with the band plan segments, markers for
    // the preset frequencies and a cursor on the tuned frequency. Frequencies in Hz.
    // The middle shows a title (band or preset name) with an optional chip (segment or mode).
    // Drawn in white on the black bottom panel.
    void drawBandRuler(uint32_t minHz, uint32_t maxHz, uint32_t curHz,
                       const BandSegment* segs, uint8_t numSegs,
                       const uint32_t* marksHz, uint16_t numMarks,
                       const char* leftLabel, const char* rightLabel,
                       const char* bandName, const char* segName) {
        const u8g2_uint_t y = RULER_Y;
        const u8g2_uint_t inner = RULER_W - 2;
        if (maxHz <= minHz) return;

        // Labels row: band edges at the sides, band name and current segment in the middle
        setWhiteColor();
        setFont(Font::FONT_18_TF);
        lcd()->drawStr(RULER_X + 1, y - 4, leftLabel);
        lcd()->drawStr(RULER_X + RULER_W - 1 - lcd()->getStrWidth(rightLabel), y - 4, rightLabel);

        setFont(Font::FONT_B20_TF);
        u8g2_uint_t nameW = lcd()->getStrWidth(bandName);
        u8g2_uint_t chipW = 0;
        if (segName != nullptr) {
            setFont(Font::FONT_18_TF);
            chipW = lcd()->getStrWidth(segName) + 10;
        }
        u8g2_uint_t cx = (W - nameW - (chipW ? chipW + 6 : 0)) / 2;
        setFont(Font::FONT_B20_TF);
        lcd()->drawStr(cx, y - 4, bandName);
        if (chipW) {
            u8g2_uint_t chipX = cx + nameW + 6;
            lcd()->drawRBox(chipX, y - 20, chipW, 18, 3);       // Centered on the name caps (y - 18 .. y - 5)
            setBlackColor();
            setFont(Font::FONT_18_TF);
            lcd()->drawStr(chipX + 5, y - 5, segName);
            setWhiteColor();
        }

        // Bar with the band plan segments
        lcd()->drawFrame(RULER_X, y, RULER_W, RULER_H);
        for (uint8_t i = 0; i < numSegs; i++) {
            uint32_t s = (uint32_t)segs[i].startKHz * 1000;
            uint32_t e = (uint32_t)segs[i].endKHz * 1000;
            if (e <= minHz || s >= maxHz) continue;
            u8g2_uint_t x0 = rulerX(max(s, minHz), minHz, maxHz, inner);
            u8g2_uint_t x1 = rulerX(min(e, maxHz), minHz, maxHz, inner);
            fillSegment(x0, x1, y + 2, RULER_H - 4, segs[i].type);
            if (s > minHz) lcd()->drawVLine(x0, y, RULER_H);
        }

        // Preset markers just above the bar
        for (uint16_t i = 0; i < numMarks; i++) {
            if (marksHz[i] < minHz || marksHz[i] > maxHz) continue;
            lcd()->drawVLine(rulerX(marksHz[i], minHz, maxHz, inner), y - 3, 2);
        }

        // Scale ticks (XOR so they show on any segment pattern)
        lcd()->setDrawColor(2);
        for (uint8_t i = 1; i < 10; i++) {
            lcd()->drawVLine(RULER_X + 1 + inner * i / 10, y + RULER_H - 4, 3);
        }

        // Cursor
        uint32_t f = constrain(curHz, minHz, maxHz);
        u8g2_uint_t px = rulerX(f, minHz, maxHz, inner);
        lcd()->drawBox(px - 1, y + 1, 3, RULER_H - 2);
        setWhiteColor();
        lcd()->drawTriangle(px, y + RULER_H + 1, px - 4, y + RULER_H + 6, px + 4, y + RULER_H + 6);
    }


    uint8_t getListPos() {
        return u8sl.current_pos;
    }

    void setListPos(uint8_t pos) {
        u8sl.current_pos = pos;
        uint8_t middlePos = u8sl.visible / 2;

        // Adjust first_pos to center the current_pos if possible
        if (u8sl.current_pos >= middlePos) {
            u8sl.first_pos = u8sl.current_pos - middlePos;
        } else {
            u8sl.first_pos = 0;
        }

        // Ensure first_pos does not exceed the total lines
        if (u8sl.first_pos + u8sl.visible > u8sl.total) {
            u8sl.first_pos = u8sl.total - u8sl.visible;
        }

        // Ensure current_pos is within the valid range
        if (u8sl.current_pos >= u8sl.total) {
            u8sl.current_pos = u8sl.total - 1;
        }
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* - - - - - - - - - - - - - - - - - - - - - - - - - - */

private:
    U8G2_LS027B7DH01_400X240_F_4W_HW_SPI u8g2;

    u8g2_uint_t rulerX(uint32_t hz, uint32_t minHz, uint32_t maxHz, u8g2_uint_t inner) {
        return RULER_X + 1 + (uint64_t)(hz - minHz) * (inner - 1) / (maxHz - minHz);
    }

    // Fills a band plan segment with the pattern of its type
    void fillSegment(u8g2_uint_t x0, u8g2_uint_t x1, u8g2_uint_t y, u8g2_uint_t h, SegmentType type) {
        if (type == SEG_PHONE) return;
        if (type == SEG_CW) {
            lcd()->drawBox(x0, y, x1 - x0 + 1, h);
            return;
        }
        for (u8g2_uint_t yy = y; yy < y + h; yy++) {
            for (u8g2_uint_t xx = x0; xx <= x1; xx++) {
                bool on = (type == SEG_DIGI) ? ((xx + yy) & 1) : ((xx & 1) == 0 && (yy & 1) == 0);
                if (on) lcd()->drawPixel(xx, yy);
            }
        }
    }

    uint8_t lastFrame[W * H / 8];       // Copy of what is currently on the display
    bool lastFrameValid = false;

};

#endif // UI_H