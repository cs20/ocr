/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Collective event overlapping all add then TEST_MAXGEN-PHASE_SPLIT satisfy
 *       do PHASE_SPLIT add
 *
 */

#define TEST_MAXGEN 1
#define PHASE_SPLIT 1

#include "testReductionEventBasic.ctpl"

#ifdef ENABLE_EXTENSION_COLLECTIVE_EVT

void test(ocrGuid_t evtGuid, ocrEventParams_t params, u64 rankId, u64 it, ocrGuid_t edtCont) {
    u32 g = 0;
    while(g < TEST_MAXGEN) {
        ocrAddDependenceSlot(evtGuid, rankId, edtCont, 0, DB_MODE_RO);
        g++;
    }
    // A B C
    u64 ptr = 0;
    g = 0;
    // Do not satisfy all phases
    while(g < (TEST_MAXGEN-PHASE_SPLIT)) {
        ptr++;
        ocrEventCollectiveSatisfySlot(evtGuid, &ptr, rankId);
        g++;
    }
    //X X C
    g = 0;
    // Re do add dependence for logical phase TEST_MAXGEN+1
    while((TEST_MAXGEN != 1) && (g < PHASE_SPLIT)) {
        ocrAddDependenceSlot(evtGuid, rankId, edtCont, 0, DB_MODE_RO);
        g++;
    }
    //D X C
    g = 0;
    // Satisfy the older phases plus the newly introduced
    u32 ub = (TEST_MAXGEN != 1) ? (PHASE_SPLIT*2) : TEST_MAXGEN;
    while(g < ub) {
        ptr++;
        ocrEventCollectiveSatisfySlot(evtGuid, &ptr, rankId);
        g++;
    }
    //X X X
}

void testCheck(u32 it, ocrEventParams_t params, u32 valueCount, void ** values) {

}

#endif


