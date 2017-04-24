/**
 * @brief Compute Platform implemented using pthread
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __COMP_PLATFORM_PTHREAD_H__
#define __COMP_PLATFORM_PTHREAD_H__

#include "ocr-config.h"
#ifdef ENABLE_COMP_PLATFORM_PTHREAD

#include "ocr-comp-platform.h"
#include "ocr-comp-target.h"
#include "ocr-worker.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#ifdef ENABLE_WORKPILE_CE
#include "utils/deque.h"
#endif
#include "comp-platform/platform-binding-info.h"

#include <pthread.h>

/**
 * @brief Structure stored on a per-thread basis to keep track of
 * "who we are"
 */
typedef struct {
    ocrPolicyDomain_t *pd;
    ocrWorker_t *worker;
} perThreadStorage_t;

typedef struct {
    ocrCompPlatformFactory_t base;
    u64 stackSize;
} ocrCompPlatformFactoryPthread_t;

typedef struct {
    ocrCompPlatform_t base;
    pthread_t osThread;
    perThreadStorage_t tls;
    u64 stackSize;
    bindingInfo_t bindingInfo;
    u32 threadStatus; // RL_NODE_MASTER or RL_PD_MASTER or 0
#ifdef OCR_RUNTIME_PROFILER
    bool doProfile;
#endif
} ocrCompPlatformPthread_t;

#define PD_ALLOC_POLICY_RR    ((u8)1)
#define PD_ALLOC_POLICY_BLOCK ((u8)2)

typedef struct {
    paramListCompPlatformInst_t base;
    u64 stackSize;
    bindingInfo_t bindingInfo;
#ifdef OCR_RUNTIME_PROFILER
    bool doProfile;
#endif
} paramListCompPlatformPthread_t;

extern ocrCompPlatformFactory_t* newCompPlatformFactoryPthread(ocrParamList_t *perType);

#endif /* ENABLE_COMP_PLATFORM_PTHREAD */
#endif /* __COMP_PLATFORM_PTHREAD_H__ */



