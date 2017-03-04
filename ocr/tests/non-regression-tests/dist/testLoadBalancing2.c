/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Recursive divide-and-conqueue with nested finish scope.
 *       - Caused a race when registering a child on its parent
 *         scope and EDT load-balancing happens in the meanwhile.
 */

#define ARRAY_SIZE 1000

typedef struct {
    u64 low;
    u64 high;
    ocrGuid_t workTemplate;
} workPRM_t;

ocrGuid_t workTask( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    workPRM_t *workParamvIn = (workPRM_t *)paramv;
    u64 low = workParamvIn->low;
    u64 high = workParamvIn->high;
    ocrGuid_t workTemplate = workParamvIn->workTemplate;
    u64 size = high - low;
    if(!(size < 8)) {
        u64 pivotIndex = low + ((high-low)/2);
        ocrGuid_t lowEdt, highEdt;
        workPRM_t workParamv;
        workParamv.low = low;
        workParamv.high = pivotIndex;
        PRINTF("Recurse1 on %d %d\n", (int) workParamv.low, (int) workParamv.high);
        workParamv.workTemplate = workTemplate;
        ocrEdtCreate(&lowEdt, workTemplate, EDT_PARAM_DEF, (u64 *)&workParamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_FINISH, NULL_HINT, NULL);
        workParamv.low = pivotIndex+1;
        workParamv.high = high;
        PRINTF("Recurse2 on %d %d\n", (int) workParamv.low, (int) workParamv.high);
        ocrEdtCreate(&highEdt, workTemplate, EDT_PARAM_DEF, (u64 *)&workParamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_FINISH, NULL_HINT, NULL);
    }
    return NULL_GUID;
}

ocrGuid_t finishTask( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t workTemplate;
    ocrGuid_t workEdt;
    ocrGuid_t outEvt;
    workPRM_t workParamv;
    ocrEdtTemplateCreate(&workTemplate, workTask, 3, 1);
    ocrGuid_t workParamvChild;
    ocrEdtTemplateCreate(&workParamvChild, workTask, 3, 0);
    workParamv.low = 0;
    workParamv.high = ARRAY_SIZE;
    workParamv.workTemplate = workParamvChild;
    ocrEdtCreate(&workEdt, workTemplate, EDT_PARAM_DEF, (u64 *)&workParamv,
        EDT_PARAM_DEF, NULL, EDT_PROP_FINISH, NULL_HINT, &outEvt);
    ocrGuid_t coordEvt;
    ocrEventCreate(&coordEvt, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG);
    ocrAddDependence(outEvt, coordEvt, 0, DB_MODE_RO);
    ocrAddDependence(NULL_GUID, workEdt, 0, DB_MODE_RO);
    ocrGuid_t finishTemplate;
    ocrGuid_t finishEdt;
    ocrEdtTemplateCreate(&finishTemplate, finishTask, 0, 1);
    ocrEdtCreate(&finishEdt, finishTemplate, EDT_PARAM_DEF, NULL,
        EDT_PARAM_DEF, &coordEvt, 0, NULL_HINT, NULL);
    return NULL_GUID;
}
