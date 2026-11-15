/****************************************************************************
 Module
     main.c
 Description
     starter main() function for Events and Services Framework applications
 Notes

*****************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Port.h"
#include "PWM_PIC32.h"     // Jay: buzzer + servo
#include "DisplayLink.h"    // SPI1 link to the display PIC32
#include "dbprintf.h"       // report PWM pin-mapping failures

void main(void)
{
  ES_Return_t ErrorType = Success;

  _HW_PIC32Init(); // basic PIC hardware init
  // Your hardware initialization function calls go here
  // Pin map (PIC32MX170F256B, 28-pin):
  //   RB0/RB1 (4,5)    PGED1/PGEC1 programming
  //   RB6/RB7 (15,16)  CP2102 USB serial
  //   RA0  (2)         SPI CS   -> display PIC32 RB15 (SS1) (DisplayLink.c)
  //   RA1  (3)         SPI SDO1 -> display PIC32 RB11
  //   RB14 (25)        SPI SCK1 -> display PIC32 RB14 (fixed SCK1 pin)
  //   RB5  (14)        break-beam input        (BeamBreak.c)
  //   RB8/9/10 (17,18,21) task LEDs            (TaskService.c)
  //   RB15 (26)        buzzer PWM, OC1         (Jay, was RB3)
  //   RB13 (24)        pot, AN11               (Potentiometer.c)
  //   RA4  (12)        servo 1 PWM, OC4        (RA4 can't use OC2)
  //   RB11 (22)        start push button, to GND (StartButton.c)
  //   RA3  (10)        servo 2 PWM, OC3 (burger dispenser, positional)
  //   RB3, RB4, RB2, RB12 rocker switches 1-4 (RockerSwitch.c)
  //   RA2  (9)         spare (SCK can't be remapped to it)

  // SPI1 link to the display PIC32. Must come BEFORE the PWM setup:
  // it writes a PPS register (RPA1R = SDO1), and if the PWM library locks
  // the PPS registers afterwards that write would be ignored.
  InitDisplayLink();

  // Buzzer: PWM ch 1 on Timer2. Servos: ch 4 (servo 1) and ch 3 (servo 2)
  // share Timer3 at 50 Hz. Ch 2 (OC2) is unused.
  ANSELBbits.ANSB15 = 0;  // RB15 digital
  TRISBbits.TRISB15 = 0;  // RB15 output (buzzer)
  TRISAbits.TRISA3  = 0;  // RA3 output (servo 2), no analog function
  TRISAbits.TRISA4  = 0;  // RA4 output (servo 1), no analog function
                          // RA4 is SOSCO: config bit FSOSCEN must be OFF

  PWMSetup_BasicConfig(4);                     // channels 1-4 (2 unused)
  PWMSetup_AssignChannelToTimer(1, _Timer2_);  // buzzer
  PWMSetup_AssignChannelToTimer(3, _Timer3_);  // servo 2
  PWMSetup_AssignChannelToTimer(4, _Timer3_);  // servo 1
  PWMSetup_SetFreqOnTimer(1000, _Timer2_);
  PWMSetup_SetFreqOnTimer(50, _Timer3_);
  // each mapping returns false if the pin can't be used (wrong channel for
  // that pin, or the pin registers are locked): report it on PuTTY
  if (!PWMSetup_MapChannelToOutputPin(1, PWM_RPB15))
  {
    DB_printf("PWM ERROR: buzzer channel 1 -> RB15 failed\r\n");
  }
  if (!PWMSetup_MapChannelToOutputPin(3, PWM_RPA3))
  {
    DB_printf("PWM ERROR: servo 2 channel 3 -> RA3 failed\r\n");
  }
  if (!PWMSetup_MapChannelToOutputPin(4, PWM_RPA4))
  {
    DB_printf("PWM ERROR: servo 1 channel 4 -> RA4 failed\r\n");
  }
  PWMOperate_SetDutyOnChannel(0, 1);           // buzzer off to start
  PWMOperate_SetPulseWidthOnChannel(2500, 4);  // servo 1: 2500 ticks @ 2.5 MHz = 1 ms
  PWMOperate_SetPulseWidthOnChannel(2500, 3);  // servo 2: 1 ms = rest (min)


  // now initialize the Events and Services Framework and start it running
  ErrorType = ES_Initialize(ES_Timer_RATE_1mS);
  if (ErrorType == Success)
  {
    ErrorType = ES_Run();
  }
  //if we got to here, there was an error
  switch (ErrorType)
  {
    case FailedPost:
    {
      ErrorType = 0; // a hack to get a readable assert message
      assert( FailedPost == ErrorType );
      //DB_printf("Failed on attempt to Post\n");
    }
    break;
    case FailedPointer:
    {
      ErrorType = 0; // a hack to get a readable assert message
      assert( FailedPointer  == ErrorType );
//      DB_printf("Failed on NULL pointer\n");
    }
    break;
    case FailedInit:
    {
      ErrorType = 0; // a hack to get a readable assert message
      assert( FailedInit  == ErrorType );
//      DB_printf("Failed Initialization\n");
    }
    break;
    default:
    {
      ErrorType = 0; // a hack to get a readable assert message
      assert( FailedOther  == ErrorType );
//      DB_printf("Other Failure\n");
    }
    break;
  }
  for ( ; ;)
  {
    ;
  }
}