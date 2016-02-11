/**
 * @brief Simple data-block implementation.
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DATABLOCK_LOCKABLE_H__
#define __DATABLOCK_LOCKABLE_H__

#include "ocr-config.h"
#ifdef ENABLE_DATABLOCK_LOCKABLE

#include "ocr-allocator.h"
#include "ocr-datablock.h"
#include "ocr-hal.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-worker.h"
#include "utils/queue.h"

#ifdef ENABLE_HINTS
/**< The number of hint properties supported by this implementation */
#define OCR_HINT_COUNT_DB_LOCKABLE   1
#else
#define OCR_HINT_COUNT_DB_LOCKABLE   0
#endif

typedef struct {
    ocrDataBlockFactory_t base;
} ocrDataBlockFactoryLockable_t;

typedef union {
    struct {
        u64 state      : 2;   // Current state of the DB
        u64 dbMode     : 2;   // Current DB mode
        u64 hasPeers   : 1;   // MD has peer instances
        u64 writeBack  : 1;   // Shall writeback on release
        u64 isFetching : 1;   // Is currently fetching remotely
        u64 isReleasing : 1;  // Is currently releasing remotely
        u64 flags      : 16;  // From the object's creation
        u64 numUsers   : 15;  // Number of consummers checked-in
        u64 freeRequested: 1; // dbDestroy has been called
        u64 singleAssign : 1; // Single assignment done
        u64 _padding   : 23;
    };
    u64 data;
} ocrDataBlockLockableAttr_t;

// Declared in .c
struct dbWaiter_t;
struct _ocrPolicyMsg_t;

// Must match DB_* definitions in the implementation
#define DB_MODE_COUNT   4

// Tracker for MD copies
#define DB_MAX_LOC (1<<GUID_PROVIDER_LOCID_SIZE)
#define DB_MAX_LOC_ARRAY ((DB_MAX_LOC/64)+1)

// DB Stats
#define CNT_LOCAL_RELEASE  0
#define CNT_REMOTE_RELEASE 1
#define CNT_LOCAL_ACQUIRE  2
#define CNT_REMOTE_ACQUIRE 3
#define CNT_MAX            4

typedef struct _ocrDataBlockLockable_t {
    ocrDataBlock_t base;
    /* Data for the data-block */
    lock_t lock; /**< Lock for this data-block */
    ocrDataBlockLockableAttr_t attributes; /**< Attributes for this data-block */
    struct _dbWaiter_t * localWaitQueues[DB_MODE_COUNT]; /** Per mode local waiters queued to acquire the db */
    Queue_t * remoteWaitQueues[DB_MODE_COUNT]; /** Per mode remote PD waiters queued to acquire the db */
    struct _ocrPolicyMsg_t * backingPtrMsg; /** Pointer to a policy message that stores the DB's data */
    // mdPeers: If the MD instance is the master one it is the PD that owns the guid. Else it is
    // the PD location to write the datablock back to.
    ocrLocation_t mdPeers;
    ocrWorker_t * worker; /**< worker currently owning the DB internal lock */
#ifdef ENABLE_RESILIENCY
    // Store number of local and remote waiters count in queue for serialization.
    // Don't really need the remote one since it's encoded in the queue...
    u32 waiterQueueCounters[DB_MODE_COUNT*2];
#endif
    u64 mdLocTracker[DB_MAX_LOC_ARRAY]; /**< Tracker for MD copies */
#ifdef LOCKABLE_DB_STATS
    u64 counters[CNT_MAX];
#endif
    ocrRuntimeHint_t hint; // Warning must be the last
} ocrDataBlockLockable_t;

extern ocrDataBlockFactory_t* newDataBlockFactoryLockable(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_DATABLOCK_LOCKABLE */
#endif /* __DATABLOCK_LOCKABLE_H__ */
