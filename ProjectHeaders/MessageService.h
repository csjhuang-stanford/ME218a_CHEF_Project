/****************************************************************************

  Header file for template service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

// Part 4.3) Modified the TemplateService.h to MessageService.h

// #ifndef ServTemplate_H
// #define ServTemplate_H

#ifndef MessageService_H
#define MessageService_H

// #include "ES_Types.h"

#include <stdint.h>
#include <stdbool.h>

#include "ES_Events.h"
#include "ES_Port.h"

// Public Function Prototypes

// bool InitTemplateService(uint8_t Priority);
// bool PostTemplateService(ES_Event_t ThisEvent);
// ES_Event_t RunTemplateService(ES_Event_t ThisEvent);

bool InitMessageService(uint8_t Priority);
bool PostMessageService(ES_Event_t ThisEvent);
ES_Event_t RunMessageService(ES_Event_t ThisEvent);

// #endif /* ServTemplate_H */

#endif /* MessageService_H */

