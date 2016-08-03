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
#define MAX_ITER 10

ocrGuid_t prodEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t * evts = ((ocrGuid_t *) paramv);
    ocrGuid_t prodEvt = evts[0];
    ocrGuid_t consEvt = evts[1];
    ocrGuid_t dbGuid = depv[1].guid;
    u32 * dbPtr = depv[1].ptr;
    u32 it = dbPtr[0];
    u32 i = 0;
    while (i < NB_ELEMS) {
        ASSERT(dbPtr[i] == (i+it));
        dbPtr[i]++;
        i++;
    }
    ocrDbRelease(dbGuid);

    // Spawn next production EDT
    ocrGuid_t curAff;
    ocrAffinityGetCurrent(&curAff);
    ocrHint_t edtProdHint;
    ocrHintInit(&edtProdHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtProdHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(curAff));
    ocrGuid_t prodEdtTpl;
    ocrEdtTemplateCreate(&prodEdtTpl, prodEdt, paramc, 2);
    ocrGuid_t ndepv[2];
    ndepv[0] = prodEvt;
    ndepv[1] = dbGuid;
    ocrGuid_t consEdt;
    ocrEdtCreate(&consEdt, prodEdtTpl, paramc, paramv,
                 depc, ndepv, EDT_PROP_NONE, &edtProdHint, NULL);
    ocrEdtTemplateDestroy(prodEdtTpl);

    ocrEventSatisfy(consEvt, dbGuid);

    return NULL_GUID;
}

ocrGuid_t consEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t * evts = ((ocrGuid_t *) paramv);
    ocrGuid_t prodEvt = evts[0];
    ocrGuid_t consEvt = evts[1];
    ocrGuid_t dbGuid = depv[0].guid;

    u32 * dbPtr = depv[0].ptr;
    u32 it = dbPtr[0];
    u32 i = 0;
    while (i < NB_ELEMS) {
        ASSERT(dbPtr[i] == (i+it));
        i++;
    }
    ocrDbRelease(dbGuid);

    if (it == MAX_ITER) {
        ocrShutdown();
    } else {
        PRINTF("it=%"PRIu32"\n", it);
        // Spawn next production EDT
        ocrGuid_t curAff;
        ocrAffinityGetCurrent(&curAff);
        ocrHint_t edtConsHint;
        ocrHintInit(&edtConsHint, OCR_HINT_EDT_T);
        ocrSetHintValue(&edtConsHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(curAff));
        ocrGuid_t consEdtTpl;
        ocrEdtTemplateCreate(&consEdtTpl, consEdt, paramc, depc);
        ocrGuid_t consEdt;
        ocrEdtCreate(&consEdt, consEdtTpl, paramc, paramv,
                     depc, NULL, EDT_PROP_NONE, &edtConsHint, NULL);
        ocrAddDependence(consEvt, consEdt, 0, DB_MODE_RO);
        ocrEdtTemplateDestroy(consEdtTpl);
        ocrEventSatisfy(prodEvt, NULL_GUID);
    }
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrEventParams_t params;
    params.EVENT_CHANNEL.maxGen = 1;
    params.EVENT_CHANNEL.nbSat = 1;
    params.EVENT_CHANNEL.nbDeps = 1;
    ocrGuid_t consEvt;
    ocrEventCreateParams(&consEvt, OCR_EVENT_CHANNEL_T, false, &params);
    ocrGuid_t prodEvt;
    ocrEventCreateParams(&prodEvt, OCR_EVENT_CHANNEL_T, false, &params);

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
    u64 nparamc = (sizeof(ocrGuid_t)/sizeof(u64))*2;
    ocrGuid_t nparamv[2];
    nparamv[0] = prodEvt;
    nparamv[1] = consEvt;
    ocrGuid_t curAff;
    ocrAffinityGetCurrent(&curAff);
    ocrGuid_t prodEdtTpl;
    ocrEdtTemplateCreate(&prodEdtTpl, prodEdt, nparamc, 2);
    ocrHint_t edtProdHint;
    ocrHintInit(&edtProdHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtProdHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(curAff));
    ocrGuid_t prodEdt;
    ocrEdtCreate(&prodEdt, prodEdtTpl, nparamc, (u64*) nparamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtProdHint, NULL);
    ocrEdtTemplateDestroy(prodEdtTpl);
    ocrAddDependence(prodEvt, prodEdt, 0, DB_MODE_RO);
    ocrAddDependence(dbGuid, prodEdt, 1, DB_MODE_RW);

    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t edtAffinity = affinities[affinityCount-1];
    ocrGuid_t consEdtTpl;
    ocrEdtTemplateCreate(&consEdtTpl, consEdt, nparamc, 1);
    ocrHint_t edtConsHint;
    ocrHintInit(&edtConsHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtConsHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(edtAffinity));
    ocrGuid_t consEdt;
    ocrEdtCreate(&consEdt, consEdtTpl, nparamc, (u64*) nparamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtConsHint, NULL);
    ocrEdtTemplateDestroy(consEdtTpl);
    ocrAddDependence(consEvt, consEdt, 0, DB_MODE_RO);

    ocrEventSatisfy(prodEvt, NULL_GUID);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_CHANNEL_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
