/****************************************************************************
 Module
   GameService.c

 Revision
   1.0.0

 Description
   CHEF project game-level state machine. Decides WHICH task runs and
   WHEN the game ends; TaskService runs the task itself.

   States, events and actions:

   GAME_WELCOME  ("Welcome Page", currentTask = 0)
     entry                 : print welcome message
     ES_NEW_KEY [SPACE]    : userScore = 0, taskIndex = 0,
                             start gameTimer countdown,
                             ES_START_TASK(taskOrder[0]) -> TaskService,
                             start taskTimer
                             -> GAME_PLAYING

   GAME_PLAYING  ("Playing")
     ES_TASK_COMPLETE [current attempt]
                           : stop taskTimer, userScore += POINTS_PER_TASK,
                             beep (+ servo 1 sweep after Task 3),
                             next task (see below)
                             -> stay in GAME_PLAYING
     ES_TIMEOUT(TASK_TIMER) ("taskTimer expired")
                           : no points, next task (see below)
                             -> stay in GAME_PLAYING
       next task           : taskIndex++, print score,
                             ES_START_TASK(taskOrder[taskIndex]),
                             restart taskTimer
     ES_USER_INPUT         : restart inactiveTimer
                             -> stay in GAME_PLAYING
     ES_TIMEOUT(INACTIVE_TIMER) ("inactiveTimer expired")
                           : print "Inactive for # seconds!", stop all
                             timers, ES_START_TASK(0) (currentTask = 0,
                             LEDs off), reset score and tasks completed
                             -> GAME_WELCOME
     ES_GAME_TIMER_EXPIRED : stop taskTimer and inactiveTimer,
                             ES_START_TASK(0) (currentTask = 0, LEDs off),
                             unfinished task scores nothing,
                             print "Game Completed!" and final score,
                             long beep
                             -> GAME_COMPLETE

   GAME_COMPLETE ("Displaying Game Completion Page")
     ES_NEW_KEY [SPACE]    : -> GAME_WELCOME

   Inactivity timer: INACTIVE_TIMER is a single inactiveTimer-second timer,
   started with the game and restarted by every ES_USER_INPUT (pot, beam
   or switch, reported by TaskService). Its timeout is the
   "inactiveTimer expired" event.

   Task timer: TASK_TIMER is a single taskTimer-second timer, restarted at
   the start of every task. Its timeout is the "taskTimer expired" event.

   Game timer: GAME_TIMER ticks once per second (so any game length works
   with the framework's timers). When the countdown reaches 0, this service
   posts ES_GAME_TIMER_EXPIRED to itself: that is the "gameTimer expired"
   event on the state diagram.

 Notes
   ES_NEW_KEY reaches this service because Check4Keystroke() in
   EventCheckers.c posts it with ES_PostAll().
   ES_TASK_COMPLETE is posted here by TaskService.
   Buzzer and servo 1 live in TestHarnessService0.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/26       CH      started: game level split out of TaskService
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
// This module
#include "../ProjectHeaders/GameService.h"

// Event & Services Framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "dbprintf.h"
#include "EventPrint.h"

// The services this one talks to
#include "TaskService.h"           // runs the tasks
#include "TestHarnessService0.h"   // buzzer + servo 1

/*----------------------------- Module Defines ----------------------------*/
#define START_KEY        ' '       // space bar
#define POINTS_PER_TASK  10        // added to userScore per completed task
#define NUM_GAME_TASKS   50        // length of taskOrder (more than enough)
#define ONE_SECOND       1000      // ms, GAME_TIMER tick
#define TASK_IDLE        0         // ES_START_TASK(0) = stop, LEDs off
#define TASK_MASK        0x00FF    // low byte of ES_START_TASK param = task
#define TASK_SWITCH      3         // Task 3 (rocker switches) gets extra time
#define TASK3_TIME_NUM   3         // Task 3 time = 3/2 x taskTimer (1.5x,
#define TASK3_TIME_DEN   2         //   15 s): stays below inactiveTimer

/*---------------------------- Module Functions ---------------------------*/
static void PrintWelcome(void);
static void StartGame(void);
static void StartTask(uint8_t Task);
static void NextTask(void);
static void EndGame(void);
static void ResetToWelcome(void);
static void PostToTestHarness(ES_EventType_t Type, uint16_t Param);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;

static GameState_t gameState;

// Game length in seconds: change this to make the game longer/shorter
static uint16_t gameTimer = 60;
static uint16_t secondsLeft;       // countdown, ticks once per second

// Time allowed per task, in seconds. Task 3 gets TASK3_TIME_NUM /
// TASK3_TIME_DEN times this (15 s). Keep every task time below
// inactiveTimer, and <= 65 s (one 16-bit ms timer).
static uint16_t taskTimer = 10;
static uint8_t  attempt;           // counts task starts, tags ES_START_TASK

// No user input (pot, beam, switches) for this many seconds ends the game
// and returns to the welcome page (max 65 s: one 16-bit ms timer)
static uint16_t inactiveTimer = 20;
static uint16_t currentStartParam; // param of the task now running

// Game order: the tasks are played in this order, one after another,
// until gameTimer runs out. Repeating 1, 2, 3 for now, randomize later.
static uint8_t taskOrder[NUM_GAME_TASKS] = {
  1, 2, 3, 1, 2, 3, 1, 2, 3, 1,
  2, 3, 1, 2, 3, 1, 2, 3, 1, 2,
  3, 1, 2, 3, 1, 2, 3, 1, 2, 3,
  1, 2, 3, 1, 2, 3, 1, 2, 3, 1,
  2, 3, 1, 2, 3, 1, 2, 3, 1, 2
};
static uint8_t  taskIndex;         // position in taskOrder of current task
static uint16_t userScore;         // POINTS_PER_TASK per completed task
static uint16_t tasksCompleted;    // tasks completed in time this game

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitGameService

 Description
     Saves the priority, starts on the welcome page, posts ES_INIT
****************************************************************************/
bool InitGameService(uint8_t Priority)
{
  ES_Event_t ThisEvent;

  MyPriority  = Priority;
  gameState   = GAME_WELCOME;
  taskIndex   = 0;
  userScore   = 0;
  tasksCompleted = 0;
  secondsLeft = 0;
  attempt     = 0;
  currentStartParam = 0;

  ThisEvent.EventType = ES_INIT;
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
     PostGameService

 Description
     Posts an event to this service's queue
****************************************************************************/
bool PostGameService(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunGameService

 Description
   The game-level state machine (see the module header for the diagram)
****************************************************************************/
ES_Event_t RunGameService(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  // announce every event this service receives (see EventPrint.h)
  PrintEvent("GameService", MyPriority, ThisEvent);

  // The game clock tick is the same in every state: count down while
  // playing, and turn 0 into the "gameTimer expired" event.
  if ((ES_TIMEOUT == ThisEvent.EventType) &&
      (GAME_TIMER == ThisEvent.EventParam))
  {
    if (GAME_PLAYING == gameState)
    {
      if (secondsLeft > 0)
      {
        secondsLeft--;
      }
      if (0 == secondsLeft)
      {
        ES_Event_t Expired;
        Expired.EventType  = ES_GAME_TIMER_EXPIRED;
        Expired.EventParam = 0;
        PostGameService(Expired);
      }
      else
      {
        ES_Timer_InitTimer(GAME_TIMER, ONE_SECOND);
      }
    }
    return ReturnEvent;
  }

  switch (gameState)
  {
    /*--------------------------------------------------------------------*/
    case GAME_WELCOME:
    {
      if (ES_INIT == ThisEvent.EventType)
      {
        PrintWelcome();
      }
      else if ((ES_NEW_KEY == ThisEvent.EventType) &&
               (START_KEY == ThisEvent.EventParam))
      {
        StartGame();
        gameState = GAME_PLAYING;
      }
    }
    break;

    /*--------------------------------------------------------------------*/
    case GAME_PLAYING:
    {
      if ((ES_TASK_COMPLETE == ThisEvent.EventType) &&
          (currentStartParam == ThisEvent.EventParam))
      {
        // completed within taskTimer: score it
        ES_Timer_StopTimer(TASK_TIMER);
        userScore += POINTS_PER_TASK;
        tasksCompleted++;

        // beep, plus servo 1 sweep if it was Task 3
        PostToTestHarness(ES_TASK_COMPLETE,
                          ThisEvent.EventParam & TASK_MASK);
        NextTask();
      }
      // an ES_TASK_COMPLETE with another param is a late completion of a
      // task that already timed out: ignore it
      else if ((ES_TIMEOUT == ThisEvent.EventType) &&
               (TASK_TIMER == ThisEvent.EventParam))
      {
        // event: taskTimer expired -> no points, move on
        DB_printf("Task %d Timed Out! No points.\r\n",
                  currentStartParam & TASK_MASK);
        NextTask();
      }
      else if (ES_USER_INPUT == ThisEvent.EventType)
      {
        // someone is playing: restart the inactivity countdown
        ES_Timer_InitTimer(INACTIVE_TIMER,
                           (uint16_t)(inactiveTimer * ONE_SECOND));
      }
      else if ((ES_TIMEOUT == ThisEvent.EventType) &&
               (INACTIVE_TIMER == ThisEvent.EventParam))
      {
        // event: inactiveTimer expired -> abandon the game
        DB_printf("\r\nInactive for %d seconds!\r\n", inactiveTimer);
        ResetToWelcome();
        gameState = GAME_WELCOME;
      }
      else if (ES_GAME_TIMER_EXPIRED == ThisEvent.EventType)
      {
        ES_Timer_StopTimer(TASK_TIMER);
        ES_Timer_StopTimer(INACTIVE_TIMER);
        EndGame();
        gameState = GAME_COMPLETE;
      }
    }
    break;

    /*--------------------------------------------------------------------*/
    case GAME_COMPLETE:
    {
      if ((ES_NEW_KEY == ThisEvent.EventType) &&
          (START_KEY == ThisEvent.EventParam))
      {
        gameState = GAME_WELCOME;
        PrintWelcome();               // entry action of GAME_WELCOME
      }
    }
    break;

    default:
    break;
  }

  return ReturnEvent;
}

/****************************************************************************
 Function
     QueryGameState / QueryUserScore
****************************************************************************/
GameState_t QueryGameState(void)
{
  return gameState;
}

uint16_t QueryUserScore(void)
{
  return userScore;
}

/***************************************************************************
 private functions
 ***************************************************************************/
/****************************************************************************
 Function
     PrintWelcome

 Description
     Entry action of GAME_WELCOME
****************************************************************************/
static void PrintWelcome(void)
{
  DB_printf("\r\nWelcome to Hell's Kitchen with Gordan Ramsay!\r\n");
  DB_printf("Press SPACE to start\r\n\r\n");
}

/****************************************************************************
 Function
     StartGame

 Description
     SPACE on the welcome page: reset the score, start the game clock and
     the first task in taskOrder.
****************************************************************************/
static void StartGame(void)
{
  taskIndex   = 0;
  userScore   = 0;                 // every game starts from 0
  tasksCompleted = 0;
  secondsLeft = gameTimer;
  ES_Timer_InitTimer(GAME_TIMER, ONE_SECOND);
  ES_Timer_InitTimer(INACTIVE_TIMER, (uint16_t)(inactiveTimer * ONE_SECOND));

  DB_printf("\r\nGame started! You have %d seconds.\r\n", gameTimer);
  StartTask(taskOrder[taskIndex]);
}

/****************************************************************************
 Function
     NextTask

 Description
     Advance to the next entry in taskOrder and start it
     (shared by "task completed" and "taskTimer expired")
****************************************************************************/
static void NextTask(void)
{
  taskIndex++;
  if (taskIndex >= NUM_GAME_TASKS)
  {
    taskIndex = 0;                 // never expected, but don't overrun
  }
  DB_printf("(%d seconds left in the game)\r\n\r\n", secondsLeft);
  StartTask(taskOrder[taskIndex]);
}

/****************************************************************************
 Function
     StartTask

 Description
     Prints the score, tells TaskService which task to run (tagged with
     a new attempt number) and starts that task's taskTimer
****************************************************************************/
static void StartTask(uint8_t Task)
{
  // Task 3 gets 1.5x as long (integer math: no floating point on this PIC);
  // every other task gets taskTimer
  uint16_t thisTaskTime = (TASK_SWITCH == Task) ?
                          ((taskTimer * TASK3_TIME_NUM) / TASK3_TIME_DEN) :
                          taskTimer;

  DB_printf("Score: %d\r\n", userScore);
  DB_printf("You have %d seconds for this task\r\n", thisTaskTime);

  attempt++;
  currentStartParam = ((uint16_t)attempt << 8) | Task;

  ES_Event_t StartEvent;
  StartEvent.EventType  = ES_START_TASK;
  StartEvent.EventParam = currentStartParam;
  PostTaskService(StartEvent);

  ES_Timer_InitTimer(TASK_TIMER, (uint16_t)(thisTaskTime * ONE_SECOND));
}

/****************************************************************************
 Function
     EndGame

 Description
     Actions for "gameTimer expired": stop the current task (currentTask
     = 0, LEDs off), the unfinished task scores nothing, print the final
     score and play the long buzzer sound.
****************************************************************************/
static void EndGame(void)
{
  ES_Timer_StopTimer(GAME_TIMER);

  ES_Event_t StopEvent;
  StopEvent.EventType  = ES_START_TASK;
  StopEvent.EventParam = TASK_IDLE;
  PostTaskService(StopEvent);

  DB_printf("\r\nTime's up!\r\n");
  DB_printf("Game Completed!\r\n");
  DB_printf("Score: %d\r\n", userScore);
  DB_printf("Total tasks completed: %d\r\n", tasksCompleted);
  DB_printf("Press SPACE to return to the welcome page\r\n");

  PostToTestHarness(ES_GAME_OVER, 0);   // long beep
}

/****************************************************************************
 Function
     ResetToWelcome

 Description
     Actions for "inactiveTimer expired": stop every game timer, stop the
     current task (currentTask = 0, LEDs off), reset the score and the
     tasks-completed count, show the welcome page again.
****************************************************************************/
static void ResetToWelcome(void)
{
  ES_Timer_StopTimer(GAME_TIMER);
  ES_Timer_StopTimer(TASK_TIMER);
  ES_Timer_StopTimer(INACTIVE_TIMER);

  ES_Event_t StopEvent;
  StopEvent.EventType  = ES_START_TASK;
  StopEvent.EventParam = TASK_IDLE;
  PostTaskService(StopEvent);

  userScore      = 0;
  tasksCompleted = 0;
  taskIndex      = 0;

  PrintWelcome();                  // entry action of GAME_WELCOME
}

/****************************************************************************
 Function
     PostToTestHarness

 Description
     Small helper: post an event to TestHarnessService0 (buzzer, servo 1)
****************************************************************************/
static void PostToTestHarness(ES_EventType_t Type, uint16_t Param)
{
  ES_Event_t ThisEvent;
  ThisEvent.EventType  = Type;
  ThisEvent.EventParam = Param;
  PostTestHarnessService0(ThisEvent);
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/