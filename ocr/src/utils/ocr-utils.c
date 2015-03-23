/**
 * @brief Utility functions for OCR
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifdef OCR_ENABLE_VISUALIZER
#include <time.h>
#endif

#include "ocr-hal.h"
#include "debug.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/*  ABORT / EXIT OCR                                  */
/******************************************************/
// TODO: These need to move. SAL or HAL?
void ocr_abort() {
    hal_abort();
}

void ocr_exit() {
    hal_exit(1);
}


u32 fls16(u16 val) {
    u32 bit = 15;

    if(!(val & 0xff00)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x8000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

u32 fls32(u32 val) {
    u32 bit = 31;

    if(!(val & 0xffff0000)) {
        val <<= 16;
        bit -= 16;
    }
    if(!(val & 0xff000000)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf0000000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc0000000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x80000000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

u32 fls64(u64 val) {
    u32 bit = 63;

    if(!(val & 0xffffffff00000000)) {
        val <<= 32;
        bit -= 32;
    }
    if(!(val & 0xffff000000000000)) {
        val <<= 16;
        bit -= 16;
    }
    if(!(val & 0xff00000000000000)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf000000000000000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc000000000000000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x8000000000000000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

void ocrGuidTrackerInit(ocrGuidTracker_t *self) {
    self->slotsStatus = 0xFFFFFFFFFFFFFFFFULL;
}

u32 ocrGuidTrackerTrack(ocrGuidTracker_t *self, ocrGuid_t toTrack) {
    u32 slot = 64;
    if(self->slotsStatus == 0) return slot;
    slot = fls64(self->slotsStatus);
    self->slotsStatus &= ~(1ULL<<slot);
    ASSERT(slot <= 63);
    self->slots[slot] = toTrack;
    return slot;
}

bool ocrGuidTrackerRemove(ocrGuidTracker_t *self, ocrGuid_t toTrack, u32 id) {
    if(id > 63) return false;
    if(self->slots[id] != toTrack) return false;

    self->slotsStatus |= (1ULL<<(id));
    return true;
}

u32 ocrGuidTrackerIterateAndClear(ocrGuidTracker_t *self) {
    u64 rstatus = ~(self->slotsStatus);
    u32 slot;
    if(rstatus) return 64;
    slot = fls64(rstatus);
    self->slotsStatus |= (1ULL << slot);
    return slot;
}

u32 ocrGuidTrackerFind(ocrGuidTracker_t *self, ocrGuid_t toFind) {
    u32 result = 64, slot;
    u64 rstatus = ~(self->slotsStatus);
    while(rstatus) {
        slot = fls64(rstatus);
        rstatus &= ~(1ULL << slot);
        if(self->slots[slot] == toFind) {
            result = slot;
            break;
        }
    }
    return result;
}

s32 ocrStrcmp(u8 *str1, u8 *str2) {
    u32 index = 0;
    while((str1[index] != '\0') && (str2[index] != '\0')) {
        if(str1[index] == str2[index]) index++;
        else break;
    }
    return(str1[index]-str2[index]);
}

u64 ocrStrlen(const char* str) {
    u64 res = 0;
    if(str == NULL) return res;
    while(*str++ != '\0') ++res;
    return res;
}

#ifdef OCR_ENABLE_VISUALIZER
u64 getTimeNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000UL + (u64) ts.tv_nsec;
}
#endif


/* This is not currently used. What to do with it?
void ocrPlaceTrackerAllocate ( ocrPlaceTracker_t** toFill ) {
    *toFill = (ocrPlaceTracker_t*) malloc(sizeof(ocrPlaceTracker_t));
}

void ocrPlaceTrackerInsert ( ocrPlaceTracker_t* self, unsigned char currPlace ) {
    self->existInPlaces |= (1ULL << currPlace);
}

void ocrPlaceTrackerRemove ( ocrPlaceTracker_t* self, unsigned char currPlace ) {
    self->existInPlaces &= ~(1ULL << currPlace);
}

void ocrPlaceTrackerInit( ocrPlaceTracker_t* self ) {
    self->existInPlaces = 0ULL;
}
*/