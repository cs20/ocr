/**
 * @brief MPI communication platform
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __MPI_COMM_PLATFORM_H__
#define __MPI_COMM_PLATFORM_H__

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_MPI

#include "utils/ocr-utils.h"
#include "utils/list.h"
#include "ocr-comm-platform.h"

#include <mpi.h>

typedef struct {
    ocrCommPlatformFactory_t base;
} ocrCommPlatformFactoryMPI_t;

#define MPI_COMM_RL_MAX 3

// Initial value for request pool
// Implementation resizes as needed
#ifndef MPI_COMM_REQUEST_POOL_SZ
#define MPI_COMM_REQUEST_POOL_SZ 1024
#endif

struct _mpiCommHandle_t;

typedef struct {
    ocrCommPlatform_t base;
    u64 msgId;
    // Pools of MPI_Request to pending communication
    MPI_Request * sendPool;    // Pending mpi isend
    MPI_Request * recvPool;    // Pending mpi irecv on non-fixed size messages
    MPI_Request * recvFxdPool; // Pending mpi irecv on fixed size messages
    // Pools of handles to pending communication
    // Maintain pointers to respective MPI_Request pools
    struct _mpiCommHandle_t * sendHdlPool;
    struct _mpiCommHandle_t * recvHdlPool;
    struct _mpiCommHandle_t * recvFxdHdlPool;
    // Current useful size of pools
    u32 sendPoolSz;
    u32 recvPoolSz;
    u32 recvFxdPoolSz;
    // Max sizes, dynamically expand when full
    u32 sendPoolMax;
    u32 recvPoolMax;
    u32 recvFxdPoolMax;
    u64 maxMsgSize;
    // The state encodes the RL (top 4 bits) and the phase (bottom 4 bits)
    // This is mainly for debugging purpose
    volatile u8 curState;
} ocrCommPlatformMPI_t;

typedef struct {
    paramListCommPlatformInst_t base;
} paramListCommPlatformMPI_t;

extern ocrCommPlatformFactory_t* newCommPlatformFactoryMPI(ocrParamList_t *perType);

#endif /* ENABLE_COMM_PLATFORM_MPI */
#endif /* __MPI_COMM_PLATFORM_H__ */
