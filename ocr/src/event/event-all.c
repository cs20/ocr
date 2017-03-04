/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "event/event-all.h"


const char * event_types [] = {
#ifdef ENABLE_EVENT_HC
    "HC",
#endif
    NULL
};

ocrEventFactory_t *newEventFactory(eventType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_EVENT_HC
    case eventHc_id:
        return newEventFactoryHc(typeArg, typeArg->id);
#endif
    default:
        ASSERT(0);
        return NULL;
    };
}

