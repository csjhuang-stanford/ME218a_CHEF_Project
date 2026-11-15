/****************************************************************************

  Header file for DisplayLink (CHEF project, leader PIC32)
  Sends text to the display PIC32 (follower) over SPI1, transmit only.
    RA0  = chip select (plain output, active low) -> follower RB15 (SS1)
    RA1  = SDO1                                   -> follower RB11 (SDI1)
    RB14 = SCK1                                   -> follower RB14 (SCK1)
    GND  <-> GND (common ground)

  The display is a 64 x 32 LED panel showing 4 lines of text (0 = top).
  Roughly 10 characters fit per line in the normal font; longer text is
  drawn in a smaller capitals-only font, up to about 16 characters.

  Message format (one message per chip-select-low period):
     0x02, 'C', 0x00                                  clear the screen
     0x02, 'L', line, color, text..., 0x00            whole line, centered
     0x02, 'P', line, color, xStart, xEnd, text..., 0x00
                                                      left-aligned in part
                                                      of a line
     0x02, 'I', icon, x, y, 0x00                      built-in icon
  Numbers are sent as '0' + value. color = a DisplayColor_t code below.
  Pixel columns are 0..63 (left to right), rows 0..31 (top to bottom);
  line n covers rows 8n..8n+7.
  The same format is documented in DisplayCommands.h on the follower.

 ****************************************************************************/

#ifndef DisplayLink_H
#define DisplayLink_H

#include <stdint.h>
#include <stdbool.h>

// colors understood by the follower
typedef enum
{
  DISP_WHITE   = 'W',
  DISP_RED     = 'R',
  DISP_GREEN   = 'G',
  DISP_BLUE    = 'B',
  DISP_YELLOW  = 'Y',
  DISP_CYAN    = 'C',
  DISP_MAGENTA = 'M',
  DISP_ORANGE  = 'O'
} DisplayColor_t;

// built-in icons on the follower
typedef enum
{
  DISP_ICON_FIRE = 'F'              // 12 x 16 pixels (two lines tall)
} DisplayIcon_t;

#define DISPLAY_NUM_LINES  4
#define DISPLAY_WIDTH_PX   64
#define DISPLAY_HEIGHT_PX  32

// Public Function Prototypes

void InitDisplayLink(void);                 // SPI1 leader + pins
void DisplayClear(void);                    // blank the whole screen
void DisplayLine(uint8_t Line, DisplayColor_t Color, const char *Text);
void DisplayLinef(uint8_t Line, DisplayColor_t Color, const char *Format, ...);
void DisplayTextAt(uint8_t Line, DisplayColor_t Color, uint8_t XStart,
                   uint8_t XEnd, const char *Text);   // left-aligned, columns
                                                      // XStart..XEnd-1 only
void DisplayIcon(DisplayIcon_t Icon, uint8_t X, uint8_t Y);  // top-left at X,Y

#endif /* DisplayLink_H */