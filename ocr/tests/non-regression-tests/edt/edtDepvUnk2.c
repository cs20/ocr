/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"

/**
 * DESC: Test EDT_PARAM_UNK usage in EDT templates
 */

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Everything went OK\n");
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t e0;
    ocrEventCreate(&e0,OCR_EVENT_ONCE_T, EVT_PROP_NONE);
    ocrGuid_t terminateEdtGuid;
    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0 /*paramc*/, EDT_PARAM_UNK /*depc*/);
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid, EDT_PARAM_DEF, NULL, 1, NULL,
                 /*properties=*/EDT_PROP_FINISH, NULL_HINT, /*outEvent=*/ NULL);
    ocrAddDependence(e0, terminateEdtGuid, 0, DB_MODE_CONST);
    ocrEventSatisfy(e0, NULL_GUID);
    return NULL_GUID;
}
