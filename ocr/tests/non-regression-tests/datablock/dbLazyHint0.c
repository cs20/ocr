/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Create a lazy datablock and have it passed through a ring of PDs a number of times.
 */

#define NB_ELEMS 10
#define NB_ROUNDS 10

ocrGuid_t consEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t lazyDb = depv[0].guid;
    u64 myId = paramv[0];
    u64 expectedValue = paramv[1];
    u64 nbRoundsLeft = paramv[2];
    u64 maxId = paramv[3];

    // Check result
    u32 * dbPtr = depv[0].ptr;
    u32 i = 0;
    while (i < NB_ELEMS) {
        ocrAssert(dbPtr[i] == expectedValue);
        dbPtr[i]++;
        i++;
    }
    ocrPrintf("Execute on affinity=%"PRIu64" maxId=%"PRIu64"\n", myId, maxId);
    if (myId == 0) {
        if (nbRoundsLeft == 0) {
            ocrShutdown();
            return NULL_GUID;
        } else {
            nbRoundsLeft--;
        }
    }
    ocrDbRelease(lazyDb);

    u64 targetId = (myId == (maxId-1)) ? 0 : myId+1;
    ocrGuid_t targetAffinity;
    ocrAffinityGetAt(AFFINITY_PD, targetId, &targetAffinity);
    paramv[0] = targetId;
    paramv[1]++;
    paramv[2] = nbRoundsLeft;
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(targetAffinity));
    ocrGuid_t edtTpl;
    ocrEdtTemplateCreate(&edtTpl, consEdt, 4, 1);
    ocrGuid_t edt;
    ocrEdtCreate(&edt, edtTpl, paramc, paramv,
                 1, &lazyDb, EDT_PROP_NONE, &edtHint, NULL);
    ocrEdtTemplateDestroy(edtTpl);
    return NULL_GUID;

}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Create the lazy DB
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ocrAssert(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);

    ocrHint_t dbHint;
    ocrHintInit( &dbHint, OCR_HINT_DB_T );
    ocrSetHintValue(&dbHint, OCR_HINT_DB_LAZY, 1);
    ocrGuid_t lazyDb;
    u32 * dbPtr;
    ocrDbCreate(&lazyDb, (void **)&dbPtr, sizeof(u32)*NB_ELEMS, 0, &dbHint, NO_ALLOC);
    u32 i = 0;
    while (i < NB_ELEMS) {
        dbPtr[i] = 0;
        i++;
    }
    ocrDbRelease(lazyDb);
    u64 nparamv[4];
    nparamv[0] = 0;
    nparamv[1] = 0;
    nparamv[2] = NB_ROUNDS;
    nparamv[3] = affinityCount;
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affinities[0]));
    ocrGuid_t edtTpl;
    ocrEdtTemplateCreate(&edtTpl, consEdt, 4, 1);
    ocrGuid_t edt;
    ocrEdtCreate(&edt, edtTpl, 4, nparamv,
                 1, &lazyDb, EDT_PROP_NONE, &edtHint, NULL);
    ocrEdtTemplateDestroy(edtTpl);

    return NULL_GUID;
}
