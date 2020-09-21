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

    __NVR_OS = 100,
    __NVR_OS_ALLOC = __NVR_OS + 0,

    __NVR_ALLOC = 200,
    __NVR_ALLOC_OSIZE = __NVR_ALLOC + 0,
    __NVR_ALLOC_UNSIZED = __NVR_ALLOC + 1,
} __nvr_t;
#define __nvr_iserr(e) (__NVR_FAIL < (e))

typedef struct
{

} __nv_parkinglot_bucket_t;

typedef struct
{

} __nv_parkinglot_t;
typedef struct
{

} __nv_lock_t;

__nvr_t __nv_lock_init (__nv_lock_t *lock);
__nvr_t __nv_lock (__nv_lock_t *lock);
__nvr_t __nv_unlock (__nv_lock_t *lock);

typedef struct __attribute__ ((packed))
{
    void *__bh_fpl;

    uint16_t __bh_osz;
    uint16_t __bh_ocnt;
    uint16_t a__bh_flag;
    uint16_t a__bh_acnt;

    uint64_t a__bh_tid;
    void *ll__bh_chnx;
    void *ll__bh_chpr;
    void *gl__bh_lkg;

    void *gl__bh_fpg;
    __nv_lock_t __bh_glck;
} __nv_block_header_t;
#define __NV_BLHDRFL_LKGHD 0x01
#define __NV_BLHDRSZ (sizeof (__nv_block_header_t))

typedef struct
{
    __nv_block_header_t *lla__lkg_active;
    __nv_lock_t lkg_ll;
    size_t lkg_idx;
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
    size_t __ch_nblk
        size_t __ch_nbyt;
} __nv_chunk_t;

typedef struct
{
    size_t __al_chsz;
    size_t __al_blksz;
    size_t __al_hpnlkg;
    size_t (*__al_lkgdistrat) (size_t);
} __nv_allocator_t;

__nvr_t __nv_chunk_new (__nv_allocator_t *__alloc, __nv_chunk_t **__chunk);
__nvr_t __nv_chunk_populate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk);
__nvr_t __nv_chunk_delete (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk, __nv_chunk_t **__chunk_mknx);
__nvr_t __nv_chunk_delete_propagate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk, __nv_chunk_t **__errch, __nv_chunk_t **__errtl);

/* \caller __nv_chunk_populate
 * \expect appropriate linkages pre-locked by __nv_chunk_populate's
 *          caller, which also ensures that when a block request ends
 *          up invoking a chunk allocation, the request will not incur
 *          spurious failures due to block snatching.
 */
__nvr_t __nv_chunk_yields_block_batch_locked (__nv_allocator_t *, void *, size_t);

__nvr_t __nv_block_bind (__nv_allocator_t *__alloc, void *__blmem);

__nvr_t __nv_heap_alloc_object (__nv_allocator_t *__alloc, __nv_heap_t *__heap, size_t osize);
__nvr_t __nv_lkg_alloc_object (__nv_allocator_t *__alloc, __nv_lkg_t *__lkg, __nv_heap_t *__heap, void **__obj);

#endif /* !@__NV_NOVA */
