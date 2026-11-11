/****************************************************************************
 Module
   TaskService.c

 Revision
   1.3.0

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
   The current task's LED is solid on; the other LEDs are off.
   When a task completes its LED turns off. When idle (task 0) all LEDs
   are off.

   Task 2 (beam-break swipes):
     State: "Waiting for numSwipes >= targetSwipes"
       entry            : numSwipes = 0, print targetSwipes once,
                          start the print timer
       ES_BEAM_BROKEN and numSwipes < targetSwipes
                        : numSwipes++ (inactivity timer reset goes here
                          later)
       numSwipes >= targetSwipes
                        : print "Task 2 Completed" immediately,
                          stop printing, LED 2 off, go idle
                          (later: start the next task instead)
       print timer      : print "numSwipes = #" every TASK2_PRINT_TIME ms

   Task 3 (rocker switches):
     entry              : print "targetDecimal = #" and
                          "currentBinary = ####" (switch 1 first)
     ES_SWITCH_CHANGED  : print "currentBinary = ####"
     currentBinary == targetDecimal (checked on each switch change)
                        : print "Task 3 Completed!", LED 3 off, go idle

 Notes
   ES_NEW_KEY reaches this service because Check4Keystroke() in
   EventCheckers.c posts it with ES_PostAll().
   ES_BEAM_BROKEN is posted here by Check4BeamBreak() in BeamBreak.c.
   Timers: TASK2_PRINT_TIMER (see ES_Configure.h).

 History
 When           Who     What/Why
 -------------- ---     --------
 11/10/26       CH      Task 3: targetDecimal, complete on match
 11/10/26       CH      LEDs solid instead of blinking; LED off on
                        task complete
 11/10/26       CH      added Task 2 (beam-break swipe counting)
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
#include "dbprintf.h"

// Beam break sensor
#include "BeamBreak.h"

// Rocker switch
#include "RockerSwitch.h"

/*----------------------------- Module Defines ----------------------------*/
#define TASK_IDLE        0
#define TASK_SWIPE       2         // Task 2 = beam-break swipes
#define TASK_SWITCH      3         // Task 3 = rocker switch

#define NUM_TASKS        3
#define LED_ON           1
#define LED_OFF          0

// Task 2
#define TASK2_TARGET_SWIPES  5     // fixed for now, randomize later
#define TASK2_PRINT_TIME     500   // ms between "numSwipes = #" prints

// Task 3
#define TASK3_TARGET_DECIMAL 8     // 0..8, fixed for now (1000), randomize later

typedef enum
{
  TASK2_INACTIVE = 0,              // Task 2 not selected
  TASK2_WAITING                    // waiting for numSwipes >= targetSwipes
} Task2State_t;

/*---------------------------- Module Functions ---------------------------*/
static void SetTask(uint8_t NewTask);
static void AllLEDsOff(void);
static void TaskLEDOn(uint8_t WhichTask);
static void StartTask2(void);
static void StopTask2(void);
static void StartTask3(void);
static void PrintSwitches(uint8_t Value);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;

// which task is active right now (0 = idle)
static uint8_t currentTask;

// Task 2
static Task2State_t Task2State;
static uint8_t targetSwipes;

// Task 3
static uint8_t targetDecimal;
static uint8_t numSwipes;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitTaskService

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, sets up the LED pins and the beam sensor,
     starts idle and posts the initial ES_INIT event
****************************************************************************/
bool InitTaskService(uint8_t Priority)
{
  ES_Event_t ThisEvent;

  MyPriority = Priority;

  // RB8, RB9 and RB10 have no analog function, so only TRIS needs setting
  TRISBbits.TRISB8  = 0;     // LED 1 output
  TRISBbits.TRISB9  = 0;     // LED 2 output
  TRISBbits.TRISB10 = 0;     // LED 3 output
  AllLEDsOff();              // start with every LED off

  InitBeamBreak();           // RB5 input for the break-beam receiver
  InitRockerSwitch();        // RB3 input (pull-up) for the rocker switch

  currentTask  = TASK_IDLE;
  Task2State   = TASK2_INACTIVE;
  targetSwipes = TASK2_TARGET_SWIPES;
  numSwipes    = 0;
  targetDecimal = TASK3_TARGET_DECIMAL;

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
   ES_BEAM_BROKEN: count a swipe while Task 2 is waiting.
   ES_SWITCH_CHANGED: only while Task 3, print currentBinary (1 = closed,
   0 = open, switch 1 first); complete Task 3 when it equals
   targetDecimal.
   ES_TIMEOUT from TASK2_PRINT_TIMER: print numSwipes.
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

    case ES_SWITCH_CHANGED:   // rocker switch: 1 = closed, 0 = open
    {
      if (TASK_SWITCH == currentTask)   // only active during Task 3
      {
        PrintSwitches((uint8_t)ThisEvent.EventParam);

        if (targetDecimal == ThisEvent.EventParam)   // switches match target
        {
          DB_printf("Task 3 Completed!\r\n");
          // LED 3 off, currentTask = 0.
          // Later: SetTask(<next task>) here to start the next task.
          SetTask(TASK_IDLE);
        }
      }
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

    case ES_BEAM_BROKEN:   // one swipe (beam H->L)
    {
      if (TASK2_WAITING == Task2State)
      {
        numSwipes++;

        if (numSwipes >= targetSwipes)
        {
          // event: numSwipes >= targetSwipes -> task complete
          DB_printf("numSwipes = %d\r\n", numSwipes);
          DB_printf("Task 2 Completed\r\n");
          // stops the prints, turns LED 2 off, currentTask = 0.
          // Later: SetTask(<next task>) here to start the next task.
          SetTask(TASK_IDLE);
        }
        else
        {
          // event: beam broke and numSwipes < targetSwipes
          // TODO: reset the 20 s inactivity timer here once it's added
        }
      }
    }
    break;

    case ES_TIMEOUT:
    {
      if (TASK2_PRINT_TIMER == ThisEvent.EventParam)   // print timer
      {
        if (TASK2_WAITING == Task2State)
        {
          DB_printf("numSwipes = %d\r\n", numSwipes);
          ES_Timer_InitTimer(TASK2_PRINT_TIMER, TASK2_PRINT_TIME);
        }
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
     tasks 1-3, turns that task's LED on (solid). For idle (task 0) all
     LEDs stay off.
     Entering task 2 starts the swipe count; leaving it stops it.
****************************************************************************/
static void SetTask(uint8_t NewTask)
{
  StopTask2();                                  // leave Task 2 if active

  currentTask = NewTask;
  DB_printf("currentTask = %d\r\n", currentTask);

  AllLEDsOff();                                 // only one LED at a time
  if ((currentTask >= 1) && (currentTask <= NUM_TASKS))
  {
    TaskLEDOn(currentTask);                     // solid on
  }

  if (TASK_SWIPE == currentTask)
  {
    StartTask2();
  }

  if (TASK_SWITCH == currentTask)
  {
    StartTask3();
  }
}

/****************************************************************************
 Function
     StartTask3

 Description
     Entry action for Task 3: set the target, print it once, then print
     the switches' starting position.
****************************************************************************/
static void StartTask3(void)
{
  targetDecimal = TASK3_TARGET_DECIMAL;         // randomize 0..8 here later

  DB_printf("targetDecimal = %d\r\n", targetDecimal);
  PrintSwitches(QueryRockerSwitch());           // starting position
}

/****************************************************************************
 Function
     StartTask2

 Description
     Entry action for "Waiting for numSwipes >= targetSwipes":
     reset the count, print the target once, start periodic prints.
****************************************************************************/
static void StartTask2(void)
{
  targetSwipes = TASK2_TARGET_SWIPES;           // randomize here later
  numSwipes    = 0;
  Task2State   = TASK2_WAITING;

  DB_printf("targetSwipes = %d\r\n", targetSwipes);
  ES_Timer_InitTimer(TASK2_PRINT_TIMER, TASK2_PRINT_TIME);
}

/****************************************************************************
 Function
     StopTask2

 Description
     Leaves Task 2: stops the print timer and goes inactive.
****************************************************************************/
static void StopTask2(void)
{
  ES_Timer_StopTimer(TASK2_PRINT_TIMER);
  Task2State = TASK2_INACTIVE;
}

/****************************************************************************
 Function
     PrintSwitches

 Description
     Prints "currentBinary = ####", the 4 switches as binary digits:
     switch 1 (RB3), switch 2 (RB4), switch 3 (RB11), switch 4 (RB12),
     e.g. "1001"
****************************************************************************/
static void PrintSwitches(uint8_t Value)
{
  DB_printf("currentBinary = %d%d%d%d\r\n",
      (Value >> SWITCH1_BIT) & 1,
      (Value >> SWITCH2_BIT) & 1,
      (Value >> SWITCH3_BIT) & 1,
      (Value >> SWITCH4_BIT) & 1);
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
  LATBbits.LATB9  = LED_OFF;
  LATBbits.LATB10 = LED_OFF;
}

/****************************************************************************
 Function
     TaskLEDOn

 Description
     Turns on the LED that belongs to WhichTask (1, 2 or 3)
****************************************************************************/
static void TaskLEDOn(uint8_t WhichTask)
{
  switch (WhichTask)
  {
    case 1:
      LATBbits.LATB8 = LED_ON;
      break;
    case 2:
      LATBbits.LATB9 = LED_ON;
      break;
    case 3:
      LATBbits.LATB10 = LED_ON;
      break;
    default:
      break;
  }
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/