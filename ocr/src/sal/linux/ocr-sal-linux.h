/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_SAL_LINUX_H__
#define __OCR_SAL_LINUX_H__

#include "ocr-hal.h"
#include "ocr-perfmon.h"

#include <assert.h>
#include <stdio.h>

#ifdef ENABLE_EXTENSION_PERF
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#endif

void sig_handler(u32 sigNum);

extern u32 salPause(bool isBlocking);

extern ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags);

extern void salResume(u32 flag);

extern void salInjectFault(void);

extern void registerSignalHandler();

#define sal_abort()   hal_abort()

#define sal_exit(x)   hal_exit(x)

#define sal_assert(x, f, l)  assert(x)

#define sal_print(msg, len)   printf("%s", msg)

#ifdef ENABLE_EXTENSION_PERF

typedef struct _salPerfCounter {
    u64 perfVal;
    u32 perfFd;
    u32 perfType;
    u64 perfConfig;
    struct perf_event_attr perfAttr;
} salPerfCounter;

u64 salPerfInit(salPerfCounter* perfCtr);
u64 salPerfStart(salPerfCounter* perfCtr);
u64 salPerfStop(salPerfCounter* perfCtr);
u64 salPerfShutdown(salPerfCounter *perfCtr);

#endif

#ifdef ENABLE_RESILIENCY
u64 salGetCalTime();
u8* salCreatePdCheckpoint(char **name, u64 size);
u8* salOpenPdCheckpoint(char *name, u64 *size);
u8  salClosePdCheckpoint(u8 *buffer, u64 size);
u8  salRemovePdCheckpoint(char *name);
u8 salSetPdCheckpoint(char *name);
char* salGetCheckpointName();
bool salCheckpointExists();
bool salCheckpointExistsResumeQuery();
#endif

#ifdef ENABLE_AMT_RESILIENCE
#include "ocr-task.h"
#include "ocr-event.h"
#include "ocr-datablock.h"
//Init/Finialize
void      salInitPublishFetch();
void      salFinalizePublishFetch();

//OCR resilience api
u8        salResilientEdtCreate(ocrTask_t *task, ocrGuid_t pguid, u64 key, u64 ip, u64 ac);
u8        salResilientDbCreate(ocrDataBlock_t *db, ocrGuid_t pguid, u64 key, u64 ip, u64 ac);
u8        salResilientEventCreate(ocrEvent_t *evt, ocrGuid_t pguid, u64 key, u64 ip, u64 ac);
u8        salResilientAddDependence(ocrGuid_t sguid, ocrGuid_t dguid, u32 slot);
u8        salResilientEventSatisfy(ocrGuid_t guid, u32 slot, ocrGuid_t data);
u8        salResilientGuidDestroy(ocrGuid_t guid);

//Runtime internal api
u8        salIsResilientGuid(ocrGuid_t guid);
u8        salIsSatisfiedResilientGuid(ocrGuid_t guid);
u8        salResilientGuidConnect(ocrGuid_t keyGuid, ocrGuid_t valGuid);
u8        salResilientAdvanceWaiters();
u8        salRecordMainEdt();
u8        salDestroyMainEdt();

//Publish-Fetch api
u8        salResilientDataBlockPublish(ocrDataBlock_t *db);
void*     salResilientDataBlockFetch(ocrGuid_t guid, u64 *size);
u8        salResilientDataBlockRemove(ocrDataBlock_t *db);
u8        salResilientTaskPublish(ocrTask_t *task);
u8        salResilientTaskRemove(ocrGuid_t guid);
u8        salRecordEdtAtNode(ocrGuid_t edt, ocrLocation_t loc);
u8        salResilientRecordTaskRoot(ocrTask_t *task);
u8        salResilientTaskRootUpdate(ocrGuid_t guid, u32 slot, ocrGuid_t data);

//Guid table api
u8        salGuidTablePut(u64 key, ocrGuid_t val);
u8        salGuidTableGet(u64 key, ocrGuid_t *val);
u8        salGuidTableRemove(u64 key, ocrGuid_t *val);

//Node failure api
void      salComputeThreadExitOnFailure();
void      salWaitForAllComputeThreadExit();
void      salComputeThreadWaitForRecovery();
u8        salCheckEdtFault(ocrGuid_t edt);
u8        salProcessNodeFailure(ocrLocation_t nodeId);
u8        salRecoverNodeFailureAtBuddy(ocrLocation_t nodeId);
u8        salRecoverNodeFailureAtNonBuddy(ocrLocation_t nodeId);
u8        salResumeAfterNodeFailure();

#endif

#endif /* __OCR_SAL_LINUX_H__ */
