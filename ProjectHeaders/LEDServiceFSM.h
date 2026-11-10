/****************************************************************************

  Header file for template Flat Sate Machine
  based on the Gen2 Events and Services Framework

 ****************************************************************************/
// Part 4.2) Modified to LED Service FSM

// #ifndef FSMTemplate_H
// #define FSMTemplate_H
#ifndef LEDServiceFSM_H
#define LEDServiceFSM_H

// Event Definitions
#include "ES_Configure.h" /* gets us event definitions */
#include "ES_Types.h"     /* gets bool type for returns */
#include "ES_Events.h"    /* gets ES_Event_t */   // Part 4.2)

// typedefs for the states
// State definitions for use with the query function

// typedef enum
// {
//   InitPState, UnlockWaiting, _1UnlockPress,
//   _2UnlockPresses, Locked
// }TemplateState_t;

typedef enum
{
  WaitForChar, UpdateDisplay
}LEDState_t;

// Public Function Prototypes

// bool InitTemplateFSM(uint8_t Priority);
// bool PostTemplateFSM(ES_Event_t ThisEvent);
// ES_Event_t RunTemplateFSM(ES_Event_t ThisEvent);
// TemplateState_t QueryTemplateSM(void);

// Part 4.2) LED Service FSM function prototypes
bool InitLEDServiceFSM(uint8_t Priority);
bool PostLEDServiceFSM(ES_Event_t ThisEvent);
ES_Event_t RunLEDServiceFSM(ES_Event_t ThisEvent);
LEDState_t QueryLEDServiceFSM(void);

// #endif /* FSMTemplate_H */
#endif /* LEDServiceFSM_H */

