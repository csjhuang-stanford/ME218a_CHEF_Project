/****************************************************************************
// Part 4.3) Modified the TemplateService.c to MessageService.c
Module
   MessageService.c

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
#include "../ProjectHeaders/MessageService.h"

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
#include "LEDServiceFSM.h" // Part 4.2)

/*----------------------------- Module Defines ----------------------------*/
// Part 3.5)
#define KEY_A 'a'       // key that triggers ES_KEY_A_PRESSED
#define KEY_B 'b'       // key that triggers ES_KEY_B_PRESSED

// these times assume a 10.000mS/tick timing
#define ONE_SEC 1000 // uncommented for Part 4.3)
// #define HALF_SEC (ONE_SEC / 2)
// #define TWO_SEC (ONE_SEC * 2)
// #define FIVE_SEC (ONE_SEC * 5)

// #define ENTER_POST     ((MyPriority<<3)|0)
// #define ENTER_RUN      ((MyPriority<<3)|1)
// #define ENTER_TIMEOUT  ((MyPriority<<3)|2)

//#define TEST_INT_POST
//#define BLINK LED
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
// #ifdef BLINK_LED
// static void InitLED(void);
// static void BlinkLED(void);
// #endif

// #ifdef TEST_INT_POST
// static void InitTMR2(void);
// static void StartTMR2(void);
// #endif
/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
// // add a deferral queue for up to 3 pending deferrals +1 to allow for overhead
// static ES_Event_t DeferralQueue[3 + 1];

// Part 4.3)
static const char Message[] =
    "My name is Cindy Huang, and I am a 1st Year ME PhD in ME218a.        ";
// index of the next character to send
static uint8_t CharIndex;

/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
 Function
     InitMessageService
 
 Parameters
     uint8_t : the priorty of this service
 
 Returns
     bool, false if error in initialization, true otherwise
 
 Description
     Saves away the priority and posts the initial ES_INIT event
****************************************************************************/
// Part 4.3)
bool InitMessageService(uint8_t Priority)
{
  ES_Event_t ThisEvent;
 
  MyPriority = Priority;
  CharIndex = 0; // initialize the index of the next character to send
 
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
     PostMessageService
 
 Parameters
     ES_Event_t ThisEvent, the event to post to the queue
 
 Returns
     bool false if the Enqueue operation failed, true otherwise
 
 Description
     Posts an event to this service's queue
****************************************************************************/
// Part 4.3)
bool PostMessageService(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}
 
/****************************************************************************
 Function
    RunMessageService
 
 Parameters
   ES_Event_t : the event to process
 
 Returns
   ES_Event_t, ES_NO_EVENT if no error ES_ERROR otherwise
 
 Description
   ES_NEW_KEY: if the key is 'a' or 'b', post the matching new event to this
   service. ES_KEY_A_PRESSED / ES_KEY_B_PRESSED: print the service, the event
   and the key.
****************************************************************************/
// Part 4.3)
ES_Event_t RunMessageService(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ES_Event_t NewEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
 
  switch (ThisEvent.EventType)
  {
    case ES_INIT:
    {
      ES_Timer_InitTimer(MESSAGE_TIMER, ONE_SEC);   // first character in 1 s
      DB_printf("\rES_INIT received in MessageService (Service %d)\r\n",
          MyPriority);
    }
    break;
 
    case ES_TIMEOUT:
    {
      if (MESSAGE_TIMER == ThisEvent.EventParam)
      {
        // send the next character to the LED Service
        NewEvent.EventType  = ES_ADD_CHAR_TO_DISPLAY;
        NewEvent.EventParam = Message[CharIndex];
        PostLEDServiceFSM(NewEvent);
 
        // move to the next character, starting over at the end
        CharIndex++;
        if ('\0' == Message[CharIndex])
        {
          CharIndex = 0;
        }
 
        ES_Timer_InitTimer(MESSAGE_TIMER, ONE_SEC); // next character in 1 s
      }
    }
    break;
 
    default:
    {}
    break;
  }
 
  return ReturnEvent;
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

