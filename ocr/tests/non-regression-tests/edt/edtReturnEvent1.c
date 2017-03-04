/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
/**
 * DESC: Test using a future Event GUID as an EDT return value
 */

#ifdef ENABLE_EXTENSION_COUNTED_EVT

#define N 8

ocrGuid_t consumer(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
    int i, *ptr = (int*)depv[0].ptr;
    for(i = 0; i < N; i++) ASSERT(N-i == ptr[i]);
    ocrDbDestroy( depv[0].guid );
    PRINTF("Everything went OK\n");
    ocrShutdown();
    return NULL_GUID;
}
ocrGuid_t producer2(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
    ocrGuid_t db_guid;
    int i, *ptr;
    ocrDbCreate(&db_guid, (void **)&ptr, sizeof(*ptr)*N,
                /*flags=*/DB_PROP_NONE, /*location=*/NULL_HINT, NO_ALLOC);
    for(i = 0; i < N; i++) ptr[i] = N - i;
    return db_guid;
}
ocrGuid_t producer1(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
    ocrGuid_t producer_template, producer_edt, producer_done_event;
    ocrEdtTemplateCreate(&producer_template, producer2, 0, 0);

    // Here we create a counted event to bridge producer's datablock
    // and avoid it being destroyed before we could have returned its guid.
    ocrEventParams_t complete_params;
    complete_params.EVENT_COUNTED.nbDeps = 1;
    ocrEventCreateParams(&producer_done_event, OCR_EVENT_COUNTED_T,
                         EVT_PROP_TAKES_ARG, &complete_params);

    ocrEdtCreate(&producer_edt, producer_template, 0, NULL, 0, NULL,
                 EDT_PROP_OEVT_VALID, NULL_HINT, &producer_done_event);
    return producer_done_event;
}
ocrGuid_t mainEdt(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
    ocrGuid_t producer_template, consumer_template;
    ocrGuid_t producer_edt, consumer_edt;
    ocrGuid_t producer_done_event;

    ocrEdtTemplateCreate(&producer_template, producer1, 0, 0);
    ocrEdtTemplateCreate(&consumer_template, consumer , 0, 1);

    // Create producer output event in advance, so that we can
    // add its dependences before it can be satisfied.
    ocrEventCreate(&producer_done_event,OCR_EVENT_ONCE_T, EVT_PROP_NONE);

    // Create consumer and add dependency on producer output
    ocrEdtCreate(&consumer_edt, consumer_template, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(producer_done_event,consumer_edt,0,DB_MODE_CONST);

    ocrEdtCreate(&producer_edt, producer_template, 0, NULL, 0, NULL,
                 EDT_PROP_OEVT_VALID, NULL_HINT, &producer_done_event);
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_COUNTED_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif // ENABLE_EXTENSION_COUNTED_EVT

