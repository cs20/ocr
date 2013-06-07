/**
 * @brief Data-block implementation for OCR
 * @authors Romain Cledat, Intel Corporation
 * @date 2012-11-09
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of Intel Corporation nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ocr-db.h"
#include "ocr-datablock.h"
#include "ocr-allocator.h"
#include "ocr-worker.h"
#include "ocr-policy-domain.h"
#include "debug.h"
#include "ocr-guid.h"
#include <errno.h>
#if (__STDC_HOSTED__ == 1)
#include <string.h>
#endif

#include "ocr.h"
#include "ocr-runtime.h"


#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-stat-user.h"
#include "ocr-config.h"
#endif


u8 ocrDbCreate(ocrGuid_t *db, void** addr, u64 len, u16 flags,
               ocrLocation_t *location, ocrInDbAllocator_t allocator) {

    // TODO: Currently location and allocator are ignored
    // ocrDataBlock_t *createdDb = newDataBlock(OCR_DATABLOCK_DEFAULT);
    ocrDataBlock_t *createdDb = newDataBlock(OCR_DATABLOCK_PLACED);

#ifdef OCR_ENABLE_STATISTICS
    // Create the statistics process for this DB.
    ocrStatsProcessCreate(&(createdDb->statProcess), createdDb->guid);
    ocrStatsFilter_t *t = NEW_FILTER(simple);
    t->create(t, GocrFilterAggregator, NULL);
    ocrStatsProcessRegisterFilter(&(createdDb->statProcess), (0x3F<<((u32)STATS_DB_CREATE-1)), t);
#endif

    // TODO: I need to get the current policy to figure out my allocator.
    // Replace with allocator that is gotten from policy

    ocrGuid_t worker_guid = ocr_get_current_worker_guid();
    ocrWorker_t * worker = NULL;
    globalGuidProvider->getVal(globalGuidProvider, worker_guid, (u64*)&worker, NULL);

    ocrScheduler_t * scheduler = get_worker_scheduler(worker);
    ocrPolicyDomain_t* policy = scheduler -> domain;

    createdDb->create(createdDb, policy->getAllocator(policy, location), len, flags, NULL);
#ifdef OCR_ENABLE_STATISTICS
    {
        ocr_task_t *task = NULL;
        ocrGuid_t edtGuid = worker->getCurrentEDT(worker);
        globalGuidProvider->getVal(globalGuidProvider, edtGuid, (u64*)&task, NULL);
        ocrStatsProcess_t *srcProcess = edtGuid==0?&GfakeProcess:&(task->statProcess);

        ocrStatsMessage_t *mess = NEW_MESSAGE(simple);
        mess->create(mess, STATS_DB_CREATE, 0, edtGuid, createdDb->guid, NULL);
        ocrStatsAsyncMessage(srcProcess, &(createdDb->statProcess), mess);

        // Acquire part
        *addr = createdDb->acquire(createdDb, edtGuid, false);
        ocrStatsMessage_t *mess2 = NEW_MESSAGE(simple);
        mess2->create(mess2, STATS_DB_ACQ, 0, edtGuid, createdDb->guid, NULL);
        ocrStatsSyncMessage(srcProcess, &(createdDb->statProcess), mess2);
    }
#else
    *addr = createdDb->acquire(createdDb, worker->getCurrentEDT(worker), false);
#endif
    if(*addr == NULL) return ENOMEM;

    *db = createdDb->guid;
    return 0;
}

u8 ocrDbDestroy(ocrGuid_t db) {
    ocrDataBlock_t *dataBlock = NULL;
    globalGuidProvider->getVal(globalGuidProvider, db, (u64*)&dataBlock, NULL);

    ocrGuid_t workerGuid = ocr_get_current_worker_guid();
    ocrWorker_t *worker = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&worker, NULL);

#ifdef OCR_ENABLE_STATISTICS
    {
        ocr_task_t *task = NULL;
        ocrDataBlock_t *dataBlock = NULL;
        ocrGuid_t edtGuid = worker->getCurrentEDT(worker);
        globalGuidProvider->getVal(globalGuidProvider, edtGuid, (u64*)&task, NULL);
        globalGuidProvider->getVal(globalGuidProvider, db, (u64*)&dataBlock, NULL);

        ocrStatsProcess_t *srcProcess = edtGuid==0?&GfakeProcess:&(task->statProcess);

        ocrStatsMessage_t *mess = NEW_MESSAGE(simple);
        mess->create(mess, STATS_DB_DESTROY, 0, edtGuid, db, NULL);
        ocrStatsAsyncMessage(srcProcess, &(dataBlock->statProcess), mess);
    }
#endif
    // Make sure you do the free *AFTER* sending the message because the free could
    // destroy the datablock (and the stat process).
    u8 status = dataBlock->free(dataBlock, worker->getCurrentEDT(worker));
    return status;
}

u8 ocrDbAcquire(ocrGuid_t db, void** addr, u16 flags) {
    ocrDataBlock_t *dataBlock = NULL;
    globalGuidProvider->getVal(globalGuidProvider, db, (u64*)&dataBlock, NULL);

    ocrGuid_t workerGuid = ocr_get_current_worker_guid();
    ocrWorker_t *worker = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&worker, NULL);

    *addr = dataBlock->acquire(dataBlock, worker->getCurrentEDT(worker), false);
#ifdef OCR_ENABLE_STATISTICS
    {
        ocr_task_t *task = NULL;
        ocrGuid_t edtGuid = worker->getCurrentEDT(worker);
        globalGuidProvider->getVal(globalGuidProvider, edtGuid, (u64*)&task, NULL);

        ocrStatsProcess_t *srcProcess = edtGuid==0?&GfakeProcess:&(task->statProcess);

        ocrStatsMessage_t *mess = NEW_MESSAGE(simple);
        mess->create(mess, STATS_DB_ACQ, 0, edtGuid, db, NULL);
        ocrStatsSyncMessage(srcProcess, &(dataBlock->statProcess), mess);
    }
#endif
    if(*addr == NULL) return EPERM;
    return 0;
}

u8 ocrDbRelease(ocrGuid_t db) {
    ocrDataBlock_t *dataBlock = NULL;
    globalGuidProvider->getVal(globalGuidProvider, db, (u64*)&dataBlock, NULL);

    ocrGuid_t workerGuid = ocr_get_current_worker_guid();
    ocrWorker_t *worker = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&worker, NULL);
#ifdef OCR_ENABLE_STATISTICS
    {
        ocr_task_t *task = NULL;
        ocrGuid_t edtGuid = worker->getCurrentEDT(worker);

        u8 result = dataBlock->release(dataBlock, edtGuid, false);

        globalGuidProvider->getVal(globalGuidProvider, edtGuid, (u64*)&task, NULL);

        ocrStatsProcess_t *srcProcess = edtGuid==0?&GfakeProcess:&(task->statProcess);

        ocrStatsMessage_t *mess = NEW_MESSAGE(simple);
        mess->create(mess, STATS_DB_REL, 0, edtGuid, db, NULL);
        ocrStatsAsyncMessage(srcProcess, &(dataBlock->statProcess), mess);

        return result;
    }
#else
    return dataBlock->release(dataBlock, worker->getCurrentEDT(worker), false);
#endif
}

u8 ocrDbMalloc(ocrGuid_t guid, u64 size, void** addr) {
    return EINVAL; /* not yet implemented */
}

u8 ocrDbMallocOffset(ocrGuid_t guid, u64 size, u64* offset) {
    return EINVAL; /* not yet implemented */
}

struct ocrDbCopy_args {
        ocrGuid_t destination;
        u64 destinationOffset;
        ocrGuid_t source;
        u64 sourceOffset;
        u64 size;
} ocrDbCopy_args;

ocrGuid_t ocrDbCopy_edt ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
        char *sptr, *dptr;

        struct ocrDbCopy_args * pv = (struct ocrDbCopy_args *) depv[0].ptr;
        ocrGuid_t destination = pv->destination;
        u64 destinationOffset = pv->destinationOffset;
        ocrGuid_t source = pv->source;
        u64 sourceOffset = pv->sourceOffset;
        u64 size = pv->size;

        ocrDbAcquire(source, (void *) &sptr, 0);
        ocrDbAcquire(destination, (void *) &dptr, 0);

        sptr += sourceOffset;
        dptr += destinationOffset;

#if (__STDC_HOSTED__ == 1)
        memcpy((void *)dptr, (const void *)sptr, size);
#else
        int i;
        for (i = 0; i < size; i++) {
                dptr[i] = sptr[i];
        }
#endif

        ocrDbRelease(source);
        ocrDbRelease(destination);

    ocrGuid_t param_db_guid = (ocrGuid_t)depv[0].guid;
        ocrDbDestroy(param_db_guid);

    return NULL_GUID;
}

u8 ocrDbCopy(ocrGuid_t destination,u64 destinationOffset, ocrGuid_t source, u64 sourceOffset, u64 size, u64 copyType, ocrGuid_t * completionEvt) {
    // Create the event
        ocrGuid_t event_guid;
    ocrEventCreate(&event_guid, OCR_EVENT_STICKY_T, true); /*TODO: Replace with ONCE after that is supported */

    // Create the EDT
    ocrGuid_t edt_guid;
    ocrEdtCreate(&edt_guid, ocrDbCopy_edt, 0, NULL, NULL, 0, 1, &event_guid, completionEvt);
        ocrEdtSchedule(edt_guid);

        // Create the copy params
        ocrGuid_t param_db_guid;
    struct ocrDbCopy_args * db_args = NULL;
    void * ptr = (void *) db_args;
    // Warning: directly casting db_args to (void **) causes a type-punning warning with gcc-4.1.2
    ocrDbCreate(&param_db_guid, &ptr, sizeof(ocrDbCopy_args), 0xdead, NULL, NO_ALLOC);

        db_args->destination = destination;
        db_args->destinationOffset = destinationOffset;
        db_args->source = source;
        db_args->sourceOffset = sourceOffset;
        db_args->size = size;

        ocrEventSatisfy(event_guid, param_db_guid);

        /* ocrDbRelease(param_db_guid); TODO: BUG: Release tries to free */

        return 0;
}

u8 ocrDbFree(ocrGuid_t guid, void* addr) {
    return EINVAL; /* not yet implemented */
}

u8 ocrDbFreeOffset(ocrGuid_t guid, u64 offset) {
    return EINVAL; /* not yet implemented */
}
