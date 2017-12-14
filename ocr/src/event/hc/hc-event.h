/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_EVENT_H__
#define __HC_EVENT_H__

#include "ocr-config.h"
#ifdef ENABLE_EVENT_HC

#include "hc/hc.h"
#include "ocr-event.h"
#include "ocr-hal.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

#ifdef ENABLE_HINTS
/**< The number of hint properties supported by this implementation */
#define OCR_HINT_COUNT_EVT_HC   0
#else
#define OCR_HINT_COUNT_EVT_HC   0
#endif

// Size for the static waiter array
#ifndef HCEVT_WAITER_STATIC_COUNT
#define HCEVT_WAITER_STATIC_COUNT 4
#endif

// Size for the dynamically allocated waiter array
#ifndef HCEVT_WAITER_DYNAMIC_COUNT
#define HCEVT_WAITER_DYNAMIC_COUNT 4
#endif

#ifndef ENABLE_EVENT_MDC
#define ENABLE_EVENT_MDC 0
#endif

#define MDC_SUPPORT_EVT(guidKind) (ENABLE_EVENT_MDC && ((guidKind == OCR_GUID_EVENT_IDEM) || (guidKind == OCR_GUID_EVENT_STICKY)))

typedef struct _paramListEventHc_t {
    paramListEvent_t base;
} paramListEventHc_t;

typedef struct {
    ocrEventFactory_t base;
} ocrEventFactoryHc_t;

typedef struct locNode_t {
    ocrLocation_t loc;
    struct locNode_t * next;
} locNode_t;

typedef struct ocrEventHcDist_t {
    ocrLocation_t satFromLoc;
    ocrLocation_t delFromLoc;
    locNode_t * peers; // A list of unique peers locations
} ocrEventHcDist_t;

typedef struct ocrEventHc_t {
    ocrEvent_t base;
    ocrEventHcDist_t mdClass;
    regNode_t waiters[HCEVT_WAITER_STATIC_COUNT]; /**< hold waiters. If overflows a dynamically
                                              allocated waiter list is stored in waitersDb */
    ocrFatGuid_t waitersDb; /**< DB containing an array of regNode_t listing the
                             * events/EDTs depending on this event */
    volatile u32 waitersCount; /**< Number of waiters in waitersDb */
    u32 waitersMax; /**< Maximum number of waiters in waitersDb */
    lock_t waitersLock;
    ocrRuntimeHint_t hint;
} ocrEventHc_t;

typedef struct _ocrEventHcPersist_t {
    ocrEventHc_t base;
    ocrGuid_t data;
} ocrEventHcPersist_t;

typedef struct _ocrEventHcCounted_t {
    ocrEventHcPersist_t base;
    u64 nbDeps; // this is only updated inside a lock
} ocrEventHcCounted_t;

typedef struct _ocrEventHcLatch_t {
    ocrEventHc_t base;
    s32 counter;
#ifdef ENABLE_AMT_RESILIENCE
    u8 readyToDestruct;
    u8 shutdownLatch;
    s32 scheduledResCounter, activeResCounter;
    ocrGuid_t resilientParentLatch;
    ocrGuid_t resilientScopeEdt;
    ocrGuid_t *dbPublishArray;
    u32 dbPublishArrayLength;
    u32 dbPublishCount;
    ocrGuid_t *guidDestroyArray;
    u32 guidDestroyArrayLength;
    u32 guidDestroyCount;
    ocrGuid_t *childDestroyArray;
    u32 childDestroyArrayLength;
    u32 childDestroyCount;
#endif
} ocrEventHcLatch_t;

typedef struct _ocrEventHcChannel_t {
    ocrEventHc_t base;
    u32 maxGen; // Maximum number of generations simultaneously in flight
    u32 nbSat;  // Number of Satisfy per generation
    u32 nbDeps; // Number of dependences per generation
    // Data-Structure to hold satisfy values
    u32 headSat;
    u32 tailSat;
    u32 satBufSz; // = maxGen * nbSat
    ocrGuid_t * satBuffer; // An array of GUID values, possibly multi-dimensional and linearized
    // Data-Structure to hold dependence registrations
    u32 headWaiter;
    u32 tailWaiter;
    u32 waitBufSz; // = maxGen * nbDeps
    regNode_t * waiters; // An array of registration node, possibly multi-dimensional and linearized
} ocrEventHcChannel_t;

ocrGuidKind eventTypeToGuidKind(ocrEventTypes_t eventType);

ocrEventFactory_t* newEventFactoryHc(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_EVENT_HC */
#endif /* __HC_EVENT_H__ */
