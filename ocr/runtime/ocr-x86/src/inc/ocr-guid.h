/**
 * @brief OCR internal API to GUID management
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_GUID_H__
#define __OCR_GUID_H__

#include "ocr-types.h"
#include "ocr-mappable.h"
#include "ocr-utils.h"

typedef enum {
    OCR_GUID_NONE = 0,
    OCR_GUID_ALLOCATOR = 1,
    OCR_GUID_DB = 2,
    OCR_GUID_EDT = 3,
    OCR_GUID_EDT_TEMPLATE = 4,
    OCR_GUID_EVENT = 5,
    OCR_GUID_POLICY = 6,
    OCR_GUID_WORKER = 7
} ocrGuidKind;

/****************************************************/
/* OCR PARAMETER LISTS                              */
/****************************************************/
typedef struct _paramListGuidProviderFact_t {
    ocrParamList_t base;
} paramListGuidProviderFact_t;

typedef struct _paramListGuidProviderInst_t {
    ocrParamList_t base;
} paramListGuidProviderInst_t;

/****************************************************/
/* OCR GUID PROVIDER                                */
/****************************************************/

struct _ocrGuidProvider_t;

typedef struct _ocrGuidProviderFcts_t {
    /**
     * @brief Destructor equivalent
     *
     * This will free the GUID provider and any
     * memory that it uses
     *
     * @param self          Pointer to this GUID provider
     */
    void (*destruct)(struct _ocrGuidProvider_t* self);

    /**
     * @brief Returns a GUID for an object of type 'type'
     * and associates the value val.
     *
     * The GUID provider basically associates a value with the
     * GUID (can be a pointer to the metadata for example). This
     * creates an association
     *
     * @param self          Pointer to this GUID provider
     * @param guid          GUID returned
     * @param val           Value to be associated
     * @param type          Type of the object that will be associated with the GUID
     * @return 0 on success or an error code
     */
    u8 (*getGuid)(struct _ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val,
                  ocrGuidKind type);

    /**
     * @brief Returns the associated value for the GUID 'guid'
     *
     * @param self          Pointer to this GUID provider
     * @param guid          GUID to "translate"
     * @param val           Value to return
     * @param kind          Kind to return. Can be NULL if the user does not care about the kind
     *
     * @return 0 on success or an error code
     */
    u8 (*getVal)(struct _ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind);

    /**
     * @brief Returns the kind of a GUID
     *
     * @param self          Pointer to this GUID provider
     * @param guid          GUID to get the type of
     * @param kind          Kind returned. Can be NULL if the user does not care about the kind
     * @return 0 on success or an error code
     */
    u8 (*getKind)(struct _ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind);

    /**
     * @brief Releases the GUID
     *
     * This potentially allows the GUID provider to re-issue this same GUID
     * for a different object
     *
     * @param self          Pointer to this GUID provider
     * @param guid          GUID to release
     * @return 0 on success or an error code
     */
    u8 (*releaseGuid)(struct _ocrGuidProvider_t *self, ocrGuid_t guid);
} ocrGuidProviderFcts_t;

/**
 * @brief Provider for GUIDs for the system
 *
 * GUIDs should be unique and are used to
 * identify and locate objects (and their associated
 * metadata mostly). GUIDs serve as a level of indirection
 * to allow objects to move around in the system and
 * support different address spaces (in the future)
 */
typedef struct _ocrGuidProvider_t {
    ocrMappable_t module;

    ocrGuidProviderFcts_t *fctPtrs;
} ocrGuidProvider_t;

/****************************************************/
/* OCR GUID PROVIDER FACTORY                        */
/****************************************************/

typedef struct _ocrGuidProviderFactory_t {
    ocrMappable_t base;

    ocrGuidProvider_t* (*instantiate)(struct _ocrGuidProviderFactory_t *factory, ocrParamList_t* perInstance);

    void (*destruct)(struct _ocrGuidProviderFactory_t *factory);

    ocrGuidProviderFcts_t providerFcts;
} ocrGuidProviderFactory_t;

#define UNINITIALIZED_GUID ((ocrGuid_t)-2)

#define ERROR_GUID ((ocrGuid_t)-1)

/****************************************************/
/* OCR GUID CONVENIENCE FUNCTIONS                   */
/****************************************************/

static inline u8 guidKind(struct _ocrPolicyDomain_t * pd, ocrGuid_t guid,
                          ocrGuidKind* kindRes) __attribute__((unused));
// TODO: REC: Actually pass a context
static inline u8 guidify(struct _ocrPolicyDomain_t * pd, u64 ptr, ocrGuid_t * guidRes,
                         ocrGuidKind kind) __attribute__((unused));
static inline u8 deguidify(struct _ocrPolicyDomain_t * pd, ocrGuid_t guid, u64* ptrRes,
                           ocrGuidKind* kindRes) __attribute__((unused));
static inline bool isDatablockGuid(ocrGuid_t guid) __attribute__((unused));
static inline bool isEventGuid(ocrGuid_t guid) __attribute__((unused));
static inline bool isEdtGuid(ocrGuid_t guid) __attribute__((unused));
static inline bool isEventLatchGuid(ocrGuid_t guid) __attribute__((unused));
static inline bool isEventSingleGuid(ocrGuid_t guid) __attribute__((unused));

#endif /* __OCR_GUID__H_ */
