/****************************************************************************

  Header file for EventPrint (CHEF project)
  One shared helper so every service announces the events it receives in
  the same format, e.g.
      ES_INIT received in KeyService (Service 1)

 ****************************************************************************/

#ifndef EventPrint_H
#define EventPrint_H

#include <stdint.h>
#include <stdbool.h>

#include "ES_Configure.h"
#include "ES_Events.h"

// true  = every service prints each event it receives
// false = no event prints (quieter terminal for normal play)
#define SHOW_EVENT_PRINTS  false

// Public Function Prototypes

const char *EventName(ES_EventType_t EventType);   // "ES_INIT", ...
void PrintEvent(const char *ServiceName, uint8_t Priority,
                ES_Event_t ThisEvent);

#endif /* EventPrint_H */
