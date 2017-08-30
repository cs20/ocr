/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test user interface definitions
 */

#ifdef ENABLE_EXTENSION_COLLECTIVE_EVT

#include "extensions/ocr-labeling.h"

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrEventParams_t params;
    params.EVENT_COLLECTIVE.maxGen        = 1;
    params.EVENT_COLLECTIVE.nbContribs    = 8;
    params.EVENT_COLLECTIVE.nbContribsPd  = 8;
    params.EVENT_COLLECTIVE.nbDatum       = 1;
    params.EVENT_COLLECTIVE.arity         = 2;
    params.EVENT_COLLECTIVE.op            = REDOP_U8_ADD;
    params.EVENT_COLLECTIVE.type          = COL_ALLREDUCE;
    params.EVENT_COLLECTIVE.reuseDbPerGen = true;
    // Just to have a use
    PRINTF("maxGen=%d\n", (int) params.EVENT_COLLECTIVE.maxGen);
    ocrGuid_t rangeGuid;
    ocrGuidRangeCreate(&rangeGuid, 1, GUID_USER_EVENT_COLLECTIVE);
    ocrGuid_t evtGuid;
    ocrGuidFromIndex(&evtGuid, rangeGuid, 0);
    ocrEventCreateParams(&evtGuid, OCR_EVENT_COLLECTIVE_T, GUID_PROP_IS_LABELED, &params);
    ocrShutdown();
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_COLLECTIVE_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
