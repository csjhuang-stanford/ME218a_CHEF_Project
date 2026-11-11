/****************************************************************************
 Module
   RockerSwitch.c

 Revision
   1.2.0

 Description
   Hardware + event checker for four 2-position rocker switches.
     Switch 1: RB3  (pin 7)  -> switch -> GND
     Switch 2: RB4  (pin 11) -> switch -> GND
     Switch 3: RB11 (pin 22) -> switch -> GND
     Switch 4: RB12 (pin 23) -> switch -> GND
   Internal pull-ups are enabled on all four pins, so for each switch:
     closed -> pin pulled to GND -> reads 0 -> report 1
     open   -> pull-up holds pin -> reads 1 -> report 0
   The four switches are combined into one value:
     bit 3 = switch 1 (first digit) ... bit 0 = switch 4 (fourth digit)
   Check4RockerSwitch() samples all pins every SAMPLE_TIME ms (debounce)
   and posts ES_SWITCH_CHANGED to TaskService whenever any switch
   changes. EventParam = the combined value (0..15).

 Notes
   RB3 is also AN5 and RB12 is AN12 (analog at reset), so ANSELB bits
   3 and 12 must be cleared.
   RB4 has no analog function, but it is SOSCI: the config bit
   FSOSCEN must be OFF or the secondary oscillator takes the pin.
   RB11 is also PGEC2/TMS: fine with ICESEL = ICS_PGx1 and JTAGEN = OFF.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/10/26       CH      added switches 3, 4 on RB11, RB12, 4-digit value
 11/10/26       CH      added switch 2 on RB4, combined 2-digit value
 11/10/26       CH      started coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/RockerSwitch.h"

#include <xc.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "TaskService.h"

/*----------------------------- Module Defines ----------------------------*/
#define SWITCH1_PIN    PORTBbits.RB3
#define SWITCH2_PIN    PORTBbits.RB4
#define SWITCH3_PIN    PORTBbits.RB11
#define SWITCH4_PIN    PORTBbits.RB12
#define PIN_LOW        0
#define SAMPLE_TIME    20        // ms between samples (debounce)

/*---------------------------- Module Variables ---------------------------*/
static uint8_t  LastSwitchState;   // combined value, 0..15
static uint16_t LastSampleTime;

/*---------------------------- Module Functions ---------------------------*/
static uint8_t ReadSwitches(void);

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitRockerSwitch

 Description
     RB3, RB4, RB11 and RB12 as digital inputs with internal pull-ups on.
     Records the
     starting state so the first sample doesn't produce a false change.
****************************************************************************/
void InitRockerSwitch(void)
{
  // Switch 1: RB3
  ANSELBbits.ANSB3 = 0;      // digital (RB3 is AN5 by default)
  TRISBbits.TRISB3 = 1;      // input
  CNPDBbits.CNPDB3 = 0;      // internal pull-down off
  CNPUBbits.CNPUB3 = 1;      // internal pull-up on (switch goes to GND)

  // Switch 2: RB4 (no analog function, so no ANSELB bit)
  TRISBbits.TRISB4 = 1;      // input
  CNPDBbits.CNPDB4 = 0;      // internal pull-down off
  CNPUBbits.CNPUB4 = 1;      // internal pull-up on (switch goes to GND)

  // Switch 3: RB11 (no analog function, so no ANSELB bit)
  TRISBbits.TRISB11 = 1;     // input
  CNPDBbits.CNPDB11 = 0;     // internal pull-down off
  CNPUBbits.CNPUB11 = 1;     // internal pull-up on (switch goes to GND)

  // Switch 4: RB12
  ANSELBbits.ANSB12 = 0;     // digital (RB12 is AN12 by default)
  TRISBbits.TRISB12 = 1;     // input
  CNPDBbits.CNPDB12 = 0;     // internal pull-down off
  CNPUBbits.CNPUB12 = 1;     // internal pull-up on (switch goes to GND)

  LastSwitchState = ReadSwitches();
  LastSampleTime  = ES_Timer_GetTime();
}

/****************************************************************************
 Function
     Check4RockerSwitch

 Returns
     bool: true if a switch change was detected and posted

 Description
     Event checker. Called by the framework on every pass; only reads the
     pins once every SAMPLE_TIME ms so contact bounce is ignored.
****************************************************************************/
bool Check4RockerSwitch(void)
{
  bool     ReturnVal = false;
  uint16_t Now       = ES_Timer_GetTime();

  if ((uint16_t)(Now - LastSampleTime) >= SAMPLE_TIME)
  {
    uint8_t CurrentSwitchState = ReadSwitches();
    LastSampleTime = Now;

    if (CurrentSwitchState != LastSwitchState)
    {
      ES_Event_t ThisEvent;
      ThisEvent.EventType  = ES_SWITCH_CHANGED;
      ThisEvent.EventParam = CurrentSwitchState;   // 0..15
      PostTaskService(ThisEvent);
      ReturnVal = true;
    }
    LastSwitchState = CurrentSwitchState;
  }

  return ReturnVal;
}

/****************************************************************************
 Function
     QueryRockerSwitch

 Returns
     uint8_t: combined value right now, bit 3 = switch 1 ... bit 0 = switch 4
****************************************************************************/
uint8_t QueryRockerSwitch(void)
{
  return ReadSwitches();
}

/***************************************************************************
 private functions
 ***************************************************************************/
// each switch: closed pulls the pin low -> 1; open (pulled up) -> 0
static uint8_t ReadSwitches(void)
{
  uint8_t Value = 0;

  if (PIN_LOW == SWITCH1_PIN)
  {
    Value |= (1 << SWITCH1_BIT);
  }
  if (PIN_LOW == SWITCH2_PIN)
  {
    Value |= (1 << SWITCH2_BIT);
  }
  if (PIN_LOW == SWITCH3_PIN)
  {
    Value |= (1 << SWITCH3_BIT);
  }
  if (PIN_LOW == SWITCH4_PIN)
  {
    Value |= (1 << SWITCH4_BIT);
  }
  return Value;
}

/*------------------------------ End of file ------------------------------*/