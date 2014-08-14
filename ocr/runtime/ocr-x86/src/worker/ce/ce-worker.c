/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_CE

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-errors.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "worker/ce/ce-worker.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-CE WORKER                                      */
/******************************************************/

// Convenient to have an id to index workers in pools
static inline u64 getWorkerId(ocrWorker_t * worker) {
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) worker;
    return ceWorker->id;
}

/**
 * The computation worker routine that asks for work to the scheduler
 */
static void workerLoop(ocrWorker_t * worker) {
    ocrPolicyDomain_t *pd = worker->pd;

    DPRINTF(DEBUG_LVL_VERB, "Starting scheduler routine of CE worker %ld\n", getWorkerId(worker));
    while(worker->fcts.isRunning(worker)) {
        ocrMsgHandle_t *handle = NULL;
        RESULT_ASSERT(pd->fcts.waitMessage(pd, &handle), ==, 0);
        ASSERT(handle);
        ocrPolicyMsg_t *msg = handle->response;
        RESULT_ASSERT(pd->fcts.processMessage(pd, msg, true), ==, 0);
        handle->destruct(handle);
    } /* End of while loop */
}

void destructWorkerCe(ocrWorker_t * base) {
    u64 i = 0;
    while(i < base->computeCount) {
        base->computes[i]->fcts.destruct(base->computes[i]);
        ++i;
    }
    runtimeChunkFree((u64)(base->computes), NULL);
    runtimeChunkFree((u64)base, NULL);
}

/**
 * Builds an instance of a CE worker
 */
ocrWorker_t* newWorkerCe (ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * base = (ocrWorker_t*)runtimeChunkAlloc(sizeof(ocrWorkerCe_t), NULL);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeWorkerCe(ocrWorkerFactory_t * factory, ocrWorker_t* base, ocrParamList_t * perInstance) {
    initializeWorkerOcr(factory, base, perInstance);
    base->type = ((paramListWorkerCeInst_t*)perInstance)->workerType;
    ASSERT(base->type == MASTER_WORKERTYPE);

    ocrWorkerCe_t * workerCe = (ocrWorkerCe_t *) base;
    workerCe->id = ((paramListWorkerCeInst_t*)perInstance)->workerId;
    workerCe->running = false;
    workerCe->secondStart = false;
}

void ceBeginWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {

    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;

    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; ++i) {
        base->computes[i]->fcts.begin(base->computes[i], policy, base->type);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
        base->computes[i]->fcts.setCurrentEnv(base->computes[i], policy, base);
    }
}

void ceStartWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {

    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;

    DPRINTF(DEBUG_LVL_VVERB, "Starting worker\n");
#ifdef ENABLE_COMP_PLATFORM_PTHREAD  // Sanjay, the below should go away
    // since it's not applicable to FSim
    if(!ceWorker->secondStart) {
        ceWorker->secondStart = true;
        return; // Don't start right away
    }
#endif

    base->location = policy->myLocation;
    // Get a GUID
    guidify(policy, (u64)base, &(base->fguid), OCR_GUID_WORKER);
    base->pd = policy;
    ASSERT(base->type == MASTER_WORKERTYPE);

    ceWorker->running = true;

    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;
    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; i++) {
        base->computes[i]->fcts.start(base->computes[i], policy, base);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
    }
}

void* ceRunWorker(ocrWorker_t * worker) {
    // Need to pass down a data-structure
    ocrPolicyDomain_t *pd = worker->pd;

    // Set who we are
    u32 i;
    for(i = 0; i < worker->computeCount; ++i) {
        worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);
    }

    workerLoop(worker);
    return NULL;
}

void* ceWorkShift(ocrWorker_t * worker) {
    ASSERT(0); // Not supported
    return NULL;
}

void ceFinishWorker(ocrWorker_t * base) {
    DPRINTF(DEBUG_LVL_INFO, "Finishing worker routine %ld\n", getWorkerId(base));
    ASSERT(base->computeCount == 1);
    u64 i = 0;
    for(i = 0; i < base->computeCount; i++) {
        base->computes[i]->fcts.finish(base->computes[i]);
    }
}

void ceStopWorker(ocrWorker_t * base) {
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;
    ceWorker->running = false;

    u64 computeCount = base->computeCount;
    u64 i = 0;
    // This code should be called by the master thread to join others
    for(i = 0; i < computeCount; i++) {
        base->computes[i]->fcts.stop(base->computes[i]);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_STOP(base->pd, base->fguid.guid, base->fguid.metaDataPtr,
                         base->computes[i]->fguid.guid,
                         base->computes[i]->fguid.metaDataPtr);
#endif
    }
    DPRINTF(DEBUG_LVL_INFO, "Stopping worker %ld\n", getWorkerId(base));

    // Destroy the GUID
    ocrPolicyMsg_t msg;
    getCurrentEnv(NULL, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid) = base->fguid;
    PD_MSG_FIELD(properties) = 0;
    RESULT_ASSERT(base->pd->fcts.processMessage(base->pd, &msg, false), ==, 0);
#undef PD_MSG
#undef PD_TYPE
    base->fguid.guid = UNINITIALIZED_GUID;
}

bool ceIsRunningWorker(ocrWorker_t * base) {
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;
    return ceWorker->running;
}

/******************************************************/
/* OCR-CE WORKER FACTORY                              */
/******************************************************/

void destructWorkerFactoryCe(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryCe(ocrParamList_t * perType) {
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryCe_t), (void *)1);

    base->instantiate = &newWorkerCe;
    base->initialize = &initializeWorkerCe;
    base->destruct = &destructWorkerFactoryCe;

    base->workerFcts.destruct = FUNC_ADDR(void (*)(ocrWorker_t*), destructWorkerCe);
    base->workerFcts.begin = FUNC_ADDR(void (*)(ocrWorker_t*, ocrPolicyDomain_t*), ceBeginWorker);
    base->workerFcts.start = FUNC_ADDR(void (*)(ocrWorker_t*, ocrPolicyDomain_t*), ceStartWorker);
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), ceRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*)(ocrWorker_t*), ceWorkShift);
    base->workerFcts.stop = FUNC_ADDR(void (*)(ocrWorker_t*), ceStopWorker);
    base->workerFcts.finish = FUNC_ADDR(void (*)(ocrWorker_t*), ceFinishWorker);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*)(ocrWorker_t*), ceIsRunningWorker);
    return base;
}

#endif /* ENABLE_WORKER_CE */
