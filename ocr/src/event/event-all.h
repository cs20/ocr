/**
 * @brief OCR events
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __EVENT_ALL_H__
#define __EVENT_ALL_H__

#include "debug.h"
#include "ocr-config.h"
#include "ocr-event.h"
#include "utils/ocr-utils.h"

typedef enum _eventType_t {
#ifdef ENABLE_EVENT_HC
    eventHc_id,
#endif
    eventMax_id
} eventType_t;

extern const char * event_types[];

#ifdef ENABLE_EVENT_HC
#include "event/hc/hc-event.h"
#endif

ocrEventFactory_t *newEventFactory(eventType_t type, ocrParamList_t *typeArg);

#endif /* __EVENT_ALL_H__ */
