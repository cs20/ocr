/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_MPI

#include "debug.h"

#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-worker.h"

#include "utils/ocr-utils.h"

#include "mpi-comm-platform.h"


#ifdef DEBUG_MPI_HOSTNAMES
// For gethostname
#include <unistd.h>
#endif

//BUG #609 system header: replace this with some INT_MAX from sal header
#include "limits.h"

//
// Compile-time constants
//

// DEBUG_MPI_HOSTNAMES: Dumps hostname MPI processes are started on

#define DEBUG_TYPE COMM_PLATFORM
#define DEBUG_LVL_NEWMPI DEBUG_LVL_VERB

#ifdef OCR_MONITOR_NETWORK
#include "ocr-sal.h"
#endif

//
// MPI library Init/Finalize
//

/**
 * @brief Initialize the MPI library.
 */
void platformInitMPIComm(int * argc, char *** argv) {
    RESULT_ASSERT(MPI_Init(argc, argv), ==, MPI_SUCCESS);
}
/**
 * @brief Finalize the MPI library (no more remote calls after that).
 */
void platformFinalizeMPIComm() {
    RESULT_ASSERT(MPI_Finalize(), ==, MPI_SUCCESS);
}

//
// MPI communication implementation strategy
//

// To tag outstanding send/recv
#define RECV_ANY_ID 0
#define SEND_ANY_ID 0

// To tag outstanding send/recv for which we know the size
#define RECV_ANY_FIXSZ_ID 1
#define SEND_ANY_FIXSZ_ID 1

// Expected maximum fixed size is the PD msg size
// Note the size doesn't account for extra payload attached at the end of the message.
// In that case, it is illegal to use the fixed message size infrastructure.
#define RECV_ANY_FIXSZ (sizeof(ocrPolicyMsg_t))

typedef struct _mpiCommHandle_t {
    u64 msgId; // The MPI comm layer message id for this communication
    u32 properties;
    ocrPolicyMsg_t * msg;
    MPI_Request * status;
    int src;
    u8 deleteSendMsg;
} mpiCommHandle_t;

static ocrLocation_t mpiRankToLocation(int mpiRank) {
    //BUG #605 Locations spec: identity integer cast for now
    return (ocrLocation_t) mpiRank;
}

static int locationToMpiRank(ocrLocation_t location) {
    //BUG #605 Locations spec: identity integer cast for now
    return (int) location;
}

/**
 * @brief Internal use - Returns a new message
 * Used to accomodate incoming messages / outgoing serialization
 */
static ocrPolicyMsg_t * allocateNewMessage(ocrCommPlatform_t * self, u32 size) {
    ocrPolicyDomain_t * pd = self->pd;
    ocrPolicyMsg_t * message = pd->fcts.pdMalloc(pd, size);
    initializePolicyMessage(message, size);
    return message;
}

#define CODE_RESIZE_POOL(SZ, MAX, POOL, HDL) \
    ocrCommPlatform_t * self = (ocrCommPlatform_t *) mpiComm; \
    u32 curSize = SZ; \
    MPI_Request * newPool = self->pd->fcts.pdMalloc(self->pd, sizeof(MPI_Request) * curSize * 2); \
    mpiCommHandle_t * newHdlPool = self->pd->fcts.pdMalloc(self->pd, sizeof(mpiCommHandle_t) * curSize * 2); \
    hal_memCopy(newPool, POOL, curSize, false); \
    hal_memCopy(newHdlPool, HDL, curSize, false); \
    u32 i; \
    for(i=0; i<curSize; i++) { \
        newHdlPool[i].status = &newPool[i]; \
    } \
    self->pd->fcts.pdFree(self->pd, POOL); \
    self->pd->fcts.pdFree(self->pd, HDL); \
    POOL = newPool; \
    HDL = newHdlPool; \
    MAX = curSize * 2;

static void resizeSendPool(ocrCommPlatformMPI_t * mpiComm) {
    CODE_RESIZE_POOL(mpiComm->sendPoolSz, mpiComm->sendPoolMax, mpiComm->sendPool, mpiComm->sendHdlPool);
}

static void resizeRecvPool(ocrCommPlatformMPI_t * mpiComm) {
    CODE_RESIZE_POOL(mpiComm->recvPoolSz, mpiComm->recvPoolMax, mpiComm->recvPool, mpiComm->recvHdlPool);
}

static void resizeRecvFxdPool(ocrCommPlatformMPI_t * mpiComm) {
    CODE_RESIZE_POOL(mpiComm->recvFxdPoolSz, mpiComm->recvFxdPoolMax, mpiComm->recvFxdPool, mpiComm->recvFxdHdlPool);
}

// This is to debug misuse of slots in pools.
// The sentinel is set when the pool is compacted
#define MPI_CP_DEBUG_SENTINEL (-2)

#define CODE_COMPACT_POOL(SZ, POOL, HDL) \
    SZ--; \
    if ((SZ != 0) && ((SZ != idx))) { \
        POOL[idx] = POOL[SZ]; \
        HDL[idx] = HDL[SZ]; \
        HDL[idx].status = &POOL[idx]; \
        ASSERT(HDL[idx].msgId != MPI_CP_DEBUG_SENTINEL); \
    } \
    DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] compactPool set to MPI_CP_DEBUG_SENTINEL hdl_addr=%p idx=%"PRIu32" msgId=%"PRIu64"\n", \
        locationToMpiRank(((ocrCommPlatform_t *)mpiComm)->pd->myLocation), &POOL[SZ], SZ, HDL[SZ].msgId);

static void compactSendPool(ocrCommPlatformMPI_t * mpiComm, u32 idx) {
    CODE_COMPACT_POOL(mpiComm->sendPoolSz, mpiComm->sendPool, mpiComm->sendHdlPool)
#ifdef MPI_CP_DEBUG
    mpiComm->sendHdlPool[mpiComm->sendPoolSz].msgId = MPI_CP_DEBUG_SENTINEL;
#endif
}

static void compactRecvPool(ocrCommPlatformMPI_t * mpiComm, u32 idx) {
    CODE_COMPACT_POOL(mpiComm->recvPoolSz, mpiComm->recvPool, mpiComm->recvHdlPool)
#ifdef MPI_CP_DEBUG
    mpiComm->recvHdlPool[mpiComm->recvPoolSz].msgId = MPI_CP_DEBUG_SENTINEL;
#endif
}

static void compactRecvFxdPool(ocrCommPlatformMPI_t * mpiComm, u32 idx) {
    CODE_COMPACT_POOL(mpiComm->recvFxdPoolSz, mpiComm->recvFxdPool, mpiComm->recvFxdHdlPool)
#ifdef MPI_CP_DEBUG
    mpiComm->recvFxdHdlPool[mpiComm->recvFxdPoolSz].msgId = MPI_CP_DEBUG_SENTINEL;
#endif
}

static inline u32 resolveHandleIdx(ocrCommPlatformMPI_t * mpiComm, mpiCommHandle_t * mpiHandle,  mpiCommHandle_t * hdlPool) {
    return mpiHandle - hdlPool;
}

#define CODE_MV_HDL(SZ, MAX, POOL, HDL, RESIZE, TYPE) \
    u32 idx = SZ; \
    HDL[idx] = *hdl; \
    HDL[idx].status = &POOL[idx]; \
    SZ++; \
    DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] Moved send msgId=%"PRIu64" " TYPE " @idx=%"PRIu32" checked as=%"PRIu32" \n", \
        locationToMpiRank(((ocrCommPlatform_t *)mpiComm)->pd->myLocation), hdl->msgId, idx, resolveHandleIdx(mpiComm, &HDL[idx], HDL)); \
    ASSERT(hdl->msgId != MPI_CP_DEBUG_SENTINEL); \
    ASSERT(HDL[idx].msgId != -1); \
    if (SZ == MAX) { \
        RESIZE(mpiComm); \
    } \
    return &HDL[idx];


static mpiCommHandle_t * moveHdlSendToRecv(ocrCommPlatformMPI_t * mpiComm, mpiCommHandle_t * hdl) {
    CODE_MV_HDL(mpiComm->recvPoolSz, mpiComm->recvPoolMax, mpiComm->recvPool, mpiComm->recvHdlPool, resizeRecvPool, "recv")
}

static mpiCommHandle_t * moveHdlSendToRecvFxd(ocrCommPlatformMPI_t * mpiComm, mpiCommHandle_t * hdl) {
    CODE_MV_HDL(mpiComm->recvFxdPoolSz, mpiComm->recvFxdPoolMax, mpiComm->recvFxdPool, mpiComm->recvFxdHdlPool, resizeRecvFxdPool, "recvFxd")
}

static bool isFixedMsgSize(u32 type) {
    // Allow one-way satisfy to go through the fixed size message channel
    // Can add more messages type here...
    return ((type & PD_MSG_TYPE_ONLY) == PD_MSG_DEP_SATISFY);
}

static bool isFixedMsgSizeResponse(u32 type) {
    //By default, will not try to receive through the fixed size message channel
    return false;
}

static void postRecvFixedSzMsg(ocrCommPlatformMPI_t * mpiComm, mpiCommHandle_t * hdl) {
    ASSERT(hdl->msg != NULL);
    RESULT_ASSERT(MPI_Irecv(hdl->msg, RECV_ANY_FIXSZ, MPI_BYTE, hdl->src, hdl->msgId, MPI_COMM_WORLD, hdl->status), ==, MPI_SUCCESS);
#ifdef OCR_MONITOR_NETWORK
    hdl->msg->rcvTime = salGetTime();
#endif

}

// The following can be received here:
// 1) An unexpected request of fixed size
// 2) A fixed size response to a request
static u8 testRecvFixedSzMsg(ocrCommPlatformMPI_t * mpiComm, ocrPolicyMsg_t ** msg) {
    // Look for outstanding incoming
#ifdef OCR_ASSERT
    MPI_Status status;
#endif
    int idx;
    int completed;
#ifdef OCR_ASSERT
    int ret = MPI_Testany(mpiComm->recvFxdPoolSz, mpiComm->recvFxdPool, &idx, &completed, &status);
    if (ret != MPI_SUCCESS) {
        char str[MPI_MAX_ERROR_STRING];
        int restr;
        MPI_Error_string(ret, (char *) &str, &restr);
        PRINTF("%s\n", str);
        ASSERT(false);
    }
#else
    RESULT_ASSERT(MPI_Testany(mpiComm->recvFxdPoolSz, mpiComm->recvFxdPool, &idx, &completed, MPI_STATUS_IGNORE), ==, MPI_SUCCESS);
#endif
    if (idx != MPI_UNDEFINED) {
        ASSERT(completed);
        // Retrieve the message buffer through indexing into the handle pool
        mpiCommHandle_t * hdl = &mpiComm->recvFxdHdlPool[idx];
        *msg = hdl->msg;
        ASSERT(((*msg)->type & PD_MSG_REQUEST) || ((*msg)->type & PD_MSG_RESPONSE));
        ASSERT((hdl->src == MPI_ANY_SOURCE) ? 1 : (hdl->msg->msgId == hdl->msgId));
        ASSERT((((*msg)->type & PD_MSG_REQUEST) || ((*msg)->type & PD_MSG_RESPONSE)) &&
           "error: Received message header seems to be corrupted");

        // Unmarshall the message. We check to make sure the size is OK
        // This should be true since MPI seems to make sure to send the whole message
        u64 baseSize = 0, marshalledSize = 0;
        ocrPolicyMsgGetMsgSize(*msg, &baseSize, &marshalledSize, MARSHALL_DBPTR | MARSHALL_NSADDR);
#ifdef OCR_ASSERT
        int count;
        ASSERT(MPI_Get_count(&status, MPI_BYTE, &count) == MPI_SUCCESS);
        ASSERT((baseSize+marshalledSize) == count);
#endif
        // The unmarshalling is just fixing up fields to point to the correct
        // payload address trailing after the base message.
        //BUG #604 Communication API extensions
        //1)     I'm thinking we can further customize un/marshalling for MPI. Because we use
        //       mpi tags, we actually don't need to send the header part of response message.
        //       We can directly recv the message at msg + header, update the msg header
        //       to be a response + flip src/dst.
        //2)     See if we can improve unmarshalling by keeping around pointers for the various
        //       payload to be unmarshalled
        //3)     We also need to deguidify all the fatGuids that are 'local' and decide
        //       where it is appropriate to do it.
        //       - REC: I think the right place would be in the user code (ie: not the comm layer)
        ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg,
                                  MARSHALL_APPEND | MARSHALL_NSADDR | MARSHALL_DBPTR);

        // In 1) it was an irecv to 'listen' to outstanding requests, reuse handle to post a new recv
        if (hdl->msgId == RECV_ANY_FIXSZ_ID) {
            // By design this is the first recv posted. We can change that but with the current compaction
            // scheme it's better to have it at the beginning else it becomes the de-facto upper bound
            ASSERT(idx == 0);
            ocrPolicyMsg_t * newMsg = allocateNewMessage((ocrCommPlatform_t *) mpiComm, RECV_ANY_FIXSZ);
            hdl->msg = newMsg;
            ASSERT(hdl->src == MPI_ANY_SOURCE);
            postRecvFixedSzMsg(mpiComm, hdl);
        } else { // case 2) recycle the mpi handle.
            compactRecvFxdPool(mpiComm, idx);
            // // Swap current entry with last one to keep the pool compact
        }
        return POLL_MORE_MESSAGE;
    }
    return POLL_NO_MESSAGE;
}

static mpiCommHandle_t * initMpiHandle(ocrCommPlatform_t * self, mpiCommHandle_t * hdl, u64 id, u32 properties, ocrPolicyMsg_t * msg, u8 deleteSendMsg) {
    hdl->msgId = id;
    hdl->properties = properties;
    hdl->msg = msg;
    hdl->deleteSendMsg = deleteSendMsg;
    return hdl;
}

static mpiCommHandle_t * createMpiSendHandle(ocrCommPlatform_t * self, u64 id, u32 properties, ocrPolicyMsg_t * msg, u8 deleteSendMsg) {
    ocrCommPlatformMPI_t * dself = (ocrCommPlatformMPI_t *) self;
    mpiCommHandle_t * hdl = &dself->sendHdlPool[dself->sendPoolSz];
    hdl->status = &dself->sendPool[dself->sendPoolSz];
    dself->sendPoolSz++;
    initMpiHandle(self, hdl, id, properties, msg, deleteSendMsg);
    if (dself->sendPoolSz == dself->sendPoolMax) {
        resizeSendPool(dself);
    }
    ASSERT(hdl->msgId != -1);
    return &dself->sendHdlPool[dself->sendPoolSz-1];
}

static mpiCommHandle_t * createMpiRecvFxdHandle(ocrCommPlatform_t * self, u64 id, u32 properties, ocrPolicyMsg_t * msg, u8 deleteSendMsg) {
    ocrCommPlatformMPI_t * dself = (ocrCommPlatformMPI_t *) self;
    mpiCommHandle_t * hdl = &dself->recvFxdHdlPool[dself->recvFxdPoolSz];
    hdl->status = &dself->recvFxdPool[dself->recvFxdPoolSz];
    dself->recvFxdPoolSz++;
    initMpiHandle(self, hdl, id, properties, msg, deleteSendMsg);
    if (dself->recvFxdPoolSz == dself->recvFxdPoolMax) {
        resizeRecvFxdPool(dself);
    }
    ASSERT(hdl->msgId != -1);
    return &dself->recvFxdHdlPool[dself->recvFxdPoolSz-1];
}


//
// Communication API
//

static u8 MPICommSendMessage(ocrCommPlatform_t * self,
                      ocrLocation_t target, ocrPolicyMsg_t * message,
                      u64 *id, u32 properties, u32 mask) {
    START_PROFILE(commplt_MPICommSendMessage);
    u64 bufferSize = message->bufferSize;
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);

    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsgGetMsgSize(message, &baseSize, &marshalledSize, MARSHALL_DBPTR | MARSHALL_NSADDR);
    u64 fullMsgSize = baseSize + marshalledSize;

    //BUG #602 multi-comm-worker: msgId incr only works if a single comm-worker per rank,
    //do we want OCR to provide PD, system level counters ?
    // Always generate an identifier for a new communication to give back to upper-layer

    u64 mpiId = mpiComm->msgId++;

    // If we're sending a request, set the message's msgId to this communication id
    if (message->type & PD_MSG_REQUEST) {
        message->msgId = mpiId;
    } else {
        // For response in ASYNC set the message ID as any.
        ASSERT(message->type & PD_MSG_RESPONSE);
        if (properties & ASYNC_MSG_PROP) {
            message->msgId = SEND_ANY_ID;
        }
        // else, for regular responses, just keep the original
        // message's msgId the calling PD is waiting on.
    }

    ocrPolicyMsg_t * messageBuffer = message;
    // Check if we need to allocate a new message buffer:
    //  - Does the serialized message fit in the current message ?
    //  - Is the message persistent (then need a copy anyway) ?
    bool deleteSendMsg = false;
    if ((fullMsgSize > bufferSize) || !(properties & PERSIST_MSG_PROP)) {
        // Allocate message and marshall a copy
        messageBuffer = allocateNewMessage(self, fullMsgSize);
        ocrPolicyMsgMarshallMsg(message, baseSize, (u8*)messageBuffer,
            MARSHALL_FULL_COPY | MARSHALL_DBPTR | MARSHALL_NSADDR);
        if (properties & PERSIST_MSG_PROP) {
            // Message was persistent, two cases:
            if ((properties & TWOWAY_MSG_PROP) && (!(properties & ASYNC_MSG_PROP))) {
                //  - The message is two-way and is not asynchronous: do not touch the
                //    message parameter, but record that we indeed made a new copy that
                //    we will have to deallocate when the communication is completed.
                deleteSendMsg = true;
            } else {
                //  - The message is one-way: By design, all one-way are heap-allocated copies.
                //    It is the comm-platform responsibility to free them, do it now since we've
                //    made our own copy.
                self->pd->fcts.pdFree(self->pd, message);
                message = NULL; // to catch misuses later in this function call
            }
        } else {
            // Message wasn't persistent, hence the caller is responsible for deallocation.
            // It doesn't matter whether the communication is one-way or two-way.
            properties |= PERSIST_MSG_PROP;
            ASSERT(false && "not used in current implementation (hence not tested)");
        }
    } else {
        ocrMarshallMode_t marshallMode = (ocrMarshallMode_t) GET_PROP_U8_MARSHALL(properties);
        if (marshallMode == 0) {
            // Marshall the message. We made sure we had enough space.
            ocrPolicyMsgMarshallMsg(messageBuffer, baseSize, (u8*)messageBuffer,
                                    MARSHALL_APPEND | MARSHALL_DBPTR | MARSHALL_NSADDR);
        } else {
            ASSERT(marshallMode == MARSHALL_FULL_COPY);
            //BUG #604 Communication API extensions
            // They are needed in a comm-platform such as mpi or gasnet
            // but it feels off that the calling context already set those
            // because it shouldn't know beforehand if the communication is
            // crossing address space
            // | MARSHALL_DBPTR :  only for acquire/release message
            // | MARSHALL_NSADDR : only used when unmarshalling so far
            ASSERT ((((messageBuffer->type & PD_MSG_TYPE_ONLY) == PD_MSG_DB_ACQUIRE) ||
                    ((messageBuffer->type & PD_MSG_TYPE_ONLY) == PD_MSG_DB_RELEASE))
                    ? (marshallMode & (MARSHALL_DBPTR | MARSHALL_NSADDR)) : 1);
        }
    }

    // Warning: From now on, exclusively use 'messageBuffer' instead of 'message'
    ASSERT(fullMsgSize == messageBuffer->usefulSize);
    // Prepare MPI call arguments
    MPI_Datatype datatype = MPI_BYTE;
    int targetRank = locationToMpiRank(target);
    ASSERT(targetRank > -1);
    MPI_Comm comm = MPI_COMM_WORLD;

    // Setup request's MPI send
    mpiCommHandle_t * handle = createMpiSendHandle(self, mpiId, properties, messageBuffer, deleteSendMsg);

    // Setup request's response
    if ((messageBuffer->type & PD_MSG_REQ_RESPONSE) && !(properties & ASYNC_MSG_PROP)) {
        // In probe mode just record the recipient id to be checked later
        handle->src = targetRank;
    }

    // If this send is for a response, use message's msgId as tag to
    // match the source recv operation that had been posted on the request send.
    // Note that msgId is set to SEND_ANY_ID a little earlier in the case of asynchronous
    // message like DB_ACQUIRE. It allows to handle the response as a one-way message that
    // is not tied to any particular request at destination
    int tag = (messageBuffer->type & PD_MSG_RESPONSE) ? messageBuffer->msgId : (isFixedMsgSize(messageBuffer->type) ? SEND_ANY_FIXSZ_ID : SEND_ANY_ID);
    MPI_Request * status = handle->status;
    // Fixed size message just never have been copied to accomodate the need for more space
    ASSERT(isFixedMsgSize(messageBuffer->type) ? (deleteSendMsg == false) : true);

    DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] posting isend for msgId=%"PRIu64" tag= %"PRId32" msg=%p type=%"PRIx32" "
            "fullMsgSize=%"PRIu64" marshalledSize=%"PRIu64" to MPI rank %"PRId32"\n",
            locationToMpiRank(self->pd->myLocation), messageBuffer->msgId, tag,
            messageBuffer, messageBuffer->type, fullMsgSize, marshalledSize, targetRank);

    //If this assert bombs, we need to implement message chunking
    //or use a larger MPI datatype to send the message.
    ASSERT((fullMsgSize < INT_MAX) && "Outgoing message is too large");
    ASSERT((messageBuffer->srcLocation == self->pd->myLocation) &&
        (messageBuffer->destLocation != self->pd->myLocation) &&
        (targetRank == messageBuffer->destLocation));

#ifdef OCR_MONITOR_NETWORK
    messageBuffer->sendTime = salGetTime();
#endif

    int res = MPI_Isend(messageBuffer, (int) fullMsgSize, datatype, targetRank, tag, comm, status);

    if (res == MPI_SUCCESS) {
        *id = mpiId;
    } else {
        //BUG #603 define error for comm-api
        ASSERT(false);
    }

    RETURN_PROFILE(res);
}

static u8 probeIncoming(ocrCommPlatform_t *self, int src, int tag, ocrPolicyMsg_t ** msg, int bufferSize) {
    //PERF: Would it be better to always probe and allocate messages for responses on the fly
    //rather than having all this book-keeping for receiving and reusing requests space ?
    //Sound we should get a pool of small messages (let say sizeof(ocrPolicyMsg_t) and allocate
    //variable size message on the fly).
    MPI_Status status;

#ifdef MPI_MSG // USE MPI messages
    MPI_Message mpiMsg;
#endif

    int available = 0;
#ifdef MPI_MSG
    RESULT_ASSERT(MPI_Improbe(src, tag, MPI_COMM_WORLD, &available, &mpiMsg, &status), ==, MPI_SUCCESS);
#else
    RESULT_ASSERT(MPI_Iprobe(src, tag, MPI_COMM_WORLD, &available, &status), ==, MPI_SUCCESS);
#endif
    if (available) {
        ASSERT(msg != NULL);
        ASSERT((bufferSize == 0) ? ((tag == RECV_ANY_ID) && (*msg == NULL)) : 1);
        src = status.MPI_SOURCE; // Using MPI_ANY_SOURCE for the receive might get a different message
        // Look at the size of incoming message
        MPI_Datatype datatype = MPI_BYTE;
        int count;
        RESULT_ASSERT(MPI_Get_count(&status, datatype, &count), ==, MPI_SUCCESS);
        ASSERT(count != 0);
        // Reuse request's or allocate a new message if incoming size is greater.
        if (count > bufferSize) {
            *msg = allocateNewMessage(self, count);
        }
        ASSERT(*msg != NULL);
        MPI_Comm comm = MPI_COMM_WORLD;
#ifdef MPI_MSG
        RESULT_ASSERT(MPI_Mrecv(*msg, count, datatype, &mpiMsg, MPI_STATUS_IGNORE), ==, MPI_SUCCESS);
#else
        RESULT_ASSERT(MPI_Recv(*msg, count, datatype, src, tag, comm, MPI_STATUS_IGNORE), ==, MPI_SUCCESS);
#endif
#ifdef OCR_MONITOR_NETWORK
        (*msg)->rcvTime = salGetTime();
#endif
        // After recv, the message size must be updated since it has just been overwritten.
        (*msg)->usefulSize = count;
        (*msg)->bufferSize = count;

        // This check usually fails in the 'ocrPolicyMsgGetMsgSize' when there
        // has been an issue in MPI. It manifest as a received buffer being complete
        // garbage whereas the sender doesn't detect any corruption of the message when
        // it is recycled. Tinkering with multiple MPI implementation it sounds the issue
        // is with the MPI library not being able to register a hook for malloc calls.
        ASSERT((((*msg)->type & (PD_MSG_REQUEST | PD_MSG_RESPONSE)) != (PD_MSG_REQUEST | PD_MSG_RESPONSE)) &&
           (((*msg)->type & PD_MSG_REQUEST) || ((*msg)->type & PD_MSG_RESPONSE)) &&
           "error: Try to link the MPI library first when compiling your OCR program");

#ifdef OCR_MONITOR_NETWORK
        (*msg)->rcvTime = salGetTime();
#endif

        // Unmarshall the message. We check to make sure the size is OK
        // This should be true since MPI seems to make sure to send the whole message
        u64 baseSize = 0, marshalledSize = 0;
        ocrPolicyMsgGetMsgSize(*msg, &baseSize, &marshalledSize, MARSHALL_DBPTR | MARSHALL_NSADDR);
        ASSERT((baseSize+marshalledSize) == count);
        // The unmarshalling is just fixing up fields to point to the correct
        // payload address trailing after the base message.
        //BUG #604 Communication API extensions
        //1)     I'm thinking we can further customize un/marshalling for MPI. Because we use
        //       mpi tags, we actually don't need to send the header part of response message.
        //       We can directly recv the message at msg + header, update the msg header
        //       to be a response + flip src/dst.
        //2)     See if we can improve unmarshalling by keeping around pointers for the various
        //       payload to be unmarshalled
        //3)     We also need to deguidify all the fatGuids that are 'local' and decide
        //       where it is appropriate to do it.
        //       - REC: I think the right place would be in the user code (ie: not the comm layer)
        ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg,
                                  MARSHALL_APPEND | MARSHALL_NSADDR | MARSHALL_DBPTR);
        return POLL_MORE_MESSAGE;
    }
    return POLL_NO_MESSAGE;
}

// Workflow:
// - 1) Check for send completion
// - 2) Check for arbitrary size receive completion
//      - Either awaited responses from src/tag or outstanding request
// - 3) Check for fixed sized receive completion
//      - Either awaited responses from src/tag or outstanding request
static u8 MPICommPollMessageInternal(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                              u32 properties, u32 *mask) {
    START_PROFILE(commplt_MPICommPollMessageInternal);
    ocrPolicyDomain_t * pd = self->pd;
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);

    ASSERT(msg != NULL);
    ASSERT((*msg == NULL) && "MPI comm-layer cannot poll for a specific message");

    // Checking send completions
    if (mpiComm->sendPoolSz > 0) {
        START_PROFILE(commplt_MPICommPollMessageInternal_progress_send);
        int idx;
        int completed;
        RESULT_ASSERT(MPI_Testany(mpiComm->sendPoolSz, mpiComm->sendPool, &idx, &completed, MPI_STATUS_IGNORE), ==, MPI_SUCCESS);
        if (idx != MPI_UNDEFINED) { // found
            ASSERT(completed);
            ASSERT((idx < mpiComm->sendPoolSz) && (idx >= 0));
            mpiCommHandle_t * mpiHandle = &mpiComm->sendHdlPool[idx];
            DPRINTF(DEBUG_LVL_VVERB,"[MPI %"PRId32"] sent msg=%p src=%"PRId32", dst=%"PRId32", msgId=%"PRIu64", type=0x%"PRIx32", usefulSize=%"PRIu64"\n",
                    locationToMpiRank(self->pd->myLocation), mpiHandle->msg,
                    locationToMpiRank(mpiHandle->msg->srcLocation), locationToMpiRank(mpiHandle->msg->destLocation),
                    mpiHandle->msg->msgId, mpiHandle->msg->type, mpiHandle->msg->usefulSize);
            u32 msgProperties = mpiHandle->properties;
            // By construction, either messages are persistent in API's upper levels
            // or they've been made persistent on the send through a copy.
            ASSERT(msgProperties & PERSIST_MSG_PROP);
            // Delete the message if one-way (request or response).
            // Otherwise message might be used to store the response later.
            if (!(msgProperties & TWOWAY_MSG_PROP) || (msgProperties & ASYNC_MSG_PROP)) {
                pd->fcts.pdFree(pd, mpiHandle->msg);
            } else { // Transition to recv pool
                // if response is fixed size
                if (isFixedMsgSizeResponse(mpiHandle->msg->type)) {
                    mpiCommHandle_t * recvHdl = moveHdlSendToRecvFxd(mpiComm, mpiHandle);
                    // mpiHandle's src is already preset to the rank we should be receiving from
                    // Directly post an irecv for this answer using (src,tag)
                    postRecvFixedSzMsg(mpiComm, recvHdl);
                } else {
                    // The message requires a response but we do not know its size: will use MPI probe
                    mpiCommHandle_t * recvHdl = moveHdlSendToRecv(mpiComm, mpiHandle);
                    DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] moving to incoming: message of type %"PRIx32" with msgId=%"PRIu64" HDL=> type %"PRIx32" with msgId=%"PRIu64", idx=%"PRIu32"\n",
                                        locationToMpiRank(self->pd->myLocation), mpiHandle->msg->type, mpiHandle->msg->msgId, recvHdl->msg->type, recvHdl->msgId, resolveHandleIdx(mpiComm, recvHdl, mpiComm->recvHdlPool));
                }
            }
            compactSendPool(mpiComm, idx);
        }
        EXIT_PROFILE;
    }

    // Checking unknown size recv completions
    u8 res = POLL_NO_MESSAGE;
    {
    START_PROFILE(commplt_MPICommPollMessageInternal_progress_probe_awaited);
    u32 i;
    mpiCommHandle_t * recvHdlPool = mpiComm->recvHdlPool;
    for (i=0; i < mpiComm->recvPoolSz;) { // Do not cache upper bound as the pool is dynamically resized
        mpiCommHandle_t * mpiHandle = &recvHdlPool[i];
        // Probe a specific incoming message. Response message overwrites the request one
        // if it fits. Otherwise, a new message is allocated. Upper-layers are responsible
        // for deallocating the request/response buffers.
        ocrPolicyMsg_t * reqMsg = mpiHandle->msg;
        res = probeIncoming(self, mpiHandle->src, (int) mpiHandle->msgId, &mpiHandle->msg, mpiHandle->msg->bufferSize);
        // The message is properly unmarshalled at this point
        if (res == POLL_MORE_MESSAGE) {
            DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] Received an awaited message of type %"PRIx32" with msgId=%"PRIu64" recvHdlPool idx=%"PRIu32"\n",
                    locationToMpiRank(self->pd->myLocation), mpiHandle->msg->type, mpiHandle->msg->msgId, resolveHandleIdx(mpiComm, mpiHandle, mpiComm->recvHdlPool));
#ifdef OCR_ASSERT
            if (reqMsg != mpiHandle->msg) {
                // Original request hasn't changed
                ASSERT((reqMsg->srcLocation == pd->myLocation) && (reqMsg->destLocation != pd->myLocation));
                // Newly received response
                ASSERT((mpiHandle->msg->srcLocation != pd->myLocation) && (mpiHandle->msg->destLocation == pd->myLocation));
            } else {
                // Reused, so it is the response
                ASSERT((reqMsg->srcLocation != pd->myLocation) && (reqMsg->destLocation == pd->myLocation));
            }
#endif
            if ((reqMsg != mpiHandle->msg) && mpiHandle->deleteSendMsg) {
                // we did allocate a new message to store the response
                // and the request message was already an internal copy
                // made by the comm-platform, hence the pointer is only
                // known here and must be deallocated. The sendMessage
                // caller still has a pointer to the original message.
                pd->fcts.pdFree(pd, reqMsg);
            }
            ASSERT(mpiHandle->msg->msgId == mpiHandle->msgId);
            *msg = mpiHandle->msg;
            // Compact take the last element and put it first.
            compactRecvPool(mpiComm, resolveHandleIdx(mpiComm, mpiHandle, mpiComm->recvHdlPool));
            break;
            // return res;
        } else {
            i++;
        }
    }
    EXIT_PROFILE;
    }
    if (res == POLL_MORE_MESSAGE) {
        RETURN_PROFILE(res);
    }


    // Checking fixed size recv completions
    {
    START_PROFILE(commplt_MPICommPollMessageInternal_progress_probe_ostd);
    // Rule 1: Try to receive a fixed size message
    // If successful, the handle associated with the message is automatically discarded or repurposed
    // u8 retCodeFix = testRecvFixedSzMsg(mpiComm, msg);
    res = testRecvFixedSzMsg(mpiComm, msg);
    EXIT_PROFILE;
    }

    if (res == POLL_MORE_MESSAGE) {
        RETURN_PROFILE(POLL_MORE_MESSAGE);
    } // else fall-through to advance other messages


    u8 retCode = POLL_NO_MESSAGE;
    {
    START_PROFILE(commplt_MPICommPollMessageInternal_progress_probe_awaited);
    // Check for outstanding incoming. If any, a message is allocated
    // and returned through 'msg'.
    retCode = probeIncoming(self, MPI_ANY_SOURCE, RECV_ANY_ID, msg, 0);
    // Message is properly un-marshalled at this point
    EXIT_PROFILE;
    }

    if (retCode == POLL_NO_MESSAGE) {
        retCode |= (mpiComm->sendPoolSz == 0) ? POLL_NO_OUTGOING_MESSAGE : 0;
        // Always one unexpected recv posted for fixed size but there should be no awaited recv
        retCode |= ((mpiComm->recvFxdPoolSz == 1) && (mpiComm->recvPoolSz == 0)) ? POLL_NO_INCOMING_MESSAGE : 0;
    } else {
        DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRId32"] Received outstanding message of type %"PRIx32" with msgId=%"PRIu64" \n",
                locationToMpiRank(self->pd->myLocation), (*msg)->type, (*msg)->msgId);
    }
    RETURN_PROFILE(retCode);
}

static u8 MPICommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                      u32 properties, u32 *mask) {
    ocrCommPlatformMPI_t * mpiComm __attribute__((unused)) = ((ocrCommPlatformMPI_t *) self);
    // Not supposed to be polled outside RL_USER_OK
    ASSERT_BLOCK_BEGIN(((mpiComm->curState >> 4) == RL_USER_OK))
    DPRINTF(DEBUG_LVL_NEWMPI,"[MPI %"PRIu64"] Illegal runlevel[%"PRId32"] reached in MPI-comm-platform pollMessage\n",
            mpiRankToLocation(self->pd->myLocation), (mpiComm->curState >> 4));
    ASSERT_BLOCK_END
    return MPICommPollMessageInternal(self, msg, properties, mask);
}

static u8 MPICommWaitMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                      u32 properties, u32 *mask) {
    START_PROFILE(commplt_MPICommWaitMessage);
    u8 ret = 0;
    do {
        ret = self->fcts.pollMessage(self, msg, properties, mask);
    } while(ret != POLL_MORE_MESSAGE);

    RETURN_PROFILE(ret);
}

static u8 MPICommSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);
    u8 toReturn = 0;
    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
    case RL_NETWORK_OK:
        // Nothing
        break;
    case RL_PD_OK:
        if ((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            //Initialize base
            self->pd = PD;
            //BUG #605 Locations spec: commPlatform and worker have a location, are the supposed to be the same ?
            int rank=0;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            DPRINTF(DEBUG_LVL_VERB,"[MPI %"PRId32"] comm-platform starts\n", rank);
            PD->myLocation = locationToMpiRank(rank);
        }
        break;
    case RL_MEMORY_OK:
        // Nothing to do
        break;
    case RL_GUID_OK:
        ASSERT(self->pd == PD);
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(self->pd, RL_GUID_OK, phase)) {
            //BUG #602 multi-comm-worker: multi-initialization if multiple comm-worker
            //Initialize mpi comm internal queues
            mpiComm->sendPool = (MPI_Request *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(MPI_Request));
            mpiComm->recvPool = (MPI_Request *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(MPI_Request));
            mpiComm->recvFxdPool = (MPI_Request *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(MPI_Request));
            mpiComm->sendHdlPool = (mpiCommHandle_t *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(mpiCommHandle_t));
            mpiComm->recvHdlPool = (mpiCommHandle_t *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(mpiCommHandle_t));
            mpiComm->recvFxdHdlPool = (mpiCommHandle_t *) self->pd->fcts.pdMalloc(self->pd, MPI_COMM_REQUEST_POOL_SZ * sizeof(mpiCommHandle_t));
            // No need to init pools, it is done whenever an element is grabbed from it
            mpiComm->sendPoolSz = 0;
            mpiComm->recvPoolSz = 0;
            mpiComm->recvFxdPoolSz = 0;
            mpiComm->sendPoolMax = MPI_COMM_REQUEST_POOL_SZ;
            mpiComm->recvPoolMax = MPI_COMM_REQUEST_POOL_SZ;
            mpiComm->recvFxdPoolMax = MPI_COMM_REQUEST_POOL_SZ;

            // Pre-post a new recv on the fixed size message channel
            ocrPolicyMsg_t * newMsg = allocateNewMessage((ocrCommPlatform_t *) mpiComm, RECV_ANY_FIXSZ);
            mpiCommHandle_t * hdl = createMpiRecvFxdHandle((ocrCommPlatform_t *) mpiComm, RECV_ANY_FIXSZ_ID, 0, newMsg, false);
            hdl->src = MPI_ANY_SOURCE;
            postRecvFixedSzMsg(mpiComm, hdl);
            // Do not need that with probe
            ASSERT(mpiComm->maxMsgSize == 0);
            // Generate the list of known neighbors (All-to-all)
            //BUG #606 Neighbor registration: neighbor information should come from discovery or topology description
            int nbRanks;
            MPI_Comm_size(MPI_COMM_WORLD, &nbRanks);
            PD->neighborCount = nbRanks - 1;
            PD->neighbors = PD->fcts.pdMalloc(PD, sizeof(ocrLocation_t) * PD->neighborCount);
            int myRank = (int) locationToMpiRank(PD->myLocation);
            int k = 0;
            while(k < (nbRanks-1)) {
                PD->neighbors[k] = mpiRankToLocation((myRank+k+1)%nbRanks);
                DPRINTF(DEBUG_LVL_VERB,"[MPI %"PRId32"] Neighbors[%"PRId32"] is %"PRIu64"\n", myRank, k, PD->neighbors[k]);
                k++;
            }
#ifdef DEBUG_MPI_HOSTNAMES
            char hostname[256];
            gethostname(hostname,255);
            PRINTF("MPI rank %"PRId32" on host %s\n", myRank, hostname);
#endif
            // Runlevel barrier across policy-domains
            MPI_Barrier(MPI_COMM_WORLD);
        }
        if ((properties & RL_TEAR_DOWN) && RL_IS_FIRST_PHASE_DOWN(self->pd, RL_GUID_OK, phase)) {
            if (mpiComm->sendPoolSz != 0) {
                // There might still be one-way messages in flight that are
                // in terms of the DAG are 'sticking out' of the EDT that
                // called shutdownEdt. This can happen when the call has
                // been issued from a hierarchy of finish EDTs.
                u32 i = 0, ub = mpiComm->sendPoolSz;
                while(i < ub) {
                    mpiCommHandle_t * dh = &(mpiComm->sendHdlPool[i]);
                    ocrPolicyMsg_t * msg = dh->msg;
#ifdef OCR_ASSERT
                    if ((msg->type & PD_MSG_TYPE_ONLY) != PD_MSG_DEP_SATISFY) {
                        DPRINTF(DEBUG_LVL_WARN, "Shutdown: message of type %"PRIx32" has not been drained\n", (u32) (msg->type & PD_MSG_TYPE_ONLY));
                    }
#endif
                    self->pd->fcts.pdFree(self->pd, msg);
                    i++;
                }
                mpiComm->sendPoolSz = 0;
            }
            ASSERT(mpiComm->sendPoolSz == 0);
            ASSERT(mpiComm->recvPoolSz == 0);
            ASSERT(mpiComm->recvFxdPoolSz == 1); // Always one listen posted
            self->pd->fcts.pdFree(self->pd, mpiComm->sendPool);
            self->pd->fcts.pdFree(self->pd, mpiComm->recvPool);
            self->pd->fcts.pdFree(self->pd, mpiComm->recvFxdPool);
            self->pd->fcts.pdFree(self->pd, mpiComm->sendHdlPool);
            self->pd->fcts.pdFree(self->pd, mpiComm->recvHdlPool);
            self->pd->fcts.pdFree(self->pd, mpiComm->recvFxdHdlPool);
            ASSERT(mpiComm->sendPoolSz == 0);
            ASSERT(mpiComm->recvPoolSz == 0);
            ASSERT(mpiComm->recvFxdPoolSz == 1); // always one fixed recv posted
            PD->fcts.pdFree(PD, PD->neighbors);
            PD->neighbors = NULL;
        }
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        // Note: This PD may reach this runlevel after other PDs. It is not
        // an issue for MPI since the library is already up and will buffer
        // the messages. The communication worker wll pick that up whenever
        // it has started
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    // Store the runlevel/phase in curState for debugging purpose
    mpiComm->curState = ((runlevel<<4) | phase);
    return toReturn;
}

//
// Init and destruct
//

static void MPICommDestruct (ocrCommPlatform_t * self) {
    //This should be called only once per rank and by the same thread that did MPI_Init.
    platformFinalizeMPIComm();
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

ocrCommPlatform_t* newCommPlatformMPI(ocrCommPlatformFactory_t *factory,
                                       ocrParamList_t *perInstance) {
    ocrCommPlatformMPI_t * commPlatformMPI = (ocrCommPlatformMPI_t*)
    runtimeChunkAlloc(sizeof(ocrCommPlatformMPI_t), PERSISTENT_CHUNK);
    //BUG #605 Locations spec: what is a comm-platform location ? is it the same as the PD ?
    commPlatformMPI->base.location = ((paramListCommPlatformInst_t *)perInstance)->location;
    commPlatformMPI->base.fcts = factory->platformFcts;
    factory->initialize(factory, (ocrCommPlatform_t *) commPlatformMPI, perInstance);
    return (ocrCommPlatform_t*) commPlatformMPI;
}


/******************************************************/
/* MPI COMM-PLATFORM FACTORY                          */
/******************************************************/

static void destructCommPlatformFactoryMPI(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

static void initializeCommPlatformMPI(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * base, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, base, perInstance);
    ocrCommPlatformMPI_t * mpiComm = (ocrCommPlatformMPI_t*) base;
    mpiComm->msgId = 2; // all recv ANY use id '0'
    mpiComm->maxMsgSize = 0;
    mpiComm->curState = 0;
    mpiComm->sendPool = NULL;
    mpiComm->sendPoolSz = 0;
    mpiComm->sendPoolMax = 0;
    mpiComm->recvPool = NULL;
    mpiComm->recvFxdPool = NULL;
    mpiComm->recvPoolSz = 0;
    mpiComm->recvFxdPoolSz = 0;
    mpiComm->recvPoolMax = 0;
    mpiComm->recvFxdPoolMax = 0;
    mpiComm->sendHdlPool = NULL;
    mpiComm->recvHdlPool = NULL;
    mpiComm->recvFxdHdlPool = NULL;
}

ocrCommPlatformFactory_t *newCommPlatformFactoryMPI(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryMPI_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newCommPlatformMPI;
    base->initialize = &initializeCommPlatformMPI;
    base->destruct = FUNC_ADDR(void (*)(ocrCommPlatformFactory_t*), destructCommPlatformFactoryMPI);

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), MPICommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                  phase_t, u32, void (*)(ocrPolicyDomain_t*,u64), u64), MPICommSwitchRunlevel);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrLocation_t,
                                               ocrPolicyMsg_t*,u64*,u32,u32), MPICommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrPolicyMsg_t**,u32,u32*),
                                               MPICommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrPolicyMsg_t**,u32,u32*),
                                               MPICommWaitMessage);
    return base;
}

#endif /* ENABLE_COMM_PLATFORM_MPI */
