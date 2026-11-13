/****************************************************************************
 Module
   TaskService.c

 Revision
   1.1.0

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
   Only the current task's LED is on (solid); the other LEDs are off.
   When idle (task 0) all LEDs are off.

   Task 1 (potentiometer):
     State: "Waiting for currentPot within +/- 10% of targetPot"
       entry            : print targetPot once, start the print timer
       print timer      : print "currentPot = #" every TASK1_PRINT_TIME ms
       ES_POT_CHANGED and currentPot outside the range
                        : stay in Task 1
       ES_POT_CHANGED and currentPot inside the range
                        : print "Task 1 completed", 1 s buzzer beep,
                          currentTask back to 0

   Task 2 (beam-break swipes):
     State: "Waiting for currentSwipes >= targetSwipes"
       entry            : currentSwipes = 0, print targetSwipes and
                          currentSwipes once
       ES_BEAM_BROKEN   : currentSwipes++, print currentSwipes
                          (inactivity timer reset goes here later)
       currentSwipes == targetSwipes
                        : print "Task 2 completed", LED 2 solid on,
                          1 s buzzer beep

 Notes
   ES_NEW_KEY reaches this service because Check4Keystroke() in
   EventCheckers.c posts it with ES_PostAll().
   ES_BEAM_BROKEN is posted here by Check4BeamBreak() in BeamBreak.c.
   ES_POT_CHANGED is posted here by Check4PotChange() in Potentiometer.c.
   ES_SWITCH_CHANGED is posted here by Check4RockerSwitch() in
   RockerSwitch.c.
   Timers: TASK1_PRINT_TIMER (see ES_Configure.h).

 History
 When           Who     What/Why
 -------------- ---     --------
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

// Rocker switches (Task 3)
#include "RockerSwitch.h"

// Potentiometer (Task 1)
#include "Potentiometer.h"

// Buzzer beep lives in TestHarnessService0
#include "TestHarnessService0.h"

/*----------------------------- Module Defines ----------------------------*/
#define TASK_IDLE        0
#define TASK_POT         1         // Task 1 = turn pot to target
#define TASK_SWIPE       2         // Task 2 = beam-break swipes
#define TASK_SWITCH      3         // Task 3 = set switches to target

#define NUM_TASKS        3
#define LED_ON           1
#define LED_OFF          0

// Task 2
#define TASK2_TARGET_SWIPES  5     // fixed for now, randomize later

// Task 1
#define TASK1_TARGET_POT     1023  // fixed for now, randomize later
#define TASK1_TOLERANCE      102   // 10% of full scale (1023)
#define TASK1_PRINT_TIME     250   // ms between "currentPot = #" prints

typedef enum
{
  TASK1_INACTIVE = 0,              // Task 1 not selected
  TASK1_WAITING                    // waiting for pot within tolerance
} Task1State_t;

// Task 3
#define TASK3_TARGET_DECIMAL 10    // fixed for now (0-15), randomize later

typedef enum
{
  TASK2_INACTIVE = 0,              // Task 2 not selected
  TASK2_WAITING,                   // waiting for currentSwipes >= targetSwipes
  TASK2_COMPLETE                   // target reached
} Task2State_t;

/*---------------------------- Module Functions ---------------------------*/
static void SetTask(uint8_t NewTask);
static void AllLEDsOff(void);
static void TaskLEDOn(uint8_t WhichTask);
static void StartTask1(void);
static void StopTask1(void);
static bool PotInRange(uint16_t Pot);
static void StartTask2(void);
static void StopTask2(void);
static void StartTask3(void);
static void PrintBinary(uint8_t Value);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;

// which task is active right now (0 = idle)
static uint8_t currentTask;

// Task 1
static Task1State_t Task1State;
static uint16_t targetPot;

// Task 2
static Task2State_t Task2State;

// Task 3
static uint8_t targetDecimal;
static uint8_t targetSwipes;
static uint8_t currentSwipes;

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
  InitRockerSwitch();        // RB3, RB4, RB2, RB12 inputs for Task 3
  InitPotentiometer();       // RB13 / AN11 analog input for Task 1

  currentTask  = TASK_IDLE;
  Task1State   = TASK1_INACTIVE;
  targetPot    = TASK1_TARGET_POT;
  targetDecimal = TASK3_TARGET_DECIMAL;
  Task2State   = TASK2_INACTIVE;
  targetSwipes = TASK2_TARGET_SWIPES;
  currentSwipes    = 0;

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
   ES_POT_CHANGED: check the pot against targetPot while Task 1 waits.
   ES_TIMEOUT from TASK1_PRINT_TIMER: print currentPot for Task 1.
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

    case ES_SWITCH_CHANGED:   // a rocker switch moved, param = 0..15
    {
      if (TASK_SWITCH == currentTask)
      {
        uint8_t currentBinary = (uint8_t)ThisEvent.EventParam;
        PrintBinary(currentBinary);

        if (currentBinary == targetDecimal)
        {
          // event: switches match target -> task complete
          DB_printf("Task 3 completed\r\n");

          // sound the buzzer
          ES_Event_t DoneEvent;
          DoneEvent.EventType  = ES_TASK_COMPLETE;
          DoneEvent.EventParam = TASK_SWITCH;
          PostTestHarnessService0(DoneEvent);

          SetTask(TASK_IDLE);   // back to currentTask = 0
        }
      }
    }
    break;

    case ES_POT_CHANGED:   // pot moved, param = new reading 0..1023
    {
      if (TASK1_WAITING == Task1State)
      {
        uint16_t currentPot = (uint16_t)ThisEvent.EventParam;

        if (PotInRange(currentPot))
        {
          // event: currentPot within +/- 10% of targetPot -> task complete
          DB_printf("currentPot = %d\r\n", currentPot);
          DB_printf("Task 1 completed\r\n");

          // sound the buzzer
          ES_Event_t DoneEvent;
          DoneEvent.EventType  = ES_TASK_COMPLETE;
          DoneEvent.EventParam = TASK_POT;
          PostTestHarnessService0(DoneEvent);

          SetTask(TASK_IDLE);   // back to currentTask = 0
        }
        // otherwise: outside the range, stay in Task 1
      }
    }
    break;

    case ES_BEAM_BROKEN:   // one swipe (beam H->L)
    {
      if (TASK2_WAITING == Task2State)
      {
        currentSwipes++;
        DB_printf("currentSwipes = %d\r\n", currentSwipes);

        if (currentSwipes >= targetSwipes)
        {
          // event: currentSwipes reached targetSwipes -> task complete
          Task2State = TASK2_COMPLETE;
          DB_printf("Task 2 completed\r\n");

          // sound the buzzer
          ES_Event_t DoneEvent;
          DoneEvent.EventType  = ES_TASK_COMPLETE;
          DoneEvent.EventParam = TASK_SWIPE;
          PostTestHarnessService0(DoneEvent);
        }
        else
        {
          // event: beam broke and currentSwipes < targetSwipes
          // TODO: reset the 20 s inactivity timer here once it's added
        }
      }
    }
    break;

    case ES_TIMEOUT:
    {
      if (TASK1_PRINT_TIMER == ThisEvent.EventParam)  // Task 1 print
      {
        if (TASK1_WAITING == Task1State)
        {
          DB_printf("currentPot = %d\r\n", QueryCurrentPot());
          ES_Timer_InitTimer(TASK1_PRINT_TIMER, TASK1_PRINT_TIME);
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

/****************************************************************************
 Function
     QueryTask2Running

 Returns
     bool true while Task 2 is active and not yet complete
     (TestHarnessService0 spins both servos during this time)
****************************************************************************/
bool QueryTask2Running(void)
{
  return (TASK2_WAITING == Task2State);
}

/***************************************************************************
 private functions
 ***************************************************************************/
/****************************************************************************
 Function
     SetTask

 Description
     Updates currentTask and prints it. Turns every LED off, then, for
     tasks 1-3, turns only that task's LED on (solid).
     For idle (task 0) all LEDs stay off.
     Entering task 2 starts the swipe count; leaving it stops it.
****************************************************************************/
static void SetTask(uint8_t NewTask)
{
  StopTask1();                                  // leave Task 1 if active
  StopTask2();                                  // leave Task 2 if active

  currentTask = NewTask;
  DB_printf("currentTask = %d\r\n", currentTask);

  AllLEDsOff();                                 // only one LED at a time
  TaskLEDOn(currentTask);                       // task 0: nothing turns on

  if (TASK_POT == currentTask)
  {
    StartTask1();
  }
  else if (TASK_SWIPE == currentTask)
  {
    StartTask2();
  }
  else if (TASK_SWITCH == currentTask)
  {
    StartTask3();
  }
}

/****************************************************************************
 Function
     StartTask3

 Description
     Entry action for "Waiting for currentBinary == targetDecimal":
     prints the target and the switches' current setting.
****************************************************************************/
static void StartTask3(void)
{
  targetDecimal = TASK3_TARGET_DECIMAL;         // randomize here later
  DB_printf("targetDecimal = %d\r\n", targetDecimal);
  PrintBinary(QueryRockerSwitch());
}

/****************************************************************************
 Function
     PrintBinary

 Description
     Prints Value as a 4-digit binary number, switch 1 first
     (e.g. 10 -> "currentBinary = 1010")
****************************************************************************/
static void PrintBinary(uint8_t Value)
{
  DB_printf("currentBinary = %d%d%d%d\r\n",
      (Value >> 3) & 1, (Value >> 2) & 1, (Value >> 1) & 1, Value & 1);
}

/****************************************************************************
 Function
     StartTask1

 Description
     Entry action for "Waiting for currentPot within +/- 10% of
     targetPot": print the target and current reading once, start the
     periodic prints.
****************************************************************************/
static void StartTask1(void)
{
  targetPot  = TASK1_TARGET_POT;                // randomize here later
  Task1State = TASK1_WAITING;

  DB_printf("targetPot = %d (+/- %d)\r\n", targetPot, TASK1_TOLERANCE);
  DB_printf("currentPot = %d\r\n", QueryCurrentPot());
  ES_Timer_InitTimer(TASK1_PRINT_TIMER, TASK1_PRINT_TIME);
}

/****************************************************************************
 Function
     StopTask1

 Description
     Leaves Task 1: stops the print timer and goes inactive.
****************************************************************************/
static void StopTask1(void)
{
  ES_Timer_StopTimer(TASK1_PRINT_TIMER);
  Task1State = TASK1_INACTIVE;
}

/****************************************************************************
 Function
     PotInRange

 Returns
     true if Pot is within TASK1_TOLERANCE of targetPot
****************************************************************************/
static bool PotInRange(uint16_t Pot)
{
  uint16_t diff = (Pot > targetPot) ? (Pot - targetPot) : (targetPot - Pot);
  return (diff <= TASK1_TOLERANCE);
}

/****************************************************************************
 Function
     StartTask2

 Description
     Entry action for "Waiting for currentSwipes >= targetSwipes":
     reset the count, print the target and the starting count.
****************************************************************************/
static void StartTask2(void)
{
  targetSwipes = TASK2_TARGET_SWIPES;           // randomize here later
  currentSwipes = 0;
  Task2State    = TASK2_WAITING;

  DB_printf("targetSwipes = %d\r\n", targetSwipes);
  DB_printf("currentSwipes = %d\r\n", currentSwipes);
}

/****************************************************************************
 Function
     StopTask2

 Description
     Leaves Task 2: goes inactive.
****************************************************************************/
static void StopTask2(void)
{
  Task2State = TASK2_INACTIVE;
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
     Turns on the LED that belongs to WhichTask:
       1 -> RB8, 2 -> RB9, 3 -> RB10. Any other value turns nothing on.
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