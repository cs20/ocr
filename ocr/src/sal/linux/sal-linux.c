/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#include "debug.h"
#ifdef SAL_LINUX

#define DEBUG_TYPE SAL
#include "ocr-types.h"
#include "ocr-errors.h"

//Including platform specific headers for fault injection
#ifdef ENABLE_RESILIENCY
#include "policy-domain/hc/hc-policy.h"
#include "task/hc/hc-task.h"
#endif
#include "utils/hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>

#if defined(linux) || defined(__APPLE__)
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <dirent.h>
#endif

#ifdef __MACH__

#include <mach/mach_time.h>
static double conversion_factor;
static int getTimeNs_initialized = 0;

u64 salGetTime(){
    double timeStamp;
    if(!getTimeNs_initialized){
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        conversion_factor = (double)timebase.numer / (double)timebase.denom;
        getTimeNs_initialized = 1;
    }
    timeStamp = mach_absolute_time()*conversion_factor;
    return (u64)timeStamp;
}

#else
#include <time.h>

u64 salGetTime(){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000UL + ((u64)ts.tv_nsec);

}
#endif /*__MACH__*/

#ifdef ENABLE_EXTENSION_PERF

static u64 perfEventOpen(struct perf_event_attr *hw_event)
{
   u64 ret;

   ret = syscall(__NR_perf_event_open, hw_event, 0, -1, -1, 0);
                                               // pid, cpu, group_perfFd, flags
   return ret;
}

// FIXME: The following should come from config file
s32 counter_type[PERF_HW_MAX] = { PERF_TYPE_HARDWARE, PERF_TYPE_HARDWARE, PERF_TYPE_HARDWARE, PERF_TYPE_RAW };
s32 counter_cfg [PERF_HW_MAX] = { PERF_COUNT_HW_BUS_CYCLES, PERF_COUNT_HW_CACHE_REFERENCES, PERF_COUNT_HW_CACHE_MISSES, 0xf110 };

u64 salPerfInit(salPerfCounter* perfCtr) {
    u32 i;
    u32 retval = 0;

    for(i = 0; i < PERF_HW_MAX; i++) {
        memset(&perfCtr[i].perfAttr, 0, sizeof(perfCtr[i].perfAttr));
        perfCtr[i].perfAttr.type = counter_type[i];
        perfCtr[i].perfAttr.size = sizeof(struct perf_event_attr);
        perfCtr[i].perfAttr.config = counter_cfg[i];
        perfCtr[i].perfAttr.disabled = 1;
        perfCtr[i].perfAttr.exclude_kernel = 1;
        perfCtr[i].perfAttr.exclude_hv = 1;

        perfCtr[i].perfFd = perfEventOpen(&perfCtr[i].perfAttr);
        if (perfCtr[i].perfFd == -1) {
            DPRINTF(DEBUG_LVL_WARN, "Error opening counter 0x%"PRIx64"\n", (u64)perfCtr[i].perfAttr.config);
            retval = OCR_EFAULT;
        }
    }

    return retval;
}

u64 salPerfStart(salPerfCounter* perfCtr) {
    u32 i;
    u32 retval = 0;

    for(i = 0; i < PERF_HW_MAX; i++) {
        retval = ioctl(perfCtr[i].perfFd, PERF_EVENT_IOC_RESET, 0);
        if(retval) DPRINTF(DEBUG_LVL_WARN, "Unable to reset counter %"PRId32"\n", i);
        retval = ioctl(perfCtr[i].perfFd, PERF_EVENT_IOC_ENABLE, 0);
        if(retval) DPRINTF(DEBUG_LVL_WARN, "Unable to enable counter %"PRId32"\n", i);
    }

    return retval;
}

u64 salPerfStop(salPerfCounter* perfCtr) {
    u32 i;
    u32 retval = 0;

    for(i = 0; i < PERF_HW_MAX; i++) {
        retval = ioctl(perfCtr[i].perfFd, PERF_EVENT_IOC_DISABLE, 0);
        if(retval) DPRINTF(DEBUG_LVL_WARN, "Unable to disable counter %"PRId32"\n", i);
        retval = read(perfCtr[i].perfFd, &perfCtr[i].perfVal, sizeof(u64));
        if(retval < 0) DPRINTF(DEBUG_LVL_WARN, "Unable to read counter %"PRId32"\n", i);
    }
    // Convert L1_REFERENCES to L1_HITS by subtracting misses
    perfCtr[PERF_L1_HITS].perfVal -= perfCtr[PERF_L1_MISSES].perfVal;

    return retval;
}

u64 salPerfShutdown(salPerfCounter *perfCtr) {
    u32 i;
    u32 retval = 0;

    for(i = 0; i < PERF_HW_MAX; i++) {
        close(perfCtr[i].perfFd);
    }
    return retval;
}

#endif

#ifdef ENABLE_RESILIENCY

#define FD_CHKPT_INITVAL -1
int fdChkpt = FD_CHKPT_INITVAL;
u64 chkptPhase = 0;

typedef enum {
    YEAR,
    MONTH,
    DAY,
    HOUR,
    MINUTE,
    SECOND,
    NUM_TIME_UNITS
} timeUnits;

u64 salGetCalTime() {
    int i;
    char fname[PATH_MAX];
    fname[0] = '\0';
    time_t t = time(NULL);
    struct tm curTime = *(localtime(&t));
    for (i = 0; i < NUM_TIME_UNITS; i++) {
        int t = 0;
        switch(i) {
        case YEAR:
            t = 1900 + curTime.tm_year;
            break;
        case MONTH:
            t = 1 + curTime.tm_mon;
            break;
        case DAY:
            t = curTime.tm_mday;
            break;
        case HOUR:
            t = curTime.tm_hour;
            break;
        case MINUTE:
            t = curTime.tm_min;
            break;
        case SECOND:
            t = curTime.tm_sec;
            break;
        default:
            break;
        }
        if (t < 10) sprintf(fname, "%s0%d", fname, t);
        else        sprintf(fname, "%s%d", fname, t);
    }
    return strtoul(fname, NULL, 10);
}

static const char* salGetExecutableName() {
    static char *execName = NULL;
    if (execName == NULL) {
        ocrPolicyDomain_t *pd;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        char filenameBuf[4096];
        u64 filenameBufSize = readlink("/proc/self/exe", filenameBuf, 4096);
        if (filenameBufSize <= 0) {
            fprintf(stderr, "readlink failed\n");
            ASSERT(0);
            return NULL;
        }
        char *filename = pd->fcts.pdMalloc(pd, filenameBufSize + 1);
        strncpy(filename, filenameBuf, filenameBufSize);
        filename[filenameBufSize] = '\0';
        execName = filename;
    }
    return (const char*)execName;
}

static const char* salGetCheckpointSummaryFileName() {
    static char *checkpointSummaryName = NULL;
    if (checkpointSummaryName == NULL) {
        ocrPolicyDomain_t *pd;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        const char* execName = salGetExecutableName();
        char appendStr[] = ".chkpt\0";
        u32 filenamesize = strlen(execName) + strlen(appendStr) + 1;
        char *filename = pd->fcts.pdMalloc(pd, filenamesize + 1);
        int rc = snprintf(filename, filenamesize, "%s%s", execName, appendStr);
        if (rc < 0 || rc >= filenamesize) {
            fprintf(stderr, "snprintf failed: (filename: %s)\n", filename);
            ASSERT(0);
            return NULL;
        }
        filename[filenamesize] = '\0';
        checkpointSummaryName = filename;
    }
    return (const char*)checkpointSummaryName;
}

static const char* salConstructPdCheckpointFileName(u64 calTime, u64 phase, u64 loc) {
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    char filenameBuf[4096];
    const char* execName = salGetExecutableName();
    sprintf(filenameBuf, "%s.%lu.%lu.%lu.chkpt", execName, calTime, phase, loc);
    u64 filenameBufSize = strlen(filenameBuf);
    char *filename = pd->fcts.pdMalloc(pd, filenameBufSize + 1);
    strncpy(filename, filenameBuf, filenameBufSize);
    filename[filenameBufSize] = '\0';
    return (const char*)filename;
}

static u8 salGetCheckpointNameTokens(char *checkpointStr, u64 *time, u64 *phase, u64 *loc) {
    char *tok = strtok(checkpointStr, ".");
    const char* execName = salGetExecutableName();
    if (strcmp(tok, execName) != 0)
        return 1;

    tok = strtok(NULL, ".");
    if (tok == NULL)
        return 1;
    u64 calTime = strtoul(tok, NULL, 10);
    ASSERT(calTime <= salGetCalTime());

    tok = strtok(NULL, ".");
    if (tok == NULL)
        return 1;
    u64 ph = strtoul(tok, NULL, 10);
    ASSERT(ph > 0);

    tok = strtok(NULL, ".");
    if (tok == NULL)
        return 1;
    u64 location = strtoul(tok, NULL, 10);

    *time = calTime;
    *phase = ph;
    *loc = location;
    return 0;
}

//Create a new checkpoint buffer
u8* salCreatePdCheckpoint(char **name, u64 size) {
    ASSERT(name);

    if (fdChkpt >= 0) {
        fprintf(stderr, "Cannot open new checkpoint buffer. Previous buffer has not been closed yet. \n");
        ASSERT(0);
        return NULL;
    }

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *hcPolicy = (ocrPolicyDomainHc_t*)pd;
    const char *filename = salConstructPdCheckpointFileName(hcPolicy->calTime, ++chkptPhase, pd->myLocation);

    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", filename);
        ASSERT(0);
        return NULL;
    }

    size = ((size >> 12) + !!(size & 0xFFFULL)) << 12; //Align size to 4096 bytes
    if (fd>=0) {
        int rc = ftruncate(fd, size);
        if (rc) {
            fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", filename, fd);
            ASSERT(0);
            return NULL;
        }
    }

    u8 *ptr = (u8*)mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, filename, fd);
        ASSERT(0);
        return NULL;
    }

    *name = (char*)filename;
    fdChkpt = fd;
    return ptr;
}

//Open a previously closed stable checkpoint
//salGetCheckpointName() provides the stable checkpoint name
u8* salOpenPdCheckpoint(char *name, u64 *size) {
    if (fdChkpt >= 0) {
        fprintf(stderr, "Cannot open new checkpoint buffer. Previous buffer has not been closed yet. \n");
        ASSERT(0);
        return NULL;
    }

    ASSERT(name);
    const char *filename = name;
    struct stat sb;
    if (stat(filename, &sb) == -1) {
        fprintf(stderr, "stat failed: (filename: %s)\n", filename);
        ASSERT(0);
        return NULL;
    }

    u64 filesize = sb.st_size;
    ASSERT(filesize > 0);

    int fd = open(filename, O_RDWR, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", filename);
        ASSERT(0);
        return NULL;
    }

    u8 *ptr = (u8*)mmap( NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", filesize, filename, fd);
        ASSERT(0);
        return NULL;
    }

    if (size)
        *size = filesize;

    fdChkpt = fd;
    return ptr;
}

//Close a previously opened checkpoint buffer
u8 salClosePdCheckpoint(u8 *buffer, u64 size) {
    ASSERT(buffer);
    if (fdChkpt < 0) {
        fprintf(stderr, "Invalid buffer %p. No checkpoint buffer found. \n", buffer);
        ASSERT(0);
        return 1;
    }

    int rc = msync(buffer, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buffer, size);
        ASSERT(0);
        return 1;
    }

    rc = munmap(buffer, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buffer, size);
        ASSERT(0);
        return 1;
    }

    rc = close(fdChkpt);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fdChkpt);
        ASSERT(0);
        return 1;
    }

    fdChkpt = FD_CHKPT_INITVAL;
    return 0;
}

//Remove a checkpoint
u8 salRemovePdCheckpoint(char *name) {
    if (name == NULL)
        return 0;
    int rc = unlink(name);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", name);
        ASSERT(0);
        return 1;
    }
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    pd->fcts.pdFree(pd, name);
    return 0;
}

//Mark the checkpoint name as stable.
//This updates the checkpoint summary file
u8 salSetPdCheckpoint(char *name) {
    ASSERT(name != NULL && chkptPhase > 0);
    if (strlen(name) >= 4096) {
        ASSERT(0);
        return 1;
    }

    const char* filename = salGetCheckpointSummaryFileName();
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "fopen failed: (filename: %s)\n", filename);
        ASSERT(0);
        return 1;
    }

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *hcPolicy = (ocrPolicyDomainHc_t*)pd;
    char checkpointStr[4096];
    strcpy(checkpointStr, name);
    u64 calTime = 0;
    u64 phase = 0;
    u64 loc = 0;
    int rc = salGetCheckpointNameTokens(checkpointStr, &calTime, &phase, &loc);
    if (rc < 0 || calTime != hcPolicy->calTime || phase != chkptPhase || loc != pd->myLocation) {
        fprintf(stderr, "Cannot set checkpoint. Invalid checkpoint name: (filename: %s)\n", name);
        ASSERT(0);
        return 1;
    }

    char filenameBuf[4096];
    const char* execName = salGetExecutableName();
    rc = sprintf(filenameBuf, "%s.%lu.%lu.%u", execName, hcPolicy->calTime, chkptPhase, pd->neighborCount + 1);
    if (rc < 0 || rc >= 4096) {
        fprintf(stderr, "sprintf failed: (filename: %s)\n", filename);
        ASSERT(0);
        return 1;
    }

    rc = fputs(filenameBuf, fp);
    if (rc == EOF || rc < 0) {
        fprintf(stderr, "fputs failed: (filename: %s)\n", filename);
        ASSERT(0);
        return 1;
    }

    rc = fclose(fp);
    if (rc != 0) {
        fprintf(stderr, "fclose failed: (filename: %s)\n", filename);
        ASSERT(0);
        return 1;
    }
    return 0;
}

//Get the name of the current stable checkpoint
char* salGetCheckpointName() {
    struct stat sb;
    const char* filename = salGetCheckpointSummaryFileName();
    if (stat(filename, &sb) == -1)
        return NULL;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "fopen failed: (filename: %s)\n", filename);
        ASSERT(0);
        return NULL;
    }

    char checkpointStr[4096];
    if (fgets(checkpointStr, 4096, fp) == NULL) {
        fprintf(stderr, "fgets failed: (filename: %s)\n", filename);
        ASSERT(0);
        return NULL;
    }

    u64 calTime = 0;
    u64 phase = 0;
    u64 numLocations = 0;
    int rc = salGetCheckpointNameTokens(checkpointStr, &calTime, &phase, &numLocations);
    if (rc < 0)
        return NULL;

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    if (numLocations != (pd->neighborCount + 1))
        return NULL;

    const char *chkptFilename = salConstructPdCheckpointFileName(calTime, phase, pd->myLocation);
    return (char*)chkptFilename;
}

//Check if any valid checkpoint exists
static bool salCheckpointExistsInternal(bool doQuery) {
    struct stat sb;
    const char* filename = salGetCheckpointSummaryFileName();
    if (stat(filename, &sb) == -1)
        return false;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "fopen failed: (filename: %s)\n", filename);
        ASSERT(0);
        return false;
    }

    char checkpointStr[4096];
    if (fgets(checkpointStr, 4096, fp) == NULL) {
        fprintf(stderr, "fgets failed: (filename: %s)\n", filename);
        ASSERT(0);
        return false;
    }

    u64 calTime = 0;
    u64 phase = 0;
    u64 numLocations = 0;
    int rc = salGetCheckpointNameTokens(checkpointStr, &calTime, &phase, &numLocations);
    if (rc < 0)
        return false;

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    if (numLocations != (pd->neighborCount + 1))
        return false;

    u32 i;
    for (i = 0; i < numLocations; i++) {
        char *chkptFilename = (char*)salConstructPdCheckpointFileName(calTime, phase, i);
        if (stat(chkptFilename, &sb) == -1) {
            pd->fcts.pdFree(pd, chkptFilename);
            return false;
        }
        pd->fcts.pdFree(pd, chkptFilename);
    }

    if (doQuery) {
        char ans;
        printf("Found existing checkpoint: (timestamp: %lu phase: %lu) \nDo you want to resume? [Y/N] : ", calTime, phase);
        scanf ("%c", &ans);
        if (ans != 'Y' && ans != 'y')
            return false;
        fprintf(stderr, "Resuming from checkpoint: (timestamp: %lu phase: %lu)\n", calTime, phase);
    }
    return true;
}

//Check if any valid checkpoint exists
bool salCheckpointExists() {
    return salCheckpointExistsInternal(false);
}

//Check if any valid checkpoint exists
//If yes, then ask user if program should
//be resumed from that checkpoint
bool salCheckpointExistsResumeQuery() {
    return salCheckpointExistsInternal(true);
}

#endif /* ENABLE_RESILIENCY */

#ifdef ENABLE_AMT_RESILIENCE
#include "task/hc/hc-task.h"

#define FNL                 256
#define RECORD_INCR_SIZE    256

//Publish-Fetch hashtable
static int pfIsInitialized = 0;
static hashtable_t * pfTable = NULL;    //Hashtable containing pointers to published data
static hashtable_t * faultTable = NULL; //Hashtable containing EDT guids which have encountered faults
static lock_t pfLock;
static lock_t depLock;
static char * nodeExt;

typedef struct _nodeStateHeader {
    size_t nodeStateBufSize;
    u64 nodeId;
    u64 numRecords;
    u64 maxRecords;
} nodeStateHeader_t;

typedef struct _nodeStateRecord {
    ocrGuid_t guid;     //Guid of EDT
    ocrGuid_t pguid;    //Guid of parent scope EDT
} nodeStateRecord_t;

static void *nodeStateBuf;
static int nodeStateFD;
static volatile u32 workerCounter;

static u8 salPublishInternal(char *fname, u64 guid, void *ptr, u64 size, u8 activeBuffer);

//Initialize the hashtable
void salInitPublishFetch() {
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    pfTable = newHashtableBucketLockedModulo(pd, RECORD_INCR_SIZE);
    faultTable = newHashtableBucketLockedModulo(pd, RECORD_INCR_SIZE);
    pfLock = INIT_LOCK;
    depLock = INIT_LOCK;
    nodeExt = NULL;
    workerCounter = 0;

    //Initialize node state buffer
    char fname[FNL];
    int c = snprintf(fname, FNL, "node%lu.state", (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for node state\n");
        ASSERT(0);
        return;
    }

    struct stat sb;
    if (stat(fname, &sb) != -1) {
        fprintf(stderr, "Found existing node state %s!\n", fname);
        ASSERT(0);
        return;
    }

    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return;
    }
    nodeStateFD = fd;

    u64 nodeStateBufSize = sizeof(nodeStateHeader_t) + (RECORD_INCR_SIZE * sizeof(nodeStateRecord_t));
    int rc = ftruncate(fd, nodeStateBufSize);
    if (rc) {
        fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
        ASSERT(0);
        return;
    }

    nodeStateBuf = mmap( NULL, nodeStateBufSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (nodeStateBuf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", nodeStateBufSize, fname, fd);
        ASSERT(0);
        return;
    }

    nodeStateHeader_t *nsHeader = (nodeStateHeader_t*)nodeStateBuf;
    nsHeader->nodeStateBufSize = nodeStateBufSize;
    nsHeader->nodeId = (u64)pd->myLocation;
    nsHeader->numRecords = 0;
    nsHeader->maxRecords = RECORD_INCR_SIZE;

    rc = msync(nodeStateBuf, sizeof(nodeStateHeader_t), MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", nodeStateBuf, nodeStateBufSize);
        ASSERT(0);
        return;
    }
    pfIsInitialized = 1;
}

void salFinalizePublishFetch() {
    if (!pfIsInitialized) return;
    nodeStateHeader_t *nsHeader = (nodeStateHeader_t*)nodeStateBuf;
    size_t nodeStateBufSize = nsHeader->nodeStateBufSize;
    int rc = munmap(nodeStateBuf, nodeStateBufSize);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", nodeStateBuf, nodeStateBufSize);
        ASSERT(0);
        return;
    }

    rc = close(nodeStateFD);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", nodeStateFD);
        ASSERT(0);
        return;
    }

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    char fname[FNL];
    int c = snprintf(fname, FNL, "node%lu.state", (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for node state\n");
        ASSERT(0);
        return;
    }

    rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return;
    }
}

u8 salIsPublishedGuid(ocrGuid_t g) {
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        return 0;
    }
    return 1;
}

u8 salIsPublishedEvent(ocrGuid_t g) {
    if (ocrGuidIsNull(g))
        return 0;

#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.evt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        return 0;
    }
    return 1;
}

//Check if guid has been published
//Returns true or false
u8 salIsPublished(ocrGuid_t g) {
    if (ocrGuidIsNull(g))
        return 0;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], g, &kind);
    if (kind & OCR_GUID_EVENT) {
        return salIsPublishedEvent(g);
    } else if ((kind == OCR_GUID_DB) || (kind == OCR_GUID_EDT)) {
        return salIsPublishedGuid(g);
    } else {
        ASSERT(0);
    }
    return 0;
}

u8 salIsSatisfied(ocrGuid_t g) {
    if (ocrGuidIsNull(g))
        return 0;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], g, &kind);
    if (kind & OCR_GUID_EVENT) {
        ocrGuid_t data = NULL_GUID;
        if (salIsSatisfiedEvent(g, &data)) {
            return 1;
        }
    } else if (kind == OCR_GUID_DB) {
        if (salIsPublishedGuid(g)) {
            return 1;
        }
    } else if (kind == OCR_GUID_EDT) {
        ASSERT(salIsPublishedGuid(g));
        if (!salIsPublishedGuid(g)) {
            return 1;
        }
    } else {
        ASSERT(0);
    }
    return 0;
}

///////////////////////// Publish Deps API ////////////////////////

typedef struct _depRecord {
    ocrGuid_t dst;
    u32 slot;
} depRecord_t;

typedef struct _depStorage {
    u32 numDeps;
    u32 maxDeps;
} depStorage_t;

#define DEPS_INCR 4

static depRecord_t * salGetPublishedDeps(u64 guid, u64 loc, u32 *ndeps) {
    *ndeps = 0;

    hal_lock(&depLock);

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.deps%lu", guid, loc);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return NULL;
    }

    struct stat sb;
    if (stat(fname, &sb) != 0) {
        hal_unlock(&depLock);
        return NULL;
    }

    //Ensure file is initialized
    u64 size = sb.st_size;
    while (size == 0) {
        RESULT_ASSERT(stat(fname, &sb), ==, 0);
        size = sb.st_size;
    }

    //open underlying file
    int fd = open(fname, O_RDWR);
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return NULL;
    }

    //Lock file
    int rc = flock(fd, LOCK_EX);
    if (rc) {
        fprintf(stderr, "file lock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return NULL;
    }

    RESULT_ASSERT(stat(fname, &sb), ==, 0);
    size = sb.st_size;

    void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return NULL;
    }

    depStorage_t *depHeader = (depStorage_t*)buf;
    depRecord_t *deprecBuf = (depRecord_t*)(((u8*)buf) + sizeof(depStorage_t));
    u32 numDeps = depHeader->numDeps;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    depRecord_t *deprec = (depRecord_t*)pd->fcts.pdMalloc(pd, sizeof(depRecord_t) * numDeps);
    memcpy(deprec, deprecBuf, sizeof(depRecord_t) * numDeps);
    depHeader->numDeps = 0;

    rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return NULL;
    }

    rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return NULL;
    }

    //Unlock file
    rc = flock(fd, LOCK_UN);
    if (rc) {
        fprintf(stderr, "file unlock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return NULL;
    }

    //close file
    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return NULL;
    }

    /*rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return NULL;
    }*/

    hal_unlock(&depLock);

    *ndeps = numDeps;
    return deprec;
}

static void doSatisfy(ocrGuid_t dst, u32 slot, ocrGuid_t data) {
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
    if (!salIsPublished(dst)) {
        ocrLocation_t loc;
        pd->guidProviders[0]->fcts.getLocation(pd->guidProviders[0], dst, &loc);
        if (checkPlatformModelLocationFault(loc)) {
#if GUID_BIT_COUNT == 64
            u64 guid = dst.guid;
#elif GUID_BIT_COUNT == 128
            u64 guid = dst.lower;
#else
#error Unknown type of GUID
#endif
            RESULT_ASSERT(salGuidTableGet(guid, &dst), ==, 0);
        }
    }
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(satisfierGuid.guid) = curEdt?curEdt->guid:NULL_GUID;
    PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(guid.guid) = dst;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(payload.guid) = data;
    PD_MSG_FIELD_I(payload.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(slot) = slot;
#ifdef REG_ASYNC_SGL
    PD_MSG_FIELD_I(mode) = DB_MODE_RW;
#endif
    PD_MSG_FIELD_I(properties) = 0;
    pd->fcts.processMessage(pd, &msg, true);
#undef PD_MSG
#undef PD_TYPE
}

u8 salReleasePublishedDeps(ocrGuid_t g, ocrGuid_t data) {
    if (!pfIsInitialized) return 1;
    if (ocrGuidIsNull(g)) return 1;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u32 i, numDeps;
    u64 n;

    for (n = 0; n < pd->neighborCount + 1; n++) {
        numDeps = 0;
        depRecord_t *deprec = salGetPublishedDeps(guid, n, &numDeps);
        for (i = 0; i < numDeps; i++) {
            doSatisfy(deprec[i].dst, deprec[i].slot, data);
        }
        if (numDeps > 0) pd->fcts.pdFree(pd, deprec);
    }
    return 0;
}

static u8 salDoSatisfy(ocrGuid_t src, ocrGuid_t dst, u32 slot) {
    ASSERT(!(ocrGuidIsNull(dst)));
    if (ocrGuidIsNull(src)) {
        doSatisfy(dst, slot, NULL_GUID);
        return 1;
    }
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], src, &kind);
    if (kind & OCR_GUID_EVENT) {
        ocrGuid_t data = NULL_GUID;
        if (salIsSatisfiedEvent(src, &data)) {
            doSatisfy(dst, slot, data);
            return 1;
        }
    } else if (kind == OCR_GUID_DB) {
        if (salIsPublishedGuid(src)) {
            doSatisfy(dst, slot, src);
            return 1;
        }
    } else if (kind == OCR_GUID_EDT) {
        ASSERT(salIsPublishedGuid(src));
        if (!salIsPublishedGuid(src)) {
            doSatisfy(dst, slot, NULL_GUID);
            return 1;
        }
    } else {
        ASSERT(0);
    }
    return 0;
}

u8 salPublishAddDependence(ocrGuid_t g, ocrGuid_t dst, u32 slot) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(dst)));

    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], dst, &kind);
    if (kind != OCR_GUID_EDT) return 1;
    if (!salIsResilientEdt(dst)) return 1;

    if (salDoSatisfy(g, dst, slot)) {
        return 0;
    }

#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif

    hal_lock(&depLock);

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.deps%lu", guid, pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    void *buf = NULL;
    u64 size = 0;
    int fd = -1;
    int rc = 0;
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
        if (fd<0) {
            fprintf(stderr, "open failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        rc = flock(fd, LOCK_EX);
        if (rc) {
            fprintf(stderr, "file lock failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        size = sizeof(depStorage_t) + (DEPS_INCR * sizeof(depRecord_t));
        rc = ftruncate(fd, size);
        if (rc) {
            fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
            ASSERT(0);
            return 1;
        }
        buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if (buf == MAP_FAILED) {
            fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
            ASSERT(0);
            return 1;
        }
        depStorage_t *depHeader = (depStorage_t*)buf;
        depHeader->numDeps = 0;
        depHeader->maxDeps = DEPS_INCR;
    } else {
        fd = open(fname, O_RDWR);
        if (fd<0) {
            fprintf(stderr, "open failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        rc = flock(fd, LOCK_EX);
        if (rc) {
            fprintf(stderr, "file lock failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        RESULT_ASSERT(stat(fname, &sb), ==, 0);
        size = sb.st_size;
        buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if (buf == MAP_FAILED) {
            fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
            ASSERT(0);
            return 1;
        }
    }

    u8 doResize = 0;
    u32 maxDeps = 0;
    u8 isSat = salIsSatisfied(g);
    if (!isSat) {
    depStorage_t *depHeader = (depStorage_t*)buf;
    u32 numDeps = depHeader->numDeps;
    depRecord_t *deprec = (depRecord_t*)(((u8*)buf) + sizeof(depStorage_t) + (numDeps * sizeof(depRecord_t)));
    deprec->dst = dst;
    deprec->slot = slot;
    maxDeps = depHeader->maxDeps;
    depHeader->numDeps = ++numDeps;
    if (numDeps == maxDeps) doResize = 1;

    rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }
    }

    rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    if (doResize) {
        size = sizeof(depStorage_t) + ((maxDeps + DEPS_INCR) * sizeof(depRecord_t));
        int rc = ftruncate(fd, size);
        if (rc) {
            fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
            ASSERT(0);
            return 1;
        }
    }

    rc = flock(fd, LOCK_UN);
    if (rc) {
        fprintf(stderr, "file unlock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    hal_unlock(&depLock);

    if (isSat) salDoSatisfy(g, dst, slot);

    return 0;
}

u8 salTransferPublishedDeps(ocrGuid_t g, ocrGuid_t dst) {
    u32 i, numDeps;
    u64 n;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    for (n = 0; n < pd->neighborCount + 1; n++) {
        numDeps = 0;
        depRecord_t *deprec = salGetPublishedDeps(guid, n, &numDeps);
        for (i = 0; i < numDeps; i++) {
            salPublishAddDependence(dst, deprec[i].dst, deprec[i].slot);
        }
        if (numDeps > 0) pd->fcts.pdFree(pd, deprec);
    }
    return 0;
}

///////////////////////// Publish DB API ////////////////////////

//Publish the guid and optionally insert the ptr to the hashtable
//Returns the file descriptor of the published data
static u8 salPublishInternal(char *fname, u64 guid, void *ptr, u64 size, u8 activeBuffer) {
    struct stat sb;
    if (stat(fname, &sb) != -1) {
        fprintf(stderr, "Found existing guid [0x%lx] during publish!\n", guid);
        ASSERT(0);
        return 1;
    }

    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    int rc = ftruncate(fd, size);
    if (rc) {
        fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
        ASSERT(0);
        return 1;
    }

    void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    memcpy(buf, ptr, size);

    rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    if (activeBuffer) {
        hashtableConcBucketLockedPut(pfTable, (void*)guid, buf);
    } else {
        rc = munmap(buf, size);
        if (rc) {
            fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
            ASSERT(0);
            return 1;
        }
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    return 0;
}

u8 salPublish(ocrGuid_t g, void *ptr, u64 size) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    void* buf = hashtableConcBucketLockedGet(pfTable, (void*)guid);
    if (buf != NULL) {
        fprintf(stderr, "Found existing guid [0x%lx] during publish!\n", guid);
        ASSERT(0);
        return 1;
    }
    salPublishInternal(fname, guid, ptr, size, 1);
    hal_fence();
    salReleasePublishedDeps(g, g);
    return 0;
}

//Copy a published guid to a new buffer and return the ptr
//Caller takes responsibility of free-ing the buffer
static void* salFetchInternal(char *fname, u64 guid, u64 *gSize, u8 activeBuffer, u8 doCopy) {
    if (gSize) *gSize = 0;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        return NULL;
    }
    u64 size = sb.st_size;

    void* buf = activeBuffer ? hashtableConcBucketLockedGet(pfTable, (void*)guid) : NULL;

    hal_lock(&pfLock);

    if (buf == NULL) {
        int fd = open(fname, O_RDWR);
        if (fd<0) {
            fprintf(stderr, "open failed: (filename: %s)\n", fname);
            ASSERT(0);
            return NULL;
        }

        buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if (buf == MAP_FAILED) {
            fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
            ASSERT(0);
            return NULL;
        }

        if (activeBuffer) hashtableConcBucketLockedPut(pfTable, (void*)guid, buf);

        int rc = close(fd);
        if (rc) {
            fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
            ASSERT(0);
            return NULL;
        }
    }

    void *ptr = buf;
    if (doCopy) {
        ptr = pd->fcts.pdMalloc(pd, size);
        memcpy(ptr, buf, size);
    }

    hal_unlock(&pfLock);

    if (gSize) *gSize = size;
    return ptr;
}

void* salFetch(ocrGuid_t g, u64 *gSize) {
    if (!pfIsInitialized) return NULL;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return NULL;
    }
    return salFetchInternal(fname, guid, gSize, 1, 1);
}

//Update a published guid's contents
//User takes responsibility of maintaining consistency
u8 salRepublish(ocrGuid_t g, void *ptr) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during republish!\n", guid);
        ASSERT(0);
        return 1;
    }
    u64 size = sb.st_size;

    void* buf = hashtableConcBucketLockedGet(pfTable, (void*)guid);

    ASSERT(buf != ptr);
    if (buf == NULL) {
        fprintf(stderr, "Guid does not exist!\n");
        ASSERT(0);
        return 1;
    }

    //Lock access to file from other threads
    hal_lock(&pfLock);

    //Update existing buffer contents
    memcpy(buf, ptr, size);

    //open underlying file
    int fd = open(fname, O_RDWR);
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //Lock access to file from other processes
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        hal_unlock(&pfLock);
        fprintf(stderr, "fcntl lock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //map file
    buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    //Update file contents
    memcpy(buf, ptr, size);

    //flush out changes
    int rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    //unmap file
    rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    //Unlock access to file from other processes
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        hal_unlock(&pfLock);
        fprintf(stderr, "fcntl unlock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //close file
    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    //Unlock access to file from other threads
    hal_unlock(&pfLock);

    return 0;
}

u8 salRemovePublished(ocrGuid_t g) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during remove!\n", guid);
        ASSERT(0);
        return 1;
    }
    u64 size = sb.st_size;

    void* buf = NULL;
    if (hashtableConcBucketLockedRemove(pfTable, (void*)guid, &buf)) {
        int rc = munmap(buf, size);
        if (rc) {
            fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
            ASSERT(0);
            return 1;
        }
    }

    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    /*u64 n;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    for (n = 0; n < pd->neighborCount + 1; n++) {
        char fname[FNL];
        int c = snprintf(fname, FNL, "%lu.deps%lu", guid, n);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for publish\n");
            ASSERT(0);
            return 1;
        }
        struct stat sb;
        if (stat(fname, &sb) == 0) {
            int rc = unlink(fname);
            if (rc) {
                fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
                ASSERT(0);
                return 1;
            }
        }
    }*/

    return 0;
}

///////////////////////// Publish EDT API ////////////////////////

typedef struct _edtStorage {
    ocrEdt_t funcPtr;
    u32 paramc, depc;
    ocrGuid_t resilientEdtParent;
    u64* paramv;
    ocrGuid_t *depv;
} edtStorage_t;

u8 salResilientEdtCreate(ocrTask_t *task) {
    if (!pfIsInitialized) return 1;
    ASSERT(task != NULL);
#if GUID_BIT_COUNT == 64
    u64 guid = task->guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = task->guid.lower;
#else
#error Unknown type of GUID
#endif
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //Create the EDT file
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.edt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    u64 locbuf = pd->myLocation;
    salPublishInternal(fname, guid, &locbuf, sizeof(u64), 0);

    //Create the temporary EDT buffer file
    c = snprintf(fname, FNL, "%lu.edt%lu", guid, pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    int i;
    u64 size = sizeof(edtStorage_t) + sizeof(u64) * task->paramc + sizeof(ocrGuid_t) * task->depc;
    u8 *buf = pd->fcts.pdMalloc(pd, size);
    edtStorage_t *edt = (edtStorage_t *)buf;
    edt->funcPtr = task->funcPtr;
    edt->paramc = task->paramc;
    edt->depc = task->depc;
    edt->resilientEdtParent = task->resilientEdtParent;
    edt->paramv = (u64*)(buf + sizeof(edtStorage_t));
    memcpy(edt->paramv, task->paramv, sizeof(u64) * task->paramc);
    edt->depv = (ocrGuid_t*)(buf + sizeof(edtStorage_t) + sizeof(u64) * task->paramc);
    for (i = 0; i < task->depc; i++) {
        edt->depv[i] = UNINITIALIZED_GUID;
    }
    salPublishInternal(fname, guid, buf, size, 0);
    pd->fcts.pdFree(pd, buf);
    return 0;
}

u8 salIsResilientEdt(ocrGuid_t g) {
    if (!pfIsInitialized) return 0;
    if (ocrGuidIsNull(g)) return 0;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.edt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 0;
    }
    struct stat sb;
    if (stat(fname, &sb) == 0) {
        return 1;
    }
    return 0;
}

u8 salResilientEdtSatisfy(ocrGuid_t data, ocrGuid_t edt, u32 slot) {
    if (!pfIsInitialized) return 1;
    ASSERT(!ocrGuidIsNull(edt));
#if GUID_BIT_COUNT == 64
    u64 guid = edt.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = edt.lower;
#else
#error Unknown type of GUID
#endif
    hal_lock(&pfLock);

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.edt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during satisfy!\n", guid);
        ASSERT(0);
        return 1;
    }
    int fd = open(fname, O_RDONLY );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    u64 size = sizeof(u64);
    void *buf = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }
    u64 loc = *((u64*)buf);
    int rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }
    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    //Now open temporary EDT buffer
    c = snprintf(fname, FNL, "%lu.edt%lu", guid, loc);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }

    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during satisfy!\n", guid);
        ASSERT(0);
        return 1;
    }
    size = sb.st_size;

    fd = open(fname, O_RDWR);
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //Lock file
    rc = flock(fd, LOCK_EX);
    if (rc) {
        fprintf(stderr, "file lock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    edtStorage_t *edtBuf = (edtStorage_t*)buf;
    edtBuf->depv = (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64)));
    ASSERT(ocrGuidIsEq(edtBuf->depv[slot], UNINITIALIZED_GUID));
    edtBuf->depv[slot] = data;

    rc = msync((void*)edtBuf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }
    rc = munmap((void*)edtBuf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    //Unlock file
    rc = flock(fd, LOCK_UN);
    if (rc) {
        fprintf(stderr, "file unlock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    hal_unlock(&pfLock);

    return 0;
}

u8 salPublishEdt(ocrTask_t *task) {
    if (!pfIsInitialized) return 1;
    ASSERT(task->flags & OCR_TASK_FLAG_RESILIENT);
    ASSERT(task->state == ALLACQ_EDTSTATE);
    ocrTaskHc_t *hcTask = (ocrTaskHc_t*)task;
    ASSERT(hcTask->resolvedDeps != NULL);
#if GUID_BIT_COUNT == 64
    u64 guid = task->guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = task->guid.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    int i;
    u64 size = sizeof(edtStorage_t) + sizeof(u64) * task->paramc + sizeof(ocrGuid_t) * task->depc;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u8 *buf = pd->fcts.pdMalloc(pd, size);
    edtStorage_t *edt = (edtStorage_t *)buf;
    edt->funcPtr = task->funcPtr;
    edt->paramc = task->paramc;
    edt->depc = task->depc;
    edt->resilientEdtParent = task->resilientEdtParent;
    edt->paramv = (u64*)(buf + sizeof(edtStorage_t));
    memcpy(edt->paramv, task->paramv, sizeof(u64) * task->paramc);
    edt->depv = (ocrGuid_t*)(buf + sizeof(edtStorage_t) + sizeof(u64) * task->paramc);
    for (i = 0; i < task->depc; i++) {
        ASSERT(ocrGuidIsNull(hcTask->resolvedDeps[i].guid) || (hcTask->resolvedDeps[i].ptr == UNINITIALIZED_DB_FETCH_PTR));
        edt->depv[i] = hcTask->resolvedDeps[i].guid;
    }
    salPublishInternal(fname, guid, buf, size, 0);
    salRecordEdtAtNode(task->guid, pd->myLocation);
    pd->fcts.pdFree(pd, buf);

    //Delete temporary buffer
    c = snprintf(fname, FNL, "%lu.edt%lu", guid, pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    RESULT_ASSERT(stat(fname, &sb), ==, 0);
    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    return 0;
}

u8 salRemovePublishedEdt(ocrGuid_t edt) {
    u64 i;
    if (!pfIsInitialized) return 1;
    ASSERT(!ocrGuidIsNull(edt));

    //Remove EDT metadata from storage
    salRemovePublished(edt);

    //Now remove the records of this EDT in every node that was impacted by it
#if GUID_BIT_COUNT == 64
    u64 guid = edt.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = edt.lower;
#else
#error Unknown type of GUID
#endif
    //Remove resilient EDT file
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.edt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during remove!\n", guid);
        ASSERT(0);
        return 1;
    }

    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //Remove node files
    u64 nbRanks = pd->neighborCount + 1;
    for (i = 0; i < nbRanks; i++) {
        char fname[FNL];
        int c = snprintf(fname, FNL, "%lu.node%lu", guid, (u64)i);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for EDT record\n");
            ASSERT(0);
            return 1;
        }
        struct stat sb;
        if (stat(fname, &sb) == 0) {
            int rc = unlink(fname);
            if (rc) {
                fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
                ASSERT(0);
                return 1;
            }
        }
    }

    return 0;
}

u8 salRecordEdtAtNode(ocrGuid_t g, ocrLocation_t loc) {
    if (!pfIsInitialized) return 1;
    if (ocrGuidIsNull(g)) return 0;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.node%lu", guid, (u64)loc);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }

    hal_lock(&pfLock);

    struct stat sb;
    if (stat(fname, &sb) == 0) {
        hal_unlock(&pfLock);
        return 0; //Found existing record
    }

    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    int rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    hal_unlock(&pfLock);

    return 0;
}

///////////////////////// Publish Event API ////////////////////////

typedef struct _evtStorage {
    ocrGuid_t data;
    u8 satisfied;
} evtStorage_t;

u8 salPublishEvent(ocrGuid_t g) {
    if (!pfIsInitialized) return 1;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.evt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    u64 size = sizeof(evtStorage_t);
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u8 *evtBuf = pd->fcts.pdMalloc(pd, size);
    evtStorage_t *evt = (evtStorage_t*)evtBuf;
    evt->data = NULL_GUID;
    evt->satisfied = 0;
    salPublishInternal(fname, guid, evtBuf, size, 0);
    pd->fcts.pdFree(pd, evtBuf);
    return 0;
}

u8 salPublishEventSatisfy(ocrGuid_t g, ocrGuid_t data) {
    if (!pfIsInitialized) return 1;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif

    hal_lock(&depLock);

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.evt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during satisfy!\n", guid);
        ASSERT(0);
        return 1;
    }
    u64 size = sb.st_size;

    int fd = open(fname, O_RDWR);
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //Lock file
    int rc = flock(fd, LOCK_EX);
    if (rc) {
        fprintf(stderr, "file lock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    evtStorage_t *evt = (evtStorage_t*)buf;
    evt->data = data;
    evt->satisfied = 1;

    rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    //Unlock file
    rc = flock(fd, LOCK_UN);
    if (rc) {
        fprintf(stderr, "file unlock failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    hal_unlock(&depLock);

    //Now release all deps
    hal_fence();
    salReleasePublishedDeps(g, data);

    return 0;
}

u8 salRemovePublishedEvent(ocrGuid_t g) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.evt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during remove!\n", guid);
        ASSERT(0);
        return 1;
    }

    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    return 0;
}

u8 salIsSatisfiedEventInternal(u64 guid, ocrGuid_t *data) {
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.evt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        return 0;
    }
    u64 size = sb.st_size;

    int fd = open(fname, O_RDONLY );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 0;
    }

    void *buf = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 0;
    }

    evtStorage_t *evt = (evtStorage_t*)buf;
    u8 isSatisfied = evt->satisfied;
    *data = evt->data;

    int rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 0;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 0;
    }

    return isSatisfied;
}

u8 salIsSatisfiedEvent(ocrGuid_t g, ocrGuid_t *data) {
    if (ocrGuidIsNull(g))
        return 0;

#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    return salIsSatisfiedEventInternal(guid, data);
}

///////////////////////// Node failure api ///////////////////

void salThreadExit() {
    pthread_exit(NULL);
}

void salThreadRecover() {
    hal_xadd32(&workerCounter, 1);
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    while(pd->faultCode != 0);
}

/* when return 1, scandir will put this dirent to the list */
static int parse_ext(const struct dirent *dir)
{
    if(dir == NULL) return 0;
    if(dir->d_type == DT_REG) { /* only deal with regular file */
        const char *ext = strrchr(dir->d_name,'.');
        if((ext == NULL) || (ext == dir->d_name))
            return 0;
        if(strcmp(ext, (const char *)nodeExt) == 0)
            return 1;
    }
    return 0;
}

u8 salProcessNodeFailureAtBuddy(ocrLocation_t nodeId) {
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //Pause execution
    pd->faultCode = OCR_NODE_FAILURE_OTHER;
    hal_fence();
    u32 maxCount = pd->workerCount - 1;
    while(workerCounter < maxCount);
    hal_fence();

    //Determine all EDT guids impacted by node failure
    char extension[FNL];
    int c = snprintf(extension, FNL, ".node%lu", (u64)nodeId);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    nodeExt = extension;

    struct dirent **namelist;
    int n = scandir(".", &namelist, parse_ext, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }
    while (n--) {
        //printf("%s\n", namelist[n]->d_name);
        char *s = strchr(namelist[n]->d_name,'.');
        *s = '\0';
        u64 guid = strtoul(namelist[n]->d_name, NULL, 10);
        ASSERT(guid > 0);
        char fname[FNL];
        int c = snprintf(fname, FNL, "%lu.guid", guid);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for EDT guid\n");
            ASSERT(0);
            return 1;
        }

        struct stat sb;
        if (stat(fname, &sb) == 0) {
            if (hashtableConcBucketLockedGet(faultTable, (void*)guid) == NULL) {
                hashtableConcBucketLockedPut(faultTable, (void*)guid, (void*)guid);
                char fname[FNL];
                int c = snprintf(fname, FNL, "%lu.fault", guid);
                if (c < 0 || c >= FNL) {
                    fprintf(stderr, "failed to create filename for EDT record\n");
                    ASSERT(0);
                    return 1;
                }
                struct stat sb;
                if (stat(fname, &sb) == 0) {
                    fprintf(stderr, "Found existing fault guid [0x%lx] during recovery!\n", guid);
                    ASSERT(0);
                    return 1;
                }
                int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
                if (fd<0) {
                    fprintf(stderr, "open failed: (filename: %s)\n", fname);
                    ASSERT(0);
                    return 1;
                }
                int rc = close(fd);
                if (rc) {
                    fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
                    ASSERT(0);
                    return 1;
                }
            }
        }
        free(namelist[n]);
    }
    free(namelist);

    //Update platform model
    notifyPlatformModelLocationFault(nodeId);

    //Resume execution
    hal_fence();
    workerCounter = 0;
    hal_fence();
    pd->faultCode = 0;

    return 0;
}

u8 salProcessNodeFailure(ocrLocation_t nodeId) {
    if (!pfIsInitialized) return 1;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //Pause execution
    pd->faultCode = OCR_NODE_FAILURE_OTHER;
    hal_fence();
    u32 maxCount = pd->workerCount - 1;
    while(workerCounter < maxCount);
    hal_fence();

    //Build fault table
    char extension[FNL];
    int c = snprintf(extension, FNL, ".fault");
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    nodeExt = extension;

    struct dirent **namelist;
    int n = scandir(".", &namelist, parse_ext, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }
    while (n--) {
        //printf("%s\n", namelist[n]->d_name);
        char *s = strchr(namelist[n]->d_name,'.');
        *s = '\0';
        u64 guid = strtoul(namelist[n]->d_name, NULL, 10);
        ASSERT(guid > 0);
        if (hashtableConcBucketLockedGet(faultTable, (void*)guid) == NULL) {
            hashtableConcBucketLockedPut(faultTable, (void*)guid, (void*)guid);
        }
        free(namelist[n]);
    }
    free(namelist);

    //Update platform model
    notifyPlatformModelLocationFault(nodeId);

    //Resume execution
    hal_fence();
    workerCounter = 0;
    hal_fence();
    pd->faultCode = 0;

    return 0;
}

void salImportEdt(void * key, void * value, void * args) {
    ASSERT((u64)key == (u64)value);
    u64 guid = (u64)key;
    hashtable_t * faultTable = (hashtable_t*)args;

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return;
    }
    u64 bufsize = 0;
    edtStorage_t *edtBuf = (edtStorage_t*)salFetchInternal(fname, guid, &bufsize, 0, 0);
    if (edtBuf == NULL) {
        fprintf(stderr, "Node state is corrupt; recovery failed\n");
        ASSERT(0);
        return;
    }
    if (!ocrGuidIsNull(edtBuf->resilientEdtParent)) {
#if GUID_BIT_COUNT == 64
        u64 pguid = edtBuf->resilientEdtParent.guid;
#elif GUID_BIT_COUNT == 128
        u64 pguid = edtBuf->resilientEdtParent.lower;
#else
#error Unknown type of GUID
#endif
        if (hashtableConcBucketLockedGet(faultTable, (void*)pguid) != NULL)
            return; //Found existing parent
    }

    //Create recovery EDT
    edtBuf->paramv = (u64*)((size_t)edtBuf + sizeof(edtStorage_t));
    edtBuf->depv = (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64)));
    ocrGuid_t tmpl;
    ocrEdtTemplateCreate(&tmpl, edtBuf->funcPtr, edtBuf->paramc, edtBuf->depc);
    ocrGuid_t edt, oEvt;
    ocrEdtCreate(&edt, tmpl, edtBuf->paramc, edtBuf->paramv, edtBuf->depc, edtBuf->depv, EDT_PROP_RESILIENT | EDT_PROP_RECOVERY, NULL_HINT, &oEvt);
    DPRINTF(DEBUG_LVL_WARN, "Recovery EDT created: "GUIDF"\n", GUIDA(edt));
}

u8 salRecoverNodeFailureAtBuddy(ocrLocation_t nodeId) {
    //Reschedule execution sub-graph dominator EDTs
    iterateHashtable(faultTable, salImportEdt, (void*)faultTable);

    //Rebuild EDT temporary buffers
    char extension[FNL];
    int c = snprintf(extension, FNL, ".edt%lu", nodeId);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    nodeExt = extension;

    struct dirent **namelist;
    int n = scandir(".", &namelist, parse_ext, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }
    while (n--) {
        //printf("%s\n", namelist[n]->d_name);
        char *s = strchr(namelist[n]->d_name,'.');
        *s = '\0';
        u64 guid = strtoul(namelist[n]->d_name, NULL, 10);
        ASSERT(guid > 0);

        //Re-create temporary EDT buffers
        char fname[FNL];
        int c = snprintf(fname, FNL, "%lu.guid", guid);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for publish\n");
            ASSERT(0);
            return 1;
        }
        struct stat sb;
        if (stat(fname, &sb) != 0) {
            c = snprintf(fname, FNL, "%lu.edt%lu", guid, nodeId);
            if (c < 0 || c >= FNL) {
                fprintf(stderr, "failed to create filename for publish\n");
                ASSERT(0);
                return 1;
            }
            u64 bufsize = 0;
            edtStorage_t *edtBuf = (edtStorage_t*)salFetchInternal(fname, guid, &bufsize, 0, 0);
            ASSERT(edtBuf != NULL);
            edtBuf->paramv = edtBuf->paramc ? (u64*)((size_t)edtBuf + sizeof(edtStorage_t)) : NULL;
            edtBuf->depv = (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64)));
            ocrGuid_t tmpl;
            ocrEdtTemplateCreate(&tmpl, edtBuf->funcPtr, edtBuf->paramc, edtBuf->depc);
            ocrGuid_t edt, oEvt;
            ocrEdtCreate(&edt, tmpl, edtBuf->paramc, edtBuf->paramv, edtBuf->depc, NULL, EDT_PROP_RESILIENT | EDT_PROP_RECOVERY, NULL_HINT, &oEvt);
            salGuidTablePut(guid, edt);
            int i;
            for (i = 0; i < edtBuf->depc; i++) {
                if (!ocrGuidIsEq(edtBuf->depv[i], UNINITIALIZED_GUID))
                    ocrAddDependence(edtBuf->depv[i], edt, i, DB_MODE_RW);
            }
            int rc = unlink(fname);
            if (rc) {
                fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
                ASSERT(0);
                return 1;
            }
            DPRINTF(DEBUG_LVL_WARN, "Recovery EDT metadata created: "GUIDF"\n", GUIDA(edt));
        }

        free(namelist[n]);
    }
    free(namelist);

    return 0;
}

u8 salCheckEdtFault(ocrGuid_t g) {
    if (!pfIsInitialized) return 0;
    if (ocrGuidIsNull(g)) return 0;
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    if (hashtableConcBucketLockedGet(faultTable, (void*)guid) != NULL)
        return 1; //Found existing record

    /*char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.fault", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }

    struct stat sb;
    if (stat(fname, &sb) == 0) {
        return 1;
    }*/

    return 0;
}

///////////////////////// Guid Table API ////////////////////////

u8 salGuidTablePut(u64 key, ocrGuid_t val) {
    ASSERT(!(ocrGuidIsNull(val)));

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.key", key);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == 0) {
        //Key exists
        ocrGuid_t g;
        salGuidTableGet(key, &g);
        ocrPolicyDomain_t *pd = NULL;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        ocrGuidKind kind;
        pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], g, &kind);
        if (kind & OCR_GUID_EVENT) {
            salTransferPublishedDeps(g, val);
        }
    }

    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    u64 size = sizeof(ocrGuid_t);
    int rc = ftruncate(fd, size);
    if (rc) {
        fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
        ASSERT(0);
        return 1;
    }

    void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    *((ocrGuid_t*)buf) = val;

    rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
    if (rc) {
        fprintf(stderr, "msync failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    return 0;
}

u8 salGuidTableGet(u64 key, ocrGuid_t *val) {
    ASSERT(val != NULL);

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.key", key);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == -1) {
        return 1;
    }
    u64 size = sb.st_size;

    int fd = open(fname, O_RDONLY );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    void *buf = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    *val = *((ocrGuid_t*)buf);

    int rc = munmap(buf, size);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    return 0;
}

u8 salGuidTableRemove(u64 key, ocrGuid_t *val) {
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.key", key);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    if (val != NULL) {
        if (salGuidTableGet(key, val)) {
            fprintf(stderr, "Cannot find existing user guid %s!\n", fname);
            ASSERT(0);
            return 1;
        }
    }

    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    return 0;
}

#endif


#ifdef ENABLE_EXTENSION_PAUSE

#include <signal.h>
#include "utils/pqr-utils.h"

/* NOTE: Below is an optional interface allowing users to
 *       send SIGUSR1 and SIGUSR2 to control pause/query/resume
 *       during execution.  By default the signaled pause command
 *       will block until it succeeds uncontended.
 *
 * SIGUSR1: Toggles Pause and Resume
 * SIGUSR2: Query the contents of queued tasks (will only
 *          succeed if runtime is paused)
 *
 *      This was implemented as a segway into actual API calls and
 *      may be deprecated in the future.
 */
void sig_handler(u32 sigNum) {

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *globalPD = (ocrPolicyDomainHc_t *) pd;


    if(sigNum == SIGUSR1 && globalPD->pqrFlags.runtimePause == false){
        DPRINTF(DEBUG_LVL_WARN, "Pausing Runtime\n");
        salPause(true);
        return;
    }

    if(sigNum == SIGUSR1 && globalPD->pqrFlags.runtimePause == true){
        DPRINTF(DEBUG_LVL_WARN, "Resuming Runtime\n");
        salResume(1);
    }

    if(sigNum == SIGUSR2){
        DPRINTF(DEBUG_LVL_WARN, "Begin fault injection\n");
        salInjectFault();
        return;
    }
}

u32 salPause(bool isBlocking){

    ocrPolicyDomain_t *pd;
    ocrWorker_t *baseWorker;
    getCurrentEnv(&pd, &baseWorker, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;

    while(hal_cmpswap32((u32*)&self->pqrFlags.runtimePause, false, true) == true) {
        // Already paused - try to pause self only if blocked
        if(isBlocking == false)
            return 0;
        // Blocking pause
        if(self->pqrFlags.runtimePause == true) {
            hal_xadd32((u32*)&self->pqrFlags.pauseCounter, 1);
            //Pause called - stop workers
            while(self->pqrFlags.runtimePause == true) {
                hal_pause();
            }
            hal_xadd32((u32*)&self->pqrFlags.pauseCounter, -1);
        }
    }

    hal_xadd32((u32*)&self->pqrFlags.pauseCounter, 1);

    u32 compWorkerCount;
    if(pd->workers[(pd->workerCount)-1]->type == SYSTEM_WORKERTYPE)
        compWorkerCount = (pd->workerCount)-1;
    else
        compWorkerCount = (pd->workerCount);
    while(self->pqrFlags.pauseCounter < compWorkerCount){
        hal_pause();
    }

    return 1;
}

ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;
    ocrGuid_t dataDb = NULL_GUID;

    if(self->pqrFlags.runtimePause == false)
        return NULL_GUID;

    switch(query){
        case OCR_QUERY_READY_EDTS:
            dataDb = hcQueryNextEdts(self, result, size);
            *size = (*size)*sizeof(ocrGuid_t);
            break;

        case OCR_QUERY_EVENTS:
            break;

        case OCR_QUERY_LAST_SATISFIED_DB:
            dataDb = hcQueryPreviousDatablock(self, result, size);
            *size = (*size)*(sizeof(ocrGuid_t));
            break;

        case OCR_QUERY_ALL_EDTS:
            dataDb = hcQueryAllEdts(self, result, size);
            *size = (*size)*(sizeof(ocrGuid_t));
            break;

        default:
            break;
    }

    return dataDb;
}

void salResume(u32 flag){
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;

    if(hal_cmpswap32((u32*)&self->pqrFlags.runtimePause, true, false) == true)
        hal_xadd32((u32*)&self->pqrFlags.pauseCounter, -1);

    return;
}

void salInjectFault(void) {
#ifdef ENABLE_RESILIENCY
    u32 i, j;
    ocrPolicyDomain_t *pd;
    ocrTask_t *task;
    PD_MSG_STACK(msg)
    getCurrentEnv(&pd, NULL, &task, &msg);
    ocrPolicyDomainHc_t *rself = (ocrPolicyDomainHc_t *)pd;

    bool faultInjected = false;
    while(!faultInjected) {
        if (rself->shutdownInProgress != 0) {
            DPRINTF(DEBUG_LVL_WARN, "Unable to inject fault - shutdown in progress\n");
            return;
        }

        if (rself->fault != 0) {
            DPRINTF(DEBUG_LVL_WARN, "Unable to inject fault - previous fault recovery pending\n");
            return;
        }

        for (i = 0; i < pd->workerCount && !faultInjected; i++) {
            ocrWorker_t *worker = pd->workers[i];
            hal_lock(&worker->notifyLock);
            if (worker->activeDepv != NULL) {
                ocrEdtDep_t *depv = (ocrEdtDep_t*)worker->activeDepv;
                u32 depc = task->depc;
                for (j = 0; j < depc; j++) {
                    ocrGuid_t corruptDb = depv[j].guid;
                    if(!ocrGuidIsNull(corruptDb)){
                        ocrFaultArgs_t faultArgs;
                        faultArgs.kind = OCR_FAULT_DATABLOCK_CORRUPTION;
                        faultArgs.OCR_FAULT_ARG_FIELD(OCR_FAULT_DATABLOCK_CORRUPTION).db.guid = corruptDb;
                        faultArgs.OCR_FAULT_ARG_FIELD(OCR_FAULT_DATABLOCK_CORRUPTION).db.metaDataPtr = NULL;
                    #define PD_MSG (&msg)
                    #define PD_TYPE PD_MSG_RESILIENCY_NOTIFY
                        msg.type = PD_MSG_RESILIENCY_NOTIFY | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                        PD_MSG_FIELD_I(properties) = 0;
                        PD_MSG_FIELD_I(faultArgs) = faultArgs;
                        u8 returnCode __attribute__((unused)) = pd->fcts.processMessage(pd, &msg, true);
                        if (PD_MSG_FIELD_O(returnDetail) == 0) {
                            DPRINTF(DEBUG_LVL_WARN, "Corrupting datablock: "GUIDF" by changing a value to 0xff...f\n", GUIDA(corruptDb));
                            faultInjected = true;
                            break;
                        } else {
                            DPRINTF(DEBUG_LVL_INFO, "Unable to inject fault - resiliency manager notify failed\n");
                        }
                    #undef PD_MSG
                    #undef PD_TYPE
                    }
                }
            }
            hal_unlock(&worker->notifyLock);
        }
    }
#endif
}
void registerSignalHandler(){

    struct sigaction action;
    action.sa_handler = ((void (*)(int))&sig_handler);
    action.sa_flags = SA_RESTART;
    sigfillset(&action.sa_mask);
    if(sigaction(SIGUSR1, &action, NULL) != 0) {
        DPRINTF(DEBUG_LVL_WARN, "Couldn't catch SIGUSR1...\n");
    }
     if(sigaction(SIGUSR2, &action, NULL) != 0) {
        DPRINTF(DEBUG_LVL_WARN, "Couldn't catch SIGUSR2...\n");
    }
}

#else
void sig_handler(u32 sigNum){
    return;
}

u32 salPause(bool isBlocking){
    DPRINTF(DEBUG_LVL_WARN, "PQR unsupported on this platform\n");
    return 0;
}

ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){
    DPRINTF(DEBUG_LVL_WARN, "PQR unsupported on this platform\n");
    return NULL_GUID;
}

void salResume(u32 flag){
    DPRINTF(DEBUG_LVL_WARN, "PQR unsupported on this platform\n");
}

void salInjectFault(void){
    DPRINTF(DEBUG_LVL_WARN, "ENABLE_EXTENSION_PAUSE currently undefined in ocr-config.h\n");
}

void registerSignalHandler(){
    return;
}

#endif /* ENABLE_EXTENSION_PAUSE  */
#endif
