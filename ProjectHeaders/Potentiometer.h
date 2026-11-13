/****************************************************************************

  Header file for Potentiometer (CHEF project)
  Potentiometer wiper on RB13 / AN11 (pin 24), ends on 3.3V and GND.
  Reading is 0..1023 (10-bit ADC).

 ****************************************************************************/

#ifndef Potentiometer_H
#define Potentiometer_H

#include <stdint.h>
#include <stdbool.h>

// Public Function Prototypes

void InitPotentiometer(void);     // RB13 analog in, ADC scanning AN11
bool Check4PotChange(void);       // event checker: posts ES_POT_CHANGED
uint16_t QueryCurrentPot(void);   // latest reading, 0..1023

#endif /* Potentiometer_H */
