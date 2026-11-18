/****************************************************************************
 Module
   TestHarnessService0.c

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
#include "../ProjectHeaders/TestHarnessService0.h"

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
#include "DisplayLink.h"        // tell the display PIC32 about the burger
#include "GameService.h"        // PostGameService(): burger dispensed
#include "EventPrint.h"
#include "PWM_PIC32.h"     // Jay: buzzer + servo PWM

/*----------------------------- Module Defines ----------------------------*/
// these times assume a 10.000mS/tick timing
#define ONE_SEC 1000
#define HALF_SEC (ONE_SEC / 2)
#define TWO_SEC (ONE_SEC * 2)
#define FIVE_SEC (ONE_SEC * 5)
#define BEEP_FREQ     2000     // Hz, "task complete" beep
#define BEEP_DURATION 1000     // ms (1 second)
#define TIMEOUT_BEEP_FREQ 400  // Hz, low "time's up, next task" beep
                               // (over 2 octaves below BEEP_FREQ)
                               // (set to BEEP_FREQ for the same sound)
// 1 = show the framework test-harness prints (banner, key echo, timer
// announcements), 0 = hide them so only the game prints show
#define SHOW_DEBUG_PRINTS 0

#define BUZZER_ON     true     // true = real beep, false = print "BEEP!" only

// Servo pulse widths in PWM ticks (2.5 MHz: 2500 = 1 ms)

// PWM channels (see main.c)
#define SERVO1_CHANNEL     4      // OC4 on RA4, 0-270 deg positional
#define SERVO2_CHANNEL     3      // OC3 on RA3, 0-270 deg positional

// Game-completion dispense sequence (both servos are 0-270 deg):
//   1. first servo  0 -> 90 deg, then stays there
//   2. pause DISPENSE_PAUSE_TIME (3 s)
//   3. second servo 0 -> 270 deg, then stays there
//   4. post ES_BURGER_DISPENSED to GameService (unlocks restart)
//   5. on ES_CLOSE_DISPENSER (back to the welcome page, or a new game
//      starts) both servos return to 0 deg
#define FIRST_SERVO_CHANNEL   SERVO1_CHANNEL   // RA4, moves first
#define SECOND_SERVO_CHANNEL  SERVO2_CHANNEL   // RA3, moves second

// 270-degree servos usually need 0.5-2.5 ms for full travel (check the
// datasheet; with 1-2 ms they only turn about half way)
#define SERVO_MIN_PULSE    1250   // 0.5 ms = 0 deg (second servo's rest)
#define SERVO_MAX_PULSE    6250   // 2.5 ms = 270 deg (second servo's target)
// First servo turns CLOCKWISE 117 deg (90 deg + 30%). On this servo a longer pulse turns
// it counter-clockwise, so to go clockwise it rests at the 2.5 ms end and
// moves to a SHORTER pulse: 117 deg = 2.0 ms * 117 / 270 = 0.867 ms less.
// (To go counter-clockwise again: rest 1250, target 2917.)
#define FIRST_REST_PULSE   6250   // 2.5 ms: first servo's "0" (rest)
#define FIRST_TARGET_PULSE 4083   // 6250 - 2167 = 117 deg clockwise from rest
                                  // (90 deg + 30%; 90 deg would be 4583)
#define FIRST_SWEEP_STEP   3      // first servo: ticks per 1 ms, ~0.6 s to 90 deg
#define SECOND_SWEEP_STEP  2      // second servo: ticks per 1 ms, ~2.5 s
                                  // (smaller = slower)

#define DISPENSE_PAUSE_TIME 3000  // ms between first and second servo

// Task 3 completion: the second servo sweeps 0 -> 180 deg and straight back
// 180 -> 0 deg (no pause). 180 deg = 0.5 ms + 2.0 ms * 180 / 270 = 1.833 ms
#define TASK3_SWEEP_PULSE  4583   // 180 deg on the 0.5-2.5 ms 270-deg servo
#define TASK3_SWEEP_STEP   SECOND_SWEEP_STEP   // same pace: ~1.7 s each way

// Test: 1 = both servos do their motion once right after power-up, so you
// can check wiring and power without playing a whole game. Set back to 0.
#define SERVO_POWERUP_TEST 0

#define ENTER_POST     ((MyPriority<<3)|0)
#define ENTER_RUN      ((MyPriority<<3)|1)
#define ENTER_TIMEOUT  ((MyPriority<<3)|2)

// Do NOT enable TEST_INT_POST: it uses TMR2, which now drives the buzzer PWM
//#define TEST_INT_POST
//#define BLINK LED
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
#ifdef BLINK_LED
static void InitLED(void);
static void BlinkLED(void);
#endif

#ifdef TEST_INT_POST
static void InitTMR2(void);
static void StartTMR2(void);
#endif
/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
// add a deferral queue for up to 3 pending deferrals +1 to allow for overhead
static ES_Event_t DeferralQueue[3 + 1];


// Game-completion dispense sequence (see the defines above)
typedef enum
{
  DISPENSE_IDLE = 0,               // both servos parked at 0 deg
  DISPENSE_FIRST_UP,               // first servo 0 -> 90 deg
  DISPENSE_PAUSE,                  // first at 90, wait DISPENSE_PAUSE_TIME
  DISPENSE_SECOND_UP,              // second servo 0 -> 270
  DISPENSE_OPEN,                   // first holds 90, second holds 270
  DISPENSE_CLOSING,                // both back to 0 deg
  DISPENSE_TASK3_UP,               // Task 3 done: second servo 0 -> 180
  DISPENSE_TASK3_DOWN              //   then straight back 180 -> 0
} DispenseState_t;

static DispenseState_t DispenseState = DISPENSE_IDLE;
static uint16_t        FirstPulse    = FIRST_REST_PULSE;
static uint16_t        SecondPulse   = SERVO_MIN_PULSE;
static uint16_t        DispenseCount;   // ms counter for the pause

// start the sequence (restarts it if already running)
static void StartDispense(void)
{
  DB_printf("Dispensing Burger!\r\n");
  // display: GameService's completion page already shows
  // "SERVING BURGER..." on line 2 while the servos move
  // (SecondPulse is not reset here: if a Task 3 sweep is still running it
  // glides back to 0 during the first servo's move and the pause)
  FirstPulse    = FIRST_REST_PULSE;
  DispenseState = DISPENSE_FIRST_UP;
}

// Task 3 completed: second servo 0 -> 180 -> 0. Only from rest, so it never
// interrupts the game-completion sequence.
static void StartTask3Sweep(void)
{
  if (DISPENSE_IDLE == DispenseState)
  {
    DispenseState = DISPENSE_TASK3_UP;
  }
}

// back to the welcome page (or a new game starts): both servos to 0 deg.
// Nothing to do if they are already at rest.
static void CloseDispenser(void)
{
  if ((FirstPulse != FIRST_REST_PULSE) || (SecondPulse != SERVO_MIN_PULSE))
  {
    DispenseState = DISPENSE_CLOSING;
  }
}

/*----------------------------- Buzzer beep -------------------------------*/
// Buzzer = PWM channel 1 on Timer2, output on RPB15 (see main.c)
// One BEEP_DURATION beep when any task completes; no music.

/* Jay's melody, not used: the buzzer only beeps on task completion now.
typedef struct
{
  uint16_t Frequency;
  uint16_t Duration;
} MelodyNote_t;

static const MelodyNote_t Melody[] = {
  {493, 500}, {440, 500}, {392, 500}, {440, 500}, {493, 500},
  {493, 500}, {493, 1000}, {440, 500}, {440, 500}, {440, 1000},
  {493, 500}, {587, 500}, {587, 1000}, {493, 500}, {440, 500},
  {392, 500}, {440, 500}, {493, 500}, {493, 500}, {493, 1000},
  {440, 500}, {440, 500}, {493, 500}, {440, 500}, {392, 2000}
};
*/

// Buzzer on at BEEP_FREQ; BUZZER_TIMER turns it off after BEEP_DURATION.
// A second completion during a beep just restarts the 1 s.
// With BUZZER_ON false (quiet debugging) it only prints "BEEP!".
static void StartTimeoutBeep(void);

static void StartBeep(void)
{
  if (BUZZER_ON)
  {
    PWMSetup_SetFreqOnTimer(BEEP_FREQ, _Timer2_);
    PWMOperate_SetDutyOnChannel(50, 1);
    ES_Timer_InitTimer(BUZZER_TIMER, BEEP_DURATION);
  }
  else
  {
    DB_printf("BEEP!\r\n");
  }
}

// Task ran out of time: same length as the task-complete beep, lower
// pitch, so the player hears that the task changed (and that it was missed).
// With BUZZER_ON false it only prints "BEEP! (time out)".
static void StartTimeoutBeep(void)
{
  if (BUZZER_ON)
  {
    PWMSetup_SetFreqOnTimer(TIMEOUT_BEEP_FREQ, _Timer2_);
    PWMOperate_SetDutyOnChannel(50, 1);
    ES_Timer_InitTimer(BUZZER_TIMER, BEEP_DURATION);
  }
  else
  {
    DB_printf("BEEP! (time out)\r\n");
  }
}

// Game over: twice as long as a task beep.
// With BUZZER_ON false it only prints "BEEP--------".
static void StartLongBeep(void)
{
  if (BUZZER_ON)
  {
    PWMSetup_SetFreqOnTimer(BEEP_FREQ, _Timer2_);
    PWMOperate_SetDutyOnChannel(50, 1);
    ES_Timer_InitTimer(BUZZER_TIMER, 2 * BEEP_DURATION);
  }
  else
  {
    DB_printf("BEEP--------\r\n");
  }
}

static void StopBeep(void)
{
  PWMOperate_SetDutyOnChannel(0, 1);
}

/*------------------------- Jay: pot -> servo -----------------------------*/
// Servo 1 = PWM channel 4 on RA4, servo 2 = channel 3 on RA3 (Timer3, 50 Hz)
// one sweep step toward Target; returns true once it is there
static bool StepToward(uint16_t *pPulse, uint16_t Target, uint16_t Step)
{
  if (*pPulse < Target)
  {
    *pPulse = ((Target - *pPulse) <= Step) ? Target : (uint16_t)(*pPulse + Step);
  }
  else if (*pPulse > Target)
  {
    *pPulse = ((*pPulse - Target) <= Step) ? Target : (uint16_t)(*pPulse - Step);
  }
  return (*pPulse == Target);
}

static void moveServo(void)
{
  // called every 1 ms: one step of the dispense sequence
  switch (DispenseState)
  {
    case DISPENSE_FIRST_UP:
    {
      // second servo back to 0 if a Task 3 sweep was still running
      StepToward(&SecondPulse, SERVO_MIN_PULSE, SECOND_SWEEP_STEP);
      if (StepToward(&FirstPulse, FIRST_TARGET_PULSE, FIRST_SWEEP_STEP))
      {
        DispenseCount = 0;                 // first servo stays at 270
        DispenseState = DISPENSE_PAUSE;
      }
    }
    break;

    case DISPENSE_PAUSE:
    {
      StepToward(&SecondPulse, SERVO_MIN_PULSE, SECOND_SWEEP_STEP);
      if (++DispenseCount >= DISPENSE_PAUSE_TIME)
      {
        DispenseState = DISPENSE_SECOND_UP;
      }
    }
    break;

    case DISPENSE_SECOND_UP:
    {
      if (StepToward(&SecondPulse, SERVO_MAX_PULSE, SECOND_SWEEP_STEP))
      {
        DispenseState = DISPENSE_OPEN;     // both stay at 270
        DB_printf("Burger Dispensed!\r\n");

        // sequence done: tell GameService, which unlocks the restart
        // button and shows "BON APETITE!"
        {
          ES_Event_t DoneEvent;
          DoneEvent.EventType  = ES_BURGER_DISPENSED;
          DoneEvent.EventParam = 0;
          PostGameService(DoneEvent);
        }
      }
    }
    break;

    case DISPENSE_OPEN:        // hold 90 / 270 until ES_CLOSE_DISPENSER
    {
      FirstPulse  = FIRST_TARGET_PULSE;
      SecondPulse = SERVO_MAX_PULSE;
    }
    break;

    case DISPENSE_CLOSING:     // both back to 0, each at its own pace
    {
      bool firstDone  = StepToward(&FirstPulse,  FIRST_REST_PULSE, FIRST_SWEEP_STEP);
      bool secondDone = StepToward(&SecondPulse, SERVO_MIN_PULSE, SECOND_SWEEP_STEP);
      if (firstDone && secondDone)
      {
        DispenseState = DISPENSE_IDLE;
      }
    }
    break;

    case DISPENSE_TASK3_UP:    // Task 3 sweep: 0 -> 180 deg
    {
      if (StepToward(&SecondPulse, TASK3_SWEEP_PULSE, TASK3_SWEEP_STEP))
      {
        DispenseState = DISPENSE_TASK3_DOWN;   // no pause at the top
      }
    }
    break;

    case DISPENSE_TASK3_DOWN:  // Task 3 sweep: 180 -> 0 deg
    {
      if (StepToward(&SecondPulse, SERVO_MIN_PULSE, TASK3_SWEEP_STEP))
      {
        DispenseState = DISPENSE_IDLE;
      }
    }
    break;

    default:                   // DISPENSE_IDLE: both parked at 0 deg
    {
      FirstPulse  = FIRST_REST_PULSE;
      SecondPulse = SERVO_MIN_PULSE;
    }
    break;
  }
  PWMOperate_SetPulseWidthOnChannel(FirstPulse,  FIRST_SERVO_CHANNEL);
  PWMOperate_SetPulseWidthOnChannel(SecondPulse, SECOND_SERVO_CHANNEL);
}

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitTestHarnessService0

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, and does any
     other required initialization for this service
 Notes

 Author
     J. Edward Carryer, 01/16/12, 10:00
****************************************************************************/
bool InitTestHarnessService0(uint8_t Priority)
{
  ES_Event_t ThisEvent;

  MyPriority = Priority;
  // CHEF_Project_LED_1_Test
  // TRISBbits.TRISB8 = 0;     // RB8 as output (LED test)
  // LATBbits.LATB8 = 0;       // start with the light off

  // When doing testing, it is useful to announce just which program
  // is running.
  clrScrn();
#if SHOW_DEBUG_PRINTS
  puts("\rStarting Test Harness for \r");
  DB_printf( "the 2nd Generation Events & Services Framework V2.4\r\n");
  DB_printf( "compiled at %s on %s\n", __TIME__, __DATE__);
  DB_printf( "\n\r\n");
  DB_printf( "Press any key to post key-stroke events to Service 0\n\r");
  DB_printf( "Press 'd' to test event deferral \n\r");
  DB_printf( "Press 'r' to test event recall \n\r");
  DB_printf( "Press 'p' to test posting from an interrupt \n\r");
#endif

  /********************************************
   in here you write your initialization code
   *******************************************/
  // initialize deferral queue for testing Deferal function
  ES_InitDeferralQueueWith(DeferralQueue, ARRAY_SIZE(DeferralQueue));
  // initialize LED drive for testing/debug output
#ifdef BLINK_LED
  InitLED();
#endif
#ifdef TEST_INT_POST
  InitTMR2();
#endif
  // initialize the Short timer system for channel A
  //ES_ShortTimerInit(MyPriority, SHORT_TIMER_UNUSED);

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
     PostTestHarnessService0

 Parameters
     ES_Event ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:25
****************************************************************************/
bool PostTestHarnessService0(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunTestHarnessService0

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes

 Author
   J. Edward Carryer, 01/15/12, 15:23
****************************************************************************/
ES_Event_t RunTestHarnessService0(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  // announce every event this service receives (see EventPrint.h),
  // except its own timer timeouts: the 1 ms servo-update tick (1000 prints
  // a second would swamp the serial port) and the buzzer-off timer
  if (ES_TIMEOUT != ThisEvent.EventType)
  {
    PrintEvent("TestHarnessService0", MyPriority, ThisEvent);
  }
  static char DeferredChar = '1';

#ifdef _INCLUDE_BYTE_DEBUG_
  _HW_ByteDebug_SetValueWithStrobe( ENTER_RUN );
#endif  
  switch (ThisEvent.EventType)
  {
    case ES_INIT:
    {
      ES_Timer_InitTimer(SERVO_MOVE_TIMER, ONE_SEC);   // Jay: start servo updates
#if SERVO_POWERUP_TEST
      DB_printf("SERVO TEST: dispense sequence\r\n");
      StartDispense();
#endif
      PWMOperate_SetDutyOnChannel(0, 1);   // buzzer silent until a task completes
#if SHOW_DEBUG_PRINTS
      puts("Service 00:");
      DB_printf("\rES_INIT received in Service %d\r\n", MyPriority);
#endif
    }
    break;
    // case ES_TIMEOUT:   // re-start timer & announce
    // {
    //   ES_Timer_InitTimer(SERVICE0_TIMER, FIVE_SEC);   // (timer removed)
    //   DB_printf("ES_TIMEOUT received from Timer %d in Service %d\r\n",
    //       ThisEvent.EventParam, MyPriority);
    // }
    // break;
    // CHEF_Project_LED_1
    case ES_TIMEOUT:   // route by which timer expired
    {
      if (BUZZER_TIMER == ThisEvent.EventParam)   // beep time is up
      {
        StopBeep();
      }
      else if (SERVO_MOVE_TIMER == ThisEvent.EventParam)    // Jay: follow pot
      {
        moveServo();
        ES_Timer_InitTimer(SERVO_MOVE_TIMER, 1);
      }
    }
    break;
    case ES_TASK_TIMED_OUT:  // task not completed in time: next-task beep
    {
      StartTimeoutBeep();
    }
    break;
    case ES_CLOSE_DISPENSER: // left the completion page / new game
    {
      CloseDispenser();
    }
    break;
    case ES_GAME_OVER:       // whole game completed: long beep + burger
    {
      StartLongBeep();
      StartDispense();         // both servos: game-completion sequence
    }
    break;
    case ES_TASK_COMPLETE:   // any task finished: one 1 s beep
    {
      StartBeep();
      if (3 == ThisEvent.EventParam)   // Task 3: second servo 0->180->0
      {
        StartTask3Sweep();
      }
    }
    break;
    case ES_SHORT_TIMEOUT:   // lower the line & announce
    {
#if SHOW_DEBUG_PRINTS
      puts("\rES_SHORT_TIMEOUT received\r\n");
#endif
    }
    break;
    case ES_NEW_KEY:   // announce
    {
#if SHOW_DEBUG_PRINTS
      DB_printf("ES_NEW_KEY received with -> %c <- in Service 0\r\n",
          (char)ThisEvent.EventParam);
#endif
      if ('d' == ThisEvent.EventParam)
      {
        ThisEvent.EventParam = DeferredChar++;   //
        if (ES_DeferEvent(DeferralQueue, ThisEvent))
        {
#if SHOW_DEBUG_PRINTS
          puts("ES_NEW_KEY deferred in Service 0\r");
#endif
        }
      }
      if ('r' == ThisEvent.EventParam)
      {
        ThisEvent.EventParam = 'Q';   // This one gets posted normally
        PostTestHarnessService0(ThisEvent);
        // but we slide the deferred events under it so it(they) should come out first
        if (true == ES_RecallEvents(MyPriority, DeferralQueue))
        {
#if SHOW_DEBUG_PRINTS
          puts("ES_NEW_KEY(s) recalled in Service 0\r");
#endif
          DeferredChar = '1';
        }
      }
#ifdef TEST_INT_POST
      if ('p' == ThisEvent.EventParam)
      {
        StartTMR2();
      }
#endif
    }
    break;
    default:
    {}
     break;
  }

  return ReturnEvent;
}

/***************************************************************************
 private functions
 ***************************************************************************/
#ifdef BLINK_LED
#define LED LATBbits.LATB6
static void InitLED(void)
{
  LED = 0; //start with it off
  TRISBbits.TRISB6 = 0; // set RB6 as an output
}

static void BlinkLED(void)
{
  // toggle state of LED
  LED = ~LED;
}
#endif

#ifdef TEST_INT_POST
#include <sys/attribs.h> // for ISR macors

// for testing posting from interrupts.
// Intializes TMR2 to gerenate an interrupt at 100ms
static void InitTMR2(void)
{
  // turn timer off
  T2CONbits.ON = 0;
  // Use internal peripheral clock
  T2CONbits.TCS = 0;
  // setup for 16 bit mode
  T2CONbits.T32 = 0;
  // set prescale to 1:1
  T2CONbits.TCKPS = 0;
  // load period value
  PR2 = 2000-1; // creates a 100ms period with a 20MHz peripheral clock
  // set priority
  IPC2bits.T2IP = 2;
  // clear interrupt flag
  IFS0bits.T2IF = 0;
  // enable the timer interrupt
  IEC0bits.T2IE = 1;
}

// Clears and Starts TMR2
static void StartTMR2(void)
{
  // clear timer
  TMR2 = 0;
  // start timer
  //LATBbits.LATB14 = 0;
  T2CONbits.ON = 1;
}

void __ISR(_TIMER_2_VECTOR, IPL2AUTO) Timer2ISR(void)
{
  // clear flag
  IFS0bits.T2IF = 0;
  // post event
  static ES_Event_t interruptEvent = {ES_SHORT_TIMEOUT, 0};
  PostTestHarnessService0(interruptEvent);
  
  // stop timer
  T2CONbits.ON = 0;
  return;
}
#endif
/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/