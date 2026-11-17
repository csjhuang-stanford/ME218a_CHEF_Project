/****************************************************************************
 Module
   BeamBreak.c

 Revision
   1.0.0

 Description
   Hardware + event checker for the IR break-beam sensor (CHEF Task 2).
     Receiver white wire (open collector) -> RB5 (pin 14)
     Receiver red -> 5V, black -> GNDREF
     RB5 internal pull-up to 3.3V supplies the high level:
     Beam intact  -> OUT high (1)
     Beam broken  -> OUT low  (0)
   Check4BeamBreak() samples RB5 every SAMPLE_TIME ms (not every pass of
   the framework loop) and posts ES_BEAM_BROKEN to TaskService once per
   swipe: when the beam has been broken for BROKEN_SAMPLES samples, and
   only if it was intact for REARM_TIME ms since the last swipe, so the
   flicker while a hand passes through counts once.

 Notes
   RB5 has no analog function, so no ANSELB setting is needed.
   RB5 is also PGED3: make sure the config bits use ICESEL = ICS_PGx1.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/26       CH      internal pull-up on: receiver is open collector
 11/15/26       CH      debounce: 20 ms broken to count, 150 ms intact to re-arm
 11/10/26       CH      started coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/BeamBreak.h"

#include <xc.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "TaskService.h"

/*----------------------------- Module Defines ----------------------------*/
#define BEAM_PIN       PORTBbits.RB5
#define BEAM_INTACT    1
#define BEAM_BROKEN    0
#define SAMPLE_TIME    10        // ms between samples of the beam

// Debounce: a hand doesn't break the beam cleanly (edges and gaps between
// fingers make it flicker), so one pass used to count as several swipes.
//   - a swipe counts after the beam reads broken BROKEN_SAMPLES times in a
//     row (rejects single-sample glitches)
//   - the next swipe can only count after the beam has read intact for
//     REARM_TIME ms in a row (ignores the flicker while the hand passes)
#define BROKEN_SAMPLES 2         // 2 x 10 ms = 20 ms solidly broken
#define REARM_TIME     150       // ms solidly intact before the next swipe
#define REARM_SAMPLES  (REARM_TIME / SAMPLE_TIME)

/*---------------------------- Module Variables ---------------------------*/
static uint16_t LastSampleTime;
static bool     Armed;           // true = ready to count the next swipe
static uint8_t  BrokenCount;     // consecutive broken samples
static uint8_t  IntactCount;     // consecutive intact samples

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitBeamBreak

 Description
     RB5 as a digital input with the internal pull-up on. The receiver
     output is open collector (it can only pull low), so without a
     pull-up the pin floats. Records the starting state so the first
     sample doesn't produce a false edge.
****************************************************************************/
void InitBeamBreak(void)
{
  TRISBbits.TRISB5 = 1;      // input
  CNPUBbits.CNPUB5 = 1;      // internal pull-up on (open-collector receiver)
  CNPDBbits.CNPDB5 = 0;      // internal pull-down off

  // start armed only if the beam is intact (a hand already in the beam at
  // power-up must leave first)
  Armed          = (BEAM_INTACT == BEAM_PIN);
  BrokenCount    = 0;
  IntactCount    = 0;
  LastSampleTime = ES_Timer_GetTime();
}

/****************************************************************************
 Function
     Check4BeamBreak

 Returns
     bool: true if a new beam break (H->L) was detected and posted

 Description
     Event checker. Called by the framework on every pass; only actually
     reads the pin once every SAMPLE_TIME ms.
****************************************************************************/
bool Check4BeamBreak(void)
{
  bool     ReturnVal = false;
  uint16_t Now       = ES_Timer_GetTime();

  if ((uint16_t)(Now - LastSampleTime) >= SAMPLE_TIME)
  {
    uint8_t CurrentBeamState = BEAM_PIN;
    LastSampleTime = Now;

    if (BEAM_BROKEN == CurrentBeamState)
    {
      IntactCount = 0;
      if (BrokenCount < 255) { BrokenCount++; }

      // armed and solidly broken: this is one swipe
      if (Armed && (BrokenCount >= BROKEN_SAMPLES))
      {
        ES_Event_t ThisEvent;
        ThisEvent.EventType  = ES_BEAM_BROKEN;
        ThisEvent.EventParam = 0;
        PostTaskService(ThisEvent);
        ReturnVal = true;
        Armed = false;               // wait for the hand to leave
      }
    }
    else                             // intact
    {
      BrokenCount = 0;
      if (IntactCount < 255) { IntactCount++; }

      // hand has been gone long enough: ready for the next swipe
      if (IntactCount >= REARM_SAMPLES)
      {
        Armed = true;
      }
    }
  }

  return ReturnVal;
}

/****************************************************************************
 Function
     IsBeamBroken

 Returns
     bool: true if the beam is broken right now
****************************************************************************/
bool IsBeamBroken(void)
{
  return (BEAM_BROKEN == BEAM_PIN);
}

/*------------------------------ End of file ------------------------------*/