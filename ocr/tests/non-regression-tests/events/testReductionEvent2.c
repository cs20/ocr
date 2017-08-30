/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Collective event overlapping add dependence phases
 */

#define TEST_MAXGEN 1

#include "testReductionEventBasic.ctpl"

#ifdef ENABLE_EXTENSION_COLLECTIVE_EVT

void test(ocrGuid_t evtGuid, ocrEventParams_t params, u64 rankId, u64 it, ocrGuid_t edtCont) {
    ocrGuid_t idemEvtGuid;
    ocrEventCreate(&idemEvtGuid, OCR_EVENT_IDEM_T, false);
    u32 g = 0;
    while(g < TEST_MAXGEN) {
        ocrAddDependenceSlot(evtGuid, rankId, idemEvtGuid, 0, DB_MODE_RO);
        // Directly satisfy since we do not use the collective event anyway
        ocrEventSatisfySlot(edtCont, NULL_GUID, g);
        g++;
    }
}

void testCheck(u32 it, ocrEventParams_t params, u32 valueCount, void ** values) {

}

#endif
