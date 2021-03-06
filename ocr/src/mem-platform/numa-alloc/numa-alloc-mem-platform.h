/**
 * @brief Simple Numa based alloc allocator
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __MEM_PLATFORM_NUMA_ALLOC_H__
#define __MEM_PLATFORM_NUMA_ALLOC_H__

#include "ocr-config.h"
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC

#include "debug.h"
#include "utils/rangeTracker.h"
#include "ocr-hal.h"
#include "ocr-mem-platform.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include <numa.h>

typedef struct {
    ocrMemPlatformFactory_t base;
} ocrMemPlatformFactoryNumaAlloc_t;

typedef struct {
    ocrMemPlatform_t base;
    rangeTracker_t *pRangeTracker;
    u32 numa_node;
    lock_t lock;
} ocrMemPlatformNumaAlloc_t;

ocrMemPlatformFactory_t* newMemPlatformFactoryNumaAlloc(ocrParamList_t *perType);

#endif /* ENABLE_MEM_PLATFORM_NUMA_ALLOC */
#endif /* __MEM_PLATFORM_NUMA_ALLOC_H__ */
