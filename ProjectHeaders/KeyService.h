/****************************************************************************

  Header file for template service
  based on the Gen 2 Events and Services Framework

 ****************************************************************************/

// Part 3.5) Modified the TemplateService.h to KeyService.h

// #ifndef ServTemplate_H
// #define ServTemplate_H

#ifndef KeyService_H
#define KeyService_H

// #include "ES_Types.h"

#include <stdint.h>
#include <stdbool.h>

#include "ES_Events.h"
#include "ES_Port.h"

// Public Function Prototypes

// bool InitTemplateService(uint8_t Priority);
// bool PostTemplateService(ES_Event_t ThisEvent);
// ES_Event_t RunTemplateService(ES_Event_t ThisEvent);

bool InitKeyService(uint8_t Priority);
bool PostKeyService(ES_Event_t ThisEvent);
ES_Event_t RunKeyService(ES_Event_t ThisEvent);

// #endif /* ServTemplate_H */

#endif /* KeyService_H */

