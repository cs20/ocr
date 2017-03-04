/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_GUIDPROVIDER_LABELED_H__
#define __OCR_GUIDPROVIDER_LABELED_H__

#include "ocr-config.h"
#ifdef ENABLE_GUID_LABELED

#include "ocr-types.h"
#include "ocr-guid.h"
#include "utils/hashtable.h"
#include "guid/guid-bitmap.h"

/**
 * @brief GUID provider that supports using labels for GUIDs.
 * It uses a map to store associations between GUIDs and the pointer
 * to the backing metadata.
 * For distributed (over MPI), this also encodes in the GUID
 * the rank of the node it has been generated on.
 * !! Performance Warning !!
 * - Hashtable implementation is backed by a simplistic hash function.
 * - The number of hashtable's buckets can be customize through 'GUID_PROVIDER_NB_BUCKETS'.
 * - GUID generation relies on an atomic incr shared by ALL the workers of the PD.
 */

#define GUID_WID_CACHE_SIZE (CACHE_LINE_SZB/sizeof(u64))

typedef struct {
    ocrGuidProvider_t base;
    hashtable_t * guidImplTable;
    // Counter for the reserved part of the GUIDs
    u64 guidReservedCounter;
#ifdef GUID_PROVIDER_WID_INGUID
    u64 guidCounters[MAX_VAL(LOCWID)*GUID_WID_CACHE_SIZE];
    //TODO could store this guid in counters because of the cache line
    u64 guidReservedCounters[MAX_VAL(LOCWID)*GUID_WID_CACHE_SIZE];
#else
    // GUID 'id' counter, atomically incr when a new GUID is requested
    u64 guidCounter;
#endif
} ocrGuidProviderLabeled_t;

typedef struct {
    ocrGuidProviderFactory_t base;
} ocrGuidProviderFactoryLabeled_t;

ocrGuidProviderFactory_t *newGuidProviderFactoryLabeled(ocrParamList_t *typeArg, u32 factoryId);

#define __GUID_END_MARKER__
#include "ocr-guid-end.h"
#undef __GUID_END_MARKER__

#endif /* ENABLE_GUID_LABELED */
#endif /* __OCR_GUIDPROVIDER_LABELED_H__ */
