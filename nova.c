//
// file nova.c
// author Maximilien M. Cura
//

#include "nova.h"

int main (int __attribute__ ((unused)) argc,
          char **__attribute__ ((unused)) argv)
{
    printf ("Saluto il nuovo mondo\n");
    __nv_allocator_t alloc;
    alloc.__al_chsz = 0x800000000;
    void *mem = NULL;
    __nvr_t r = __nv_os_challoc (&alloc, &mem);
    printf ("%i, %p\n", r, mem);
    return 0;
}

size_t __nv_chunk_nblocks (__nv_allocator_t *__alloc, size_t __meml)
{
    return __meml / __alloc->__al_blksz;
}

__nvr_t __nv_chunk_new (__nv_allocator_t *__alloc, __nv_chunk_t **__chunk)
{
    void *__chmem = NULL;
    __nvr_t r = __nv_os_challoc (__alloc, &__chmem);
    if (__nvr_iserr (r))
        return r;
    *__chunk = (__nv_chunk_t *)(__chmem);
    (*__chunk)->__ch_chlsnx = NULL;
    (*__chunk)->__ch_nblk = __nv_chunk_nblocks (
        __alloc,
        __alloc->__al_chsz - sizeof (__nv_chunk_t));
    (*__chunk)->__ch_nbyt = __alloc->__al_chsz;

    __nv_chunk_populate (__alloc, *__chunk);

    return __NVR_OK;
}

__nvr_t __nv_block_init (__nv_allocator_t *__alloc, void *__blmem);

__nvr_t __nv_chunk_yields_block_batch_locked (__nv_allocator_t *__alloc,
                                              void *__chbase,
                                              size_t __nblocks)
{
    if (__nblocks == 0)
        return __NVR_OK;
    __nv_lkg_t *__lkg = &__alloc->__al_ghp->__hp_lkgs[0];
    __nv_block_header_t *__curr = ((__nv_block_header_t *)__chbase),
                        *__next, *__prev = NULL;
    __curr->ll__bh_chpr = NULL;
    for (size_t __bli = 0; __bli < __nblocks; ++__bli) {
        __next = (((uint8_t *)__curr + 1) + __alloc->__al_blksz);
        __curr->ll__bh_chnx = __next;
        __next->ll__bh_chpr = __curr;
        __prev = __curr;
        __curr = __next;
    }
    __curr = __prev;
    __curr->ll__bh_chnx = __atomic_load_n (&__lkg->lla__lkg_active,
                                           __ATOMIC_SEQ_CST);
    __atomic_store_n (&__lkg->lla__lkg_active,
                      __curr,
                      __ATOMIC_SEQ_CST);
    return __NVR_OK;
}

__nvr_t __nv_toplvl_alloc_block (__nv_allocator_t *__alloc,
                                 __nv_block_header_t **__blockhp)
{
    __nv_chunk_t *__chunk;
    __nv_lkg_t *__lkg = &__alloc->__al_ghp->__hp_lkgs[0].lkg_ll;
    __nv_lock (&__lkg->lkg_ll);
    __nvr_t r = __nv_chunk_new (__alloc, &__chunk);
    if (__nvr_iserr (r)) {
        __nv_unlock (&__lkg->lkg_ll);
        return r;
    }
    (*__blockhp) = __atomic_load_n (&__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);
    if ((*__blockhp) == NULL) {
        __nv_unlock (&__lkg->lkg_ll);
        return __NVR_ALLOC_SPOILED_CHUNK;
    }
    __atomic_store_n (&__lkg->lla__lkg_active, (*__blockhp)->ll__bh_chnx,
                      __ATOMIC_SEQ_CST);
    if ((*__blockhp)->ll__bh_chnx != NULL)
        (*__blockhp)->ll__bh_chnx->ll__bh_chpr = NULL;
    __nv_unlock (&__lkg->lkg_ll);
    (*__blockhp)->ll__bh_chpr = NULL;
    (*__blockhp)->ll__bh_chnx = NULL;
    __nv_lock (&(*__blockhp)->__bh_glck);
    return __NVR_OK;
}

__nvr_t __nv_chunk_populate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk)
{
    const size_t __chnblk = __chunk->__ch_nblk;
    uint8_t *const __chbase = (uint8_t *)__chunk + sizeof (__nv_chunk_t);
    uint8_t *__choff = __chbase;
    /* \perftarget tight pack v. 8-aligned v. pagealign
     * My intuition is that tight or 8-aligned will give the best result
     */
    for (size_t __bli = 0; __bli < __chnblk; ++__bli) {
        /* In-place block bind.
         */
        __nv_block_init (__alloc, __choff);
        /* Next block.
         */
        __choff += sizeof (__nv_block_header_t);
        __choff += __alloc->__al_blksz;
    }
    /* Pass blocks in batch to allocator.
     */
    return __nv_chunk_yields_block_batch_locked (__alloc, __chbase,
                                                 __chunk->__ch_nblk);
}

__nvr_t __nv_chunk_delete (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk,
                           __nv_chunk_t **__chunk_mknx)
{
#if __NVC_NULLCHECK
    if (__chunk == NULL || __chunk_mknx == NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    *__chunk_mknx = __chunk->__ch_chlsnx;
    /* use __chunk->__ch_nbyt instead of __alloc->__al_chsz because it's likely
     * on the same cache line as __ch_chlsnx */
    __nvr_t r = __nv_os_chdealloc (__alloc, __chunk, __chunk->__ch_nbyt);
    if (__nvr_iserr (r))
        return r;
    return __NVR_OK;
}

__nvr_t __nv_chunk_delete_propagate (__nv_allocator_t *__alloc,
                                     __nv_chunk_t *__chunk,
                                     __nv_chunk_t **__errch,
                                     __nv_chunk_t **__errtl)
{
#if __NVC_NULLCHECK
    if (__chunk == NULL || __chunk_mknx = NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    __nv_chunk_t *__tmp__chlsnx;
    do {
        __tmp__chlsnx = __chunk->__ch_chlsnx;
        __nvr_t r = __nv_os_chdealloc (__alloc, __chunk, __chunk->__ch_nbyt);
        if (__nvr_iserr (r)) {
            if (__errch != NULL)
                *__errch = __chunk;
            if (__errtl != NULL)
                *__errtl = __tmp__chlsnx;
            return r;
        }
        __chunk = __tmp__chlsnx;
    } while (__chunk != NULL);
    return __NVR_OK;
}

__nvr_t __nv_block_init (__nv_allocator_t *__alloc, void *__blmem)
{
#if __NVC_NULLCHECK
    if (__blmem == NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    __nv_block_header_t *__bh = (__nv_block_header_t *)__blmem;
    __bh->__bh_fpl = NULL;
    __bh->__bh_osz = 0;
    __bh->__bh_ocnt = 0;
    __bh->a__bh_flag = 0;
    __bh->a__bh_acnt = 0;
    __bh->a__bh_tid = 0;
    __bh->ll__bh_chnx = NULL;
    __bh->ll__bh_chpr = NULL;
    __bh->gl__bh_lkg = NULL;
    __bh->gl__bh_fpg = NULL;
    __nv_lock_init (&__bh->__bh_glck);

    return __NVR_OK;
}

__nvr_t __nv_block_fmt (__nv_allocator_t *__alloc,
                        __nv_block_header_t *__bh,
                        size_t __osz)
{
#if __NVC_FMT_OSZ_SANITY
    if (__osz < __NV_HW_CACHELINE_SIZE) {
        return __NVR_CACHEINVAL;
    }
#endif
    uint8_t *__bhm = (uint8_t *)__bh + __NV_BLOCK_MEMORY_OFFSET;
    __bh->__bh_fpl = __bh;
    __bh->__bh_osz = __osz;
    __bh->__bh_ocnt = __alloc->__al_blksz / __bh->__bh_osz;
    __bh->a__bh_flag = 0;
    __bh->a__bh_acnt = 0;
    __atomic_store_n (&__bh->gl__bh_fpg, NULL, __ATOMIC_SEQ_CST);

    /* start from near the header because that gives us better cache locality
     * for a lot of situations */
    for (size_t i = 0; i < __bh->__bh_ocnt; ++i) {
        *(uint64_t *)__bhm = __bhm + __osz;
        __bhm += __osz;
    }
    /* handle last object */
    __bhm -= __osz;
    *(uint64_t *)__bhm = NULL;
    return __NVR_OK;
}

__nvr_t __nv_heap_req_block_from_slkg (__nv_allocator_t *__alloc,
                                       __nv_lkg_t *__lkg,
                                       __nv_block_header_t **__blockhp)
{
    __nv_lock (&__lkg->lkg_ll);
    __nv_block_header_t *__blcache = __atomic_load_n (
        &__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);
    if (__blcache != NULL) {
        /* disallow deallocations
         * By the time GL is unlocked, BLFL:ISHEAD will be set, so FPL/G
         * clearing will be a nonissue.
         * We just need to do a lift guard.
         *
         * HEAD LIFT-GUARD
         */
        _Bool __reqable = YES;
        while (1) {
            __nv_lock (&__blcache->__bh_glck);
            if (__atomic_load_n (&__blcache->gl__bh_fpg, __ATOMIC_SEQ_CST) == NULL
                && __atomic_load_n (&__blcache->a__bh_acnt, __ATOMIC_SEQ_CST) == 0
                && __blcache->__bh_fpl == NULL) {
                __atomic_store_n (&__lkg->lla__lkg_active,
                                  __blcache->ll__bh_chnx,
                                  __ATOMIC_SEQ_CST);
                if (__blcache->ll__bh_chnx != NULL) {
                    __blcache->ll__bh_chnx->ll__bh_chpr = NULL;
                }
                __blcache->ll__bh_chnx = NULL;
                __blcache->ll__bh_chpr = NULL;
                __nv_unlock (&__blcache->__bh_glck);
                __blcache = __atomic_load_n (&__lkg->lla__lkg_active,
                                             __ATOMIC_SEQ_CST);
                if (__blcache == NULL) {
                    __reqable = NO;
                    break;
                }
            } else {
                break;
            }
        }
        if (!__reqable) {
            __nv_unlock (&__lkg->lkg_ll);
            return __NVR_FAIL;
        }
        __atomic_exchange (&__lkg->lla__lkg_active,
                           &__blcache->ll__bh_chnx,
                           &__lkg->lla__lkg_active,
                           __ATOMIC_SEQ_CST);
        __nv_unlock (&__lkg->lkg_ll);
        return __NVR_OK;
    } else {
        __nv_unlock (&__lkg->lkg_ll);
        return __NVR_FAIL;
    }
}

__nvr_t __nv_heap_req_block_from_ulkg (__nv_allocator_t *__alloc,
                                       __nv_lkg_t *__lkg,
                                       size_t __osz,
                                       __nv_block_header_t **__blockhp)
{
    /* % ulkg doesn't have to check for spurious FPL/G clearing. */
    __nv_lock (&__lkg->lkg_ll);
    __nv_block_header_t *__blcache = __atomic_load_n (
        &__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);
    if (__blcache != NULL) {
        /* no LIFT-GUARD necessary, but we do need to lock it */
        __nv_lock (&__blcache->__bh_glck);
        __atomic_exchange (&__lkg->lla__lkg_active,
                           &__blcache->ll__bh_chnx,
                           &__lkg->lla__lkg_active,
                           __ATOMIC_SEQ_CST);
        __nv_unlock (&__lkg->lkg_ll);
        if (__blcache->__bh_osz != __osz) {
            __nv_block_fmt (__alloc, __blcache, __osz);
        }
        return __NVR_OK;
    } else {
        __nv_unlock (&__lkg->lkg_ll);
        return __NVR_FAIL;
    }
}

inline _Bool __nv_heap_is_toplvl (__nv_allocator_t *__alloc,
                                  __nv_heap_t *__heap)
{
    return __heap->__hp_parent != NULL;
}

__nvr_t __nv_heap_req_block_from_heap (__nv_allocator_t *__alloc,
                                       __nv_heap_t *__heap,
                                       size_t __lkg_idx,
                                       __nv_block_header_t **__blockhp)
{
    /* % .caller heap */
    /* % .callee backline heap */

    /* (1) try sized linkage
     * (2) try unsized linkage
     * (3) __nv_heap_req_block_from_heap if possible */

#if __NVC_LKGNRANGECHECK
    if (__lkg_idx >= __alloc->__al_hpnlkg)
        return __NVR_ALLOC_LKG_IDX_RANGE;
#endif
    __nvr_t r;
    r = __nv_heap_req_block_from_slkg (__alloc, &__heap->__hp_lkgs[__lkg_idx],
                                       __blockhp);
    if (r == __NVR_OK)
        return __NVR_OK;
    r = __nv_heap_req_block_from_ulkg (__alloc, &__heap->__hp_lkgs[0],
                                       __nv_rlslup (__alloc, __lkg_idx),
                                       __blockhp);
    if (r == __NVR_OK)
        return __NVR_OK;
    if (__NV_UNLIKELY (__nv_heap_is_toplvl (__alloc, __heap))) {
        size_t attempts = 0lu;
        while (attempts++ < __alloc->__al_permtrytplvalloc) {
            r = __nv_toplvl_alloc_block (__alloc, __blockhp);
            if (r == __NVR_OK) {
                __nv_block_fmt (__alloc, *__blockhp,
                                __nv_rlslup (__alloc, __lkg_idx));
                return __NVR_OK;
            }
        }
        return r;
    } else {
        __nv_heap_req_block_from_heap (__alloc, __heap->__hp_parent, __lkg_idx,
                                       __blockhp);
    }
}

__nvr_t __nv_lkg_req_block_from_heap (__nv_allocator_t *__alloc,
                                      __nv_heap_t *__heap,
                                      size_t __lkg_idx,
                                      __nv_block_header_t **__blockhp)
{
    /* % .caller live linkage */
    /* % .callee live (frontline) heap */

    /* (1) try unsized linkage (L0)
     * (2) __nv_heap_req_block_from_heap */

    __nv_lkg_t *__ulkg = &__heap->__hp_lkgs[0];
    __nvr_t r = __nv_heap_req_block_from_ulkg (__alloc, __ulkg,
                                               __nv_rlslup (__alloc, __lkg_idx),
                                               __blockhp);
    if (r == __NVR_OK) {
        return __NVR_OK;
    }

    return __nv_heap_req_block_from_heap (__alloc, __heap->__hp_parent,
                                          __lkg_idx, __blockhp);
}

__nvr_t __nv_block_requests_lift (__nv_allocator_t *__alloc,
                                  __nv_lkg_t *__origin_lkg,
                                  __nv_block_header_t *__blockh)
{

}