/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Collective event consecutive phases back to back going over maxGen
 */

#define TEST_MAXGEN 1

#include "testReductionEventBasic.ctpl"

#ifdef ENABLE_EXTENSION_COLLECTIVE_EVT

void test(ocrGuid_t evtGuid, ocrEventParams_t params, u64 rankId, u64 it, ocrGuid_t edtCont) {
    u32 g = 0;
    u64 ptr = 1;
    while(g < (TEST_MAXGEN)) {
        ocrAddDependenceSlot(evtGuid, rankId, edtCont, 0, DB_MODE_RO);
        ocrEventCollectiveSatisfySlot(evtGuid, &ptr, rankId);
        g++;
    }
}

void testCheck(u32 it, ocrEventParams_t params, u32 valueCount, void ** values) {

}

#endif


