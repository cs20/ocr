/**
 * @brief OCR interface to the memory allocators
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_ALLOCATOR_H__
#define __OCR_ALLOCATOR_H__

#include "ocr-mem-target.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#define OCR_ALLOC_HINT_NONE                   0x0
#define OCR_ALLOC_HINT_PDMALLOC               0x20000000  // for pdMalloc
#define OCR_ALLOC_HINT_USER                   0x40000000  // for user DBs
#define OCR_ALLOC_HINT_REDUCE_CONTENTION      0x1         // used by tlsf

struct _ocrPolicyDomain_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

/**
 * @brief Parameter list to create an allocator factory
 */
typedef struct _paramListAllocatorFact_t {
    ocrParamList_t base;
} paramListAllocatorFact_t;

/**
 * @brief Parameter list to create an allocator instance
 */
typedef struct _paramListAllocatorInst_t {
    ocrParamList_t base;
    u64 size;
} paramListAllocatorInst_t;


/****************************************************/
/* OCR ALLOCATOR                                    */
/****************************************************/

struct _ocrAllocator_t;
struct _ocrPolicyDomain_t;

/**
 * @brief Allocator function pointers
 *
 * The function pointers are separate from the allocator instance to allow for
 * the sharing of function pointers for allocators from the same factory
 */
typedef struct _ocrAllocatorFcts_t {
    /**
     * @brief Destructor equivalent
     *
     * Cleans up the allocator. Calls free on self as well as
     * any memory allocated from the low-memory allocators
     *
     * @param self              Pointer to this allocator
     */
    void (*destruct)(struct _ocrAllocator_t* self);

    /**
     * @brief Switch runlevel
     *
     * @param[in] self         Pointer to this object
     * @param[in] PD           Policy domain this object belongs to
     * @param[in] runlevel     Runlevel to switch to
     * @param[in] phase        Phase for this runlevel
     * @param[in] properties   Properties (see ocr-runtime-types.h)
     * @param[in] callback     Callback to call when the runlevel switch
     *                         is complete. NULL if no callback is required
     * @param[in] val          Value to pass to the callback
     *
     * @return 0 if the switch command was successful and a non-zero error
     * code otherwise. Note that the return value does not indicate that the
     * runlevel switch occured (the callback will be called when it does) but only
     * that the call to switch runlevel was well formed and will be processed
     * at some point
     */
    u8 (*switchRunlevel)(struct _ocrAllocator_t* self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                         phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64), u64 val);

    /**
     * @brief Actual allocation
     *
     * Allocates a continuous chunk of memory of the requested size
     *
     * @param self              Pointer to this allocator
     * @param size              Size to allocate (in bytes)
     * @param hints             Hints for the allocation. These
     *                          hints help guide allocation inside the allocator.
     *                          Not all allocators implement all types of hints so
     *                          allocation should not fail if an unknown hint is
     *                          passed in. Hints should also not be relied on
     *                          as directives by the upper-levels. See
     *                          ocrAllocatorHints_t.
     * @return NULL if allocation is unsuccessful
     **/
    void* (*allocate)(struct _ocrAllocator_t *self, u64 size, u64 hints);

#if 0
// The pointer to the "free" function has been removed.  Instead, the
// appropriate "free" function is derived from information stored in
// the data block's header at the time it was allocated.  This information
// allows the Policy Domain to track the block back to the pool it came
// from, both the type and the instance of that pool.
    /**
     * @brief Frees a previously allocated block
     *
     * The block should have been allocated by the same
     * allocator
     *
     * @param self              Pointer to this allocator
     * @param address           Address to free
     **/
    void (*free)(struct _ocrAllocator_t *self, void* address);
#endif

    /**
     * @brief Reallocate within the chunk managed by this allocator
     *
     * @param self              Pointer to this allocator
     * @param address           Address to reallocate
     * @param size              New size
     *
     * Note that regular rules on the special values of
     * address and size apply:
     *   - if address is NULL, equivalent to malloc
     *   - if size is 0, equivalent to free
     */
    void* (*reallocate)(struct _ocrAllocator_t *self, void* address, u64 size);
} ocrAllocatorFcts_t;

struct _ocrMemTarget_t;

/**
 * @brief Allocator is the interface to the allocator to a zone
 * of memory.
 *
 * This is *not* the OS/system memory allocator. This allows memory to be
 * managed in "chunks" (for example one per hierarchical level) and each
 * can have an independent allocator. Specifically, this enables the
 * modeling of scratchpads and makes NUMA memory explicit
 */
typedef struct _ocrAllocator_t {
    ocrFatGuid_t fguid;       /**< The allocator also has a GUID so that
                               * data-blocks can know what allocated/freed them */
    struct _ocrPolicyDomain_t *pd; /**< The PD this allocator belongs to */
#ifdef OCR_ENABLE_STATISTICS
    ocrStatsProcess_t *statProcess;
#endif

    struct _ocrMemTarget_t **memories; /**< Allocators are mapped to ocrMemTarget_t (0+) */
    u64 memoryCount;                   /**< Number of memories associated */

    ocrAllocatorFcts_t fcts;
} ocrAllocator_t;


/****************************************************/
/* OCR ALLOCATOR FACTORY                            */
/****************************************************/

/**
 * @brief Allocator factory
 */
typedef struct _ocrAllocatorFactory_t {
    /**
     * @brief Allocator factory
     *
     * Initiates a new allocator and returns a pointer
     * to it.
     *
     * @param factory       Pointer to this factory
     * @param instanceArg   Arguments specific for the allocator instance
     */
    struct _ocrAllocator_t * (*instantiate)(struct _ocrAllocatorFactory_t * factory,
                                            ocrParamList_t *instanceArg);
    void (*initialize) (struct _ocrAllocatorFactory_t * factory, ocrAllocator_t * derived, ocrParamList_t * perInstance);

    /**
     * @brief Allocator factory destructor
     *
     * @param factory       Pointer to the factory to destruct.
     */
    void (*destruct)(struct _ocrAllocatorFactory_t * factory);

    ocrAllocatorFcts_t allocFcts;
} ocrAllocatorFactory_t;

void initializeAllocatorOcr(ocrAllocatorFactory_t *factory, ocrAllocator_t *self, ocrParamList_t *perInstance);

#endif /* __OCR_ALLOCATOR_H__ */
