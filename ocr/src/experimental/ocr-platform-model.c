/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_EXTENSION_AFFINITY

#include "debug.h"

#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "experimental/ocr-platform-model.h"
#include "experimental/ocr-placer.h"
#include "extensions/ocr-affinity.h"

//
// Internal Placement API
//

//Part of policy-domain debug messages
#define DEBUG_TYPE POLICY

// Assumptions:
// - neighbors are locations described as ranks [0:N[
// - neighbors contains all ranks but self
// - placer's affinities array represents all the PD locations and is sorted by rank id

//
// Begin platform model based on affinities
//

//TODO defined in hc-dist-policy for now but should go away
#ifdef ENABLE_POLICY_DOMAIN_HC_DIST
extern u8 resolveRemoteMetaData(ocrPolicyDomain_t * pd, ocrFatGuid_t * fatGuid,
                                ocrPolicyMsg_t * msg, bool isBlocking);
#else
static u8 resolveRemoteMetaData(ocrPolicyDomain_t * pd, ocrFatGuid_t * fatGuid,
                                ocrPolicyMsg_t * msg, bool isBlocking) {
    u64 val;
    // On the XE, we don't have a GUID provider and on the CE, we do some mean trick to get
    // the right value (pretending to be another CE)
#if defined(TG_XE_TARGET) || defined(TG_CE_TARGET)
    PD_MSG_STACK(msg2);
    getCurrentEnv(NULL, NULL, NULL, &msg2);
#define PD_MSG (&msg2)
#define PD_TYPE PD_MSG_GUID_INFO
    msg2.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = fatGuid->guid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP;
    RESULT_ASSERT(pd->fcts.processMessage(pd, &msg2, true), ==, 0);
    u8 res __attribute__((unused)) = 0; // The call returns the mode so we just ignore it and val will be non-zero
    // if all went well
    val = (u64)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_MSG
#undef PD_TYPE

#else
    u8 res __attribute__((unused)) = pd->guidProviders[0]->fcts.getVal(pd->guidProviders[0], fatGuid->guid, &val, NULL, MD_LOCAL, NULL);
#endif
    ASSERT(val != 0);
    ASSERT(res == 0);
    fatGuid->metaDataPtr = (void *) val;
    return 0;
}
#endif

u8 affinityToLocation(ocrLocation_t* result, ocrGuid_t affinityGuid) {
    ocrFatGuid_t fguid;
    fguid.guid = affinityGuid;
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    resolveRemoteMetaData(pd, &fguid, NULL, true);
    ASSERT((fguid.metaDataPtr != NULL) && "ERROR: cannot deguidify affinity GUID");
    *result = ((ocrAffinity_t *) fguid.metaDataPtr)->place;
    return 0;
}

#ifdef ENABLE_AMT_RESILIENCE
//Notify platform of a node failure
u8 notifyPlatformModelLocationFault(ocrLocation_t loc) {
    u64 i, j;
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ASSERT(loc != pd->myLocation);
    ocrPlatformModelAffinity_t * model = (ocrPlatformModelAffinity_t *) (pd->platformModel);
    ASSERT((model->nodevec[loc/64] & (0x1UL << (loc % 64))) == 0);
    model->nodevec[loc/64] |= (0x1UL << (loc % 64));
    u64 countAff = pd->neighborCount + 1;
    for(i = 0; i < countAff; i++) {
        ocrGuid_t affinityGuid = model->pdLocAffinities[i];
        ocrFatGuid_t fguid;
        fguid.guid = affinityGuid;
        fguid.metaDataPtr = NULL;
        resolveRemoteMetaData(pd, &fguid, NULL, true);
        ASSERT((fguid.metaDataPtr != NULL) && "ERROR: cannot deguidify affinity GUID");
        ocrAffinity_t *aff = (ocrAffinity_t *) fguid.metaDataPtr;
        if (aff->place == loc) {
            u8 found = 0;
            for (j = 1; j < countAff; j++) {
                u64 indx = (i+j) % countAff;
                ocrGuid_t nextGuid = model->pdLocAffinities[indx];
                ocrFatGuid_t nguid;
                nguid.guid = nextGuid;
                nguid.metaDataPtr = NULL;
                resolveRemoteMetaData(pd, &nguid, NULL, true);
                ASSERT((nguid.metaDataPtr != NULL) && "ERROR: cannot deguidify affinity GUID");
                ocrLocation_t newplace = ((ocrAffinity_t *) nguid.metaDataPtr)->place;
                if (newplace != loc) {
                    aff->place = newplace;
                    found = 1;
                    break;
                }
            }
            ASSERT(found);
        }
    }
    return 0;
}

u8 checkPlatformModelLocationFault(ocrLocation_t loc) {
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPlatformModelAffinity_t * model = (ocrPlatformModelAffinity_t *) (pd->platformModel);
    return (model->nodevec[loc/64] & (0x1UL << (loc % 64))) ? 1 : 0;
}
#endif

ocrPlatformModel_t * createPlatformModelAffinity(ocrPolicyDomain_t *pd) {
    u64 i=0;
    ocrPlatformModelAffinity_t * model = pd->fcts.pdMalloc(pd, sizeof(ocrPlatformModelAffinity_t));
    u64 countAff = pd->neighborCount + 1;
    model->pdLocAffinities = NULL;
    model->pdLocAffinitiesSize = countAff;
    model->pdLocAffinities = pd->fcts.pdMalloc(pd, sizeof(ocrGuid_t)*countAff);
#ifdef ENABLE_AMT_RESILIENCE
    u64 veclen = (countAff / 64) + 1;
    model->veclen = veclen;
    model->nodevec = pd->fcts.pdMalloc(pd, sizeof(u64)*veclen);
    for (i = 0; i < veclen; i++) model->nodevec[i] = 0;
#endif
    // Returns an array of affinity where each affinity maps to a PD.
    // The array is ordered by PD's location (rank in mpi/gasnet)
    // pd->neighbors is initialized at boot before creating the affinity list
    // These are the known neighbors at startup
    for(i=0; i < pd->neighborCount; i++) {
        ASSERT(pd->neighbors[i] < countAff);
        ocrFatGuid_t fguid;
        pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY, pd->myLocation, GUID_PROP_NONE);
        ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->neighbors[i];
        model->pdLocAffinities[pd->neighbors[i]] = fguid.guid;
    }
    // Do current PD
    model->current = (u32)pd->myLocation;
    ocrFatGuid_t fguid;
    pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY, pd->myLocation, GUID_PROP_NONE);
    ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->myLocation;
    model->pdLocAffinities[model->current] = fguid.guid;

    for(i=0; i < countAff; i++) {
        DPRINTF(DEBUG_LVL_VVERB,"affinityGuid[%"PRId32"]="GUIDF"\n",
                (u32)i, GUIDA(model->pdLocAffinities[i]));
    }

    return (ocrPlatformModel_t *) model;
}

void destroyPlatformModelAffinity(ocrPolicyDomain_t *pd) {
    ocrPlatformModelAffinity_t * model = (ocrPlatformModelAffinity_t *) (pd->platformModel);
    u64 i=0;
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);
    for(i=0; i < pd->neighborCount + 1; ++i) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
      msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
      PD_MSG_FIELD_I(guid.guid) = model->pdLocAffinities[i];
      PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
      PD_MSG_FIELD_I(properties) = 1; // Free metadata
      pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    }

    pd->fcts.pdFree(pd, model->pdLocAffinities);
    pd->fcts.pdFree(pd, model);
    pd->platformModel = NULL;
}


#ifdef TG_XE_TARGET
#include "xstg-map.h"
#include "tg-bin-files.h"

ocrPlatformModel_t * createPlatformModelAffinityXE(ocrPolicyDomain_t *pd) {
    ocrPlatformModelAffinity_t * model = pd->fcts.pdMalloc(pd, sizeof(ocrPlatformModelAffinity_t));
    u64 countAff = pd->neighborCount + 1;
    model->pdLocAffinities = NULL;
    model->pdLocAffinitiesSize = countAff;
    model->pdLocAffinities = pd->fcts.pdMalloc(pd, sizeof(ocrGuid_t)*countAff);
    // Returns an array of affinity where each affinity maps to a PD.
    // The array is ordered by PD's location (locationToIdx in TG)
    u64 i=0;
    u64 clusterCount = 0;
    u64 blockCount = 0;
    u64 myCluster = CLUSTER_FROM_ID(pd->myLocation);
    u64 myBlock = BLOCK_FROM_ID(pd->myLocation);
    ocrLocation_t myBlockLocation = MAKE_CORE_ID(0, 0, 0, myCluster, myBlock, ID_AGENT_CE);
    PD_MSG_STACK(msg);
    // I can reference all other blocks on the system
    for( ; i < countAff; ++i) {
        ocrLocation_t tLocation = MAKE_CORE_ID(0, 0, 0, clusterCount, blockCount, ID_AGENT_CE);
        u64 idx = locationToIdx(tLocation);
        ASSERT(idx < countAff + 1);
        ocrFatGuid_t fguid;
        getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
        msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD_IO(guid.guid) = NULL_GUID;
        PD_MSG_FIELD_I(size) = sizeof(ocrAffinity_t);
        PD_MSG_FIELD_I(kind) = OCR_GUID_AFFINITY;
        PD_MSG_FIELD_I(targetLoc) = myBlockLocation;
        PD_MSG_FIELD_I(properties) = GUID_PROP_NONE;
        RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
        fguid = PD_MSG_FIELD_IO(guid);
        ((ocrAffinity_t*)fguid.metaDataPtr)->place = tLocation;
        model->pdLocAffinities[idx] = fguid.guid;
        blockCount += 1;
        if(blockCount == MAX_NUM_BLOCK) {
            clusterCount += 1;
            blockCount = 0;
        }
#undef PD_MSG
#undef PD_TYPE
    }

    model->current = locationToIdx(MAKE_CORE_ID(0, 0, 0, myCluster, myBlock, ID_AGENT_CE));

    for(i=0; i < countAff; i++) {
        DPRINTF(DEBUG_LVL_VVERB,"affinityGuid[%"PRId32"]="GUIDF"\n",
                (u32)i, GUIDA(model->pdLocAffinities[i]));
    }

    return (ocrPlatformModel_t *) model;
}

u32 locationToIdx(ocrLocation_t loc) {
    return CLUSTER_FROM_ID(loc)*MAX_NUM_BLOCK+BLOCK_FROM_ID(loc);
}
#endif /* end TG_XE_TARGET */


//
// End platform model based on affinities
//

#else

#include "debug.h"

#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "experimental/ocr-platform-model.h"
#include "experimental/ocr-placer.h"
#include "extensions/ocr-affinity.h"

// Stubs
u8 affinityToLocation(ocrLocation_t* result, ocrGuid_t affinityGuid) {

    return OCR_ENOTSUP;
}

#endif /* ENABLE_EXTENSION_AFFINITY */



