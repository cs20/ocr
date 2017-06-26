/*
 * This file is subject to the license agreement located in the file LIXENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_XE

#define DEBUG_TYPE SCHEDULER

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "scheduler/xe/xe-scheduler.h"

/******************************************************/
/* OCR-XE SCHEDULER                                   */
/******************************************************/

void xeSchedulerDestruct(ocrScheduler_t * self) {
}

u8 xeSchedulerSwitchRunlevel(ocrScheduler_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                             phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {
    u8 toReturn = 0;

    ocrSchedulerXe_t *rself = (ocrSchedulerXe_t*) self;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE) && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    // Take care of all other sub-objects
    int i;
    for(i = 0; i < self->workpileCount; ++i) {
        toReturn |= self->workpiles[i]->fcts.switchRunlevel(
            self->workpiles[i], PD, runlevel, phase, properties, NULL, 0);
    }

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        if((properties & RL_BRING_UP) && phase == 0) {
            RL_ENSURE_PHASE_UP(PD, RL_MEMORY_OK, RL_PHASE_SCHEDULER, 2);
           RL_ENSURE_PHASE_DOWN(PD, RL_MEMORY_OK, RL_PHASE_SCHEDULER, 2);
        }
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            self->pd = PD;
            rself->getwork_lock = INIT_LOCK;
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_SCHEDULER);
            }
        } else {
            // Tear-down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
                msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(guid) = self->fguid;
                PD_MSG_FIELD_I(properties) = 0;
                toReturn |= self->pd->fcts.processMessage(self->pd, &msg, false);
                self->fguid.guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
            }
        }
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }

    return toReturn;
}

u8 xeSchedulerTake(ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts) {
    return 0;
}

u8 xeSchedulerGive(ocrScheduler_t* base, u32* count, ocrFatGuid_t* edts) {
    return 0;
}

u8 xeSchedulerTakeComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return OCR_ENOSYS;
}

u8 xeSchedulerGiveComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return OCR_ENOSYS;
}

///////////////////////////////
//      Scheduler 1.0        //
///////////////////////////////

u8 xeSchedulerGetWorkInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrPolicyMsg_t * msg = (ocrPolicyMsg_t *)hints;
    u8 retVal = 0;
    ocrSchedulerXe_t *rself = (ocrSchedulerXe_t*) self;
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
    // We can't handle OCR_SCHED_WORD_MULTI_EDTS_USER kinds of work requests
    ASSERT(PD_MSG_FIELD_IO(schedArgs).kind == OCR_SCHED_WORK_EDT_USER);
#undef PD_MSG
#undef PD_TYPE

    ocrFatGuid_t edt;
    ocrFatGuid_t * edtp = NULL;
    edt=self->workpiles[0]->fcts.pop(self->workpiles[0], POP_WORKPOPTYPE, NULL);

    if (edt.metaDataPtr != NULL) {
        // Use the edt which we just got
        DPRINTF(DEBUG_LVL_VVERB, "Popped EDT with GUID "GUIDF" from workpile.\n", GUIDA(edt.guid));
        edtp = &edt;
    } else {
        // Get the lock to prevent multiple XEs all asking for 8 edts.
        hal_lock(&(rself->getwork_lock));

        ocrWorker_t * worker;
        getCurrentEnv(NULL, &worker, NULL, NULL);

        if (worker->fcts.isRunning(worker)) {

            // Check if another XE already got more work
            edt=self->workpiles[0]->fcts.pop(self->workpiles[0], POP_WORKPOPTYPE, NULL);
            if (edt.metaDataPtr != NULL) {
                hal_unlock(&(rself->getwork_lock));
                // Use the edt which we just got
                DPRINTF(DEBUG_LVL_VVERB, "Popped EDT with GUID "GUIDF" from workpile.\n", GUIDA(edt.guid));
                edtp = &edt;
            } else {
                // Request more work for the work pile
                // We need a new message to use the OCR_SCHED_WORK_MULTI_EDTS_USER capability

                // We cant use PD_MSG_STACK because that doesn't provide enough room to unmarshal
                // the return message with the list of work. Instead make an array to guarantee
                // that there is room on the stack to unmarshal.
                u8 mMsg_buffer[sizeof(ocrPolicyMsg_t) + GET_MULTI_WORK_MAX_SIZE * sizeof(ocrFatGuid_t)];
                ocrPolicyMsg_t * mMsg = (ocrPolicyMsg_t*)mMsg_buffer;
                mMsg->usefulSize = 0;
                mMsg->bufferSize = sizeof(ocrPolicyMsg_t) * 2;
                mMsg->srcLocation = mMsg->destLocation = INVALID_LOCATION;

                getCurrentEnv(NULL, NULL, NULL, mMsg);
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
                bool discardAffinity = PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).discardAffinity;
#undef PD_MSG
#undef PD_TYPE

#define PD_MSG (mMsg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
                mMsg->type = PD_MSG_SCHED_GET_WORK | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                mMsg->destLocation = self->pd->parentLocation;
                PD_MSG_FIELD_IO(schedArgs).kind = OCR_SCHED_WORK_MULTI_EDTS_USER;
                PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_MULTI_EDTS_USER).edts = NULL;
                PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_MULTI_EDTS_USER).guidCount = GET_MULTI_WORK_MAX_SIZE;
                PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_MULTI_EDTS_USER).discardAffinity = discardAffinity;
                PD_MSG_FIELD_I(properties) = 0;
#undef PD_MSG
#undef PD_TYPE

                retVal = self->pd->fcts.processMessage(self->pd, mMsg, true);

                if (retVal == 0) {
#define PD_MSG (mMsg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
                    if (PD_MSG_FIELD_IO(schedArgs).kind == OCR_SCHED_WORK_MULTI_EDTS_USER) {
                        // We got a response with (potentially) multiple edts
                        ocrFatGuid_t *edts = PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_MULTI_EDTS_USER).edts;
                        u32 guidCount = PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_MULTI_EDTS_USER).guidCount;
                        DPRINTF(DEBUG_LVL_VVERB, "Received multiple EDT response with %d GUIDs\n", guidCount);
                        // Place the guids (except for the first one) into the workpile
                        int i;
                        for (i=1; i<guidCount; i++) {
                            self->workpiles[0]->fcts.push(self->workpiles[0], PUSH_WORKPUSHTYPE, edts[i]);
                        }
                        edtp = edts; // Point to the first returned edt to return
                    } else if (PD_MSG_FIELD_IO(schedArgs).kind == OCR_SCHED_WORK_EDT_USER) {
                        // We got a response with one (or zero) edt(s).
                        edtp = &(PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt);
                        DPRINTF(DEBUG_LVL_VVERB, "Received single EDT response with GUID "GUIDF"\n", GUIDA(edtp->guid));
                    } else {
                        ASSERT(0);
                    }
#undef PD_MSG
#undef PD_TYPE
                }
                hal_unlock(&(rself->getwork_lock));
            }
        } else {
            // We are shutting down. Just abort.
            hal_unlock(&(rself->getwork_lock));
            retVal = 1;
        }
    }

#define PD_MSG (msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
    if (retVal == 0 && edtp != NULL) {
        if (!(ocrGuidIsNull(edtp->guid))) {
            DPRINTF(DEBUG_LVL_VVERB, "Received EDT with GUID "GUIDF"\n", GUIDA(edtp->guid));
            DPRINTF(DEBUG_LVL_VVERB, "Received EDT ("GUIDF"; %p)\n", GUIDA(edtp->guid), edtp->metaDataPtr);
        }

        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.guid = edtp->guid;
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.metaDataPtr = edtp->metaDataPtr;
    }
#undef PD_MSG
#undef PD_TYPE
    return retVal;
}

u8 xeSchedulerNotifyInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerTransactInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerAnalyzeInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerUpdate(ocrScheduler_t *self, u32 properties) {
    return OCR_ENOTSUP;
}

ocrScheduler_t* newSchedulerXe(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* derived = (ocrScheduler_t*) runtimeChunkAlloc(
                                  sizeof(ocrSchedulerXe_t), PERSISTENT_CHUNK);
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeSchedulerXe(ocrSchedulerFactory_t * factory, ocrScheduler_t * derived, ocrParamList_t * perInstance) {
    initializeSchedulerOcr(factory, derived, perInstance);
}

void destructSchedulerFactoryXe(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryXe(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerFactoryXe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newSchedulerXe;
    base->initialize  = &initializeSchedulerXe;
    base->destruct = &destructSchedulerFactoryXe;

    base->schedulerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                          phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), xeSchedulerSwitchRunlevel);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), xeSchedulerDestruct);
    base->schedulerFcts.takeEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), xeSchedulerTake);
    base->schedulerFcts.giveEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), xeSchedulerGive);
    base->schedulerFcts.takeComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), xeSchedulerTakeComm);
    base->schedulerFcts.giveComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), xeSchedulerGiveComm);

    //Scheduler 1.0
    base->schedulerFcts.update = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32), xeSchedulerUpdate);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerGetWorkInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerNotifyInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerTransactInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerAnalyzeInvoke);
    return base;
}

#endif /* ENABLE_SCHEDULER_XE */
