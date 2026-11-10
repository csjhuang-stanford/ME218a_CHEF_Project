/****************************************************************************

  Header file for TaskService (CHEF project)
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

#ifndef TaskService_H
#define TaskService_H

#include <stdint.h>
#include <stdbool.h>

#include "ES_Events.h"
#include "ES_Port.h"                // needed for definition of REENTRANT

// Public Function Prototypes

bool InitTaskService(uint8_t Priority);
bool PostTaskService(ES_Event_t ThisEvent);
ES_Event_t RunTaskService(ES_Event_t ThisEvent);
uint8_t QueryCurrentTask(void);

#endif /* TaskService_H */
