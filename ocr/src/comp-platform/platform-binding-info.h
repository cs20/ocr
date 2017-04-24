/**
 * @brief Platform binding information to actual system resources
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __PLATFORM_BINDING_INFO_H__
#define __PLATFORM_BINDING_INFO_H__

#include "ocr-types.h"

typedef struct {
    s32 offset;
    u16 idsPerPackage;
    u8 nbPackages;
    u8 pdAllocPolicy;
} bindingInfo_t;

#endif /* __PLATFORM_BINDING_INFO_H__ */



