/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler-object/scheduler-object-all.h"
#include "debug.h"

const char * schedulerObject_types[] = {
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    "NULL",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    "WST",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    "DEQ",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    "LIST",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    "MAP",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
    "PDSPACE",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
    "DBSPACE",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
    "DBTIME",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
    "PR_WSH",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    "BIN_HEAP",
#endif
    NULL
};

ocrSchedulerObjectFactory_t * newSchedulerObjectFactory(schedulerObjectType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    case schedulerObjectNull_id:
        return newOcrSchedulerObjectFactoryNull(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    case schedulerObjectWst_id:
        return newOcrSchedulerObjectFactoryWst(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    case schedulerObjectDeq_id:
        return newOcrSchedulerObjectFactoryDeq(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    case schedulerObjectList_id:
        return newOcrSchedulerObjectFactoryList(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    case schedulerObjectMap_id:
        return newOcrSchedulerObjectFactoryMap(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
    case schedulerObjectPdspace_id:
        return newOcrSchedulerObjectFactoryPdspace(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
    case schedulerObjectDbspace_id:
        return newOcrSchedulerObjectFactoryDbspace(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
    case schedulerObjectDbtime_id:
        return newOcrSchedulerObjectFactoryDbtime(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
    case schedulerObjectPrWsh_id:
        return newOcrSchedulerObjectFactoryPrWsh(perType, perType->id);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    case schedulerObjectBinHeap_id:
        return newOcrSchedulerObjectFactoryBinHeap(perType, perType->id);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

