/****************************************************************************
 Module
   TaskService.c

 Revision
   1.0.0

 Description
   CHEF project task service. Keyboard keys select the current task:
     '0' -> currentTask = 0 (idle)
     '1' -> currentTask = 1
     '2' -> currentTask = 2
     '3' -> currentTask = 3
   Every change prints "currentTask = #" to the terminal.
   Each task has its own LED, driven through the 74ACT244:
     task 1 -> LED 1 on RB8  (pin 17) -> '244 pin 8  -> pin 12
     task 2 -> LED 2 on RB9  (pin 18) -> '244 pin 17 -> pin 3
     task 3 -> LED 3 on RB10 (pin 21) -> '244 pin 15 -> pin 5
   Only the current task's LED blinks; the other LEDs are off.
   When idle (task 0) all LEDs are off.

 Notes
   ES_NEW_KEY reaches this service because Check4Keystroke() in
   EventCheckers.c posts it with ES_PostAll().
   The blink uses framework timer TASK_TIMER (see ES_Configure.h).

 History
 When           Who     What/Why
 -------------- ---     --------
 11/06/26       CH      started from TestHarnessService0.c
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
// This module
#include "../ProjectHeaders/TaskService.h"

// Hardware
#include <xc.h>

// Event & Services Framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Port.h"
#include "dbprintf.h"

/*----------------------------- Module Defines ----------------------------*/
#define TASK_IDLE        0
#define BLINK_TIME       250       // ms on / ms off while blinking

#define NUM_TASKS        3
#define LED_ON           1
#define LED_OFF          0

/*---------------------------- Module Functions ---------------------------*/
static void SetTask(uint8_t NewTask);
static void AllLEDsOff(void);
static void ToggleTaskLED(uint8_t WhichTask);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;

// which task is active right now (0 = idle)
static uint8_t currentTask;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitTaskService

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, sets up the LED pin, starts idle and posts
     the initial ES_INIT event
****************************************************************************/
bool InitTaskService(uint8_t Priority)
{
  ES_Event_t ThisEvent;

  MyPriority = Priority;

  // RB8, RB9 and RB10 have no analog function, so only TRIS needs setting
  TRISBbits.TRISB8  = 0;     // LED 1 output
  TRISBbits.TRISB4  = 0;     // LED 2 output
  TRISBbits.TRISB10 = 0;     // LED 3 output
  AllLEDsOff();              // start with every LED off

  currentTask = TASK_IDLE;

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
     PostTaskService

 Parameters
     ES_Event_t ThisEvent, the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this service's queue
****************************************************************************/
bool PostTaskService(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunTaskService

 Parameters
   ES_Event_t : the event to process

 Returns
   ES_Event_t, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   ES_NEW_KEY '0'-'3': change currentTask (see SetTask).
   ES_TIMEOUT from TASK_TIMER: while a task (1-3) is active, toggle that
   task's LED and restart the blink timer.
****************************************************************************/
ES_Event_t RunTaskService(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  switch (ThisEvent.EventType)
  {
    case ES_INIT:
    {
      DB_printf("\rES_INIT received in TaskService (Service %d)\r\n",
          MyPriority);
      DB_printf("currentTask = %d\r\n", currentTask);
    }
    break;

    case ES_NEW_KEY:   // keys '0'..'3' select the task; others ignored
    {
      if ((ThisEvent.EventParam >= '0') && (ThisEvent.EventParam <= '3'))
      {
        SetTask((uint8_t)(ThisEvent.EventParam - '0'));
      }
    }
    break;

    case ES_TIMEOUT:   // blink timer
    {
      if ((TASK_TIMER == ThisEvent.EventParam) && (TASK_IDLE != currentTask))
      {
        ToggleTaskLED(currentTask);                 // blink this task's LED
        ES_Timer_InitTimer(TASK_TIMER, BLINK_TIME); // keep blinking
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
     QueryCurrentTask

 Returns
     uint8_t the current task (0 = idle)
****************************************************************************/
uint8_t QueryCurrentTask(void)
{
  return currentTask;
}

/***************************************************************************
 private functions
 ***************************************************************************/
/****************************************************************************
 Function
     SetTask

 Description
     Updates currentTask and prints it. Turns every LED off, then, for
     tasks 1-3, turns that task's LED on and starts the blink timer.
     For idle (task 0) the timer is stopped and all LEDs stay off.
****************************************************************************/
static void SetTask(uint8_t NewTask)
{
  currentTask = NewTask;
  DB_printf("currentTask = %d\r\n", currentTask);

  AllLEDsOff();                                 // only one LED at a time
  if ((currentTask >= 1) && (currentTask <= NUM_TASKS))
  {
    ToggleTaskLED(currentTask);                 // off -> on, and it stays on
  }
}

/****************************************************************************
 Function
     AllLEDsOff

 Description
     Turns all three task LEDs off
****************************************************************************/
static void AllLEDsOff(void)
{
  LATBbits.LATB8  = LED_OFF;
  LATBbits.LATB4  = LED_OFF;
  LATBbits.LATB10 = LED_OFF;
}

/****************************************************************************
 Function
     ToggleTaskLED

 Description
     Toggles the LED that belongs to WhichTask (1, 2 or 3)
****************************************************************************/
static void ToggleTaskLED(uint8_t WhichTask)
{
  switch (WhichTask)
  {
    case 1:
      LATBbits.LATB8 = !LATBbits.LATB8;
      break;
    case 2:
      LATBbits.LATB4 = !LATBbits.LATB4;
      break;
    case 3:
      LATBbits.LATB10 = !LATBbits.LATB10;
      break;
    default:
      break;
  }
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/