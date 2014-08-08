/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <stdio.h>
#include <assert.h>

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - create a remote EDT with paramc
 */

ocrGuid_t remoteEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    assert(paramc == 2);
    assert(paramv[0] == 333);
    assert(paramv[1] == 555);
    printf("[remote] RemoteEdt: paramv checked\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    assert(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t edtAffinity = affinities[affinityCount-1]; //TODO this implies we know current PD is '0'

    ocrGuid_t remoteEdtTemplateGuid;
    ocrEdtTemplateCreate(&remoteEdtTemplateGuid, remoteEdt, 2, 0);
    ocrGuid_t edtGuid;
    u64 edtParamv[2] = {333,555};
    ocrEdtCreate(&edtGuid, remoteEdtTemplateGuid, 2, (u64*) &edtParamv, 0, NULL_GUID,
        EDT_PROP_NONE, edtAffinity, NULL);
    return NULL_GUID;
}
