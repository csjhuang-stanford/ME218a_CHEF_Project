/****************************************************************************
 Module
   EventPrint.c

 Revision
   1.0.0

 Description
   Shared helper: each service calls PrintEvent() at the top of its Run
   function, so the terminal shows every event and who received it:
       ES_INIT received in TaskService (Service 1)
       ES_START_TASK received in TaskService (Service 1)
   Turn all of these prints off with SHOW_EVENT_PRINTS in EventPrint.h.

 Notes
   EventName() must list every event in ES_EventType_t (ES_Configure.h).
   If you add an event there, add a case here too, otherwise it prints
   as "ES_UNKNOWN_EVENT".

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/26       CH      started coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/EventPrint.h"

#include "dbprintf.h"

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     EventName

 Returns
     the event's name as text, e.g. ES_INIT -> "ES_INIT"
****************************************************************************/
const char *EventName(ES_EventType_t EventType)
{
  switch (EventType)
  {
    case ES_NO_EVENT:               return "ES_NO_EVENT";
    case ES_ERROR:                  return "ES_ERROR";
    case ES_INIT:                   return "ES_INIT";
    case ES_TIMEOUT:                return "ES_TIMEOUT";
    case ES_SHORT_TIMEOUT:          return "ES_SHORT_TIMEOUT";
    case ES_NEW_KEY:                return "ES_NEW_KEY";
    case ES_LOCK:                   return "ES_LOCK";
    case ES_UNLOCK:                 return "ES_UNLOCK";
    case ES_ENTRY:                  return "ES_ENTRY";
    case ES_ENTRY_HISTORY:          return "ES_ENTRY_HISTORY";
    case ES_EXIT:                   return "ES_EXIT";
    case ES_ADD_CHAR_TO_DISPLAY:    return "ES_ADD_CHAR_TO_DISPLAY";
    case ES_ROW_UPDATE_COMPLETE:    return "ES_ROW_UPDATE_COMPLETE";
    case ES_BEAM_BROKEN:            return "ES_BEAM_BROKEN";
    case ES_SWITCH_CHANGED:         return "ES_SWITCH_CHANGED";
    case ES_POT_CHANGED:            return "ES_POT_CHANGED";
    case ES_GAME_OVER:              return "ES_GAME_OVER";
    case ES_GAME_TIMER_EXPIRED:     return "ES_GAME_TIMER_EXPIRED";
    case ES_START_TASK:             return "ES_START_TASK";
    case ES_TASK_COMPLETE:          return "ES_TASK_COMPLETE";
    case ES_USER_INPUT:             return "ES_USER_INPUT";
    default:                        return "ES_UNKNOWN_EVENT";
  }
}

/****************************************************************************
 Function
     PrintEvent

 Description
     Prints "<EVENT> received in <ServiceName> (Service <Priority>)"
****************************************************************************/
void PrintEvent(const char *ServiceName, uint8_t Priority,
                ES_Event_t ThisEvent)
{
  if (SHOW_EVENT_PRINTS)
  {
    DB_printf("%s received in %s (Service %d)\r\n",
        EventName(ThisEvent.EventType), ServiceName, Priority);
  }
}

/*------------------------------ End of file ------------------------------*/