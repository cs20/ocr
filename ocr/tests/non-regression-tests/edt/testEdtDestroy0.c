/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"

/**
 * DESC: Test ocrEdtDestruct API call correctness in various scenarios
 *
 * Commented some tests that are not compatible with one-sided asynchronous
 * registrations (REG_ASYNC_SGL). Basically an event holds an EDT guid to
 * be satisfied but the EDT has been destroyed in the meanwhile. The EDT
 * doesn't know its satisfiers and cannot notify them.
 */

#define MAX_SLOT 5

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Should never get executed
    ASSERT(false);
    return NULL_GUID;
}

void noDeps() {
    ocrGuid_t workEdtGuid;
    ocrGuid_t workEdtTplGuid;
    ocrEdtTemplateCreate(&workEdtTplGuid, workEdt, 0 /*paramc*/, MAX_SLOT /*depc*/);
    ocrEdtCreate(&workEdtGuid, workEdtTplGuid, 0, NULL, MAX_SLOT, NULL,
                 /*properties=*/EDT_PROP_NONE, NULL_HINT, /*outEvent=*/ NULL);
    ocrEdtDestroy(workEdtGuid);
}

void allEventDeps(ocrEventTypes_t eventType) {
    ocrGuid_t workEdtGuid;
    ocrGuid_t workEdtTplGuid;
    ocrEdtTemplateCreate(&workEdtTplGuid, workEdt, 0 /*paramc*/, MAX_SLOT /*depc*/);
    ocrEdtCreate(&workEdtGuid, workEdtTplGuid, 0, NULL, MAX_SLOT, NULL,
                 /*properties=*/EDT_PROP_NONE, NULL_HINT, /*outEvent=*/ NULL);
    u32 i = 0;
    while (i < MAX_SLOT) {
        ocrGuid_t e0;
        ocrEventCreate(&e0, eventType, false);
        ocrAddDependence(e0, workEdtGuid, i, DB_MODE_CONST);
        i++;
    }
    ocrEdtDestroy(workEdtGuid);
}

// Creates an event, and edt, add dependence, destroy, opt satisfy event
void eventDeps(u32 slot, bool isPreSatisfied, bool isPostSatisfied, ocrEventTypes_t eventType) {
    ASSERT(slot < MAX_SLOT);
    ocrGuid_t e0;
    ocrEventCreate(&e0, eventType, false);
    if (isPreSatisfied) {
        ocrEventSatisfy(e0, NULL_GUID);
    }
    ocrGuid_t workEdtGuid;
    ocrGuid_t workEdtTplGuid;
    ocrEdtTemplateCreate(&workEdtTplGuid, workEdt, 0 /*paramc*/, MAX_SLOT /*depc*/);
    ocrEdtCreate(&workEdtGuid, workEdtTplGuid, 0, NULL, MAX_SLOT, NULL,
                 /*properties=*/EDT_PROP_NONE, NULL_HINT, /*outEvent=*/ NULL);
    ocrAddDependence(e0, workEdtGuid, slot, DB_MODE_CONST);
    ocrEdtDestroy(workEdtGuid);
    if (isPostSatisfied) {
        ocrEventSatisfy(e0, NULL_GUID);
    }
}

void testPersistentEventDeps(u32 slot, ocrEventTypes_t eventType) {
    // No event satisfaction
    eventDeps(slot, false, false, eventType);
    // Satisfy event before addDependence & destruction
    eventDeps(slot, true, false, eventType);
    // Commenting this as it is not compatible with option REG_ASYNC_SGL
    // // Satisfy event after addDependence & destruction
    // eventDeps(slot, false, true, eventType);
}

void testNonPersistentEventDeps(u32 slot, ocrEventTypes_t eventType) {
    // No event satisfaction
    eventDeps(slot, false, false, eventType);
    // Commenting this as it is not compatible with option REG_ASYNC_SGL
    // // Satisfy event after addDependence & destruction
    // eventDeps(slot, false, true, eventType);
    // Note: Illegal to satisfy before addDependence since it is non-persistent
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    // Destroys an EDT that has no dependences set up yet
    noDeps();

    // Persistent event is the first dependence
    testPersistentEventDeps(0, OCR_EVENT_STICKY_T);

    // Persistent event is not the first dependence
    testPersistentEventDeps(1, OCR_EVENT_STICKY_T);

    // Non-persistent event is the first dependence
    testNonPersistentEventDeps(0, OCR_EVENT_ONCE_T);

    // Non-persistent event is not the first dependence
    testNonPersistentEventDeps(1, OCR_EVENT_ONCE_T);

    //All dependences added but none satisfied
    allEventDeps(OCR_EVENT_STICKY_T);
    allEventDeps(OCR_EVENT_ONCE_T);

    ocrShutdown();
    return NULL_GUID;
}
