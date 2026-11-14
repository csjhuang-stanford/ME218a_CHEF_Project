/****************************************************************************

  Header file for StartButton (CHEF project)
  Normally-open push button between RB11 (pin 22) and GND.
  Replaces the keyboard SPACE bar for starting the game.

 ****************************************************************************/

#ifndef StartButton_H
#define StartButton_H

#include <stdint.h>
#include <stdbool.h>

// Public Function Prototypes

void InitStartButton(void);       // RB11 input with internal pull-up
bool Check4StartButton(void);     // event checker: posts ES_START_PRESSED
bool IsStartButtonDown(void);     // true while the button is held

#endif /* StartButton_H */
