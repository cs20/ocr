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

#if defined(linux) || defined(__APPLE__)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
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
static hashtable_t * pfTable = NULL;
static hashtable_t * rectable = NULL;
static lock_t pfLock;
static lock_t recLock;

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

//Initialize the hashtable
void salInitPublishFetch() {
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    pfTable = newHashtableBucketLockedModulo(pd, RECORD_INCR_SIZE);
    rectable = newHashtableBucketLockedModulo(pd, RECORD_INCR_SIZE);
    pfLock = INIT_LOCK;
    recLock = INIT_LOCK;

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

//Check if guid has been published
//Returns true or false
u8 salIsPublished(ocrGuid_t g) {
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

//Publish the guid and optionally insert the ptr to the hashtable
//Returns the file descriptor of the published data
static u8 salPublishInternal(ocrGuid_t g, void *ptr, u64 size, u8 activeBuffer) {
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    void* buf = hashtableConcBucketLockedGet(pfTable, (void*)guid);
    if (buf != NULL) {
        fprintf(stderr, "Found existing guid [0x%lx] during publish!\n", guid);
        ASSERT(0);
        return 1;
    }

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

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

    buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
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
    return salPublishInternal(g, ptr, size, 1);
}

//Copy a published guid to a new buffer and return the ptr
//Caller takes responsibility of free-ing the buffer
void* salFetchInternal(ocrGuid_t g, u64 *gSize, u8 activeBuffer, u8 doCopy) {
    ASSERT(!(ocrGuidIsNull(g)));
    if (gSize) *gSize = 0;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
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
    return salFetchInternal(g, gSize, 1, 1);
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
    return 0;
}

typedef struct _edtStorage {
    ocrEdt_t funcPtr;
    u32 paramc, depc;
    u64* paramv;
    ocrGuid_t *depv;
} edtStorage_t;

u8 salPublishEdt(ocrTask_t *task) {
    if (!pfIsInitialized) return 1;
    int i;
    ASSERT(task->flags & OCR_TASK_FLAG_RESILIENT);
    ASSERT(task->state == ALLACQ_EDTSTATE);
    ocrTaskHc_t *hcTask = (ocrTaskHc_t*)task;
    ASSERT(hcTask->resolvedDeps != NULL);
    u64 size = sizeof(edtStorage_t) + sizeof(u64) * task->paramc + sizeof(ocrGuid_t) * task->depc;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u8 *buf = pd->fcts.pdMalloc(pd, size);
    edtStorage_t *edt = (edtStorage_t *)buf;
    edt->funcPtr = task->funcPtr;
    edt->paramc = task->paramc;
    edt->depc = task->depc;
    edt->paramv = (u64*)(buf + sizeof(edtStorage_t));
    memcpy(edt->paramv, task->paramv, sizeof(u64) * task->paramc);
    edt->depv = (ocrGuid_t*)(buf + sizeof(edtStorage_t) + sizeof(u64) * task->paramc);
    for (i = 0; i < task->depc; i++) {
        ASSERT(ocrGuidIsNull(hcTask->resolvedDeps[i].guid) || (hcTask->resolvedDeps[i].ptr == UNINITIALIZED_DB_FETCH_PTR));
        edt->depv[i] = hcTask->resolvedDeps[i].guid;
    }
    salPublishInternal(task->guid, buf, size, 0);
    salRecordEdt(task->guid);
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
    //Remove from current node
    ASSERT(hashtableConcBucketLockedRemove(rectable, (void*)guid, NULL));
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.node%lu", guid, (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
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

    //Remove from all other nodes
    u64 nbRanks = pd->neighborCount + 1;
    for (i = 0; i < nbRanks; i++) {
        if (i == (u64)pd->myLocation) continue;

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

u8 salRecordEdt(ocrGuid_t g) {
    if (!pfIsInitialized) return 1;
    ASSERT(!(ocrGuidIsNull(g)));
#if GUID_BIT_COUNT == 64
    u64 guid = g.guid;
#elif GUID_BIT_COUNT == 128
    u64 guid = g.lower;
#else
#error Unknown type of GUID
#endif
    if (hashtableConcBucketLockedGet(rectable, (void*)guid) != NULL)
        return 0; //Found existing record

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.node%lu", guid, (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }

    hal_lock(&recLock);

    struct stat sb;
    if (stat(fname, &sb) == 0) {
        ASSERT(hashtableConcBucketLockedGet(rectable, (void*)guid) != NULL);
        hal_unlock(&recLock);
        return 0; //Found existing record
    }

    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    ASSERT(hashtableConcBucketLockedGet(rectable, (void*)guid) == NULL);
    hashtableConcBucketLockedPut(rectable, (void*)guid, (void*)((u64)fd));

    int rc = ftruncate(fd, 0);
    if (rc) {
        fprintf(stderr, "ftruncate failed: (filename: %s filedesc: %d)\n", fname, fd);
        ASSERT(0);
        return 1;
    }

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    hal_unlock(&recLock);

    return 0;
}

static u8 salImportPublishedEdts(ocrLocation_t nodeId, int *rankMap) {
    u64 i, n;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    hashtable_t * htable = newHashtableModulo(pd, RECORD_INCR_SIZE); //Create hashtable for EDT pruning
    hashtable_t * rtable = newHashtableModulo(pd, RECORD_INCR_SIZE); //Create hashtable for EDT recovery

    //Add current node edt state to hashtable
    {
        nodeStateHeader_t *nsHeader = (nodeStateHeader_t*)nodeStateBuf;
        nodeStateRecord_t *records = (nodeStateRecord_t*)((size_t)nodeStateBuf + sizeof(nodeStateHeader_t));
        u64 numRecords = nsHeader->numRecords;
        for (i = 0; i < numRecords; i++) {
            ocrGuid_t guid = records[i].guid;
#if GUID_BIT_COUNT == 64
            u64 guidVal = guid.guid;
#elif GUID_BIT_COUNT == 128
            u64 guidVal = guid.lower;
#else
#error Unknown type of GUID
#endif
            void* ptr = hashtableNonConcGet(htable, (void*)guidVal);
            if (ptr == NULL) {
                hashtableNonConcPut(htable, (void*)guidVal, (void*)(&records[i]));
            }
        }
    }

    //Add remaining node edt states to hashtable
    void *failedNodeStateBuf = NULL;
    u64 failedNodeStateBufSize = 0;
    u64 nbRanks = pd->neighborCount + 1;
    for (n = 0; n < nbRanks; n++) {
        if (n != (u64)pd->myLocation && rankMap[n] != -1) {
            char fname[FNL];
            int c = snprintf(fname, FNL, "node%lu.state", (u64)n);
            if (c < 0 || c >= FNL) {
                fprintf(stderr, "failed to create filename for node state\n");
                ASSERT(0);
                return 1;
            }

            struct stat sb;
            if (stat(fname, &sb) == -1) {
                fprintf(stderr, "Cannot find existing node state %s!\n", fname);
                ASSERT(0);
                return 1;
            }
            u64 size = sb.st_size;
            ASSERT(size > 0);

            int fd = open(fname, O_RDWR);
            if (fd<0) {
                fprintf(stderr, "open failed: (filename: %s)\n", fname);
                ASSERT(0);
                return 1;
            }

            void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
            if (buf == MAP_FAILED) {
                fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
                ASSERT(0);
                return 1;
            }
            ASSERT(buf != NULL);
            if (n == (u64)nodeId) {
                failedNodeStateBuf = buf;
                failedNodeStateBufSize = size;
            }

            nodeStateHeader_t *nsHeader = (nodeStateHeader_t*)buf;
            nodeStateRecord_t *records = (nodeStateRecord_t*)((size_t)buf + sizeof(nodeStateHeader_t));
            u64 numRecords = nsHeader->numRecords;
            for (i = 0; i < numRecords; i++) {
                ocrGuid_t guid = records[i].guid;
#if GUID_BIT_COUNT == 64
                u64 guidVal = guid.guid;
#elif GUID_BIT_COUNT == 128
                u64 guidVal = guid.lower;
#else
#error Unknown type of GUID
#endif
                void* ptr = hashtableNonConcGet(htable, (void*)guidVal);
                if (ptr == NULL) {
                    hashtableNonConcPut(htable, (void*)guidVal, (void*)(&records[i]));
                }
            }

            int rc = close(fd);
            if (rc) {
                fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
                ASSERT(0);
                return 1;
            }
        }
    }

    //Find dominators of failed node state EDTs and repeat execution of those EDTs
    ASSERT(failedNodeStateBuf != NULL);
    nodeStateHeader_t *nsHeader = (nodeStateHeader_t*)failedNodeStateBuf;
    nodeStateRecord_t *records = (nodeStateRecord_t*)((size_t)failedNodeStateBuf + sizeof(nodeStateHeader_t));
    u64 numRecords = nsHeader->numRecords;
    for (i = 0; i < numRecords; i++) {
        ocrGuid_t edtGuid = NULL_GUID;
        ocrGuid_t guid = records[i].guid;
        ocrGuid_t pguid = records[i].pguid;
#if GUID_BIT_COUNT == 64
        u64 pguidVal = pguid.guid;
#elif GUID_BIT_COUNT == 128
        u64 pguidVal = pguid.lower;
#else
#error Unknown type of GUID
#endif
        void* ptr = hashtableNonConcGet(htable, (void*)pguidVal);
        if (ptr == NULL) {
            edtGuid = guid;
        } else {
            nodeStateRecord_t *precord = (nodeStateRecord_t*)ptr;
            ocrGuid_t ppguid = precord->pguid;
#if GUID_BIT_COUNT == 64
            u64 ppguidVal = ppguid.guid;
#elif GUID_BIT_COUNT == 128
            u64 ppguidVal = ppguid.lower;
#else
#error Unknown type of GUID
#endif
            if (hashtableNonConcGet(htable, (void*)ppguidVal) != NULL) {
                fprintf(stderr, "Node state is corrupt; recovery failed for node %lu\n", nodeId);
                return 1;
            }
            if (hashtableNonConcGet(rtable, (void*)pguidVal) == NULL) {
                edtGuid = pguid;
            }
        }
        if (!ocrGuidIsNull(edtGuid)) {
            //Create EDT
            u64 bufsize = 0;
            edtStorage_t *edtBuf = (edtStorage_t*)salFetchInternal(edtGuid, &bufsize, 0, 0);
            if (edtBuf == NULL) {
                fprintf(stderr, "Node state is corrupt; recovery failed for node %lu\n", nodeId);
                return 1;
            }
            edtBuf->paramv = (u64*)((size_t)edtBuf + sizeof(edtStorage_t));
            edtBuf->depv = (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64)));

            ocrGuid_t tmpl;
            ocrEdtTemplateCreate(&tmpl, edtBuf->funcPtr, edtBuf->paramc, edtBuf->depc);
            ocrGuid_t edt, oEvt;
            ocrEdtCreate(&edt, tmpl, edtBuf->paramc, edtBuf->paramv, edtBuf->depc, edtBuf->depv, EDT_PROP_RESILIENT | EDT_PROP_RECOVERY, NULL_HINT, &oEvt);
        }
    }

    //Remove failed node state
    rankMap[nodeId] = -1;

    int rc = munmap(failedNodeStateBuf, failedNodeStateBufSize);
    if (rc) {
        fprintf(stderr, "munmap failed for buffer %p of size %lu\n", failedNodeStateBuf, failedNodeStateBufSize);
        ASSERT(0);
        return 1;
    }

    char fname[FNL];
    int c = snprintf(fname, FNL, "node%lu.state", (u64)nodeId);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for node state\n");
        ASSERT(0);
        return 1;
    }

    rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    return 0;
}

u8 salHandleNodeFailure(ocrLocation_t nodeId, int *rankMap) {
    if (!pfIsInitialized) return 1;
    //return 1;
    return salImportPublishedEdts(nodeId, rankMap);
}

u8 salGuidTablePut(u64 key, ocrGuid_t val) {
    ASSERT(!(ocrGuidIsNull(val)));

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.key", key);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
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
