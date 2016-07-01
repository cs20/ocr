/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


/*
 * Defines parts of the GUID bit map and utils functions.
 *
 * Implementations are responsible for defining the full bit map.
 */


// Locations can be compounded:
// 1) HOME: where the GUID officially lives
// 2) GEN:  where it has been generated
// 3) WID:  a worker ID

// 1) Home location of the GUID
// - can be set from common.mk
#ifndef GUID_PROVIDER_LOCID_SIZE
#define GUID_LOCHOME_SIZE   7 // Warning! 2^7 locId max, bump that up for more.
#else
#define GUID_LOCHOME_SIZE   GUID_PROVIDER_LOCID_SIZE
#endif

// 2) Allocation location of the GUID
// - Same size as the home location
#ifdef GUID_PROVIDER_LOC_MANGLE // TODO find a better name
#define GUID_LOCALLOC_SIZE   GUID_LOCHOME_SIZE
#else
#define GUID_LOCALLOC_SIZE   0
#endif

// 3) Worker ID location
// - This has to be able to accomodate the max number of workers per location
#ifdef GUID_PROVIDER_WID_INGUID
#ifndef GUID_PROVIDER_WID_SIZE
#define GUID_LOCWID_SIZE    4
#else
#define GUID_LOCWID_SIZE    GUID_PROVIDER_WID_SIZE
#endif
#else
#define GUID_LOCWID_SIZE    0
#endif

// Sizes for each field
#define GUID_LOCID_SIZE     (GUID_LOCHOME_SIZE+GUID_LOCALLOC_SIZE+GUID_LOCWID_SIZE)
#define GUID_KIND_SIZE      5 // Warning! check ocrGuidKind struct definition for correct size

// NOTE: Counter size and the exact bitmap are defined in implementations

// Helper Macros

// Size of the field
#define SIZE(name)          (GUID_##name##_SIZE)
// Start index of the field (MSB)
#define SIDX(name)          (SIDX_##name)
// End index of the field (LSB)
#define EIDX(name)          (SIDX(name)-SIZE(name))
// Mask of a field at the LSB
#define MASK_ZB(name)       ((((u64)1)<<(GUID_##name##_SIZE))-1)
// Mask for a field at its rightful position in the GUID
#define GUID_MASK(name)     (MASK_ZB(name)<<(EIDX(name)))
// Shift a value representing a field toward MSB at its rightful position in the GUID
#define LSHIFT(name, val)   (((u64)val) << EIDX(name))
// Shift a value representing a field from its position
// in the GUID toward LSB and masks the result
#define RSHIFT(name, val)   ((val >> EIDX(name)) & (MASK_ZB(name)))
#define MAX_VAL(name)       (((u64)1) << SIZE(name))
