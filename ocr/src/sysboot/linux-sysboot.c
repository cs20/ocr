/**
 * @brief System boot dependences for x86
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SYSBOOT_LINUX
#include "debug.h"

#include "ocr-mem-target.h"
#include "ocr-sysboot.h"

#include <stdlib.h>
#include <stdio.h>


static void * packedUserArgs = NULL;
static ocrEdt_t mainEdt = NULL;

void x86UserArgsSet(void * packedArgs) {
    packedUserArgs = packedArgs;
}

void * x86UserArgsGet() {
    return packedUserArgs;
}

void x86MainEdtSet(ocrEdt_t mainEdtFct) {
    mainEdt = mainEdtFct;
}

ocrEdt_t x86MainEdtGet() {
    return mainEdt;
}

u64 x86RuntimeChunkAlloc(u64 size, u64 *extra) {
    void* returnValue = calloc(1, size);
    ASSERT(returnValue);
    return (u64)returnValue;
}

void x86RuntimeChunkFree(u64 addr, u64* extra) {
    ASSERT(addr);
    free((void*)addr);
}

void x86RuntimeUpdateMemTarget(ocrMemTarget_t *me, u64 extra) {
    // On Linux, we don't need to do anything since the mallocs never
    // overlap
    return;
}

void x86BootUpAbort() {
    abort();
}

void x86BootUpPrint(const char* str, u64 length) {
    printf("%s", str);
}

void (*userArgsSet)(void *) = &x86UserArgsSet;
void * (*userArgsGet)() = &x86UserArgsGet;
void (*mainEdtSet)(ocrEdt_t) = &x86MainEdtSet;
ocrEdt_t (*mainEdtGet)() = &x86MainEdtGet;
u64 (*runtimeChunkAlloc)(u64, u64*) = &x86RuntimeChunkAlloc;
void (*runtimeChunkFree)(u64, u64*) = &x86RuntimeChunkFree;
void (*runtimeUpdateMemTarget)(ocrMemTarget_t *, u64) = &x86RuntimeUpdateMemTarget;
void (*bootUpAbort)() = &x86BootUpAbort;
void (*bootUpPrint)(const char*, u64) = &x86BootUpPrint;


#endif /* ENABLE_SYSBOOT_LINUX */
