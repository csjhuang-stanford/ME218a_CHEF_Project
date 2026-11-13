/****************************************************************************

  Header file for RockerSwitch (CHEF project)
  Four 2-position rocker switches, each between a pin and GND:
    Switch 1: RB3  (pin 7)   -> first digit
    Switch 2: RB4  (pin 11)  -> second digit
    Switch 3: RB2  (pin 6)   -> third digit
    Switch 4: RB12 (pin 23)  -> fourth digit

 ****************************************************************************/

#ifndef RockerSwitch_H
#define RockerSwitch_H

#include <stdint.h>
#include <stdbool.h>

// Bit positions in the combined switch value
#define SWITCH1_BIT  3   // RB3,  first digit
#define SWITCH2_BIT  2   // RB4,  second digit
#define SWITCH3_BIT  1   // RB2,  third digit
#define SWITCH4_BIT  0   // RB12, fourth digit

// Public Function Prototypes

void InitRockerSwitch(void);      // RB3/RB4/RB2/RB12 inputs with pull-ups
bool Check4RockerSwitch(void);    // event checker: posts ES_SWITCH_CHANGED
uint8_t QueryRockerSwitch(void);  // bit3 = switch 1 ... bit0 = switch 4
                                  // (1 = closed, 0 = open)

#endif /* RockerSwitch_H */