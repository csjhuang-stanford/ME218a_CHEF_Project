/****************************************************************************
 Module
   TaskService.c

 Revision
   2.0.0

 Description
   CHEF project task service: runs ONE task at a time, as told by
   GameService. It knows nothing about the game order, timer or score.

   From GameService:  ES_START_TASK, EventParam = (attempt << 8) | task
                        task: 1 = pot, 2 = beam break, 3 = rocker switches,
                              0 = stop (idle, all LEDs off)
   To GameService:    ES_TASK_COMPLETE, EventParam = the same value that
                        started the task, so GameService can tell a
                        completion of the current attempt from a late one
                        (e.g. one that arrives just after taskTimer ran out)

   Each task has its own LED, driven through the 74ACT244:
     task 1 -> LED 1 on RB8  (pin 17)
     task 2 -> LED 2 on RB9  (pin 18)
     task 3 -> LED 3 on RB10 (pin 21)
   Only the current task's LED is on (solid); the others are off.

   Task 1 (potentiometer):
     State: "Waiting for currentPot within +/- 10% of targetPot"
       entry            : targetPot = next entry of targetPotOrder (0..10,
                          mapped to 0..1023), print targetPot and
                          currentPot, start print timer
       print timer      : print "currentPot = #" every TASK1_PRINT_TIME ms
       ES_POT_CHANGED and currentPot outside the range : stay
       ES_POT_CHANGED and currentPot inside the range  : task complete

   Task 2 (beam-break swipes):
     State: "Waiting for currentSwipes == targetSwipes"
       entry            : targetSwipes = next entry of targetSwipesOrder,
                          currentSwipes = 0, print targetSwipes/currentSwipes
       ES_BEAM_BROKEN   : currentSwipes++, print currentSwipes
       currentSwipes == targetSwipes                   : task complete

   Task 3 (rocker switches):
     State: "Waiting for currentBinary == targetDecimal"
       entry            : targetDecimal = next entry of targetDecimalOrder,
                          print targetDecimal and currentBinary
       ES_SWITCH_CHANGED: print currentBinary
       currentBinary == targetDecimal                  : task complete

   Task complete: print "Task # Completed!", stop the task, post
   ES_TASK_COMPLETE to GameService, then wait for the next ES_START_TASK.

 Notes
   ES_BEAM_BROKEN is posted here by Check4BeamBreak() in BeamBreak.c.
   ES_POT_CHANGED is posted here by Check4PotChange() in Potentiometer.c.
   ES_SWITCH_CHANGED is posted here by Check4RockerSwitch() in
   RockerSwitch.c.
   Timers: TASK1_PRINT_TIMER (see ES_Configure.h).

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/26       CH      game level (order, timer, score) moved to GameService
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
#include "EventPrint.h"

// Sensors, one per task
#include "Potentiometer.h"   // Task 1
#include "BeamBreak.h"       // Task 2
#include "RockerSwitch.h"    // Task 3

// Completion reports go to the game level
#include "GameService.h"

/*----------------------------- Module Defines ----------------------------*/
#define TASK_IDLE        0
#define TASK_POT         1         // Task 1 = turn pot to target
#define TASK_SWIPE       2         // Task 2 = beam-break swipes
#define TASK_SWITCH      3         // Task 3 = set switches to target
#define NUM_TASKS        3

#define LED_ON           1
#define LED_OFF          0

// Task 1
#define NUM_TASK1_TARGETS    30    // length of targetPotOrder
#define POT_LEVELS           10    // targetPot levels 0..10
#define POT_FULL_SCALE       1023  // 10-bit ADC
#define TASK1_TOLERANCE      102   // 10% of full scale (1023)
#define TASK1_PRINT_TIME     500   // ms between "currentPot = #" prints

// Task 2
#define NUM_TASK2_TARGETS    30    // length of targetSwipesOrder

// Task 3
#define NUM_TASK3_TARGETS    30    // length of targetDecimalOrder

typedef enum
{
  TASK1_INACTIVE = 0,              // Task 1 not running
  TASK1_WAITING                    // waiting for pot within tolerance
} Task1State_t;

typedef enum
{
  TASK2_INACTIVE = 0,              // Task 2 not running
  TASK2_WAITING                    // waiting for currentSwipes == target
} Task2State_t;

typedef enum
{
  TASK3_INACTIVE = 0,              // Task 3 not running
  TASK3_WAITING                    // waiting for currentBinary == target
} Task3State_t;

/*---------------------------- Module Functions ---------------------------*/
static void SetTask(uint8_t NewTask);
static void StopAllTasks(void);
static void CompleteCurrentTask(void);
static void AllLEDsOff(void);
static void TaskLEDOn(uint8_t WhichTask);
static void StartTask1(void);
static bool PotInRange(uint16_t Pot);
static void StartTask2(void);
static void StartTask3(void);
static void PrintBinary(uint8_t Value);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;

// which task is running right now (0 = idle)
static uint8_t currentTask;
static uint16_t currentStartParam;   // ES_START_TASK param, echoed back

// Task 1
static Task1State_t Task1State;
static uint8_t  targetPot;         // level 0..10
static uint16_t targetPotRaw;      // same target on the ADC scale 0..1023

// Pre-generated "random" Task 1 targets (levels), one per Task 1.
//   - every value is 0..10, mapped to the ADC as level * 1023 / 10:
//       0->0  1->102  2->204  3->306  4->409  5->511
//       6->613  7->716  8->818  9->920  10->1023
//   - each of 0..10 appears at least once
//   - back-to-back values (including last -> first) differ by at least
//     2 levels: one level is 102 counts, the same as the +/-10% tolerance,
//     so a 1-level step could be "completed" without moving the pot
// 30 entries is plenty for a 60 s game; regenerate to change the pattern.
static const uint8_t targetPotOrder[NUM_TASK1_TARGETS] = {
  2, 0, 7, 1, 10, 4, 7, 4, 6, 2,
  5, 8, 0, 7, 2, 9, 2, 8, 3, 9,
  3, 0, 6, 0, 2, 0, 6, 8, 0, 6
};
static uint8_t targetPotIndex;     // next entry of targetPotOrder

// Task 2
static Task2State_t Task2State;
static uint8_t targetSwipes;

// Pre-generated "random" Task 2 targets, used in order, one per Task 2.
//   - every value is 3..10 swipes
//   - each of 3..10 appears at least once
//   - no value repeats back to back (including last -> first, since the
//     list wraps)
// 30 entries is plenty for a 60 s game; regenerate to change the pattern.
static const uint8_t targetSwipesOrder[NUM_TASK2_TARGETS] = {
   7,  6,  8,  5,  6,  9,  4, 10,  5,  3,
   4, 10,  9,  3,  6,  3,  6,  3, 10,  9,
   4,  6, 10,  6,  8,  6, 10,  4,  5, 10
};
static uint8_t targetSwipesIndex;    // next entry of targetSwipesOrder
static uint8_t currentSwipes;

// Task 3
static Task3State_t Task3State;
static uint8_t targetDecimal;

// Pre-generated "random" Task 3 targets, used in order, one per Task 3.
//   - every value is 1..8 (4-bit binary 0001..1000)
//   - each of 1..8 appears at least once
//   - no value repeats back to back (including last -> first, since the
//     list wraps), so the switches must change every time Task 3 comes up
// 30 entries is plenty for a 60 s game; regenerate to change the pattern.
static const uint8_t targetDecimalOrder[NUM_TASK3_TARGETS] = {
  6, 8, 2, 5, 8, 7, 8, 4, 6, 7,
  2, 5, 4, 7, 6, 3, 7, 5, 2, 8,
  1, 5, 6, 2, 3, 6, 1, 3, 1, 3
};
static uint8_t targetDecimalIndex;   // next entry of targetDecimalOrder

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitTaskService

 Description
     Saves the priority, sets up the LED pins and all three sensors,
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

  InitPotentiometer();       // RB13 / AN11 analog input for Task 1
  InitBeamBreak();           // RB5 input for the break-beam receiver
  InitRockerSwitch();        // RB3, RB4, RB2, RB12 inputs for Task 3

  currentTask   = TASK_IDLE;
  Task1State    = TASK1_INACTIVE;
  Task2State    = TASK2_INACTIVE;
  Task3State    = TASK3_INACTIVE;
  targetPotIndex = 0;
  targetPot      = targetPotOrder[0];
  targetPotRaw   = (uint16_t)(((uint32_t)targetPot * POT_FULL_SCALE) / POT_LEVELS);
  targetSwipes  = targetSwipesOrder[0];
  targetSwipesIndex = 0;
  currentSwipes = 0;
  targetDecimal = targetDecimalOrder[0];
  targetDecimalIndex = 0;

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

 Description
   ES_START_TASK     : start the task in EventParam (0 = stop everything).
   ES_POT_CHANGED    : Task 1 check.
   ES_BEAM_BROKEN    : Task 2 swipe.
   ES_SWITCH_CHANGED : Task 3 check.
   ES_TIMEOUT from TASK1_PRINT_TIMER: print currentPot for Task 1.
****************************************************************************/
ES_Event_t RunTaskService(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  // announce every event this service receives (see EventPrint.h)
  PrintEvent("TaskService", MyPriority, ThisEvent);

  switch (ThisEvent.EventType)
  {
    case ES_INIT:
    {}
    break;

    case ES_START_TASK:   // from GameService
    {
      currentStartParam = ThisEvent.EventParam;
      SetTask((uint8_t)(ThisEvent.EventParam & 0xFF));   // low byte = task
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
          CompleteCurrentTask();
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
          CompleteCurrentTask();
        }
        // TODO: reset the 20 s inactivity timer here once it's added
      }
    }
    break;

    case ES_SWITCH_CHANGED:   // a rocker switch moved, param = 0..15
    {
      if (TASK3_WAITING == Task3State)
      {
        uint8_t currentBinary = (uint8_t)ThisEvent.EventParam;
        PrintBinary(currentBinary);

        if (currentBinary == targetDecimal)
        {
          // event: switches match target -> task complete
          CompleteCurrentTask();
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
     bool true while Task 2 is waiting for swipes
     (TestHarnessService0 spins servo 2 during this time)
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
     Stops whatever task was running and updates currentTask.
     Turns every LED off, then turns only the new task's LED on and runs
     that task's entry action. Task 0 = idle: everything stays off.
****************************************************************************/
static void SetTask(uint8_t NewTask)
{
  StopAllTasks();

  currentTask = NewTask;   // "Task # Activated!" below shows the new value

  AllLEDsOff();                                 // only one LED at a time
  TaskLEDOn(currentTask);                       // task 0: nothing turns on

  switch (currentTask)
  {
    case TASK_POT:
      DB_printf("Task 1 Activated!\r\n");
      StartTask1();
      break;
    case TASK_SWIPE:
      DB_printf("Task 2 Activated!\r\n");
      StartTask2();
      break;
    case TASK_SWITCH:
      DB_printf("Task 3 Activated!\r\n");
      StartTask3();
      break;
    default:                                    // idle
      break;
  }
}

/****************************************************************************
 Function
     StopAllTasks

 Description
     Every task goes inactive and Task 1's print timer stops, so no
     sensor event can complete a task until the next ES_START_TASK.
****************************************************************************/
static void StopAllTasks(void)
{
  ES_Timer_StopTimer(TASK1_PRINT_TIMER);
  Task1State = TASK1_INACTIVE;
  Task2State = TASK2_INACTIVE;
  Task3State = TASK3_INACTIVE;
}

/****************************************************************************
 Function
     CompleteCurrentTask

 Description
     The current task's goal is met: announce it, stop the task (so a
     late sensor event can't complete it twice) and report to GameService,
     which decides what happens next.
****************************************************************************/
static void CompleteCurrentTask(void)
{
  DB_printf("Task %d Completed!\r\n", currentTask);
  StopAllTasks();

  ES_Event_t DoneEvent;
  DoneEvent.EventType  = ES_TASK_COMPLETE;
  DoneEvent.EventParam = currentStartParam;   // echo: which attempt finished
  PostGameService(DoneEvent);
}

/****************************************************************************
 Function
     StartTask1

 Description
     Entry action for "Waiting for currentPot within +/- 10% of targetPot"
****************************************************************************/
static void StartTask1(void)
{
  // next pre-generated level; the index is never reset, so consecutive
  // Task 1s (even across games) always need the pot to move
  targetPot = targetPotOrder[targetPotIndex];
  targetPotIndex++;
  if (targetPotIndex >= NUM_TASK1_TARGETS)
  {
    targetPotIndex = 0;                         // wrap: last != first
  }
  // level 0..10 -> ADC 0..1023 (32-bit math so 10 * 1023 can't overflow)
  targetPotRaw = (uint16_t)(((uint32_t)targetPot * POT_FULL_SCALE) / POT_LEVELS);
  Task1State   = TASK1_WAITING;

  DB_printf("targetPot = %d of %d (%d +/- %d)\r\n",
      targetPot, POT_LEVELS, targetPotRaw, TASK1_TOLERANCE);
  DB_printf("currentPot = %d\r\n", QueryCurrentPot());
  ES_Timer_InitTimer(TASK1_PRINT_TIMER, TASK1_PRINT_TIME);
}

/****************************************************************************
 Function
     PotInRange

 Returns
     true if Pot (0..1023) is within TASK1_TOLERANCE of targetPotRaw
****************************************************************************/
static bool PotInRange(uint16_t Pot)
{
  uint16_t diff = (Pot > targetPotRaw) ? (Pot - targetPotRaw) : (targetPotRaw - Pot);
  return (diff <= TASK1_TOLERANCE);
}

/****************************************************************************
 Function
     StartTask2

 Description
     Entry action for "Waiting for currentSwipes == targetSwipes"
****************************************************************************/
static void StartTask2(void)
{
  // next pre-generated target; the index is never reset, so consecutive
  // Task 2s (even across games) always get different targets
  targetSwipes = targetSwipesOrder[targetSwipesIndex];
  targetSwipesIndex++;
  if (targetSwipesIndex >= NUM_TASK2_TARGETS)
  {
    targetSwipesIndex = 0;                      // wrap: last != first
  }
  currentSwipes = 0;
  Task2State    = TASK2_WAITING;

  DB_printf("targetSwipes = %d\r\n", targetSwipes);
  DB_printf("currentSwipes = %d\r\n", currentSwipes);
}

/****************************************************************************
 Function
     StartTask3

 Description
     Entry action for "Waiting for currentBinary == targetDecimal"
****************************************************************************/
static void StartTask3(void)
{
  // next pre-generated target; the index is never reset, so consecutive
  // Task 3s (even across games) always get different targets
  targetDecimal = targetDecimalOrder[targetDecimalIndex];
  targetDecimalIndex++;
  if (targetDecimalIndex >= NUM_TASK3_TARGETS)
  {
    targetDecimalIndex = 0;                     // wrap: last != first
  }
  Task3State    = TASK3_WAITING;

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