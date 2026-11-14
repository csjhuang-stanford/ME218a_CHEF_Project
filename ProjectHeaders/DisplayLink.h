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
     0x02, 'C', 0x00                       clear the screen
     0x02, 'L', line, color, text..., 0x00 write one line
  line = '0'..'3', color = one of the DisplayColor_t codes below.
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

#define DISPLAY_NUM_LINES  4

// Public Function Prototypes

void InitDisplayLink(void);                 // SPI1 leader + pins
void DisplayClear(void);                    // blank the whole screen
void DisplayLine(uint8_t Line, DisplayColor_t Color, const char *Text);
void DisplayLinef(uint8_t Line, DisplayColor_t Color, const char *Format, ...);

#endif /* DisplayLink_H */
