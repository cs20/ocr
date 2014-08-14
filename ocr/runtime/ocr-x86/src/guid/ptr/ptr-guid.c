/**
 * @brief Trivial implementation of GUIDs
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_GUID_PTR

#include "debug.h"
#include "guid/ptr/ptr-guid.h"
#include "ocr-policy-domain.h"

#include "ocr-sysboot.h"

typedef struct {
    ocrGuid_t guid;
    ocrGuidKind kind;
    ocrLocation_t location;
} ocrGuidImpl_t;

void ptrDestruct(ocrGuidProvider_t* self) {
    runtimeChunkFree((u64)self, NULL);
}

void ptrBegin(ocrGuidProvider_t *self, ocrPolicyDomain_t *pd) {
    // Nothing else to do
}

void ptrStart(ocrGuidProvider_t *self, ocrPolicyDomain_t *pd) {
    self->pd = pd;
    // Nothing else to do
}

void ptrStop(ocrGuidProvider_t *self) {
    // Nothing to do
}

void ptrFinish(ocrGuidProvider_t *self) {
    // Nothing to do
}

u8 ptrGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *policy = NULL;
    ocrTask_t *task = NULL;
    getCurrentEnv(&policy, NULL, &task, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(size) = sizeof(ocrGuidImpl_t);
    PD_MSG_FIELD(properties) = 0; // TODO:  What flags should be defined?  Where are symbolic constants for them defined?
    PD_MSG_FIELD(type) = GUID_MEMTYPE;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));

    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *)PD_MSG_FIELD(ptr);
    guidInst->guid = (ocrGuid_t)val;
    guidInst->kind = kind;
    guidInst->location = self->pd->myLocation;
    *guid = (ocrGuid_t) guidInst;
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 ptrCreateGuid(ocrGuidProvider_t* self, ocrFatGuid_t *fguid, u64 size, ocrGuidKind kind) {
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *policy = NULL;
    ocrTask_t *task = NULL;
    getCurrentEnv(&policy, NULL, &task, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(size) = sizeof(ocrGuidImpl_t) + size;
    PD_MSG_FIELD(properties) = 0; // TODO:  What flags should be defined?  Where are symbolic constants for them defined?
    PD_MSG_FIELD(type) = GUID_MEMTYPE;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));

    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *)PD_MSG_FIELD(ptr);
    guidInst->guid = (ocrGuid_t)((u64)guidInst + sizeof(ocrGuidImpl_t));
    guidInst->kind = kind;
    guidInst->location = self->pd->myLocation;
    fguid->guid = (ocrGuid_t)guidInst;
    fguid->metaDataPtr = (void*)((u64)guidInst + sizeof(ocrGuidImpl_t));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 ptrGetVal(ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind) {
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *val = (u64) guidInst->guid;
    if(kind)
        *kind = guidInst->kind;
    return 0;
}

u8 ptrGetKind(ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind) {
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *kind = guidInst->kind;
    return 0;
}

u8 ptrGetLocation(ocrGuidProvider_t* self, ocrGuid_t guid, ocrLocation_t* location) {
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *location = guidInst->location;
    return 0;
}

u8 ptrRegisterGuid(ocrGuidProvider_t* self, ocrGuid_t guid, u64 val) {
    ASSERT(0); // Not supported
    return 0;
}

u8 ptrReleaseGuid(ocrGuidProvider_t *self, ocrFatGuid_t guid, bool releaseVal) {
    if(releaseVal) {
        ASSERT(guid.metaDataPtr);
        ASSERT((u64)guid.metaDataPtr == (u64)guid.guid + sizeof(ocrGuidImpl_t));
    }
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *policy = NULL;
    getCurrentEnv(&policy, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_UNALLOC
    msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(ptr) = ((void *) guid.guid);
    PD_MSG_FIELD(type) = GUID_MEMTYPE;
    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

ocrGuidProvider_t* newGuidProviderPtr(ocrGuidProviderFactory_t *factory,
                                      ocrParamList_t *perInstance) {
    ocrGuidProvider_t *base = (ocrGuidProvider_t*)runtimeChunkAlloc(
                                  sizeof(ocrGuidProviderPtr_t), NULL);
    base->fcts = factory->providerFcts;
    base->pd = NULL;
    base->id = factory->factoryId;
    return base;
}

/****************************************************/
/* OCR GUID PROVIDER PTR FACTORY                    */
/****************************************************/

void destructGuidProviderFactoryPtr(ocrGuidProviderFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrGuidProviderFactory_t *newGuidProviderFactoryPtr(ocrParamList_t *typeArg, u32 factoryId) {
    ocrGuidProviderFactory_t *base = (ocrGuidProviderFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrGuidProviderFactoryPtr_t), (void *)1);

    base->instantiate = &newGuidProviderPtr;
    base->destruct = &destructGuidProviderFactoryPtr;
    base->factoryId = factoryId;
    base->providerFcts.destruct = FUNC_ADDR(void (*)(ocrGuidProvider_t*), ptrDestruct);
    base->providerFcts.begin = FUNC_ADDR(void (*)(ocrGuidProvider_t*, ocrPolicyDomain_t*), ptrBegin);
    base->providerFcts.start = FUNC_ADDR(void (*)(ocrGuidProvider_t*, ocrPolicyDomain_t*), ptrStart);
    base->providerFcts.stop = FUNC_ADDR(void (*)(ocrGuidProvider_t*), ptrStop);
    base->providerFcts.finish = FUNC_ADDR(void (*)(ocrGuidProvider_t*), ptrFinish);
    base->providerFcts.getGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64, ocrGuidKind), ptrGetGuid);
    base->providerFcts.createGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t*, u64, ocrGuidKind), ptrCreateGuid);
    base->providerFcts.getVal = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64*, ocrGuidKind*), ptrGetVal);
    base->providerFcts.getKind = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrGuidKind*), ptrGetKind);
    base->providerFcts.getLocation = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrLocation_t*), ptrGetLocation);
    base->providerFcts.registerGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64), ptrRegisterGuid);
    base->providerFcts.releaseGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t, bool), ptrReleaseGuid);
    return base;
}

#endif /* ENABLE_GUID_PTR */
