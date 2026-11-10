/****************************************************************************
// Part 4.2) Modified the TemplateService.c to LEDServiceFSM.c
Module
   LEDServiceFSM.c

 Revision
   1.0.1

 Description
   This is the first service for the Test Harness under the
   Gen2 Events and Services Framework.

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 10/26/17 18:26 jec     moves definition of ALL_BITS to ES_Port.h
 10/19/17 21:28 jec     meaningless change to test updating
 10/19/17 18:42 jec     removed referennces to driverlib and programmed the
                        ports directly
 08/21/17 21:44 jec     modified LED blink routine to only modify bit 3 so that
                        I can test the new new framework debugging lines on PF1-2
 08/16/17 14:13 jec      corrected ONE_SEC constant to match Tiva tick rate
 11/02/13 17:21 jec      added exercise of the event deferral/recall module
 08/05/13 20:33 jec      converted to test harness service
 01/16/12 09:58 jec      began conversion from TemplateFSM.c
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
// This module
#include "../ProjectHeaders/LEDServiceFSM.h"

// debugging printf()

// Hardware
#include <xc.h>
//#include <proc/p32mx170f256b.h>

// Event & Services Framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_DeferRecall.h"
#include "ES_Port.h"
#include "terminal.h"
#include "dbprintf.h"

// Part 4.2) Hardware abstraction layers
#include "PIC32_SPI_HAL.h"
#include "DM_Display.h"

/*----------------------------- Module Defines ----------------------------*/
// // Part 3.5)
// #define KEY_A 'a'       // key that triggers ES_KEY_A_PRESSED
// #define KEY_B 'b'       // key that triggers ES_KEY_B_PRESSED

// Part 4.2)
#define COLS_PER_CHAR    4        // each font character is 4 columns wide
#define SPI_BIT_TIME_NS  1000     // 1 us bit time = 1 MHz SPI clock, so a
                                  // row (4 x 16 bits) takes ~64 us to send

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
// Part 4.2)
static void InitSPI(void);

// #ifdef BLINK_LED
// static void InitLED(void);
// static void BlinkLED(void);
// #endif

// #ifdef TEST_INT_POST
// static void InitTMR2(void);
// static void StartTMR2(void);
// #endif
/*---------------------------- Module Variables ---------------------------*/
// Part 4.2) State Variable
static LEDState_t CurrentState;

// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
// add a deferral queue for up to 3 pending deferrals +1 to allow for overhead
static ES_Event_t DeferralQueue[3 + 1];

/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
 Function
     InitLEDServiceFSM
 
 Parameters
     uint8_t : the priorty of this service
 
 Returns
     bool, false if error in initialization, true otherwise
 
 Description
     Saves away the priority and posts the initial ES_INIT event
****************************************************************************/
// Part 4.2)
bool InitLEDServiceFSM(uint8_t Priority)
{
  ES_Event_t ThisEvent;
 
  MyPriority = Priority;
 
  // initialize the deferral queue for characters that arrive mid-update
  ES_InitDeferralQueueWith(DeferralQueue, ARRAY_SIZE(DeferralQueue));
 
  // set up SPI1 for the display, then initialize the display.
  // This runs once, before the framework starts dispatching events, so the
  // state machine can assume the display is already initialized.
  InitSPI();
  while (false == DM_TakeInitDisplayStep())
  {}
 
  // put us into the first state
  CurrentState = WaitForChar;
 
  // post the initial transition event
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService(MyPriority, ThisEvent) == true)
  {
    return true;
  }
  else
  {
    return false;
  }
}
 
/****************************************************************************
 Function
     PostLEDServiceFSM
 
 Parameters
     ES_Event_t ThisEvent, the event to post to the queue
 
 Returns
     bool false if the Enqueue operation failed, true otherwise
 
 Description
     Posts an event to this service's queue
****************************************************************************/
// Part 4.2)
bool PostLEDServiceFSM(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}
 
/****************************************************************************
 Function
    RunLEDServiceFSM
 
 Parameters
   ES_Event_t : the event to process
 
 Returns
   ES_Event_t, ES_NO_EVENT if no error ES_ERROR otherwise
 
 Description
   ES_NEW_KEY: if the key is 'a' or 'b', post the matching new event to this
   service. ES_KEY_A_PRESSED / ES_KEY_B_PRESSED: print the service, the event
   and the key.
****************************************************************************/
// Part 4.2)
ES_Event_t RunLEDServiceFSM(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ES_Event_t NewEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
 
  switch (CurrentState)
  {
    case WaitForChar:        // idle until a character arrives
    {
      if (ES_ADD_CHAR_TO_DISPLAY == ThisEvent.EventType)
      {
        // scroll the display 1 character (4 columns) to the left,
        // write the new character in the rightmost position,
        // and start updating the display with the first row
        DM_ScrollDisplayBuffer(COLS_PER_CHAR);
        DM_AddChar2DisplayBuffer((unsigned char)ThisEvent.EventParam);
        DM_TakeDisplayUpdateStep();     // 8 rows, so never done after 1 row
 
        NewEvent.EventType = ES_ROW_UPDATE_COMPLETE;
        PostLEDServiceFSM(NewEvent);
        CurrentState = UpdateDisplay;
      }
      // ES_INIT and anything else: nothing to do
    }
    break;
 
    case UpdateDisplay:      // send the display to the LEDs, one row per event
    {
      switch (ThisEvent.EventType)
      {
        case ES_ROW_UPDATE_COMPLETE:
        {
          if (true == DM_TakeDisplayUpdateStep())   // all 8 rows sent
          {
            // bring back any characters that arrived during the update
            ES_RecallEvents(MyPriority, DeferralQueue);
            CurrentState = WaitForChar;
          }
          else                                      // rows left
          {
            NewEvent.EventType = ES_ROW_UPDATE_COMPLETE;
            PostLEDServiceFSM(NewEvent);
          }
        }
        break;
 
        case ES_ADD_CHAR_TO_DISPLAY:   // arrived mid-update: save for later
        {
          ES_DeferEvent(DeferralQueue, ThisEvent);
        }
        break;
 
        default:
        {}
        break;
      }
    }
    break;
 
    default:
    {}
    break;
  }
 
  return ReturnEvent;
}

/****************************************************************************
 Function
     QueryLEDServiceFSM
 
 Parameters
     None
 
 Returns
     LEDState_t The current state of the LED Service state machine
 
 Description
     returns the current state of the LED Service state machine
****************************************************************************/
LEDState_t QueryLEDServiceFSM(void)
{
  return CurrentState;
}

/***************************************************************************
 private functions
 ***************************************************************************/
/****************************************************************************
// Part 4.2)
 Function
     InitSPI
 
 Description
     Sets up SPI1 for the MAX7219 display with the SPI HAL (same settings as
     Parts 1 and 2, except the faster bit rate)
****************************************************************************/
static void InitSPI(void)
{
  SPISetup_BasicConfig(SPI_SPI1);
  SPISetup_SetLeader(SPI_SPI1, SPI_SMP_MID);
  SPISetup_SetBitTime(SPI_SPI1, SPI_BIT_TIME_NS);
  SPISetup_MapSSOutput(SPI_SPI1, SPI_RPA0);          // after SetLeader
  SPISetup_MapSDOutput(SPI_SPI1, SPI_RPA1);
  SPISetup_SetClockIdleState(SPI_SPI1, SPI_CLK_HI);  // CKP = 1
  SPISetup_SetActiveEdge(SPI_SPI1, SPI_SECOND_EDGE); // CKE = 0
  SPISetup_SetXferWidth(SPI_SPI1, SPI_16BIT);
  SPISetEnhancedBuffer(SPI_SPI1, true);
  SPISetup_EnableSPI(SPI_SPI1);
}

// #ifdef BLINK_LED
// #define LED LATBbits.LATB6
// static void InitLED(void)
// {
//   LED = 0; //start with it off
//   TRISBbits.TRISB6 = 0; // set RB6 as an output
// }

// static void BlinkLED(void)
// {
//   // toggle state of LED
//   LED = ~LED;
// }
// #endif

// #ifdef TEST_INT_POST
// #include <sys/attribs.h> // for ISR macors

// // for testing posting from interrupts.
// // Intializes TMR2 to gerenate an interrupt at 100ms
// static void InitTMR2(void)
// {
//   // turn timer off
//   T2CONbits.ON = 0;
//   // Use internal peripheral clock
//   T2CONbits.TCS = 0;
//   // setup for 16 bit mode
//   T2CONbits.T32 = 0;
//   // set prescale to 1:1
//   T2CONbits.TCKPS = 0;
//   // load period value
//   PR2 = 2000-1; // creates a 100ms period with a 20MHz peripheral clock
//   // set priority
//   IPC2bits.T2IP = 2;
//   // clear interrupt flag
//   IFS0bits.T2IF = 0;
//   // enable the timer interrupt
//   IEC0bits.T2IE = 1;
// }

// // Clears and Starts TMR2
// static void StartTMR2(void)
// {
//   // clear timer
//   TMR2 = 0;
//   // start timer
//   //LATBbits.LATB14 = 0;
//   T2CONbits.ON = 1;
// }

// void __ISR(_TIMER_2_VECTOR, IPL2AUTO) Timer2ISR(void)
// {
//   // clear flag
//   IFS0bits.T2IF = 0;
//   // post event
//   static ES_Event_t interruptEvent = {ES_SHORT_TIMEOUT, 0};
//   PostTestHarnessService0(interruptEvent);
  
//   // stop timer
//   T2CONbits.ON = 0;
//   return;
// }
// #endif

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

