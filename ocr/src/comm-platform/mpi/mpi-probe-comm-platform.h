/**
 * @brief MPI communication platform
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __MPI_PROBE_COMM_PLATFORM_H__
#define __MPI_PROBE_COMM_PLATFORM_H__

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_MPI_PROBE

#include "utils/ocr-utils.h"
#include "utils/list.h"
#include "ocr-comm-platform.h"

typedef struct {
    ocrCommPlatformFactory_t base;
} ocrCommPlatformFactoryMPIProbe_t;

#define MPI_COMM_RL_MAX 3

struct _mpiCommHandle_t;

typedef struct {
    ocrCommPlatform_t base;
    u64 msgId;
    linkedlist_t * incoming;
    linkedlist_t * outgoing;
    iterator_t * incomingIt;
    iterator_t * outgoingIt;
    u64 maxMsgSize;
    // The state encodes the RL (top 4 bits) and the phase (bottom 4 bits)
    // This is mainly for debugging purpose
    volatile u8 curState;
} ocrCommPlatformMPIProbe_t;

typedef struct {
    paramListCommPlatformInst_t base;
} paramListCommPlatformMPIProbe_t;

extern ocrCommPlatformFactory_t* newCommPlatformFactoryMPIProbe(ocrParamList_t *perType);

#endif /* ENABLE_COMM_PLATFORM_MPI_PROBE */
#endif /* __MPI_PROBE_COMM_PLATFORM_H__ */
