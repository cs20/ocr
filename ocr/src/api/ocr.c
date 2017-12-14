/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sal.h"
#include "ocr-types.h"

#define DEBUG_TYPE API

#ifdef ENABLE_AMT_RESILIENCE
//Notify resilient latch that shutdown has been called
static u8 notifyShutdownLatch() {
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *task = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &task, &msg);
    ocrGuid_t latch = (task != NULL) ? task->resilientLatch : NULL_GUID;
    if (!ocrGuidIsNull(latch)) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
        getCurrentEnv(NULL, NULL, NULL, &msg);
        msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(satisfierGuid.guid) = task->guid;
        PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = task;
        PD_MSG_FIELD_I(guid.guid) = latch;
        PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(payload.guid) = NULL_GUID;
        PD_MSG_FIELD_I(payload.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(currentEdt.guid) = task->guid;
        PD_MSG_FIELD_I(currentEdt.metaDataPtr) = task;
        PD_MSG_FIELD_I(slot) = OCR_EVENT_LATCH_SHUTDOWN_SLOT;
#ifdef REG_ASYNC_SGL
        PD_MSG_FIELD_I(mode) = -1;
#endif
        PD_MSG_FIELD_I(properties) = 0;
#ifdef ENABLE_OCR_API_DEFERRABLE
        tagDeferredMsg(&msg, task);
#endif
        RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
    }
#undef PD_TYPE
#undef PD_MSG
    return 0;
}
#endif

static void ocrShutdownInternal(u8 errorCode) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrShutdown()\n");
#ifdef ENABLE_AMT_RESILIENCE
    notifyShutdownLatch();
#endif
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrPolicyMsg_t * msgPtr = &msg;
    ocrTask_t * curTask;
    getCurrentEnv(&pd, NULL, &curTask, msgPtr);
#define PD_MSG msgPtr
#define PD_TYPE PD_MSG_MGT_RL_NOTIFY
    msgPtr->type = PD_MSG_MGT_RL_NOTIFY | PD_MSG_REQUEST;
#ifdef ENABLE_OCR_API_DEFERRABLE
    if (!errorCode) {
        tagDeferredMsg(msgPtr, curTask);
    }
#endif
#ifdef ENABLE_AMT_RESILIENCE
    pd->faultCode = OCR_NODE_SHUTDOWN;
#endif
    PD_MSG_FIELD_I(runlevel) = RL_COMPUTE_OK;
    PD_MSG_FIELD_I(properties) = RL_REQUEST | RL_BARRIER | RL_TEAR_DOWN;
    PD_MSG_FIELD_I(errorCode) = errorCode;
    u8 returnCode __attribute__((unused)) = pd->fcts.processMessage(pd, msgPtr, true);
    ASSERT((returnCode == 0));
#undef PD_MSG
#undef PD_TYPE
}

void ocrShutdown() {
    START_PROFILE(api_ocrShutdown);
    ocrShutdownInternal(0);
    RETURN_PROFILE();
}

void ocrAbort(u8 errorCode) {
    START_PROFILE(api_ocrAbort);
    ocrShutdownInternal(errorCode);
    RETURN_PROFILE();
}

u64 getArgc(void* dbPtr) {
    START_PROFILE(api_getArgc);
    DPRINTF(DEBUG_LVL_INFO, "ENTER getArgc(dbPtr=%p)\n", dbPtr);
    DPRINTF(DEBUG_LVL_INFO, "EXIT getArgc -> %"PRIu64"\n", ((u64*)dbPtr)[0]);
    RETURN_PROFILE(((u64*)dbPtr)[0]);

}

char* getArgv(void* dbPtr, u64 count) {
    START_PROFILE(api_getArgv);
    DPRINTF(DEBUG_LVL_INFO, "ENTER getArgv(dbPtr=%p, count=%"PRIu64")\n", dbPtr, count);
    u64* dbPtrAsU64 = (u64*)dbPtr;
    ASSERT(count < dbPtrAsU64[0]); // We can't ask for more args than total
    u64 offset = dbPtrAsU64[1 + count];
    DPRINTF(DEBUG_LVL_INFO, "EXIT getArgv -> %s\n", ((char*)dbPtr) + offset);
    RETURN_PROFILE(((char*)dbPtr) + offset);
}

#ifdef ENABLE_AMT_RESILIENCE
#include <pthread.h>
void ocrInjectNodeFailure() {
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curTask = NULL;
    getCurrentEnv(&pd, NULL, &curTask, NULL);
    if ((curTask != NULL) && ((curTask->flags & OCR_TASK_FLAG_RECOVERY) == 0)) {
        pd->faultCode = OCR_NODE_FAILURE_SELF;
        hal_fence();
        salComputeThreadExitOnFailure();
    } else {
        DPRINTF(DEBUG_LVL_WARN, "User fault injection ignored in recovery EDT...\n");
    }
}

u64 ocrGetLocation() {
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    return (u64)pd->myLocation;
}

u8 ocrGuidTablePut(u64 key, ocrGuid_t val) {
    return salGuidTablePut(key, val);
}

u8 ocrGuidTableGet(u64 key, ocrGuid_t *val) {
    return salGuidTableGet(key, val);
}

u8 ocrGuidTableRemove(u64 key, ocrGuid_t *val) {
    return salGuidTableRemove(key, val);
}

#endif

