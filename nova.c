//
// file nova.c
// author Maximilien M. Cura
//

#include "nova.h"

int main (int argc, char **argv)
{
    printf ("Saluto il nuovo mondo\n");
    return 0;
}

__nvr_t __nv_chunk_new (__nv_allocator_t *__alloc, __nv_chunk_t **__chunk)
{
    void *__chmem = NULL;
    __nvr_t r = __nv_os_alloc (__alloc, &__chmem, __alloc->__al_chsz);
    if (__nvr_iserr (r))
        return r;
    *__chunk = (__nv_chunk_t *)(__chmem);
    (*__chunk)->__ch_chlsnx = NULL;
    (*__chunk)->__ch_nblk = __nv_chunk_nblocks (__alloc, __alloc->__al_chsz - sizeof (__nv_chunk_t));
    (*__chunk)->__ch_nbyt = __alloc->__al_chsz;

    __nv_chunk_populate (__alloc, *__chunk);

    return __NVR_OK;
}

__nvr_t __nv_chunk_populate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk)
{
    const size_t __chnblk = __chunk->__ch_nblk;
    const uint8_t *__chbase = (uint8_t *)__chunk + sizeof (__nv_chunk_t);
    uint8_t *__choff = __chbase;
    /* \perftarget tight pack v. 8-aligned v. pagealign
     * My intuition is that tight or 8-aligned will give the best result
     */
    for (size_t __bli = 0; __bli < __chnblk; ++i) {
        /* In-place block bind.
         */
        __nv_block_bind (__alloc, __choff);
        /* Next block.
         */
        choff += sizeof (__nv_block_header_t);
        choff += __alloc->__al__blksz;
    }
    /* Pass blocks in batch to allocator.
     */
    __nvr_t r = __nv_chunk_yields_block_batch_locked (__alloc, __chbase, __chunk->__ch_nblk);
    return r;
}

__nvr_t __nv_chunk_delete (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk, __nv_chunk_t **__chunk_mknx)
{
#if __NVC_NULLCHECK
    if (__chunk == NULL || __chunk_mknx == NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    *__chunk_mknx = __chunk->__ch_chlsnx;
    /* use __chunk->__ch_nbyt instead of __alloc->__al_chsz because it's likely on
     * the same cache line as __ch_chlsnx */
    __nvr_t r = __nv_os_dealloc (__alloc, __chunk, __chunk->__ch_nbyt);
    if (__nvr_iserr (r))
        return r;
    return __NVR_OK;
}

__nvr_t __nv_chunk_delete_propagate (__nv_allocator_t *__alloc, __nv_chunk_t *__chunk, __nv_chunk_t **__errch, __nv_chunk_t **__errtl)
{
#if __NVC_NULLCHECK
    if (__chunk == NULL || __chunk_mknx = NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    __nv_chunk_t *__tmp_chlsnx;
    do {
        __tmp__chlsnx = __chunk->__ch_chlsnx;
        __nvr_t r = __nv_os_dealloc (__alloc, __chunk, __chunk->__ch_nbyt);
        if (__nvr_iserr (r)) {
            if (__errch != NULL)
                *__errch = __chunk;
            if (__errtl != NULL)
                *__errtl = __tmp_chlsnx;
            return r;
        }
        __chunk = __tmp__chlsnx;
    } while (__chunk != NULL);
}

__nvr_t __nv_block_bind (__nv_allocator_t *__alloc, void *__blmem)
{
#if __NVC_NULLCHECK
    if (__blmem == NULL || __alloc == NULL)
        return __NVR_INVALP;
#endif
    __nv_block_header_t *__bh = (__nv_block_header_t)*__blmem;
    __bh->__bh_fpl = NULL;
    __bh->__bh_osz = 0;
    __bh->__bh_ocnt = 0;
    __bh->a__bh_flag = 0;
    __bh->a__bh_acnt = 0;
    __bh->a__bh_tid = 0;
    __bh->ll__bh_chnx = NULL;
    __bh->ll__bh_chpr = NULL;
    __bh->gl_bh_lkg = NULL;
    __bh->gl__bh_fpg = NULL;
    __nv_lock_init (&__bh->__bh_glck);

    return __NVR_OK;
}

__nvr_t __nv_heap_alloc_object (__nv_allocator_t *__alloc, __nv_heap_t *__heap, size_t __osize, void **__obj)
{
#if __NVC_NULLCHECK
    if (__alloc == NULL || __heap == NULL || __obj == NULL)
        return __NVR_INVALP;
#endif
    const size_t __lkgi = (*__alloc->__al_lkgdistrat) (__osize);
#if __NVC_LKGNRANGECHECK
    if (__lkgi >= __alloc->__al_hpnlkg)
        return __NVR_ALLOC_OSIZE;
    if (__lkgi == 0)
        return __NVR_ALLOC_UNSIZED;
#endif
    return __nv_lkg_alloc_object (__alloc, &__heap->__hp_lkgs[__lkgi], __heap, __obj);
}

/* __nv_lkg_alloc_object (lkg.alloc-object) is the central allocation routine.
 * 
 * Fast path goes: mov, test, branch (ignored), call, cmp, branch (?), ret.
 */
__nvr_t __nv_lkg_alloc_object (__nv_allocator_t *__alloc, __nv_lkg_t *__lkg, __nv_heap_t *__heap, void **__obj)
{
#if __NVC_NULLCHECK
    if (__alloc == NULL || __heap == NULL || __obj == NULL)
        return __NVR_INVALP;
#endif
    __nv_block_header_t *__blcache = __atomic_load_n (&__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);

    /* NULL HEAD CASE */

    if (__NV_UNLIKELY (__blcache == NULL)) {
        /* given lla__lkg_active == NULL, lla__lkg_active will not spuriously change. */
        __nv_lock (&__lkg->lkg_ll);
        __nv_block_header_t *__tmpbl;
        { /* <GL> impl by lkg_RBFH */
            __nvr_t r = __nv_lkg_req_block_from_heap (__alloc, __heap, lkg_idx, &__tmpbl);
            if (__nvr_iserr (r))
                return r;
            __atomic_or_fetch (&__tmpbl->a__bh_flag, __NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->a__bh_tid, __nv_tid (), __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->gl__bh_lkg, __lkg, __ATOMIC_SEQ_CST);
            __tmpbl->ll__bh_chnx = NULL;
            __tmpbl->ll__bh_chpr = NULL;
            __atomic_store_n (&__lkg->lla__lkg_active, __tmpbl, __ATOMIC_SEQ_CST);
        }
        __nv_unlock (&__tmbpl->__bh_glck);
        __nv_unlock (&__lkg->lkg_ll);
        /* Guarantee: freshly dropped block is always alloc-able */
        return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
    }

    /* STANDARD CASE */

    if (__NV_LIKELY (!__nvr_iserr (__nv_block_alloc_object (__alloc, __blcache, __obj)))) {
        return __NVR_OK;
    }

    /* SLIDE CASE */

    __nv_lock (&__lkg->lkg_ll);
    __blcache = __atomic_load_n (&__lkg->lla_lkg_active, __ATOMIC_SEQ_CST);
    /* random thought: does having both branches start with the same instruction help eliminate branch cost? */
    if (__blcache->ll__bh_chnx != NULL) {
        __nv_lock (&__blcache->__bh_glck);
        /* Manually check if we're slideable; along the way, remove and cauterize any blocks that are getting lifted.
         */
        _Bool __slideable = true;
        while (true) {
            __nv_lock (&__blcache->ll__bh_chnx->__bh_glck);
            /* Check next head for concurrent FPL/G clear from block.dealloc-object */
            if (__atomic_load_n (&__blcache->ll__bh_chnx->gl__bh_fpg, __ATOMIC_SEQ_CST) == NULL
                && __atomic_load_n (&__blcache->ll__bh_chnx->a__bh_acnt, __ATOMIC_SEQ_CST) == 0
                /* we can touch FPL because this is the owning thread. */
                && __blcache->ll__bh_chnx->__bh_fpl == NULL) {
                /* okay, this block is being lifted */
                /* try again: remove block */
                __nv_block_header_t *__liftee = __blcache->ll__bh_chnx;
                __blcache->ll__bh_chnx = __liftee->ll__bh_chnx;
                if (__liftee->ll__bh_chnx != NULL)
                    __liftee->ll__bh_chnx->ll__bh_chpr = __blcache;
                /* cauterize */
                __liftee->ll__bh_chnx = NULL;
                __liftee->ll__bh_chpr = NULL;
                /* clear the GLCK so the lift doesn't get blocked */
                __nv_unlock (&__liftee->__bh_glck);
                /* loop guard */
                if (__blcache->ll_bh_chnx == NULL) {
                    __slideable = false;
                    break;
                }
                /* otherwise continue until we find a block that isn't getting lifted... */
            } else {
                break;
            }
        }
        if (__slideable) {
            /* Guarantee: __blcache->ll__bh_chnx exists, and is not being lifted.
             */

            /* swap head flag */
            __atomic_and_fetch (&__blcache->a__bh_flag, ~__NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
            __atomic_or_fetch (&__blcache->ll__bh_chnx->a__bh_flag, __NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
            /* chain is preserved, just gotta move the active */
            __atomic_store_n (&__lkg->lla__lkg_active, __blcache->ll__bh_chnx, __ATOMIC_SEQ_CST);

            /* the unlocking of the mutexes... */
            __nv_unlock (&__blcache->ll__bh_chnx->__bh_glck);
            __nv_unlock (&__blcache->__bh_glck);
            __nv_unlock (&__lkg->lkg_ll);

            /* Guarantee: freshly promoted block is always alloc-able */
            return __nv_block_alloc_object (__alloc, __blcache->ll_bh_chnx, __obj);
        }
    }

    /* PULL CASE */

    __nv_lock (&__blcache->__bh_glck);
    __nv_block_header_t *__tmpbl;
    {
        __nvr_t r = __nv_lkg_req_block_from_heap (__alloc, __heap, lkg_idx, &__tmpbl);
        /* lkg RBFH will ignore lifted blocks */
        if (__nvr_iserr (r))
            return r;
        __atomic_or_fetch (&__tmpbl->a__bh_flag, __NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
        __atomic_store_n (&__tmpbl->a__bh_tid, __nv_tid (), __ATOMIC_SEQ_CST);
        __atomic_store_n (&__tmpbl->gl__bh_lkg, __lkg, __ATOMIC_SEQ_CST);

        __atomic_and_fetch (&__blcache->a__bh_flag, ~__NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
        __tmpbl->ll__bh_chpr = __blcache;
        __tmpbl->ll__bh_chnx = __blcache->ll__bh_chnx;
        if (__tmpbl->ll__bh_chnx != NULL)
            __tmpbl->ll__bh_chnx->ll__bh_chpr = __tmpbl;
        __blcache->ll__bh_chnx = __tmpbl;
        __atomic_store_n (&__lkg->lla__lkg_active, __tmpbl, __ATOMIC_SEQ_CST);
        __nv_unlock (&__tmpbl->__bh_glck)
    }
    __nv_unlock (&__blcache->__bh_glck);
    __nv_unlock (&__lkg->lkg_ll);

    /* Guarantee: freshly promoted block is always alloc-able */
    return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
}

__nvr_t __nv_block_dealloc_object (__nv_allocator_t *__alloc, __nv_block_header_t *__block, void *__obj)
{
    if (__NV_LIKELY (__nova_tid () == __atomic_load_n (&__block->a__bh_tid, __ATOMIC_SEQ_CST))) {
        /* % local */
    } else {
        /* % global */
    }

    if (__atomic_sub_fetch (&__block->a__bh_acnt, 1, __ATOMIC_SEQ_CST) == 0) {
        __nv_lock (&__block->__bh_glck);
        if (0 == __atomic_load_n (&__block->a__bh_flag, __ATOMIC_SEQ_CST) & __NV_BLHDRFL_LKGHD) {
            if (__atomic_load_n (&__block->a__bh_acnt, __ATOMIC_SEQ_CST) == 0) {
                void *__cfpl = __block->__bh_fpl, __cfpg = __block->gl__bh_fpg;
                __block->__bh_fpl = NULL;
                __block->gl__bh_fpg = NULL;
                __nv_unlock (&__block->__bh_glck);
                __nv_lkg_t *__lkgcache = __atomic_load_n (&__block->gl__bh_lkg, __ATOMIC_SEQ_CST);
                __nv_lock (&__lkgcache->lkg_ll);
                __nv_lock (&__block->__bh_glck);
                __block->__bh_fpl = __cfpl;
                __block->gl__bh_fpg = __cfpg;
                return __nv_block_requests_lift (__alloc, __lkgcache, __block);
            } else {
                __nv_unlock (&__block->__bh_glck);
                return __NVR_OK;
            }
        } else {
            __nv_unlock (&__block->__bh_glck);
            return __NVR_OK;
        }
    }
}
