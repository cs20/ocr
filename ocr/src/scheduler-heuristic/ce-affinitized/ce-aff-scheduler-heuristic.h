/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __CE_AFF_SCHEDULER_HEURISTIC_H__
#define __CE_AFF_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE_AFF

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/**
 * @brief Type of pending request sent to another CE
 */
typedef enum {
    NO_REQUEST,         /**< No request was sent */
    AFF_REQUEST,        /**< An affinitized request was sent */
    AFF_REQUEST_FAIL,   /**< An affinitized request was sent but no work was found */
    NO_AFF_REQUEST,     /**< A non-affinitized request was sent */
} reqPendingAff_t;
/****************************************************/
/* CE SCHEDULER_HEURISTIC                           */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextCeAff_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    u64 msgId;
    u32 stealSchedulerContextIndex;             // Cached index of the context lasted visited during steal attempts
    reqPendingAff_t outWorkRequestPending;         // Work request sent to this location:
                                                // NO_REQUEST: No work request sent
                                                // AFF_REQUEST: Request sent in an affinitized manner
                                                // AFF_REQUEST_FAIL: Request was sent and response received (no work available)
                                                // NO_AFF_REQUEST: Request sent in a non-affinitized manner
    reqPendingAff_t inWorkRequestPending;          // Work request coming in from remote location (remote loc is out of work); will be NO_REQUEST, AFF_REQUEST or NO_AFF_REQUEST.
    bool canAcceptWorkRequest;                  // Identifies a context that can accept a work request from current CE
    bool isChild;                               // Identifies if a context will report to this CE for shutdown protocol (all XEs)
} ocrSchedulerHeuristicContextCeAff_t;

typedef struct _ocrSchedulerHeuristicCeAff_t {
    ocrSchedulerHeuristic_t base;
    u64 workCount;
    u32 inPendingCount;                         // Number of pending agents (XEs + CEs)
    u32 pendingXeCount;                         // Number of pending XEs (sleeping)
    u32 outWorkVictimsAvailable[2];             // The remaining number of work requests that can be made
                                                // @ idx 0: victims available in affinitized mode
                                                // @ idx 1: victims available in non-affinitized mode
    u32 rrXE;                                   // Index of the XE that we push an incoming work to.
    u32 xeCount;                                // Cached value of the number of XEs
    u32 rrInsert;                               // Which neighbor/child to use to insert non-affinitized work
                                                // if affinity is strictly enforced. This allows work to be
                                                // spread out even if there are no affinities
    bool shutdownMode;                          // This indicates whether PD is shutting down or not
    bool enforceAffinity;                       // If true, affinity will be strongly enforced
                                                // and affinitized work will not be stolen
} ocrSchedulerHeuristicCeAff_t;

/****************************************************/
/* CE SCHEDULER_HEURISTIC FACTORY                   */
/****************************************************/

typedef struct _paramListSchedulerHeuristicCeAff_t {
    paramListSchedulerHeuristic_t base;
    bool enforceAffinity;
} paramListSchedulerHeuristicCeAff_t;

typedef struct _ocrSchedulerHeuristicFactoryCeAff_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryCeAff_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryCeAff(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_CE_AFF */
#endif /* __CE_AFF_SCHEDULER_HEURISTIC_H__ */

