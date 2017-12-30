#include <Gamebuino-Meta.h>
#include "utils.h"

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both#14733008
Color hsvToRgb565(unsigned char h, unsigned char s, unsigned char v)
{
    unsigned char r, g, b;
    unsigned char region, remainder, p, q, t;

    if (s == 0)
    {
        r = v;
        g = v;
        b = v;
        return (Color)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            r = v; g = t; b = p;
            break;
        case 1:
            r = q; g = v; b = p;
            break;
        case 2:
            r = p; g = v; b = t;
            break;
        case 3:
            r = p; g = q; b = v;
            break;
        case 4:
            r = t; g = p; b = v;
            break;
        default:
            r = v; g = p; b = q;
            break;
    }

    return (Color)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void drawMenuBox(int width, int height) {
  int top = (gb.display.height() - height) / 2;
  int left = (gb.display.width() - width) / 2;

  gb.display.setColor(WHITE);
  gb.display.drawFastHLine(left, top, width - 1);
  gb.display.drawFastVLine(left, top + 1, height - 2);
  gb.display.drawFastHLine(left + 2, top + height - 2, width - 3);
  gb.display.drawFastVLine(left + width - 2, top + 2, height - 4);

  gb.display.setColor(BROWN);
  gb.display.drawFastHLine(left + 1, top + 1, width - 3);
  gb.display.drawFastVLine(left + 1, top + 2, height - 4);
  gb.display.drawFastHLine(left + 1, top + height - 1, width - 1);
  gb.display.drawFastVLine(left + width - 1, top + 1, height - 2);

  gb.display.setColor(BEIGE);
  gb.display.drawPixel(left + width - 1, top);
  gb.display.drawPixel(left + width - 2, top + 1);
  gb.display.drawPixel(left, top + height - 1);
  gb.display.drawPixel(left + 1, top + height - 2);
  gb.display.fillRect(left + 2, top + 2, width - 4, height - 4);
}

int menu(const char* const* items, int length, bool allowExit) {
  int width = 0, height = 0;
  int activeItem = 0;

  // Animate menu opening.
  while (width < gb.display.height() - 10) {
    if (gb.update()) {
      width += 6;
      height += 6;
      drawMenuBox(width, height);
    }
  }

  while (true) {
    if (gb.update()) {
      if (gb.buttons.pressed(BUTTON_A)) {
        gb.sound.playOK();
        return activeItem;
      }
      if (allowExit && (gb.buttons.pressed(BUTTON_MENU) || gb.buttons.pressed(BUTTON_B))) {
        gb.sound.playCancel();
        return -1;
      }

      if (gb.buttons.repeat(BUTTON_DOWN, 8)) {
        activeItem++;
        gb.sound.playTick();
      }
      if (gb.buttons.repeat(BUTTON_UP, 8)) {
        activeItem--;
        gb.sound.playTick();
      }
      //don't go out of the menu
      if (activeItem == length) activeItem = 0;
      if (activeItem < 0) activeItem = length - 1;

      drawMenuBox(width, height);
      int top = (gb.display.height() - height) / 2;
      int left = (gb.display.width() - width) / 2;

      gb.display.setCursor(left + 7, top + 5);
      for (int i = 0; i < length; i++) {
        gb.display.cursorX = left + 7;
        gb.display.setColor(BROWN);
        if (i == activeItem){
          gb.display.setColor(BLACK);
          gb.display.cursorX = left + 5;
        }
        gb.display.println((const char*)items[i]);
      }
    }
  }
}

