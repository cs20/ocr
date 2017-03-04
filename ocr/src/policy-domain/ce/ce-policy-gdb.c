/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_CE

#define DEBUG_TYPE POLICY
void __ceWaitForGdb() {
#ifdef TG_GDB_SUPPORT
    volatile u32 i=0;
    for(; i < 100; i++)
        ;
#endif
}
#endif