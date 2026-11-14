/****************************************************************************
 Module
   StartButton.c

 Revision
   1.0.0

 Description
   Hardware + event checker for the start push button (replaces SPACE).
     Button NO terminal 1 -> RB11 (pin 22)
     Button NO terminal 2 -> GNDREF
   Internal pull-up on RB11:
     released -> pull-up holds pin high -> reads 1
     pressed  -> pin shorted to GND     -> reads 0
   The button is a LATCHING (push-on / push-off) type: each press flips
   the contacts and they stay that way, so the pin alternates between 0
   and 1 on successive presses.
   Check4StartButton() samples RB11 every SAMPLE_TIME ms (debounce) and
   posts ES_START_PRESSED to GameService on EVERY change of the pin
   (either edge), so each physical press is one event, whichever
   position the button was left in.

 Notes
   RB11 has no analog function, so no ANSELB setting is needed.
   RB11 is also PGEC2: fine with ICESEL = ICS_PGx1 (programming on PGx1).
   The button's built-in LED is separate from the switch contacts and is
   not used by this module.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/13/26       CH      latching button: post on both edges, not just H->L
 11/13/26       CH      started coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/StartButton.h"

#include <xc.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "GameService.h"

/*----------------------------- Module Defines ----------------------------*/
#define BUTTON_PIN     PORTBbits.RB11
#define BUTTON_UP      1         // released (pulled up)
#define BUTTON_DOWN    0         // pressed (shorted to GND)
#define SAMPLE_TIME    20        // ms between samples (debounce)

/*---------------------------- Module Variables ---------------------------*/
static uint8_t  LastButtonState;
static uint16_t LastSampleTime;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitStartButton

 Description
     RB11 as a digital input with the internal pull-up on. Records the
     starting state so the first sample doesn't produce a false press.
****************************************************************************/
void InitStartButton(void)
{
  TRISBbits.TRISB11 = 1;     // input
  CNPDBbits.CNPDB11 = 0;     // internal pull-down off
  CNPUBbits.CNPUB11 = 1;     // internal pull-up on (button goes to GND)

  LastButtonState = BUTTON_PIN;
  LastSampleTime  = ES_Timer_GetTime();
}

/****************************************************************************
 Function
     Check4StartButton

 Returns
     bool: true if a press (a change in either direction) was posted

 Description
     Event checker. Called by the framework on every pass; only reads the
     pin once every SAMPLE_TIME ms so contact bounce is ignored.
****************************************************************************/
bool Check4StartButton(void)
{
  bool     ReturnVal = false;
  uint16_t Now       = ES_Timer_GetTime();

  if ((uint16_t)(Now - LastSampleTime) >= SAMPLE_TIME)
  {
    uint8_t CurrentButtonState = BUTTON_PIN;
    LastSampleTime = Now;

    // latching button: every change (on->off or off->on) is one press
    if (CurrentButtonState != LastButtonState)
    {
      ES_Event_t ThisEvent;
      ThisEvent.EventType  = ES_START_PRESSED;
      ThisEvent.EventParam = 0;
      PostGameService(ThisEvent);
      ReturnVal = true;
    }
    LastButtonState = CurrentButtonState;
  }

  return ReturnVal;
}

/****************************************************************************
 Function
     IsStartButtonDown

 Returns
     bool: true while the contacts are closed (latched "on")
****************************************************************************/
bool IsStartButtonDown(void)
{
  return (BUTTON_DOWN == BUTTON_PIN);
}

/*------------------------------ End of file ------------------------------*/