/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Collective event overlapping all add then all satisfy
 */

#define TEST_MAXGEN 1

#include "testReductionEventBasic.ctpl"

#ifdef ENABLE_EXTENSION_COLLECTIVE_EVT

void test(ocrGuid_t evtGuid, ocrEventParams_t params, u64 rankId, u64 it, ocrGuid_t edtCont) {
    u32 g = 0;
    while(g < TEST_MAXGEN) {
        ocrAddDependenceSlot(evtGuid, rankId, edtCont, 0, DB_MODE_RO);
        g++;
    }
    u64 ptr = 1;
    g = 0;
    while(g < TEST_MAXGEN) {
        ocrEventCollectiveSatisfySlot(evtGuid, &ptr, rankId);
        g++;
    }
}

void testCheck(u32 it, ocrEventParams_t params, u32 valueCount, void ** values) {
    u32 i = 0;
    ASSERT(valueCount == TEST_MAXGEN); // got all contributions
    u32 res = (it+1)*params.EVENT_COLLECTIVE.nbContribs;
    while (i < TEST_MAXGEN) {
        PRINTF("it=%"PRIu32" ptr=%p reducedValue=%"PRIu64" expected=%"PRIu32"\n", it, values[i], ((u64*)values[i])[0], res);
        ASSERT(((u64*)values[i])[0] == res);
        i++;
    }
}

#endif
