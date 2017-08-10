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

typedef struct _edtStorage {
    ocrGuid_t guid;
    ocrGuid_t resilientEdtParent;
    ocrEdt_t funcPtr;
    u32 paramc, depc;
    u64* paramv;
    ocrGuid_t *depv;
} edtStorage_t;

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Init/Destroy Functions  ///////////////////////
///////////////////////////////////////////////////////////////////////////////

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
    ASSERT(pfIsInitialized);
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


///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Utility Functions  ////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//Publish the guid and insert the ptr to the hashtable
static u8 salPublishData(char *fname, u64 key, void *ptr, u64 size) {
    struct stat sb;
    if (stat(fname, &sb) == 0) {
        fprintf(stderr, "Found existing file %s during publish!\n", fname);
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

    //TODO: Cleanup
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

    if (hashtableConcBucketLockedGet(pfTable, (void*)key) != NULL) {
        fprintf(stderr, "Found existing buffer for guid [0x%lx] during publish!\n", key);
        ASSERT(0);
        return 1;
    }
    hashtableConcBucketLockedPut(pfTable, (void*)key, buf);

    rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    return 0;
}

//Read from hashtable ptr. If not found, then from exisiting metadata file.
static u8 salFetchData(char *fname, u64 key, void *ptr, u64 size) {
    void* buf = hashtableConcBucketLockedGet(pfTable, (void*)key);
    if (buf != NULL) {
        memcpy(ptr, buf, size);
        return 0;
    }

    hal_lock(&pfLock);

    buf = hashtableConcBucketLockedGet(pfTable, (void*)key);
    if (buf == NULL) {
        struct stat sb;
        if (stat(fname, &sb) != 0) {
            fprintf(stderr, "Cannot find existing file %s during fetch!\n", fname);
            ASSERT(0);
            return 1;
        }
        int fd = open(fname, O_RDWR);
        if (fd<0) {
            fprintf(stderr, "open failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if (buf == MAP_FAILED) {
            fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
            ASSERT(0);
            return 1;
        }
        hashtableConcBucketLockedPut(pfTable, (void*)key, buf);
        int rc = close(fd);
        if (rc) {
            fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
            ASSERT(0);
            return 1;
        }
    }

    hal_unlock(&pfLock);

    memcpy(ptr, buf, size);
    return 0;
}

//Publish the guid metadata for the first time
static u8 salPublishMetaData(char *fname, void *ptr, u64 size, u8 overwrite) {
    struct stat sb;
    if (!overwrite && stat(fname, &sb) == 0) {
        fprintf(stderr, "Found existing file %s during publish!\n", fname);
        ASSERT(0);
        return 1;
    }
    int fd = open(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    if (ptr != NULL && size > 0) {
        u64 sz = write(fd, (const void *)ptr, size);
        if (sz<0) {
            fprintf(stderr, "write failed: (filename: %s, size: %lu)\n", fname, size);
            ASSERT(0);
            return 1;
        }
        ASSERT(sz == size);
    }
    int rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }
    return 0;
}

//Update exisiting metadata file
static u8 salUpdateMetaData(char *fname, void *ptr, u64 size) {
    ASSERT(ptr != NULL && size > 0);
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing file %s during write!\n", fname);
        ASSERT(0);
        return 1;
    }
    int fd = open(fname, O_WRONLY);
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    u64 sz = write(fd, (const void *)ptr, size);
    if (sz<0) {
        fprintf(stderr, "write failed: (filename: %s, size: %lu)\n", fname, size);
        ASSERT(0);
        return 1;
    }
    ASSERT(sz == size);
    int rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }
    return 0;
}

//Read from exisiting metadata file
static u8 salFetchMetaData(char *fname, void *ptr, u64 size, u8 tryFetch) {
    ASSERT(ptr != NULL && size > 0);
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        if (tryFetch) return 1;
        fprintf(stderr, "Cannot find existing file %s during read!\n", fname);
        ASSERT(0);
        return 1;
    }
    int fd = open(fname, O_RDONLY );
    if (fd<0) {
        if (tryFetch) return 1;
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    u64 sz = read(fd, ptr, size);
    if (sz<0) {
        if (tryFetch) return 1;
        fprintf(stderr, "read failed: (filename: %s, size: %lu)\n", fname, size);
        ASSERT(0);
        return 1;
    }
    ASSERT(sz == size);
    int rc = close(fd);
    if (rc) {
        if (tryFetch) return 1;
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }
    return 0;
}

//Utility function to write a guid to a metadata file
static u8 salWriteGuidPayload(ocrGuid_t guid, ocrGuid_t payload) {
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    //Read DB payload in EVT
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salUpdateMetaData(fname, &payload, sizeof(ocrGuid_t));
    return 0;
}

//Utility function to read a guid from a metadata file
static u8 salReadGuidPayload(ocrGuid_t guid, ocrGuid_t *payload) {
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    //Read DB payload in EVT
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salFetchMetaData(fname, payload, sizeof(ocrGuid_t), 0);
    return 0;
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

/* when return 1, scandir will put this dirent to the list */
static int parse_dep(const struct dirent *dir)
{
    if(dir == NULL) return 0;
    if(dir->d_type == DT_REG) { /* only deal with regular file */
        const char *ext = strrchr(dir->d_name,'.');
        if((ext == NULL) || (ext == dir->d_name))
            return 0;
        if(strcmp(ext, ".dep") == 0)
            return 1;
    }
    return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int parse_fault(const struct dirent *dir)
{
    if(dir == NULL) return 0;
    if(dir->d_type == DT_REG) { /* only deal with regular file */
        const char *ext = strrchr(dir->d_name,'.');
        if((ext == NULL) || (ext == dir->d_name))
            return 0;
        if(strcmp(ext, ".fault") == 0)
            return 1;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Static Functions  /////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//Satisfy a dependence
static u8 salResilientGuidSatisfy(ocrGuid_t guid, u32 slot, ocrGuid_t data) {
    ASSERT(!ocrGuidIsNull(guid));
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], data, &kind);
    ASSERT(((kind == OCR_GUID_DB) && salIsResilientGuid(data)) || ocrGuidIsNull(data));
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], guid, &kind);
    if ((kind & OCR_GUID_EVENT) && salIsResilientGuid(guid))
        return salResilientEventSatisfy(guid, slot, data);
    ASSERT(kind == OCR_GUID_EDT);
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif

    //Check if guid is valid
    ocrLocation_t loc;
    pd->guidProviders[0]->fcts.getLocation(pd->guidProviders[0], guid, &loc);
    if (checkPlatformModelLocationFault(loc)) {
        ocrGuid_t newGuid = NULL_GUID;
        salGuidTableGet(g, &newGuid);
        if (ocrGuidIsNull(newGuid)) {
            DPRINTF(DEBUG_LVL_WARN, "Ignoring SATISFY for guid: "GUIDF" on slot %u due to node failure.\n", GUIDA(guid), slot);
            return 1;
        }
        pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], newGuid, &kind);
        ASSERT(kind == OCR_GUID_EDT);
        guid = newGuid;
    }

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(satisfierGuid.guid) = curEdt?curEdt->guid:NULL_GUID;
    PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(guid.guid) = guid;
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
    return 0;
}

//Ensure only one of signaler and waiter will satisfy a dependence
static u8 salResilientGuidSatisfyMutex(ocrGuid_t guid, u32 slot, ocrGuid_t data, char *depfile) {
    ASSERT(!ocrGuidIsNull(guid));
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    //Try to create sat file exclusively
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.%u.sat", g, slot);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) == 0) {
        return 1; //Found existing sat file
    }

    int fd = open(fname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );
    if (fd<0) {
        return 1; //Could not create sat file exclusively
    }

    //We have successfully and exclusively created the sat file.

    int rc = close(fd);
    if (rc) {
        fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
        ASSERT(0);
        return 1;
    }

    //Ensure dep file exists
    if (stat(depfile, &sb) == 0) {

        //Create node record for sat
        ocrPolicyDomain_t *pd = NULL;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        char sname[FNL];
        int c = snprintf(sname, FNL, "%lu.%u.sat%lu", g, slot, (u64)(pd->myLocation));
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for publish\n");
            ASSERT(0);
            return 1;
        }
        fd = open(sname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
        if (fd<0) {
            fprintf(stderr, "open failed: (filename: %s)\n", sname);
            ASSERT(0);
            return 1;
        }
        rc = close(fd);
        if (rc) {
            fprintf(stderr, "close failed: (filedesc: %d)\n", fd);
            ASSERT(0);
            return 1;
        }

        //Now proceed to satisfy.
        salResilientGuidSatisfy(guid, slot, data);

        //Delete node sat file
        rc = unlink(sname);
        if (rc) {
            fprintf(stderr, "unlink failed: (filename: %s)\n", sname);
            ASSERT(0);
            return 1;
        }

        hal_fence();

        //Delete dep file
        rc = unlink(depfile);
        if (rc) {
            fprintf(stderr, "unlink failed: (filename: %s)\n", depfile);
            ASSERT(0);
            return 1;
        }
    }

    hal_fence();

    rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    return 0;
}

//Release all waiters after a satisfy
static u8 salResilientReleaseWaiters(ocrGuid_t guid, ocrGuid_t data) {
    ASSERT(!ocrGuidIsNull(guid));
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    struct dirent **namelist;
    int n = scandir(".", &namelist, parse_dep, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }
    while (n--) {
        char *c = strchr(namelist[n]->d_name,'.');
        *c = '\0';
        u64 sg = strtoul(namelist[n]->d_name, NULL, 10);
        ASSERT(sg > 0);
        *c = '.';
        if (g == sg) {
            char *d = ++c;
            c = strchr(d,'.');
            *c = '\0';
            u64 dg = strtoul(d, NULL, 10);
            ASSERT(dg > 0);
            *c = '.';
            char *s = ++c;
            c = strchr(s,'.');
            *c = '\0';
            u64 slot = strtoul(s, NULL, 10);
            *c = '.';

            //printf("%s\n", namelist[n]->d_name);
            ocrGuid_t dguid = NULL_GUID;
            u8 ret = salFetchMetaData(namelist[n]->d_name, &dguid, sizeof(ocrGuid_t), 1);
            if (ret == 0) {
                ASSERT(!ocrGuidIsNull(dguid));
#if GUID_BIT_COUNT == 64
                u64 d = dguid.guid;
#elif GUID_BIT_COUNT == 128
                u64 d = dguid.lower;
#else
#error Unknown type of GUID
#endif
                ASSERT(d == dg);
                salResilientGuidSatisfyMutex(dguid, slot, data, namelist[n]->d_name);
            }
        }
        free(namelist[n]);
    }
    free(namelist);

    //Now check if guid has any older execution version
    ocrGuid_t oldguid = NULL_GUID;
    salGuidTableGet(g, &oldguid);
    if (!ocrGuidIsNull(oldguid)) {
        return salResilientReleaseWaiters(oldguid, data);
    }
    return 0;
}

//Create global and local records for EDTs that have failed
static u8 salCreateFaultRecord(u64 guid) {
    //Create fault record
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
    //Record fault guid locally
    hashtableConcBucketLockedPut(faultTable, (void*)guid, (void*)guid);
    return 0;
}

//Find EDTs in failed node
static u8 salCreateEdtSetForRecoveryExtensions(char *extension) {
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
        *s = '.';
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
                salCreateFaultRecord(guid);
            }
        }
        free(namelist[n]);
    }
    free(namelist);
    return 0;
}

static u8 salCreateRootEdtSetForRecovery(u64 nodeId) {
    char extension[FNL];
    int c = snprintf(extension, FNL, ".root");
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
        ocrLocation_t loc;
        salFetchMetaData(namelist[n]->d_name, &loc, sizeof(ocrLocation_t), 0);
        if (loc == nodeId) {
            char *s = strchr(namelist[n]->d_name,'.');
            *s = '\0';
            u64 guid = strtoul(namelist[n]->d_name, NULL, 10);
            ASSERT(guid > 0);
            *s = '.';
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
                    salCreateFaultRecord(guid);
                }
            }
        }
        free(namelist[n]);
    }
    free(namelist);
    return 0;
}

static u8 salCreateEdtSetForRecovery(ocrLocation_t nodeId) {
    //First process the .node# files
    char extension[FNL];
    int c = snprintf(extension, FNL, ".node%lu", (u64)nodeId);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    salCreateEdtSetForRecoveryExtensions(extension);

    //Next process the .sat# files
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    c = snprintf(extension, FNL, ".sat%lu", (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    salCreateEdtSetForRecoveryExtensions(extension);

    salCreateRootEdtSetForRecovery(nodeId);
    return 0;
}

//Find sat files in failed node
static u8 salCleanupFailedSatisfy(ocrLocation_t nodeId) {
    char extension[FNL];
    int c = snprintf(extension, FNL, ".sat%lu", (u64)nodeId);
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
        //Delete node sat file
        int rc = unlink(namelist[n]->d_name);
        if (rc) {
            fprintf(stderr, "unlink failed: (filename: %s)\n", namelist[n]->d_name);
            ASSERT(0);
            return 1;
        }

        //Delete sat file
        char *s = strrchr(namelist[n]->d_name,'.');
        strcpy(s, ".sat");
        struct stat sb;
        if (stat(namelist[n]->d_name, &sb) != 0) {
            fprintf(stderr, "stat failed: (filename: %s)\n", namelist[n]->d_name);
            ASSERT(0);
            return 1;
        }
        free(namelist[n]);
    }
    free(namelist);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  API Functions  ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//Returns true if guid is resilient
u8 salIsResilientGuid(ocrGuid_t guid) {
    if (ocrGuidIsNull(guid)) return 0;
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }
    struct stat sb;
    if (stat(fname, &sb) == 0) {
        return 1;
    }
    return 0;
}

//Returns true if guid is satisfied
u8 salIsSatisfiedResilientGuid(ocrGuid_t guid) {
    if (ocrGuidIsNull(guid)) return 0;
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 0;
    }
    struct stat sb;
    if (stat(fname, &sb) == 0) {
        if (sb.st_size > 0) {
            ASSERT(sb.st_size == sizeof(ocrGuid_t));
            return 1;
        }
    }
    return 0;
}

//Create a resilient guid during metadata creation
u8 salResilientGuidCreate(ocrGuid_t guid, ocrGuid_t pguid, u64 key, u64 ip, u64 ac) {
    ASSERT(!ocrGuidIsNull(guid));
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    //Create guid file
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishMetaData(fname, NULL, 0, 0);

#if GUID_BIT_COUNT == 64
    u64 p = pguid.guid;
#elif GUID_BIT_COUNT == 128
    u64 p = pguid.lower;
#else
#error Unknown type of GUID
#endif
    //Create api signature for guid
    char sname[FNL];
    c = snprintf(sname, FNL, "%lu.%lu.%lu.api", p, ip, ac);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishMetaData(sname, &guid, sizeof(ocrGuid_t), 1);

    //If old verion exists, then insert old guid into guid table
    ocrGuid_t faultGuid = NULL_GUID;
    salGuidTableGet(p, &faultGuid);
    if (!ocrGuidIsNull(faultGuid)) {
#if GUID_BIT_COUNT == 64
        u64 f = faultGuid.guid;
#elif GUID_BIT_COUNT == 128
        u64 f = faultGuid.lower;
#else
#error Unknown type of GUID
#endif
        char fname[FNL];
        c = snprintf(fname, FNL, "%lu.%lu.%lu.api", f, ip, ac);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for publish\n");
            ASSERT(0);
            return 1;
        }
        struct stat sb;
        if (stat(fname, &sb) == 0) {
            ocrGuid_t oldguid = NULL_GUID;
            salFetchMetaData(fname, &oldguid, sizeof(ocrGuid_t), 0);
            ASSERT(!ocrGuidIsNull(oldguid));
            //salGuidTableRemove(g, NULL);
            salGuidTablePut(g, oldguid);
        }
    }

    return 0;
}

//Create a mapping from new EDT guid to old guid for recovery EDTs
u8 salResilientFaultGuidMap(ocrGuid_t keyGuid, ocrGuid_t valGuid) {
    ASSERT(!ocrGuidIsNull(keyGuid));
    ASSERT(!ocrGuidIsNull(valGuid));
#if GUID_BIT_COUNT == 64
    u64 g = keyGuid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = keyGuid.lower;
#else
#error Unknown type of GUID
#endif
    salGuidTablePut(g, valGuid);
    return 0;
}

//Add a dependence between resilient objects
u8 salResilientAddDependence(ocrGuid_t sguid, ocrGuid_t dguid, u32 slot) {
    if (!ocrGuidIsNull(sguid) && !salIsResilientGuid(sguid)) return 1;
    if (!salIsResilientGuid(dguid)) return 1;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind skind, dkind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], sguid, &skind);
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], dguid, &dkind);
    ASSERT((skind & OCR_GUID_EVENT) || (skind == OCR_GUID_DB) || (skind == OCR_GUID_NONE));
    ASSERT((dkind & OCR_GUID_EVENT) || (dkind == OCR_GUID_EDT));
    if ((skind & OCR_GUID_EVENT) && salIsSatisfiedResilientGuid(sguid)) {
        salReadGuidPayload(sguid, &sguid);
        pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], sguid, &skind);
        ASSERT(((skind == OCR_GUID_DB) && salIsResilientGuid(sguid)) || ocrGuidIsNull(sguid));
    }

    //Convert add dep to satisfy
    if (((skind == OCR_GUID_DB) && salIsSatisfiedResilientGuid(sguid)) || ocrGuidIsNull(sguid)) {
        salResilientGuidSatisfy(dguid, slot, sguid);
        return 0;
    }

#if GUID_BIT_COUNT == 64
    u64 s = sguid.guid;
    u64 d = dguid.guid;
#elif GUID_BIT_COUNT == 128
    u64 s = sguid.lower;
    u64 d = dguid.lower;
#else
#error Unknown type of GUID
#endif
    //Create dependence file
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.%lu.%u.dep", s, d, slot);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishMetaData(fname, &dguid, sizeof(ocrGuid_t), 0);

    hal_fence();

    //Check again if src is now satisfied
    if (salIsSatisfiedResilientGuid(sguid)) {
        if (skind & OCR_GUID_EVENT) {
            salReadGuidPayload(sguid, &sguid);
            pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], sguid, &skind);
            ASSERT(((skind == OCR_GUID_DB) && salIsResilientGuid(sguid)) || ocrGuidIsNull(sguid));
        }
        salResilientGuidSatisfyMutex(dguid, slot, sguid, fname);
    }

    return 0;
}

//Satisfy a resilient event
u8 salResilientEventSatisfy(ocrGuid_t guid, u32 slot, ocrGuid_t data) {
    if (!salIsResilientGuid(guid)) return 1;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], guid, &kind);
    ASSERT(kind & OCR_GUID_EVENT);

    //Publish data to event guid
    salWriteGuidPayload(guid, data);

    //Release event deps
    salResilientReleaseWaiters(guid, data);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Runtime Functions  ////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//Publish the data-block when parent scope completes
u8 salResilientDataBlockPublish(ocrDataBlock_t *db) {
    ASSERT(db != NULL);
    ocrGuid_t dbGuid = db->guid;
    ASSERT(salIsResilientGuid(dbGuid));
#if GUID_BIT_COUNT == 64
    u64 g = dbGuid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = dbGuid.lower;
#else
#error Unknown type of GUID
#endif
    //Publish DB contents
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.db", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishData(fname, g, db->ptr, db->size);

    //Publish DB guid
    salWriteGuidPayload(dbGuid, dbGuid);

    //Release DB deps
    salResilientReleaseWaiters(dbGuid, dbGuid);
    return 0;
}

//Update a published guid's contents
//User takes responsibility of maintaining consistency
u8 salResilientDataBlockRepublish(ocrGuid_t guid, void *ptr) {
    ASSERT(pfIsInitialized);
    ASSERT(!(ocrGuidIsNull(guid)));
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], guid, &kind);
    if (kind != OCR_GUID_DB) {
        DPRINTF(DEBUG_LVL_WARN, "Cannot re-publish DB: (GUID: "GUIDF" is not a data-block)\n", GUIDA(guid));
        return 1;
    }
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.db", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for re-publish\n");
        ASSERT(0);
        return 1;
    }

    struct stat sb;
    if (stat(fname, &sb) != 0) {
        DPRINTF(DEBUG_LVL_WARN, "Cannot re-publish DB "GUIDF": (data-block is not yet published)\n", GUIDA(guid));
        return 1;
    }
    u64 size = sb.st_size;

    void* buf = hashtableConcBucketLockedGet(pfTable, (void*)g);
    ASSERT(buf != ptr);
    u8 available = (buf == NULL) ? 0 : 1;

    //Lock access to file from other threads
    hal_lock(&pfLock);

    //Update existing buffer contents
    if (available) memcpy(buf, ptr, size);

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

    if (available) {
        //unmap file
        rc = munmap(buf, size);
        if (rc) {
            fprintf(stderr, "munmap failed for buffer %p of size %lu\n", buf, size);
            ASSERT(0);
            return 1;
        }
    } else {
        hashtableConcBucketLockedPut(pfTable, (void*)g, buf);
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

//Fetch the contents of a published data-block in a newly created buffer
void* salResilientDataBlockFetch(ocrGuid_t guid, u64 *gSize) {
    ASSERT(pfIsInitialized);
    if (gSize) *gSize = 0;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrGuidKind kind;
    pd->guidProviders[0]->fcts.getKind(pd->guidProviders[0], guid, &kind);
    if (kind != OCR_GUID_DB) {
        DPRINTF(DEBUG_LVL_WARN, "Cannot fetch DB: (GUID: "GUIDF" is not a data-block)\n", GUIDA(guid));
        return NULL;
    }
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.db", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for fetch\n");
        ASSERT(0);
        return NULL;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        DPRINTF(DEBUG_LVL_WARN, "Cannot fetch DB "GUIDF": (data-block is not yet published)\n", GUIDA(guid));
        return NULL;
    }
    u64 size = sb.st_size;
    void *buf = pd->fcts.pdMalloc(pd, size);
    RESULT_ASSERT(salFetchData(fname, g, buf, size), ==, 0);
    if (gSize) *gSize = size;
    return buf;
}

//Remove resilient data-block
u8 salResilientDataBlockRemove(ocrDataBlock_t *db) {
    //TODO
    return 0;
}

//Publish the task when it is ready to be scheduled for execution
u8 salResilientTaskPublish(ocrTask_t *task) {
    ASSERT(task != NULL);
    ASSERT(task->state == ALLACQ_EDTSTATE);
    ASSERT(task->flags & OCR_TASK_FLAG_RESILIENT);
    ASSERT(salIsResilientGuid(task->guid));
#if GUID_BIT_COUNT == 64
    u64 g = task->guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = task->guid.lower;
#else
#error Unknown type of GUID
#endif
    //Remove EDT root info
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.root", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT\n");
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
        c = snprintf(fname, FNL, "%lu.edt", g);
        if (c < 0 || c >= FNL) {
            fprintf(stderr, "failed to create filename for publish\n");
            ASSERT(0);
            return 1;
        }
        if (stat(fname, &sb) != 0) {
            fprintf(stderr, "Root EDT state is corrupt! (Cannot find EDT storage: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
        rc = unlink(fname);
        if (rc) {
            fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
            ASSERT(0);
            return 1;
        }
    }

    //Create EDT storage metadata
    int i;
    ocrTaskHc_t *hcTask = (ocrTaskHc_t*)task;
    ASSERT(hcTask->resolvedDeps != NULL);
    u64 size = sizeof(edtStorage_t) + sizeof(u64) * task->paramc + sizeof(ocrGuid_t) * task->depc;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u8 *buf = pd->fcts.pdMalloc(pd, size);
    edtStorage_t *edt = (edtStorage_t *)buf;
    edt->guid = task->guid;
    edt->resilientEdtParent = task->resilientEdtParent;
    edt->funcPtr = task->funcPtr;
    edt->paramc = task->paramc;
    edt->depc = task->depc;
    edt->paramv = task->paramc ? (u64*)(buf + sizeof(edtStorage_t)) : NULL;
    if (task->paramc) memcpy(edt->paramv, task->paramv, sizeof(u64) * task->paramc);
    edt->depv = task->depc ? (ocrGuid_t*)(buf + sizeof(edtStorage_t) + sizeof(u64) * task->paramc) : NULL;
    for (i = 0; i < task->depc; i++) {
        ASSERT(ocrGuidIsNull(hcTask->resolvedDeps[i].guid) || (hcTask->resolvedDeps[i].ptr == UNINITIALIZED_DB_FETCH_PTR));
        edt->depv[i] = hcTask->resolvedDeps[i].guid;
    }

    //Publish EDT metadata
    c = snprintf(fname, FNL, "%lu.edt", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishMetaData(fname, buf, size, 0);
    pd->fcts.pdFree(pd, buf);

    //Record node state
    salRecordEdtAtNode(task->guid, pd->myLocation);
    return 0;
}

//Record the resilient EDT scope when it is used in a location
u8 salRecordEdtAtNode(ocrGuid_t guid, ocrLocation_t loc) {
    if (ocrGuidIsNull(guid)) return 0;
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.node%lu", g, (u64)loc);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    if (stat(fname, &sb) == 0) {
        return 0; //Found existing record
    }

    ASSERT(salIsResilientGuid(guid));
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
    return 0;
}

//Create a record for a root EDT
u8 salResilientRecordTaskRoot(ocrTask_t *task) {
    ASSERT(task != NULL);
    ASSERT(salIsResilientGuid(task->guid));
#if GUID_BIT_COUNT == 64
    u64 g = task->guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = task->guid.lower;
#else
#error Unknown type of GUID
#endif
    //Create root file
    char gname[FNL];
    int c = snprintf(gname, FNL, "%lu.root", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrLocation_t loc = pd->myLocation;
    salPublishMetaData(gname, &loc, sizeof(ocrLocation_t), 0);

    //Create EDT storage metadata
    int i;
    u64 size = sizeof(edtStorage_t) + sizeof(u64) * task->paramc + sizeof(ocrGuid_t) * task->depc;
    u8 *buf = pd->fcts.pdMalloc(pd, size);
    edtStorage_t *edt = (edtStorage_t *)buf;
    edt->guid = task->guid;
    edt->resilientEdtParent = task->resilientEdtParent;
    edt->funcPtr = task->funcPtr;
    edt->paramc = task->paramc;
    ASSERT(task->depc > 0);
    edt->depc = task->depc;
    edt->paramv = task->paramc ? (u64*)(buf + sizeof(edtStorage_t)) : NULL;
    memcpy(edt->paramv, task->paramv, sizeof(u64) * task->paramc);
    edt->depv = (ocrGuid_t*)(buf + sizeof(edtStorage_t) + sizeof(u64) * task->paramc);
    for (i = 0; i < task->depc; i++) {
        edt->depv[i] = UNINITIALIZED_GUID;
    }

    //Publish EDT metadata
    char fname[FNL];
    c = snprintf(fname, FNL, "%lu.edt", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    salPublishMetaData(fname, buf, size, 0);
    pd->fcts.pdFree(pd, buf);
    return 0;
}

//Update EDT storage buffer for root task
u8 salResilientTaskRootUpdate(ocrGuid_t guid, u32 slot, ocrGuid_t data) {
    ASSERT(!ocrGuidIsNull(guid));
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif
    //Check if root file needs to be updated
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.root", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for root\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        return 0; //No available root node
    }

    c = snprintf(fname, FNL, "%lu.edt", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Root EDT state is corrupt! (Cannot find EDT storage: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if (fd<0) {
        fprintf(stderr, "open failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    u64 size = sb.st_size;
    void *buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if (buf == MAP_FAILED) {
        fprintf(stderr, "mmap failed for size %lu (filename: %s filedesc: %d)\n", size, fname, fd);
        ASSERT(0);
        return 1;
    }

    edtStorage_t *edtBuf = (edtStorage_t*)buf;
    edtBuf->depv = (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64)));
    ASSERT((slot < edtBuf->depc) && (ocrGuidIsUninitialized(edtBuf->depv[slot])));
    edtBuf->depv[slot] = data;

    int rc = msync(buf, size, MS_INVALIDATE | MS_SYNC);
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

u8 salResilientTaskRemove(ocrGuid_t guid) {
    u64 i;
    ASSERT(pfIsInitialized);
    ASSERT(!ocrGuidIsNull(guid));
#if GUID_BIT_COUNT == 64
    u64 g = guid.guid;
#elif GUID_BIT_COUNT == 128
    u64 g = guid.lower;
#else
#error Unknown type of GUID
#endif

    //Remove EDT metadata
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.guid", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during remove!\n", g);
        ASSERT(0);
        return 1;
    }
    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    hal_fence();

    //Remove EDT storage
    c = snprintf(fname, FNL, "%lu.edt", g);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT\n");
        ASSERT(0);
        return 1;
    }
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Cannot find existing guid [0x%lx] during remove!\n", g);
        ASSERT(0);
        return 1;
    }
    rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }

    //Now remove the records of this EDT in every node that was impacted by it
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    u64 nbRanks = pd->neighborCount + 1;
    for (i = 0; i < nbRanks; i++) {
        char fname[FNL];
        int c = snprintf(fname, FNL, "%lu.node%lu", g, (u64)i);
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

u8 salResilientGuidDestroy(ocrGuid_t guid) {
    //TODO
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Guid Table Functions  /////////////////////////
///////////////////////////////////////////////////////////////////////////////

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
        //If old verion exists, then insert old guid into guid table
        ocrGuid_t oldguid = NULL_GUID;
        salGuidTableRemove(key, &oldguid);
        ASSERT(!ocrGuidIsNull(oldguid));
#if 1
#if GUID_BIT_COUNT == 64
        u64 g = val.guid;
#elif GUID_BIT_COUNT == 128
        u64 g = val.lower;
#else
#error Unknown type of GUID
#endif
        ocrGuid_t oldguidRef = NULL_GUID;
        salGuidTableGet(g, &oldguidRef);
        if (ocrGuidIsNull(oldguidRef)) {
            salGuidTablePut(g, oldguid);
        } else {
            ASSERT(ocrGuidIsEq(oldguid, oldguidRef));
        }
#endif
    }

    salPublishMetaData(fname, &val, sizeof(ocrGuid_t), 0);
    return 0;
}

u8 salGuidTableGet(u64 key, ocrGuid_t *val) {
    if (val == NULL) return 1;
    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.key", key);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return 1;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        return 1;
    }
    salFetchMetaData(fname, (void*)val, sizeof(ocrGuid_t), 0);
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
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        return 1; //Cannot find existing key
    }
    salGuidTableGet(key, val);
    int rc = unlink(fname);
    if (rc) {
        fprintf(stderr, "unlink failed: (filename: %s)\n", fname);
        ASSERT(0);
        return 1;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  Fault Handler Functions  //////////////////////
///////////////////////////////////////////////////////////////////////////////

void salThreadExit() {
    pthread_exit(NULL);
}

void salThreadRecover() {
    hal_xadd32(&workerCounter, 1);
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    while(pd->faultCode != 0);
}

//Check if EDT is part of a failed sub-graph
u8 salCheckEdtFault(ocrGuid_t g) {
    ASSERT(pfIsInitialized);
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
    return 0;
}

//First response after failure on buddy node
u8 salProcessNodeFailureAtBuddy(ocrLocation_t nodeId) {
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //Pause execution on buddy node
    pd->faultCode = OCR_NODE_FAILURE_OTHER;
    hal_fence();
    u32 maxCount = pd->workerCount - 1;
    while(workerCounter < maxCount);
    hal_fence();

    //Determine all EDT guids impacted by node failure
    salCreateEdtSetForRecovery(nodeId);

    //Update platform model
    notifyPlatformModelLocationFault(nodeId);

    //Resume execution
    hal_fence();
    workerCounter = 0;
    hal_fence();
    pd->faultCode = 0;

    return 0;
}

//Process node failure on all existing nodes other than buddy node
u8 salProcessNodeFailure(ocrLocation_t nodeId) {
    ASSERT(pfIsInitialized);
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //Pause execution
    pd->faultCode = OCR_NODE_FAILURE_OTHER;
    hal_fence();
    u32 maxCount = pd->workerCount - 1;
    while(workerCounter < maxCount);
    hal_fence();

    //Next process the .sat# files
    char extension[FNL];
    int c = snprintf(extension, FNL, ".sat%lu", (u64)pd->myLocation);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for EDT record\n");
        ASSERT(0);
        return 1;
    }
    salCreateEdtSetForRecoveryExtensions(extension);

    //Build fault table on local node
    struct dirent **namelist;
    int n = scandir(".", &namelist, parse_fault, alphasort);
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
        *s = '.';
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

    char fname[FNL];
    int c = snprintf(fname, FNL, "%lu.edt", guid);
    if (c < 0 || c >= FNL) {
        fprintf(stderr, "failed to create filename for publish\n");
        ASSERT(0);
        return;
    }
    struct stat sb;
    if (stat(fname, &sb) != 0) {
        fprintf(stderr, "Node state is corrupt, recovery failed! (Cannot find EDT storage: %s)\n", fname);
        ASSERT(0);
        return;
    }
    u64 bufsize = sb.st_size;
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    void *buf = pd->fcts.pdMalloc(pd, bufsize);
    RESULT_ASSERT(salFetchMetaData(fname, buf, bufsize, 0), ==, 0);
    edtStorage_t *edtBuf = (edtStorage_t*)buf;

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
    edtBuf->paramv = edtBuf->paramc ? (u64*)((size_t)edtBuf + sizeof(edtStorage_t)) : NULL;
    edtBuf->depv = edtBuf->depc ? (ocrGuid_t*)((size_t)edtBuf + sizeof(edtStorage_t) + (edtBuf->paramc * sizeof(u64))) : NULL;
    ocrGuid_t tmpl;
    ocrEdtTemplateCreate(&tmpl, edtBuf->funcPtr, edtBuf->paramc, edtBuf->depc);
    ocrGuid_t edt = edtBuf->guid;
    ocrGuid_t oEvt;
    ocrEdtCreate(&edt, tmpl, edtBuf->paramc, edtBuf->paramv, edtBuf->depc, edtBuf->depv, EDT_PROP_RESILIENT | EDT_PROP_RECOVERY, NULL_HINT, &oEvt);
    DPRINTF(DEBUG_LVL_WARN, "Recovery EDT created: "GUIDF"\n", GUIDA(edt));
    pd->fcts.pdFree(pd, buf);
}

//Recover from fault by re-executing all EDTs impacted by fault
u8 salRecoverNodeFailureAtBuddy(ocrLocation_t nodeId) {
    //Finalize all EDT guids impacted by node failure
    salCreateEdtSetForRecovery(nodeId);

    //Cleanup failed satisfies
    salCleanupFailedSatisfy(nodeId);

    //Reschedule execution sub-graph dominator EDTs
    iterateHashtable(faultTable, salImportEdt, NULL);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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
