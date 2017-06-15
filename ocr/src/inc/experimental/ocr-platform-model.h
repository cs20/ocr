/**
 * @brief Platform Model (converting affinities to locations)
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_PLATFORM_MODEL_H__
#define __OCR_PLATFORM_MODEL_H__

#include "ocr-config.h"

#include "ocr-types.h"
#include "ocr-runtime-types.h"

typedef struct _ocrPlatformModel_t {
} ocrPlatformModel_t;

// Affinity data-structure, tailored to location placer
typedef struct _ocrAffinity_t {
    ocrLocation_t place;
} ocrAffinity_t;

typedef struct _ocrPlatformModelAffinity_t {
    ocrPlatformModel_t base;
    u64 pdLocAffinitiesSize; /**< Count of available locations */
    u32 current;
    ocrGuid_t * pdLocAffinities;
#ifdef ENABLE_AMT_RESILIENCE
    u64 *nodevec; //bit-vector to mark failed nodes
    u64 veclen;
#endif
} ocrPlatformModelAffinity_t;

struct _ocrPolicyDomain_t;

u8 affinityToLocation(ocrLocation_t* result, ocrGuid_t affinityGuid);
#ifdef ENABLE_AMT_RESILIENCE
u8 notifyPlatformModelLocationFault(ocrLocation_t loc);
u8 checkPlatformModelLocationFault(ocrLocation_t loc);
#endif
ocrPlatformModel_t * createPlatformModelAffinity(struct _ocrPolicyDomain_t * pd);
#ifdef TG_XE_TARGET
ocrPlatformModel_t * createPlatformModelAffinityXE(struct _ocrPolicyDomain_t * pd);
#endif
void destroyPlatformModelAffinity(struct _ocrPolicyDomain_t * pd);

#ifdef TG_XE_TARGET
u32 locationToIdx(ocrLocation_t location);
#endif

#endif /* __OCR_PLATFORM_MODEL_H__ */
