/****************************************************************************
 Module
   BeamBreak.c

 Revision
   1.0.0

 Description
   Hardware + event checker for the IR break-beam sensor (CHEF Task 2).
     Receiver OUT -> series resistor -> RB5 (pin 14, 5V tolerant)
     Beam intact  -> OUT high (1)
     Beam broken  -> OUT low  (0)
   Check4BeamBreak() samples RB5 every SAMPLE_TIME ms (not every pass of
   the framework loop) and posts ES_BEAM_BROKEN to TaskService on each
   intact -> broken (H->L) transition. Sampling at 10 ms also acts as a
   simple debounce.

 Notes
   RB5 has no analog function, so no ANSELB setting is needed.
   RB5 is also PGED3: make sure the config bits use ICESEL = ICS_PGx1.

 History
 When           Who     What/Why
 -------------- ---     --------
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

/*---------------------------- Module Variables ---------------------------*/
static uint8_t  LastBeamState;
static uint16_t LastSampleTime;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitBeamBreak

 Description
     RB5 as a digital input with no internal pull-up/pull-down (the
     receiver module has its own). Records the starting state so the
     first sample doesn't produce a false edge.
****************************************************************************/
void InitBeamBreak(void)
{
  TRISBbits.TRISB5 = 1;      // input
  CNPUBbits.CNPUB5 = 0;      // internal pull-up off
  CNPDBbits.CNPDB5 = 0;      // internal pull-down off

  LastBeamState  = BEAM_PIN;
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

    // only the intact -> broken transition counts as one swipe
    if ((CurrentBeamState != LastBeamState) &&
        (BEAM_BROKEN == CurrentBeamState))
    {
      ES_Event_t ThisEvent;
      ThisEvent.EventType  = ES_BEAM_BROKEN;
      ThisEvent.EventParam = 0;
      PostTaskService(ThisEvent);
      ReturnVal = true;
    }
    LastBeamState = CurrentBeamState;
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
