#include "perfs.h"
#include "ocr.h"

/**
 * DESC:
 */

#ifndef TEST_NBCONTRIBSPD
#define TEST_NBCONTRIBSPD 1000
#endif


#if defined(ENABLE_EXTENSION_LABELING)

#include "extensions/ocr-labeling.h"
#include "extensions/ocr-affinity.h"

 typedef struct _infoDb_t {
    ocrGuid_t stickyGuid;
    ocrGuid_t latchGuid;
    timestamp_t addStart;
    timestamp_t addStop;
    timestamp_t satStart;
    timestamp_t satStop;
 } infoDb_t;

ocrGuid_t pdHeadEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
#ifdef SHOW_PRINT
    PRINTF("HEAD %"PRIu64"\n", (u64)TEST_NBCONTRIBSPD);
#endif
#ifdef SYNC_HEAD
    ocrGuid_t dbGuid = depv[0].guid;
    infoDb_t * dbPtr = (infoDb_t *) depv[0].ptr;
    ocrGuid_t stickyGuid = dbPtr->stickyGuid;
    ocrGuid_t parentLatchGuid = dbPtr->latchGuid;
#else
    ocrGuid_t stickyGuid;
    stickyGuid.guid = paramv[0];
    ocrGuid_t parentLatchGuid;
    parentLatchGuid.guid = paramv[1];
#endif
    ocrGuid_t rankAffGuid;
    ocrAffinityGetCurrent(&rankAffGuid);
    ocrGuid_t latchGuid;
    ocrEventParams_t params;
    params.EVENT_LATCH.counter = TEST_NBCONTRIBSPD;
    ocrEventCreateParams(&latchGuid, OCR_EVENT_LATCH_T, false, &params);
    ocrAddDependence(latchGuid, parentLatchGuid, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
    u32 i = 0;
    while (i < TEST_NBCONTRIBSPD) {
        ocrAddDependence(stickyGuid, latchGuid, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
        i++;
    }
    return NULL_GUID;
}

ocrGuid_t shutEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    infoDb_t * info = (infoDb_t *) depv[0].ptr;
#ifndef SATSENDER_PRINT
    get_time(&(info->satStop));
#endif
    timestamp_t * addStart = (timestamp_t *) &(info->addStart);
    timestamp_t * addStop = (timestamp_t *) &(info->addStop);
    timestamp_t * satStart = (timestamp_t *) &(info->satStart);
    timestamp_t * satStop = (timestamp_t *) &(info->satStop);
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
#ifdef DEP_PRINT
    print_throughput_custom("AddDep", TEST_NBCONTRIBSPD*affinityCount, elapsed_sec(satStart, satStop), "");
#endif
#ifdef SAT_PRINT
    print_throughput_custom("Satisfy", TEST_NBCONTRIBSPD*affinityCount, elapsed_sec(addStart, addStop), "");
#endif
#ifdef SATSENDER_PRINT
    print_throughput_custom("SatisfySender", TEST_NBCONTRIBSPD*affinityCount, elapsed_sec(addStart, addStop), "");
#endif

    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t satEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
#ifdef SHOW_PRINT
    PRINTF("SAT\n");
#endif
    ocrGuid_t dbGuid = depv[0].guid;
    infoDb_t * info = (infoDb_t *) depv[0].ptr;
    get_time(&(info->addStop));
    get_time(&(info->satStart));
    // Now satisfy the event
    //TODO start stop
    ocrEventSatisfy(info->stickyGuid, NULL_GUID);
#ifdef SATSENDER_PRINT
    get_time(&(info->satStop));
#endif
    return NULL_GUID;
}

ocrGuid_t spawnerEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dbGuid = depv[0].guid;
    infoDb_t * info = (infoDb_t *) depv[0].ptr;
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
#ifdef SYNC_HEAD
    ocrGuid_t pdHeadEdtTpl;
    ocrEdtTemplateCreate(&pdHeadEdtTpl, pdHeadEdt, 0, 1);
#else
    ocrGuid_t pdHeadEdtTpl;
    ocrEdtTemplateCreate(&pdHeadEdtTpl, pdHeadEdt, 2, 0);
#endif

    get_time(&(info->addStart));
    ocrDbRelease(dbGuid);
    u32 i = 0;
    while (i < affinityCount) {
#ifdef SHOW_PRINT
        PRINTF("SPAWN\n");
#endif
        ocrHint_t edtHint;
        ocrHintInit(&edtHint, OCR_HINT_EDT_T);
        ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affinities[i]));
#ifdef SYNC_HEAD
        ocrGuid_t edtGuid;
        ocrEdtCreate(&edtGuid, pdHeadEdtTpl, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);
        ocrAddDependence(dbGuid, edtGuid, 0, DB_MODE_RO);
#else
        u64 nparamv[2];
        nparamv[0] = (u64) info->stickyGuid.guid;
        nparamv[1] = (u64) info->latchGuid.guid;
        ocrEdtCreate(NULL, pdHeadEdtTpl, 2, nparamv, 0, NULL, EDT_PROP_NONE, &edtHint, NULL);
#endif
        i++;
    }
    return NULL_GUID;
}


ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t latchGuid;
    ocrEventParams_t params;
    params.EVENT_LATCH.counter = affinityCount;
    ocrEventCreateParams(&latchGuid, OCR_EVENT_LATCH_T, false, &params);
    ocrGuid_t stickyGuid;
    ocrEventCreate(&stickyGuid, OCR_EVENT_STICKY_T, false);
    ocrGuid_t dbGuid;
    infoDb_t * dbPtr;
    ocrDbCreate(&dbGuid, (void**)&dbPtr, sizeof(infoDb_t), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    dbPtr->stickyGuid = stickyGuid;
    dbPtr->latchGuid = latchGuid;
    ocrDbRelease(dbGuid);

    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affinities[0]));

    ocrGuid_t shutGuidTpl;
    ocrEdtTemplateCreate(&shutGuidTpl, shutEdt, 0, 2);
    ocrGuid_t shutGuid;
    ocrEdtCreate(&shutGuid, shutGuidTpl, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);
    ocrAddDependence(dbGuid, shutGuid, 0, DB_MODE_RW);
    ocrAddDependence(latchGuid, shutGuid, 1, DB_MODE_RO);

    ocrGuid_t finishGuid;
    ocrEventCreate(&finishGuid, OCR_EVENT_STICKY_T, false);

    ocrGuid_t spawnerGuidTpl;
    ocrEdtTemplateCreate(&spawnerGuidTpl, spawnerEdt, 0, 1);
    ocrGuid_t spawnerGuid;
    ocrEdtCreate(&spawnerGuid, spawnerGuidTpl, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, &dbGuid, EDT_PROP_FINISH | EDT_PROP_OEVT_VALID, &edtHint, &finishGuid);

    ocrGuid_t satGuidTpl;
    ocrEdtTemplateCreate(&satGuidTpl, satEdt, 0, 2);
    ocrGuid_t satGuid;
    ocrEdtCreate(&satGuid, satGuidTpl, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);
    ocrAddDependence(dbGuid, satGuid, 0, DB_MODE_RW);
    ocrAddDependence(finishGuid, satGuid, 1, DB_MODE_RW);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_LABELING not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
