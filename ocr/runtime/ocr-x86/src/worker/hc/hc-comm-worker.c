/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_HC_COMM

#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-db.h"
#include "ocr-worker.h"
#include "worker/hc/hc-worker.h"
#include "worker/hc/hc-comm-worker.h"

#include <stdio.h>

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-HC COMMUNICATION WORKER                        */
/* Extends regular HC workers                         */
/******************************************************/

//DIST-TODO: temporary, this is only to create the template and edt.
#include "ocr-edt.h"

ocrGuid_t processRequestEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrPolicyMsg_t * requestMsg = (ocrPolicyMsg_t *) paramv[0];
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    // This is only meant to execute incoming request, not processing responses.
    // Responses are routed back to requesters by the scheduler and are processed by them.
    ASSERT(requestMsg->type & PD_MSG_REQUEST);
    // Important to read this before calling processMessage. If the request requires
    // a response, the runtime reuses the request's message to post the response.
    // Hence there's a race between this code and the code posting the response.
    bool toBeFreed = !(requestMsg->type & PD_MSG_REQ_RESPONSE);
    DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: Process incoming EDT request of type %p %x\n", (int)pd->myLocation, requestMsg, requestMsg->type);
    pd->fcts.processMessage(pd, requestMsg, true);
    DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: [done] Process incoming EDT request %p %x\n", (int)pd->myLocation, requestMsg, requestMsg->type);
    if (toBeFreed) {
        // Makes sure the runtime doesn't try to reuse this message
        // even though it was not supposed to issue a response.
        // If that's the case, this check is racy
        ASSERT(!(requestMsg->type & PD_MSG_RESPONSE));
        DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: Deleted incoming EDT request %p %x\n", (int)pd->myLocation, requestMsg, requestMsg->type);
        // if request was an incoming one-way we can delete the message now.
        pd->fcts.pdFree(pd, requestMsg);
    }

    return NULL_GUID;
}

static u8 takeFromSchedulerAndSend(ocrPolicyDomain_t * pd) {
    // When the communication-worker is not stopping only a single iteration is
    // executed. Otherwise it is executed until the scheduler's 'take' do not
    // return any more work.
    ocrMsgHandle_t * outgoingHandle = NULL;
    ocrPolicyMsg_t msgCommTake;
    u8 ret = 0;
    getCurrentEnv(NULL, NULL, NULL, &msgCommTake);
    ocrFatGuid_t handlerGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
    //IMPL: MSG_COMM_TAKE implementation must be consistent across PD, Scheduler and Worker.
    // We expect the PD to fill-in the guids pointer with an ocrMsgHandle_t pointer
    // to be processed by the communication worker or NULL.
    //PERF: could request 'n' for internal comm load balancing (outgoing vs pending vs incoming).
    #define PD_MSG (&msgCommTake)
    #define PD_TYPE PD_MSG_COMM_TAKE
    msgCommTake.type = PD_MSG_COMM_TAKE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guids) = &handlerGuid;
    PD_MSG_FIELD(extra) = 0; /*unused*/
    PD_MSG_FIELD(guidCount) = 1;
    PD_MSG_FIELD(properties) = 0;
    PD_MSG_FIELD(type) = OCR_GUID_COMM; /*unused*/
    ret = pd->fcts.processMessage(pd, &msgCommTake, true);
    if (!ret && (PD_MSG_FIELD(guidCount) != 0)) {
        ASSERT(PD_MSG_FIELD(guidCount) == 1); //LIMITATION: single guid returned by comm take
        ocrFatGuid_t handlerGuid = PD_MSG_FIELD(guids[0]);
        ASSERT(handlerGuid.metaDataPtr != NULL);
        outgoingHandle = (ocrMsgHandle_t *) handlerGuid.metaDataPtr;
    #undef PD_MSG
    #undef PD_TYPE
        if (outgoingHandle != NULL) {
            // This code handles the pd's outgoing messages. They can be requests or responses.
            DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: outgoing handle comm take successful handle=%p, msg=%p\n", (int) pd->myLocation,
                outgoingHandle, outgoingHandle->msg);
            //We can never have an outgoing handle with the response ptr set because
            //when we process an incoming request, we lose the handle by calling the
            //pd's process message. Hence, a new handle is created for one-way response.
            ASSERT(outgoingHandle->response == NULL);
            //DIST-TODO: IMPL: need to know the properties: can recover TWOWAY info by re-inspecting the
            //message. Also at this stage, message must be PERSIST since there already is a
            //decoupling between caller and callee. This would better be stored in the handle I think
            u32 properties = PERSIST_MSG_PROP | ((outgoingHandle->msg->type & PD_MSG_REQ_RESPONSE) ? (TWOWAY_MSG_PROP) : 0);
            //DIST-TODO: Not sure where to draw the line between one-way with/out ack implementation
            //If the worker was not aware of the no-ack policy, is it ok to always give a handle
            //and the comm-api contract is to at least set the HDL_SEND_OK flag ?
            ocrMsgHandle_t ** sendHandle = (properties & TWOWAY_MSG_PROP) ? &outgoingHandle : NULL;

            //DIST-TODO, who's responsible for deallocating the handle ?
            //If the message is two-way, the creator of the handle is responsible for deallocation
            //If one-way, the comm-layer disposes of the handle when it is not needed anymore
            //=> Sounds like if an ack is expected, caller is responsible for dealloc, else callee
            pd->fcts.sendMessage(pd, outgoingHandle->msg->destLocation, outgoingHandle->msg, sendHandle, properties);
            //Communication is posted. If TWOWAY, subsequent calls to poll may return the response
            //to be processed
            return POLL_MORE_MESSAGE;
        }
    }
    return POLL_NO_MESSAGE;
}

static void workerLoopHcComm_RL2(ocrWorker_t * worker) {
    ocrWorkerHcComm_t * self = (ocrWorkerHcComm_t *) worker;
    ocrPolicyDomain_t *pd = worker->pd;

    // Take and send all the work
    while(takeFromSchedulerAndSend(pd) == POLL_MORE_MESSAGE);

    // Poll for completion of all outstanding
    ocrMsgHandle_t * handle = NULL;
    pd->fcts.pollMessage(pd, &handle);
    // DIST-TODO: this is borderline, because we've transitioned
    // the comm-platform to RL2, we assume that poll blocks until
    // all messages have been processed.
    self->rl_completed[2] = true;
}

static void workerLoopHcComm_RL3(ocrWorker_t * worker) {
    ocrWorkerHcComm_t * self = (ocrWorkerHcComm_t *) worker;
    ocrPolicyDomain_t *pd = worker->pd;

    ocrGuid_t processRequestTemplate;
    ocrEdtTemplateCreate(&processRequestTemplate, &processRequestEdt, 1, 0);

    // This loop exits on the first call to stop.
    while(!self->rl_completed[3]) {
        // First, Ask the scheduler if there are any communication
        // to be scheduled and sent them if any.
        takeFromSchedulerAndSend(pd);

        ocrMsgHandle_t * handle = NULL;
        u8 ret = pd->fcts.pollMessage(pd, &handle);
        if (ret == POLL_MORE_MESSAGE) {
            //IMPL: for now only support successful polls on incoming request and responses
            ASSERT((handle->status == HDL_RESPONSE_OK)||(handle->status == HDL_NORMAL));
            ocrPolicyMsg_t * message = (handle->status == HDL_RESPONSE_OK) ? handle->response : handle->msg;
            //To catch misuses, assert src is not self and dst is self
            ASSERT((message->srcLocation != pd->myLocation) && (message->destLocation == pd->myLocation));
            if (message->type & PD_MSG_REQUEST) {
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: Received message request, msgId: %ld\n", (int) pd->myLocation, message->msgId);
                // This is an outstanding request, delegate to PD for processing
                ocrGuid_t processRequestGuid;
                u64 msgParamv = (u64) message;
                ocrEdtCreate(&processRequestGuid, processRequestTemplate, 1, &msgParamv, 0, NULL, 0, NULL_GUID, NULL_GUID);
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: Created processRequest EDT with guid %ld\n", (int)pd->myLocation, processRequestGuid);
                // We do not need the handle anymore
                handle->destruct(handle);
                //DIST-TODO-3: depending on comm-worker implementation, the received
                //message could then be 'wrapped' in an EDT and pushed to the
                //deque for load-balancing purpose.
            } else {
                // Poll a response to a message we had sent.
                ASSERT(message->type & PD_MSG_RESPONSE);
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: Received message response for msgId: %ld\n", (int) pd->myLocation, message->msgId); // debug
                //DIST-TODO: If it's a response, how do know if a worker is blocking on the message
                //      or if we need to spawn a new task (i.e. two-way but asynchronous handling of response)
                // => Sounds this should be specified in the handler

                // Give the answer back to the PD
                ocrFatGuid_t fatGuid;
                fatGuid.metaDataPtr = handle;
                ocrPolicyMsg_t giveMsg;
                getCurrentEnv(NULL, NULL, NULL, &giveMsg);
            #define PD_MSG (&giveMsg)
            #define PD_TYPE PD_MSG_COMM_GIVE
                giveMsg.type = PD_MSG_COMM_GIVE | PD_MSG_REQUEST;
                PD_MSG_FIELD(guids) = &fatGuid;
                PD_MSG_FIELD(guidCount) = 1;
                PD_MSG_FIELD(properties) = 0;
                PD_MSG_FIELD(type) = OCR_GUID_COMM;
                ret = pd->fcts.processMessage(pd, &giveMsg, false);
                ASSERT(ret == 0);
            #undef PD_MSG
            #undef PD_TYPE
                //For now, assumes all the responses are for workers that are
                //waiting on the response handler provided by sendMessage, reusing
                //the request msg as an input buffer for the response.
            }
        } else {
            //DIST-TODO-1 No messages ready for processing, ask PD for EDT work.
        }
    } // run-loop
}

static void workerLoopHcComm(ocrWorker_t * worker) {
    ocrWorkerHcComm_t * self = (ocrWorkerHcComm_t *) worker;

    // RL3: take, send / poll, dispatch, execute
    workerLoopHcComm_RL3(worker);
    ASSERT(self->rl_completed[3]);
    self->rl_completed[3] = false;
    while(self->rl == 3); // transitioned by 'stop'

    // RL2: Empty the scheduler of all communication work,
    // then keep polling so that the comm-api can drain messages
    workerLoopHcComm_RL2(worker);
    while(self->rl == 2); // transitioned by 'stop'

    // RL1: Wait for PD stop.
    while(worker->fcts.isRunning(worker));
}

//DIST-TODO HACK
#define HACK_RANK_SHIFT 64

void* runWorkerHcComm(ocrWorker_t * worker) {
    bool blessedWorker = (worker->type == MASTER_WORKERTYPE);
#ifdef ENABLE_COMM_PLATFORM_MPI
        //DIST-HACK7 for MPI the blessed worker is master of rank 0
        blessedWorker &= (worker->pd->myLocation == (ocrLocation_t) (0+HACK_RANK_SHIFT));
        DPRINTF(DEBUG_LVL_INFO,"[%d] hc-comm-worker: blessed worker at location %d\n", (int)worker->pd->myLocation);
#endif
    if (blessedWorker) {
        // This is all part of the mainEdt setup
        // and should be executed by the "blessed" worker.
        void * packedUserArgv = userArgsGet();
        ocrEdt_t mainEdt = mainEdtGet();

        u64 totalLength = ((u64*) packedUserArgv)[0]; // already exclude this first arg
        // strip off the 'totalLength first argument'
        packedUserArgv = (void *) (((u64)packedUserArgv) + sizeof(u64)); // skip first totalLength argument
        ocrGuid_t dbGuid;
        void* dbPtr;
        ocrDbCreate(&dbGuid, &dbPtr, totalLength,
                    DB_PROP_NONE, NULL_GUID, NO_ALLOC);
        DPRINTF(DEBUG_LVL_INFO,"mainDb guid 0x%lx ptr %p\n", dbGuid, dbPtr);
        // copy packed args to DB
        hal_memCopy(dbPtr, packedUserArgv, totalLength, 0);

        // Prepare the mainEdt for scheduling
        ocrGuid_t edtTemplateGuid, edtGuid;
        ocrEdtTemplateCreate(&edtTemplateGuid, mainEdt, 0, 1);
        ocrEdtCreate(&edtGuid, edtTemplateGuid, EDT_PARAM_DEF, /* paramv = */ NULL,
                    /* depc = */ EDT_PARAM_DEF, /* depv = */ &dbGuid,
                    EDT_PROP_NONE, NULL_GUID, NULL);
    } else {
        // Set who we are
        ocrPolicyDomain_t *pd = worker->pd;
        u32 i;
        for(i = 0; i < worker->computeCount; ++i) {
            worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);
        }
    }

    DPRINTF(DEBUG_LVL_INFO, "Starting scheduler routine of worker %ld\n", ((ocrWorkerHc_t *) worker)->id);
    workerLoopHcComm(worker);
    return NULL;
}


void stopWorkerHcComm(ocrWorker_t * selfBase) {
    ocrWorkerHcComm_t * self = (ocrWorkerHcComm_t *) selfBase;
    ocrWorker_t * currWorker = NULL;
    getCurrentEnv(NULL, &currWorker, NULL, NULL);

    if (self->rl_completed[self->rl]) {
        self->rl--;
    }
    // Some other worker wants to stop the communication worker.
    if (currWorker != selfBase) {
        switch(self->rl) {
            case 3:
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: begin shutdown RL3\n", (int) selfBase->pd->myLocation);
                // notify worker of level completion
                self->rl_completed[3] = true;
                while(self->rl_completed[3]);
                self->rl_completed[3] = true; // so ugly
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: done shutdown RL3\n", (int) selfBase->pd->myLocation);
                // Guarantees RL3 is fully completed. Avoids race where the RL is flipped
                // but the comm-worker is in the middle of its RL3 loop.
            break;
            case 2:
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: begin shutdown RL2\n", (int) selfBase->pd->myLocation);
                // Wait for runlevel to complete
                while(!self->rl_completed[2]);
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: done shutdown RL2\n", (int) selfBase->pd->myLocation);
                // All communications completed.
            break;
            case 1:
                //DIST-TODO we don't need this RL I think
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: begin shutdown RL1\n",(int) selfBase->pd->myLocation);
                self->rl_completed[1] = true;
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: done shutdown RL1\n",(int) selfBase->pd->myLocation);
            break;
            case 0:
                DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-worker: begin shutdown RL0\n", (int) selfBase->pd->myLocation);
                // We go on and call the base stop function that
                // shuts down the comm-api and comm-platform.
                self->baseStop(selfBase);
            break;
            default:
            ASSERT(false && "hc-comm-worker: Illegal runlevel in shutdown");
        }
    }  else {
        //DIST-TODO: hc-comm-worker: Implement self shutdown
        ASSERT(false && "hc-comm-worker: Implement self shutdown");
        // worker stopping itself, just call the appropriate run-level
    }
}

/**
 * Builds an instance of a HC Communication worker
 */
ocrWorker_t* newWorkerHcComm(ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * worker = (ocrWorker_t*)
            runtimeChunkAlloc(sizeof(ocrWorkerHcComm_t), NULL);
    factory->initialize(factory, worker, perInstance);
    return (ocrWorker_t *) worker;
}

void initializeWorkerHcComm(ocrWorkerFactory_t * factory, ocrWorker_t *self, ocrParamList_t *perInstance) {
    ocrWorkerFactoryHcComm_t * derivedFactory = (ocrWorkerFactoryHcComm_t *) factory;
    derivedFactory->baseInitialize(factory, self, perInstance);
    // Override base's default value
    ocrWorkerHc_t * workerHc = (ocrWorkerHc_t *) self;
    workerHc->hcType = HC_WORKER_COMM;
    // Initialize comm=worker's members
    ocrWorkerHcComm_t * workerHcComm = (ocrWorkerHcComm_t *) self;
    workerHcComm->baseStop = derivedFactory->baseStop;
    int i = 0;
    while (i < (HC_COMM_WORKER_RL_MAX+1)) {
        workerHcComm->rl_completed[i++] = false;
    }
    workerHcComm->rl = HC_COMM_WORKER_RL_MAX;
}

/******************************************************/
/* OCR-HC COMMUNICATION WORKER FACTORY                */
/******************************************************/

void destructWorkerFactoryHcComm(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryHcComm(ocrParamList_t * perType) {
    ocrWorkerFactory_t * baseFactory = newOcrWorkerFactoryHc(perType);
    ocrWorkerFcts_t baseFcts = baseFactory->workerFcts;

    ocrWorkerFactoryHcComm_t* derived = (ocrWorkerFactoryHcComm_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryHcComm_t), (void *)1);
    ocrWorkerFactory_t * base = (ocrWorkerFactory_t *) derived;
    base->instantiate = FUNC_ADDR(ocrWorker_t* (*)(ocrWorkerFactory_t*, ocrParamList_t*), newWorkerHcComm);
    base->initialize  = FUNC_ADDR(void (*)(ocrWorkerFactory_t*, ocrWorker_t*, ocrParamList_t*), initializeWorkerHcComm);
    base->destruct    = FUNC_ADDR(void (*)(ocrWorkerFactory_t*), destructWorkerFactoryHcComm);

    // Copy base's function pointers
    base->workerFcts = baseFcts;
    derived->baseInitialize = baseFactory->initialize;
    derived->baseStop = baseFcts.stop;

    // Specialize comm functions
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), runWorkerHcComm);
    base->workerFcts.stop = FUNC_ADDR(void (*)(ocrWorker_t*), stopWorkerHcComm);

    baseFactory->destruct(baseFactory);
    return base;
}

#endif /* ENABLE_WORKER_HC_COMM */
