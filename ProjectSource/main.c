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

void main(void)
{
  ES_Return_t ErrorType = Success;

  _HW_PIC32Init(); // basic PIC hardware init
  // Your hardware initialization function calls go here
  // Pin map (PIC32MX170F256B, 28-pin):
  //   RB0/RB1 (4,5)    PGED1/PGEC1 programming
  //   RB6/RB7 (15,16)  CP2102 USB serial
  //   RA0/RA1/RB14     SPI (SS / SDO / SCK)
  //   RB5  (14)        break-beam input        (BeamBreak.c)
  //   RB8/9/10 (17,18,21) task LEDs            (TaskService.c)
  //   RB15 (26)        buzzer PWM, OC1         (Jay, was RB3)
  //   RB13 (24)        pot, AN11               (Potentiometer.c)
  //   RA4  (12)        servo 1 PWM, OC4        (was RB11; RA4 can't use OC2)
  //   RA3  (10)        servo 2 PWM, OC3
  //   RB3, RB4, RB2, RB12 rocker switches 1-4 (RockerSwitch.c)
  //   RA2, RB11        spare

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
  PWMSetup_MapChannelToOutputPin(1, PWM_RPB15);
  PWMSetup_MapChannelToOutputPin(3, PWM_RPA3);
  PWMSetup_MapChannelToOutputPin(4, PWM_RPA4);
  PWMOperate_SetDutyOnChannel(0, 1);           // buzzer off to start
  PWMOperate_SetPulseWidthOnChannel(2500, 4);  // servo 1: 2500 ticks @ 2.5 MHz = 1 ms
  PWMOperate_SetPulseWidthOnChannel(3750, 3);  // servo 2: 1.5 ms = stopped


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