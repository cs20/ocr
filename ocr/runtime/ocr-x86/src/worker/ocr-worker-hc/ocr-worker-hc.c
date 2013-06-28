/* Copyright (c) 2012, Rice University

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1.  Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   2.  Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
   3.  Neither the name of Intel Corporation
   nor the names of its contributors may be used to endorse or
   promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <pthread.h>
#include <stdio.h>

#include "debug.h"
#include "ocr-runtime.h"
#include "hc.h"
#include "ocr-guid.h"
#include "ocr-comp-platform.h"

/******************************************************/
/* OCR-HC WORKER                                      */
/******************************************************/

static void associate_comp_platform_and_worker(ocrPolicyDomain_t * policy, ocrWorker_t * worker) {
    // This function must only be used when the contextFactory has its PD set
    ocrPolicyCtx_t * ctx = policy->contextFactory->instantiate(policy->contextFactory);
    ctx->sourceObj = worker->guid;
    ctx->sourceId = ((ocrWorkerHc_t *) worker)->id;
    setCurrentPD(policy);
    setCurrentWorkerContext(ctx);
}

void destructWorkerHc ( ocrWorker_t * base ) {
    ocrGuidProvider_t * guidProvider = getCurrentPD()->guidProvider;
    guidProvider->fctPtrs->releaseGuid(guidProvider, base->guid);
    free(base);
}

/**
 * The computation worker routine that asks work to the scheduler
 */
void * worker_computation_routine(void * arg);

void hcStartWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    hcWorker->run = true;
    if (hcWorker->id != 0) {
      u64 computeCount = base->computeCount;
      // What the compute target will execute
      launchArg_t launchArg;
      launchArg.routine = worker_computation_routine;
      launchArg.arg = base;
      launchArg.PD = policy;
      u64 i = 0;
      for(i = 0; i < computeCount; i++) {
        base->computes[i]->fctPtrs->start(base->computes[i], &launchArg);
      }
    } else {
      // Worker zero doesn't start the underlying thread since it is
      // falling through after that start. However, it stills need 
      // to set its local storage data.
      associate_comp_platform_and_worker(policy, base);
    }
}

void hcStopWorker(ocrWorker_t * base) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    hcWorker->run = false;
    log_worker(INFO, "Stopping worker %d\n", hcWorker->id);
}

bool hc_is_running_worker(ocrWorker_t * base) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    return hcWorker->run;
}

ocrGuid_t hc_getCurrentEDT (ocrWorker_t * base) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    return hcWorker->currentEDT_guid;
}

void hc_setCurrentEDT (ocrWorker_t * base, ocrGuid_t curr_edt_guid) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    hcWorker->currentEDT_guid = curr_edt_guid;
}

/**
 * Builds an instance of a HC worker
 */
ocrWorker_t* newWorkerHc (ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorkerHc_t * worker = checkedMalloc(worker, sizeof(ocrWorkerHc_t));
    worker->run = false;
    worker->id = ((paramListWorkerHcInst_t*)perInstance)->workerId;
    worker->currentEDT_guid = NULL_GUID;

    ocrWorker_t * base = (ocrWorker_t *) worker;
    base->guid = UNINITIALIZED_GUID;
    guidify(getCurrentPD(), (u64)base, &(base->guid), OCR_GUID_WORKER);
    base->routine = worker_computation_routine;
    base->fctPtrs = &(factory->workerFcts);
    return base;
}

// Convenient to have an id to index workers in pools
int get_worker_id(ocrWorker_t * worker) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    return hcWorker->id;
}

void * worker_computation_routine(void * arg) {
    // Need to pass down a data-structure
    launchArg_t * launchArg = (launchArg_t *) arg;
    ocrPolicyDomain_t * pd = launchArg->PD;
    ocrWorker_t * worker = (ocrWorker_t *) launchArg->arg;
    // associate current thread with the worker
    associate_comp_platform_and_worker(pd, worker);
    // Setting up this worker context to takeEdts
    // This assumes workers are not relocatable
    log_worker(INFO, "Starting scheduler routine of worker %d\n", get_worker_id(worker));
    ocrPolicyCtx_t * contextTake = pd->contextFactory->instantiate(pd->contextFactory);
    contextTake->type = PD_MSG_EDT_TAKE;
    // Entering the worker loop
    while(worker->fctPtrs->isRunning(worker)) {
        ocrGuid_t taskGuid;
        u32 count;
        pd->takeEdt(pd, NULL, &count, &taskGuid, contextTake);
        // remove this when we can take a bunch and make sure there's
        // an agreement whether it's the callee or the caller that 
        // allocates the taskGuid array 
        ASSERT(count <= 1);
        if (count != 0) {
            ocrTask_t* curr_task = NULL;
            deguidify(pd, taskGuid, (u64*)&(curr_task), NULL);
            worker->fctPtrs->setCurrentEDT(worker,taskGuid);
            curr_task->fctPtrs->execute(curr_task);
            worker->fctPtrs->setCurrentEDT(worker, NULL_GUID);
            curr_task->fctPtrs->destruct(curr_task);
        }
    }
    return NULL;
}


/******************************************************/
/* OCR-HC WORKER FACTORY                              */
/******************************************************/

void destructWorkerFactoryHc(ocrWorkerFactory_t * factory) {
    free(factory);
}

ocrWorkerFactory_t * newOcrWorkerFactoryHc(ocrParamList_t * perType) {
    ocrWorkerFactoryHc_t* derived = (ocrWorkerFactoryHc_t*) checkedMalloc(derived, sizeof(ocrWorkerFactoryHc_t));
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*) derived;
    base->instantiate = newWorkerHc;
    base->destruct =  destructWorkerFactoryHc;
    base->workerFcts.destruct = destructWorkerHc;
    base->workerFcts.start = hcStartWorker;
    base->workerFcts.stop = hcStopWorker;
    base->workerFcts.isRunning = hc_is_running_worker;
    base->workerFcts.getCurrentEDT = hc_getCurrentEDT;
    base->workerFcts.setCurrentEDT = hc_setCurrentEDT;
    return base;
}
