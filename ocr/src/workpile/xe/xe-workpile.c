/*
 * This file is subject to the license agreement located in the file LIXENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKPILE_XE

#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "workpile/xe/xe-workpile.h"


/******************************************************/
/* OCR-XE WorkPile                                    */
/******************************************************/

void xeWorkpileDestruct ( ocrWorkpile_t * base ) {
    runtimeChunkFree((u64)base, NULL);
}

u8 xeWorkpileSwitchRunlevel(ocrWorkpile_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                            phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ocrAssert(callback == NULL);

    // Verify properties for this call
    ocrAssert((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ocrAssert(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP)
            self->pd = PD;
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        // We have memory, we can now allocate a deque
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_GUID_OK, phase)) {
            ocrWorkpileXe_t* derived = (ocrWorkpileXe_t*)self;
            derived->deque = newDeque(self->pd, (void *) NULL, WORK_STEALING_DEQUE);
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
            ocrWorkpileXe_t* derived = (ocrWorkpileXe_t*)self;
            derived->deque->destruct(PD, derived->deque);
        }
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_WORKPILE);
            }
        } else {
            // Tear-down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
                msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(guid) = self->fguid;
                PD_MSG_FIELD_I(properties) = 0;
                toReturn |= self->pd->fcts.processMessage(self->pd, &msg, false);
                self->fguid.guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
            }
        }
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ocrAssert(0);
    }
    return toReturn;
}

ocrFatGuid_t xeWorkpilePop(ocrWorkpile_t * base, ocrWorkPopType_t type,
                           ocrCost_t *cost) {
    ocrWorkpileXe_t* derived = (ocrWorkpileXe_t*) base;
    ocrFatGuid_t fguid, * fguidp;
    fguid.guid = NULL_GUID;
    fguid.metaDataPtr = NULL;
    switch(type) {
    case POP_WORKPOPTYPE:
        fguidp = derived->deque->popFromHead(derived->deque, 0);
        if (fguidp != NULL) {
            fguid.guid = fguidp->guid;
            fguid.metaDataPtr = fguidp->metaDataPtr;
            base->pd->fcts.pdFree(base->pd, fguidp);
        }
        break;
    default:
        ocrAssert(0);
    }
    return fguid;
}

void xeWorkpilePush(ocrWorkpile_t * base, ocrWorkPushType_t type,
                    ocrFatGuid_t g ) {
    ocrWorkpileXe_t* derived = (ocrWorkpileXe_t*) base;
    ocrFatGuid_t* gp = base->pd->fcts.pdMalloc(base->pd, sizeof(ocrFatGuid_t));
    gp->guid = g.guid;
    gp->metaDataPtr = g.metaDataPtr;
    derived->deque->pushAtTail(derived->deque, (void *)(gp), 0);
}

ocrWorkpile_t * newWorkpileXe(ocrWorkpileFactory_t * factory, ocrParamList_t *perInstance) {
    ocrWorkpile_t* derived = (ocrWorkpile_t*) runtimeChunkAlloc(sizeof(ocrWorkpileXe_t), NULL);

    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeWorkpileXe(ocrWorkpileFactory_t * factory, ocrWorkpile_t* self, ocrParamList_t * perInstance) {
    initializeWorkpileOcr(factory, self, perInstance);
}


/******************************************************/
/* OCR-XE WorkPile Factory                            */
/******************************************************/

void destructWorkpileFactoryXe(ocrWorkpileFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkpileFactory_t * newOcrWorkpileFactoryXe(ocrParamList_t *perType) {
    ocrWorkpileFactory_t* base = (ocrWorkpileFactory_t*)runtimeChunkAlloc(sizeof(ocrWorkpileFactoryXe_t), NULL);

    base->instantiate = &newWorkpileXe;
    base->initialize = &initializeWorkpileXe;
    base->destruct = &destructWorkpileFactoryXe;

    base->workpileFcts.destruct = FUNC_ADDR(void (*)(ocrWorkpile_t*), xeWorkpileDestruct);
    base->workpileFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrWorkpile_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), xeWorkpileSwitchRunlevel);
    base->workpileFcts.pop = FUNC_ADDR(ocrFatGuid_t (*)(ocrWorkpile_t*, ocrWorkPopType_t, ocrCost_t*), xeWorkpilePop);
    base->workpileFcts.push = FUNC_ADDR(void (*)(ocrWorkpile_t*, ocrWorkPushType_t, ocrFatGuid_t), xeWorkpilePush);

    return base;
}
#endif /* ENABLE_WORKPILE_XE */
