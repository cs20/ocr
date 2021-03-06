/**
 * @brief Gasnet communication platform
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __GASNET_COMM_PLATFORM_H__
#define __GASNET_COMM_PLATFORM_H__

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_GASNET

#include "ocr-hal.h"
#include "utils/ocr-utils.h"
#include "utils/list.h"
#include "ocr-comm-api.h"
#include "ocr-comm-platform.h"

typedef struct {
    ocrCommPlatformFactory_t base;
} ocrCommPlatformFactoryGasnet_t;

typedef struct {
    ocrCommPlatform_t base;
    u64 msgId;
    linkedlist_t * incoming;
    iterator_t * incomingIt;
    lock_t queueLock;
} ocrCommPlatformGasnet_t;

typedef struct {
    paramListCommPlatformInst_t base;
} paramListCommPlatformGasnet_t;

extern ocrCommPlatformFactory_t* newCommPlatformFactoryGasnet(ocrParamList_t *perType);

void libdepInitGasnet(ocrParamList_t *perType);

#endif /* ENABLE_COMM_PLATFORM_Gasnet */
#endif /* __GASNET_COMM_PLATFORM_H__ */
