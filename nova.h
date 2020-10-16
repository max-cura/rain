//
// file nova.h
// author Maximilien M. Cura
//

#ifndef __NV_NOVA
#define __NV_NOVA

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    __NVR_OK = 0,
    __NVR_FAIL = 1,

    __NVR_COMMON = 2,
    __NVR_INVALP = __NVR_COMMON + 0,
    __NVR_CACHEINVAL = __NVR_COMMON + 1,

    __NVR_OS = 100,
    __NVR_OS_ALLOC = __NVR_OS + 0,
    __NVR_OS_ALIGN = __NVR_OS + 1,
    __NVR_OS_CHUNK_DEALLOC = __NVR_OS + 2,
    __NVR_OS_ALLOC_SMALL = __NVR_OS + 3,

    __NVR_ALLOC = 200,
    __NVR_ALLOC_OSIZE = __NVR_ALLOC + 0,
    __NVR_ALLOC_UNSIZED = __NVR_ALLOC + 1,
    __NVR_ALLOC_SPOILED_PROMOTEE = __NVR_ALLOC + 2,
    __NVR_ALLOC_LKG_IDX_RANGE = __NVR_ALLOC + 3,
    __NVR_ALLOC_SPOILED_CHUNK = __NVR_ALLOC + 4,

    __NVR_SYNC = 1000,
    __NVR_TID = __NVR_SYNC,
    __NVR_TID_INSUFFICIENT_TIDS = __NVR_TID + 0,
    __NVR_TID_SYS_UNINITIALIZED = __NVR_TID + 1,
    __NVR_TID_THR_UNINITIALIZED = __NVR_TID + 2,
    __NVR_SYNC_MUTEX = 1010,
    __NVR_SYNC_MUTEX_INIT = __NVR_SYNC_MUTEX + 0,
} __nvr_t;
#define __nvr_iserr(e) (__NVR_FAIL < (e))

typedef struct
{

} __nv_parkinglot_bucket_t;
typedef struct
{

} __nv_parkinglot_t;
#include <pthread.h>
typedef struct
{
/*    uint8_t __l_inner;
 */
    pthread_mutex_t __l_inner;
} __nv_lock_t;

__nvr_t __nv_lock_init (__nv_lock_t *__lock);
__nvr_t __nv_lock_destroy (__nv_lock_t *__lock);
__nvr_t __nv_lock (__nv_lock_t *__lock);
__nvr_t __nv_unlock (__nv_lock_t *__lock);

/* aligned(8) might actually slow things down (cache eviction)
 */
typedef struct __attribute__ ((packed, aligned (8))) __nv_block_header_s
{
    void *__bh_fpl;

    uint16_t __bh_osz;
    uint16_t __bh_ocnt;
    uint16_t a__bh_flag;
    uint16_t a__bh_acnt;

    uint64_t a__bh_tid;
    struct __nv_block_header_s *ll__bh_chnx;
    struct __nv_block_header_s *ll__bh_chpr;
    void *gl__bh_lkg;

    void *gl__bh_fpg;
    __nv_lock_t __bh_glck;
} __nv_block_header_t;
#define __NV_BLHDRFL_LKGHD 0x01
#define __NV_BLHDRSZ (sizeof (__nv_block_header_t))

typedef struct
{
    __nv_block_header_t *lla__lkg_active;
    __nv_lock_t __lkg_ll;
    size_t __lkg_idx;
    size_t ll__lkg_nblk;
} __nv_lkg_t;

typedef struct
{
    void *__hp_parent;

    __nv_lkg_t __hp_lkgs[];
} __nv_heap_t;

typedef struct
{
    void *__ch_chlsnx;
    size_t __ch_nblk;
    size_t __ch_nbyt;
} __nv_chunk_t;
#define __NV_CHPTRFLMASK 0x7lu
#define __NV_CHPTRFL_ALLOC_MACOS_VM 0x01lu

typedef enum {
    __NVH_LSLUP = 0,
    __NVH_RLSLUP,
    __NVH_CAN_TAKE_BLOCK,
    __NVH_NHEURISTICS,
} __nv_heuristic_t;

typedef struct __nv_allocator_s
{
    size_t __al_chsz;
    size_t __al_blksz;
    size_t __al_hpnlkg;
    size_t __al_permtrytplvalloc;
    __nv_heap_t *__al_ghp;
    __nv_chunk_t *__al_chls;

    void *__al_ht[__NVH_NHEURISTICS];
} __nv_allocator_t;

typedef struct __nv_tidmgr_recylsch
{
    size_t __tidrecylschidx;
    uint64_t __tidrecylschtids[0x7fe];
    struct __nv_tidmgr_recylsch *__tidrecylschnx;
} __nv_tidmgr_recylsch_t;
typedef struct
{
    __nv_tidmgr_recylsch_t *__recyls;
    uint64_t __recylsmono;
} __nv_tidmgr_recycling_t;

typedef struct
{
    uint64_t __mono;
} __nv_tidmgr_monotonic_t;

typedef struct
{
    void *__tidmun;
    uint64_t __tidmfl;
    __nv_lock_t __tidlck;
} __nv_tidmgr_t;
#define __NV_TIDSYS_LAZY 0x01ul
#define __NV_TIDSYS_RECYCLING 0x02ul
#define __NV_TIDSYS_MONOEXHAUSTED 0x8000000000000000ul

#define __NV_NULL_TID 0ul

__nvr_t __nv_os_smalloc (void **__mem, size_t size);
__nvr_t __nv_os_smdealloc (void *__mem, size_t size);
__nvr_t __nv_os_challoc (__nv_allocator_t *__alloc, void **__mem);
__nvr_t __nv_os_chdealloc (__nv_allocator_t *__alloc, __nv_chunk_t *__ch);

__nvr_t __nv_tid_thread_init ();
__nvr_t __nv_tid_thread_destroy ();
__nvr_t __nv_tid_sys_init (uint64_t __fl);
__nvr_t __nv_tid_sys_destroy ();
uint64_t __nv_tid ();
__nvr_t __nv_tid_state ();

__nvr_t __nv_chunk_new (__nv_allocator_t *__alloc, __nv_chunk_t **__chunk);
__nvr_t __nv_chunk_populate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk);
__nvr_t __nv_chunk_delete (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk,
                           __nv_chunk_t **__chunk_mknx);
__nvr_t __nv_chunk_delete_propagate (__nv_allocator_t *__alloc,
                                     __nv_chunk_t *__chunk,
                                     __nv_chunk_t **__errch,
                                     __nv_chunk_t **__errtl);

/* \caller __nv_chunk_populate
 * \expect appropriate linkages pre-locked by __nv_chunk_populate's
 *          caller, which also ensures that when a block request ends
 *          up invoking a chunk allocation, the request will not incur
 *          spurious failures due to block snatching.
 */
__nvr_t __nv_chunk_yields_block_batch_locked (__nv_allocator_t *,
                                              void *, size_t);

inline size_t __nv_rlslup (__nv_allocator_t *__alloc, size_t __lkgidx)
{
    return (*(size_t (*) (size_t))__alloc->__al_ht[__NVH_RLSLUP]) (__lkgidx);
}

inline size_t __nv_lslup(__nv_allocator_t *__alloc, size_t __osize) {
    return (*(size_t (*) (size_t))__alloc->__al_ht[__NVH_LSLUP]) (__osize);
}

__nvr_t __nv_lkg_alloc_object (__nv_allocator_t *__alloc, __nv_lkg_t *__lkg,
                               __nv_heap_t *__heap, void **__obj);
__nvr_t __nv_lkg_req_block_from_heap (__nv_allocator_t *__alloc,
                                      __nv_heap_t *__heap,
                                      size_t __lkg_idx,
                                      __nv_block_header_t **__blockhp);

__nvr_t __nv_block_init (__nv_allocator_t *__alloc, void *__blmem);
__nvr_t __nv_block_dealloc_object (__nv_allocator_t *__alloc,
                                   __nv_block_header_t *__blockh,
                                   void *__obj);
__nvr_t __nv_block_alloc_object_inner (__nv_allocator_t *__alloc,
                                       __nv_block_header_t *__blockh,
                                       void **__obj);
__nvr_t __nv_block_alloc_object (__nv_allocator_t *__alloc,
                                 __nv_block_header_t *__blockh,
                                 void **__obj);
__nvr_t __nv_block_requests_lift(__nv_allocator_t *__alloc,
                                  __nv_lkg_t *__origin_lkg,
                                  __nv_block_header_t *__blockh);

#define __NV_LIKELY(x) __builtin_expect (!!(x), 1)
#define __NV_UNLIKELY(x) __builtin_expect (!!(x), 0)
#define __NV_BLOCK_MEMORY_OFFSET sizeof (__nv_block_header_t)

#define YES 1
#define NO 0

#endif /* !@__NV_NOVA */
