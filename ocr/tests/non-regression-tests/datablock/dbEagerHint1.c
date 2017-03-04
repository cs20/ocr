/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Channel-event: String of producers EDTs sending to consumers
 */

#ifdef ENABLE_EXTENSION_CHANNEL_EVT

#define NB_ELEMS 100

ocrGuid_t prodEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u32 * dbPtr = depv[0].ptr;
    u32 i = 0;
    while (i < NB_ELEMS) {
        ASSERT(dbPtr[i] == i);
        i++;
    }
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrEventParams_t params;
    params.EVENT_CHANNEL.maxGen = 1;
    params.EVENT_CHANNEL.nbSat = 1;
    params.EVENT_CHANNEL.nbDeps = 1;
    ocrGuid_t chEvt;
    ocrEventCreateParams(&chEvt, OCR_EVENT_CHANNEL_T, false, &params);


    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t edtAffinity = affinities[affinityCount-1];

    ocrHint_t dbHint;
    ocrHintInit( &dbHint, OCR_HINT_DB_T );
    ocrSetHintValue(&dbHint, OCR_HINT_DB_EAGER, 1);
    ocrGuid_t dbGuid;
    u32 * dbPtr;
    ocrDbCreate(&dbGuid, (void **)&dbPtr, sizeof(u32)*NB_ELEMS, 0, &dbHint, NO_ALLOC);
    u32 i = 0;
    while (i < NB_ELEMS) {
        dbPtr[i] = i;
        i++;
    }
    ocrDbRelease(dbGuid);
    ocrGuid_t edtTpl;
    ocrEdtTemplateCreate(&edtTpl, prodEdt, 0, 1);
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( edtAffinity ) );
    ocrGuid_t edt;
    ocrEdtCreate(&edt, edtTpl, EDT_PARAM_DEF, NULL,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);

    ocrAddDependence(chEvt, edt, 0, DB_MODE_RO);
    ocrEventSatisfy(chEvt, dbGuid);
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_CHANNEL_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
