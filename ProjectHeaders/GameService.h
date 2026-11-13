/****************************************************************************

  Header file for GameService (CHEF project)
  Game-level state machine: welcome page, playing, game completion page.
  Owns the game timer, the score and the task order.
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef GameService_H
#define GameService_H

#include <stdint.h>
#include <stdbool.h>

#include "ES_Events.h"
#include "ES_Port.h"                // needed for definition of REENTRANT

// Game states
typedef enum
{
  GAME_WELCOME = 0,                 // welcome page, waiting for SPACE
  GAME_PLAYING,                     // working through taskOrder
  GAME_COMPLETE                     // Displaying Game Completion Page
} GameState_t;

// Public Function Prototypes

bool InitGameService(uint8_t Priority);
bool PostGameService(ES_Event_t ThisEvent);
ES_Event_t RunGameService(ES_Event_t ThisEvent);
GameState_t QueryGameState(void);
uint16_t QueryUserScore(void);

#endif /* GameService_H */
