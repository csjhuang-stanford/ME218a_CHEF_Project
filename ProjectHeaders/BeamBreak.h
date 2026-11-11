/****************************************************************************

  Header file for BeamBreak (CHEF project)
  IR break-beam receiver on RB5 (pin 14), 5V-tolerant input.

 ****************************************************************************/

#ifndef BeamBreak_H
#define BeamBreak_H

#include <stdint.h>
#include <stdbool.h>

// Public Function Prototypes

void InitBeamBreak(void);      // set up RB5 as input, capture starting state
bool Check4BeamBreak(void);    // event checker: posts ES_BEAM_BROKEN on H->L
bool IsBeamBroken(void);       // true if the beam is currently broken

#endif /* BeamBreak_H */
