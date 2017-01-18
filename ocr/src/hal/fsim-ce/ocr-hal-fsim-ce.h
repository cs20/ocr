/**
 * @brief Macros for hardware-level primitives that the
 * rest of OCR uses. These macros are mostly related
 * to memory primitives
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#ifndef __OCR_HAL_FSIM_CE_H__
#define __OCR_HAL_FSIM_CE_H__

#include "ocr-types.h"

#include "mmio-table.h"
#include "tg-mmio.h"
#include "fsim-api.h"

/****************************************************/
/* OCR LOW-LEVEL MACROS                             */
/****************************************************/

// FIXME TG
//#warning HAL layer for CE needs to be reworked

#ifdef OCR_TICKETLOCK
typedef struct {
    u32 ticket;
    volatile u32 now;
} lock_t;
#define INIT_LOCK ((lock_t){0, 0})

#ifndef OCR_TICKETLOCK_BACKOFF
#define OCR_TICKETLOCK_BACKOFF 0
#endif

#else
/* Default is a test test-and-set type of locking */
typedef volatile u32 lock_t;
#define INIT_LOCK 0
#endif

/**
 * @brief Perform a memory fence
 *
 * @todo Do we want to differentiate different types
 * of fences?
 */
#define hal_fence() tg_fence_fbm()

/**
 * @brief Memory copy from source to destination
 *
 * @param destination[in]    A u64 pointing to where the data is to be copied
 *                           to
 * @param source[in]         A u64 pointing to where the data is to be copied
 *                           from
 * @param size[in]           A u64 indicating the number of bytes to be copied
 * @param isBackground[in]   A u8: A zero value indicates that the call will
 *                           return only once the copy is fully complete and a
 *                           non-zero value indicates the copy may proceed
 *                           in the background. A fence will then be
 *                           required to ensure completion of the copy
 * @todo Define what behavior we want for overlapping
 * source and destination
 */
#define hal_memCopy(dst, src, n, isBackground)                          \
    ({                                                                  \
        tg_dma_copyregion_async((void*)dst, (void*)src, n);             \
        if(!isBackground) tg_fence_b();                                 \
        n;                                                              \
    })


/**
 * @brief Memory move from source to destination. As if overlapping portions
 * were copied to a temporary array
 *
 * @param destination[in]    A u64 pointing to where the data is to be copied
 *                           to
 * @param source[in]         A u64 pointing to where the data is to be copied
 *                           from
 * @param size[in]           A u64 indicating the number of bytes to be copied
 * @param isBackground[in]   A u8: A zero value indicates that the call will
 *                           return only once the copy is fully complete and a
 *                           non-zero value indicates the copy may proceed
 *                           in the background. A fence will then be
 *                           required to ensure completion of the copy
 * source and destination
 * @todo This implementation is heavily *NOT* optimized
 */
#define hal_memMove(destination, source, size, isBackground) do {       \
    u64 _source = (u64)source;                                          \
    u64 _destination = (u64)destination;                                \
    u64 count = 0;                                                      \
    if(_source != _destination) {                                       \
        if(_source < _destination) {                                    \
            for(count = size -1 ; count > 0; --count)                   \
                ((char*)_destination)[count] = ((char*)_source)[count]; \
            ((char*)_destination)[0] = ((char*)_source)[0];             \
        } else {                                                        \
            for(count = 0; count < size; ++count)                       \
                ((char*)_destination)[count] = ((char*)_source)[count]; \
        }                                                               \
    }                                                                   \
    } while(0)

/**
 * @brief Atomic swap (64 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u64*: Pointer to the atomic value (location)
 * @param newValue      u64: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap64(atomic, newValue)                                    \
    ({                                                                  \
        u64 __tmp = tg_xchg64((u64*)atomic, newValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Compare and swap (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u64*: Pointer to the atomic value (location)
 * @param cmpValue      u64: Expected value of the atomic
 * @param newValue      u64: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap64(atomic, cmpValue, newValue)                       \
    ({                                                                  \
        u64 __tmp = tg_cmpxchg64((u64*)atomic, cmpValue, newValue);     \
        __tmp;                                                          \
    })

/**
 * @brief Atomic add (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u64*: Pointer to the atomic value (location)
 * @param addValue  u64: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd64(atomic, addValue)                                    \
    ({                                                                  \
        u64 __tmp = tg_xadd64((u64*)atomic, addValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Remote atomic add (64 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u64*: Pointer to the atomic value (location)
 * @param addValue  u64: Value to add to location
 */
#define hal_radd64(atomic, addValue) hal_xadd64(atomic, addValue)

/**
 * @brief Atomic swap (32 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u32*: Pointer to the atomic value (location)
 * @param newValue      u32: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap32(atomic, newValue)                                    \
    ({                                                                  \
        u32 __tmp = tg_xchg32((u32*)atomic, newValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Compare and swap (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u32*: Pointer to the atomic value (location)
 * @param cmpValue      u32: Expected value of the atomic
 * @param newValue      u32: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap32(atomic, cmpValue, newValue)                       \
    ({                                                                  \
        u32 __tmp = tg_cmpxchg32((u32*)atomic, cmpValue, newValue);     \
        __tmp;                                                          \
    })

/**
 * @brief Atomic add (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u32*: Pointer to the atomic value (location)
 * @param addValue  u32: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd32(atomic, addValue)                                    \
    ({                                                                  \
        u32 __tmp = tg_xadd32((u32*)atomic, addValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Remote atomic add (32 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u32*: Pointer to the atomic value (location)
 * @param addValue  u32: Value to add to location
 */
#define hal_radd32(atomic, addValue) hal_xadd32(atomic, addValue)

/**
 * @brief Atomic swap (16 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u16*: Pointer to the atomic value (location)
 * @param newValue      u16: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap16(atomic, newValue)                                    \
    ({                                                                  \
        u16 __tmp = tg_xchg16((u16*)atomic, newValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Compare and swap (16 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u16*: Pointer to the atomic value (location)
 * @param cmpValue      u16: Expected value of the atomic
 * @param newValue      u16: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap16(atomic, cmpValue, newValue)                       \
    ({                                                                  \
        u16 __tmp = tg_cmpxchg16((u16*)atomic, cmpValue, newValue);     \
        __tmp;                                                          \
    })

/**
 * @brief Atomic add (16 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u16*: Pointer to the atomic value (location)
 * @param addValue  u16: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd16(atomic, addValue)                                    \
    ({                                                                  \
        u16 __tmp = tg_xadd16((u16*)atomic, addValue);                  \
        __tmp;                                                          \
    })

/**
 * @brief Remote atomic add (16 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u16*: Pointer to the atomic value (location)
 * @param addValue  u16: Value to add to location
 */
#define hal_radd16(atomic, addValue) hal_xadd16(atomic, addValue)

/**
 * @brief Atomic swap (8 bit)
 *
 * Atomically swap:
 *
 * @param atomic        u8*: Pointer to the atomic value (location)
 * @param newValue      u8: New value to set
 *
 * @return Old value of the atomic
 */
#define hal_swap8(atomic, newValue)                                    \
    ({                                                                 \
        u8 __tmp = tg_xchg8((u8*)atomic, newValue);                    \
        __tmp;                                                         \
    })

/**
 * @brief Compare and swap (8 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - if location is cmpValue, atomically replace with
 *       newValue and return cmpValue
 *     - if location is *not* cmpValue, return value at location
 *
 * @param atomic        u8*: Pointer to the atomic value (location)
 * @param cmpValue      u8: Expected value of the atomic
 * @param newValue      u8: Value to set if the atomic has the expected value
 *
 * @return Old value of the atomic
 */
#define hal_cmpswap8(atomic, cmpValue, newValue)                       \
    ({                                                                 \
        u8 __tmp = tg_cmpxchg8((u8*)atomic, cmpValue, newValue);       \
        __tmp;                                                         \
    })

/**
 * @brief Atomic add (8 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - return old value (before addition)
 *
 * @param atomic    u8*: Pointer to the atomic value (location)
 * @param addValue  u8: Value to add to location
 * @return Old value of the location
 */
#define hal_xadd8(atomic, addValue)                                    \
    ({                                                                 \
        u8 __tmp = tg_xadd8((u8*)atomic, addValue);                    \
        __tmp;                                                         \
    })

/**
 * @brief Remote atomic add (8 bit)
 *
 * The semantics are as follows (all operations performed atomically):
 *     - atomically increment location by addValue
 *     - no value is returned (the increment will happen "at some
 *       point")
 *
 * @param atomic    u8*: Pointer to the atomic value (location)
 * @param addValue  u8: Value to add to location
 */
#define hal_radd8(atomic, addValue) hal_xadd8(atomic, addValue)


#ifdef OCR_TICKETLOCK
/* We use a full memory barrier where we would only need
 * an acquire one */
#define hal_lock(__l)                                       \
    do {                                                    \
        volatile u32 _i;                                    \
        u32 _t = hal_xadd32(&((__l)->ticket), 1);           \
        while(true) {                                       \
            u32 _spinTime = OCR_TICKETLOCK_BACKOFF*(_t - (__l)->now);            \
            for(_i = 0; _i < _spinTime; ++_i)               \
                ;                                           \
            if(_t == (__l)->now) break;                     \
        }                                                   \
        __asm__ __volatile__ ("" : : : "memory");           \
    } while(0);

/* We use a full memory barrier where we would only need
 * a release one */
#define hal_unlock(__l)                                     \
    do {                                                    \
        __asm__ __volatile__ ("" : : : "memory");           \
        /* TODO: Blocking flush with no-invalidate */       \
        ++((__l)->now);                                     \
    } while(0);

#define hal_trylock(__l)                                    \
    ({                                                      \
        u8 _r = 0;                                          \
        u32 _val = (__l)->ticket;
        if(_val == (__l)->now) {                            \
            u32 _old = hal_cmpswap32(&((__l)->ticket), _val, _val+1);\
            if(_old != _val) _r = 1;                        \
            else                                            \
                __asm__ __volatile__ ("" : : : "memory");   \
        } else {                                            \
            _r = 1;                                         \
        }                                                   \
        _r;                                                 \
    })

#define hal_islocked(__l) ((__l)->ticket > (__l)->now)

#else /* ! OCR_TICKETLOCK */
/**
 * @brief Convenience function that basically implements a simple
 * lock
 *
 * This will usually be a wrapper around cmpswap32. This function
 * will block until the lock can be acquired
 *
 * @param lock      Pointer to a 32 bit value
 */
#define hal_lock(__l)                               \
    do {                                            \
        while(tg_cmpxchg32((u32*)(__l), 0, 1) != 0) \
            while(*(__l))                           \
                ;                                   \
        __asm__ __volatile__ ("" : : : "memory");   \
    } while(0)

/**
 * @brief Convenience function to implement a simple
 * unlock
 *
 * @param lock      Pointer to a 32 bit value
 */
 // NOTE: We really need a release barrier here ideally but
 // putting something to make sure things don't move up.
#define hal_unlock(__l)                                   \
      do {                                                \
          __asm__ __volatile__ ("" : : : "memory");       \
          /* TODO: Blocking flush with no-invalidate */   \
          *(__l) = 0;                                     \
      } while(0);

/**
 * @brief Convenience function to implement a simple
 * trylock
 *
 * @param lock      Pointer to a 32 bit value
 * @return 0 if the lock has been acquired and a non-zero
 * value if it cannot be acquired
 */
#define hal_trylock(__l)                                        \
      ({                                                        \
          u32 _r = tg_cmpxchg32((u32*)(__l), 0, 1);             \
          if(_r == 0) {                                         \
              __asm__ __volatile__ ("" : : : "memory");         \
          }                                                     \
          _r;                                                   \
      })

#define hal_islocked(__l) (*(__l) == 1)

#endif

/**
 * @brief Abort the runtime
 *
 * Will crash the runtime
 */
#define hal_abort()                                             \
    __asm__ __volatile__("int %0\n\t"                           \
                         : : "i" (INT_CRASHNBURN))

/**
 * @brief Exit the runtime
 *
 * This will exit the runtime more cleanly than abort
 */
#define hal_exit(arg)                                           \
    __asm__ __volatile__("int %0\n\t"                           \
                         : : "i" (INT_QEMU_SHUTDOWN))

/**
 * @brief Pause execution
 *
 * This is used to support a primitive version of ocrWait
 * and may be deprecated in the future
 */
#define hal_pause() do {                                        \
        /* Works because there is a periodic timer interupt */  \
        __asm__ __volatile__("sti;hlt;");                       \
    } while(0)

/**
 * @brief Put the XE core to sleep
 *
 * This is used by CE to put an XE core in its block to sleep
 * @warning 'id' is the agent ID (ID_AGENT_XE0 ... ID_AGENT_XE7)
 */
#define hal_sleep(id) do {                                                     \
        *((volatile u8*)(BR_XE_CONTROL(id - ID_AGENT_XE0))) = XE_CTL_CLK_GATE; \
    } while(0)

/**
 * @brief Wake the XE core from sleep
 *
 * This is used by CE to wake an XE core in its block from sleep
 * @warning 'id' is the agent ID (ID_AGENT_XE0 ... ID_AGENT_XE7)
 */
#define hal_wake(id) do {                                             \
          *((volatile u8*)(BR_XE_CONTROL(id - ID_AGENT_XE0))) = 0x00; \
    } while(0)

/**
 * @brief On architectures (like TG) that
 * have different address "formats", canonicalize it
 * to the unique form
 */
#define hal_globalizeAddr(addr) tg_canonical(addr)

/**
 * @brief On architectures (like TG) that have
 * different address "formats", this returns the
 * smallest usable address from the global address 'addr'
 */
#define hal_localizeAddr(addr, me) tg_localize(addr, me)

// Support for abstract load and store macros
#define IS_REMOTE(addr) (__builtin_clzl(addr) < ((sizeof(u64) - 1) - MAP_AGENT_SHIFT))

// Abstraction to do a load operation from any level of the memory hierarchy
//static inline u8 AbstractLoad8(u64 addr) { return *((u8 *) addr); }
//#define GET8(temp,addr)  temp = AbstractLoad8(addr)
#define GET8(temp,addr)  ({ (temp) = *((u8 *) (addr)); })

//static inline u16 AbstractLoad16(u64 addr) { return *((u16 *) addr); }
//#define GET16(temp,addr)  temp = AbstractLoad16(addr)
#define GET16(temp,addr)  ({ (temp) = *((u16 *) (addr)); })

//static inline u32 AbstractLoad32(u64 addr) { return *((u32 *) addr); }
//#define GET32(temp,addr)  temp = AbstractLoad32(addr)
#define GET32(temp,addr)  ({ (temp) = *((u32 *) (addr)); })

//static inline u64 AbstractLoad64(u64 addr) { return *((u64 *) addr); }
//#define GET64(temp,addr)  temp = AbstractLoad64(addr)
#define GET64(temp,addr)  ({ (temp) = *((u64 *) (addr)); })

// Abstraction to do a store operation to any level of the memory hierarchy
//static inline void SET8(u64 addr, u8 value) { *((u8 *) addr) = value; }
#define  SET8(addr, value) ({ *((u8 *) (addr)) = (value); })

//static inline void SET16(u64 addr, u16 value) { *((u16 *) addr) = value; }
#define  SET16(addr, value) ({ *((u16 *) (addr)) = (value); })

//static inline void SET32(u64 addr, u32 value) { *((u32 *) addr) = value; }
#define  SET32(addr, value) ({ *((u32 *) (addr)) = (value); })

//static inline void SET64(u64 addr, u64 value) { *((u64 *) addr) = value; }
#define  SET64(addr, value) ({ *((u64 *) (addr)) = (value); })

/**
 * @brief Start a new chain
 *
 * @param decaddr[in]     u64 address of a memory location in tg to decrement
 *                        after sucessful completion of entire chain.
 * @param interrupt[in]   u8 0 or 1 to indicate if an interrupt is desired after
 *                        completion of this chain.
 * @param decrement[in]   u8 0 or 1 to indicate if a memory location is to be
 *                        decremented upon completion of this chain.
 * @return id of allotted chain.
 */
#define hal_chain_start(decaddr, interrupt, decrement)                  \
      ({                                                                \
          u64 __cid = tg_chain_init(decaddr, interrupt, decrement);     \
          __cid;                                                        \
      })

/**
 * @brief End current chain
 */
#define hal_chain_end()                                                 \
    ({                                                                  \
        tg_chain_end();                                                 \
    })

/**
 * @brief Wait on a chain whose id matches with supplied hwid.
 * If no match found, call returns.
 * @param hwid[in]      u64 chain id to wait upon.
 */
#define hal_chain_wait(hwid)                                            \
    ({                                                                  \
        tg_chain_wait(hwid);                                            \
    })

/**
 * @brief Poll chain whose id matches with supplied hwid.
 * If no match found, call returns.
 * @param hwid[in]      u64 chain id to wait upon.
 *
 * @return u64 status of the chain.
 */
#define hal_chain_poll(hwid)                                            \
    ({                                                                  \
        u64 __status = tg_chain_poll(hwid);                             \
        __status;                                                       \
    })

/**
 * @brief Kill chain whose id matches with supplied hwid.
 * If no match found, call returns.
 * @param hwid[in]      u64 chain id of chain to kill.
 *
 */
#define hal_chain_kill(hwid)                                            \
    ({                                                                  \
        tg_chain_kill(hwid);                                            \
    })

/**
 * @brief Add 1 item to specified queue in blocking fashion.
 *
 * @param item[in]     u64 item to added in queue
 * @param qbuff[in]    u64 Target queue's qbuff
 * @param ht[in]       u8 QUEUE_HEAD or QUEUE_TAIL to indicate operation at
 *                     head or tail resp.
 * @param size[in]     TG_SIZE_{64/32/16/8}BIT indicates size of item.
 *
 */
#define hal_q_add1_blocking(item, qbuff, ht, size)                      \
    ({                                                                  \
        tg_qmaadd1w(u64 item, u64 qbuff, u8 ht, u64 size);              \
    })


/**
 * @brief Remove 1 item from specified queue in blocking fashion.
 *
 * @param qbuff[in]    u64 Target queue's qbuff
 * @param ht[in]       u8 QUEUE_HEAD or QUEUE_TAIL to indicate operation at
 *                     head or tail resp.
 * @param size[in]     TG_SIZE_{64/32/16/8}BIT indicates size of item.
 *
 * @return u64 element popped from queue. Cast it to size you desire.
 */
#define hal_q_rem1_blocking(qbuff, ht, size)                            \
    ({                                                                  \
        u64 __item = tg_qmarem1w(qbuff, ht, size);                      \
        __item;                                                         \
    })

/**
 * @brief Add n items to specified queue in blocking fashion.
 *
 * @param daddr[in]     u64 tg-address where elements are located
 *                     (address of 1st element)
 * @param dnum[in]     u64 number of elements to be read from addr.
 * @param qbuff[in]    u64 Target queue's qbuff
 * @param ht[in]       u8 QUEUE_HEAD or QUEUE_TAIL to indicate operation at
 *                     head or tail resp.
 * @param size[in]     TG_SIZE_{64/32/16/8}BIT indicates size of item.
 *
 */
#define hal_q_addx_blocking(qbuff, daddr, dnum, ht, size)               \
    ({                                                                  \
        tg_qmaaddXw(daddr, qbuff, dnum, ht, size);                      \
    })

/**
 * @brief Remove n items from specified queue in blocking fashion.
 *
 * @param daddr[in]    u64 tg-address where elements popped from queue
 *                     will be written.
 * @param dnum[in]     u64 number of elements to be removed from queue.
 * @param qbuff[in]    u64 Target queue's qbuff
 * @param ht[in]       u8 QUEUE_HEAD or QUEUE_TAIL to indicate operation at
 *                     head or tail resp.
 * @param size[in]     TG_SIZE_{64/32/16/8}BIT indicates size of item.
 *
 */
#define hal_q_remx_blocking(qbuff, daddr, dnum, ht, size)               \
      ({                                                                \
          tg_qmaremXw(u64 daddr, u64 qbuff, u64 dnum, u8 ht, u64 size); \
      })


#endif /* __OCR_HAL_FSIM_CE_H__ */
