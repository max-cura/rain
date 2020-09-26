//
// file nova.c
// author Maximilien M. Cura
//

#include "nova.h"

#include <sys/mman.h>
#include <string.h>

int main (int __attribute__ ((unused)) argc, char __attribute__ ((unused)) * *argv)
{
    printf ("Saluto il nuovo mondo\n");
    return 0;
}

__nvr_t __nv_tid_thread_init_mono (__nv_tidmgr_t *__mgr)
{
    __nv_lock(&((__nv_tidmgr_monotonic_t *)(__mgr->__tidmun))->__monolck);
    if(__mgr->__tidmfl & __NV_TIDSYS_MONOEXHAUSTED) {
        __nv_unlock(&((__nv_tidmgr_monotonic_t*)(__mgr->__tidmun))->__monolck);
        return __NVR_TID_INSUFFICIENT_TIDS;
    }
    uint64_t *__mono = &((__nv_tidmgr_monotonic_t *)(__mgr->__tidmun))->__mono;
    __mgr->__tidml = (*__mono)++;
    if (__mgr->__tidml == 0) {
        /* so that subsequent calls also error */
        __mgr->__tidmfl |= __NV_TIDSYS_MONOEXHAUSTED;

        __nv_unlock (&((__nv_tidmgr_monotonic_t *)(__mgr->__tidmun))->__monolck);
        return __NVR_TID_INSUFFICIENT_TIDS;
    }
    __nv_unlock (&((__nv_tidmgr_monotonic_t *)(__mgr->__tidmun))->__monolck);
    return __NVR_OK;
}

__nvr_t __nv_tid_thread_destroy_mono (__nv_tidmgr_t *__mgr)
{
    /* cheap, so why not? */
    __atomic_compare_exchange_n (&((__nv_tidmgr_monotonic_t *)(__mgr->__tidmun))->__mono,
                                 &__mgr->__tidml,
                                 __mgr->__tidml - 1,
                                 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    __mgr->__tidml = __NV_NULL_TID;
    return __NVR_OK;
}

__nvr_t __nv_tidrecyls_newch (__nv_tidmgr_recylsch_t **__recyls)
{
    (*__recyls) = mmap (NULL, 0x4000, (unsigned)PROT_READ | (unsigned)PROT_WRITE, MAP_ANON, /* spec'd */ -1, 0);
    if ((*__recyls) == MAP_FAILED) {
        return __NVR_OS_ALLOC;
    }
    (*__recyls)->__tidrecylschidx = 0;
    memset (&(*__recyls)->__tidrecylschtids, 0, sizeof (*__recyls)->__tidrecylschtids);
    (*__recyls)->__tidrecylschnx = NULL;
    return __NVR_OK;
}

__nvr_t __nv_tid_thread_init_recy (__nv_tidmgr_t *__mgr)
{
    __nv_tidmgr_recycling_t *__rmgr = (__nv_tidmgr_recycling_t *)(__mgr->__tidmun);
    __nv_lock (&__rmgr->__recylslck);
    if (__rmgr->__recyls != NULL) {
        __mgr->__tidml = __rmgr->__recyls->__tidrecylschtids[__rmgr->__recyls->__tidrecylschidx--];
        if (__rmgr->__recyls->__tidrecylschidx == 0) {
            /* chop-at-end */
            __nv_tidmgr_recylsch_t *__swap = __rmgr->__recyls->__tidrecylschnx;
            munmap (__rmgr->__recyls, 0x4000);
            __rmgr->__recyls = __swap;
        }
        __nv_unlock (&__rmgr->__recylslck);
        return __NVR_OK;
    } else {
        __mgr->__tidml = __atomic_add_fetch (&__rmgr->__recylsmono, 1, __ATOMIC_SEQ_CST);
        if (__mgr->__tidml == 0) {
            __nv_unlock (&__rmgr->__recylslck);
            return __NVR_TID_INSUFFICIENT_TIDS;
        }
        __nv_unlock (&__rmgr->__recylslck);
        return __NVR_OK;
    }
}

__nvr_t __nv_tid_thread_destroy_recy (__nv_tidmgr_t *__mgr)
{
    __nv_tidmgr_recycling_t *__rmgr = (__nv_tidmgr_recycling_t *)(__mgr->__tidmun);
    __nv_lock (&__rmgr->__recylslck);
    if (__rmgr->__recyls == NULL) {
        __nvr_t r = __nv_tidrecyls_newch (&__rmgr->__recyls);
        if (__nvr_iserr (r)) {
            __nv_unlock (&__rmgr->__recylslck);
            return r;
        }
    } else if (__rmgr->__recyls->__tidrecylschidx == 0x7fe) {
        __nv_tidmgr_recylsch_t *__swap = __rmgr->__recyls;
        __nvr_t r = __nv_tidrecyls_newch (&__rmgr->__recyls);
        if (__nvr_iserr (r)) {
            __nv_unlock (&__rmgr->__recylslck);
            return r;
        }
        __rmgr->__recyls->__tidrecylschnx = __swap;
    }
    uint64_t __returning = __mgr->__tidml;
    __rmgr->__recyls->__tidrecylschtids[__rmgr->__recyls->__tidrecylschidx++] = __returning;
    __mgr->__tidml = __NV_NULL_TID;

    __nv_unlock (&__rmgr->__recylslck);
}

__nvr_t __nv_tid_thread_init (__nv_tidmgr_t *__mgr)
{
    if (__mgr->__tidmfl & __NV_TIDSYS_LAZY) {
        return;
    } else {
        if (__mgr->__tidmfl & __NV_TIDSYS_RECYCLING) {
            return __nv_tid_thread_init_recy (__mgr);
        } else {
            return __nv_tid_thread_init_mono (__mgr);
        }
    }
}

__nvr_t __nv_tid_thread_destroy (__nv_tidmgr_t *__mgr)
{
    if (__mgr->__tidmfl & __NV_TIDSYS_RECYCLING) {
        return __nv_tid_thread_destroy_recy (__mgr);
    } else {
        return __nv_tid_thread_destroy_mono (__mgr);
    }
}

__nvr_t __nv_tid_sys_init (__nv_tidmgr_t **__mgr, uint64_t __fl)
{
    __nvr_t r = __nv_os_smalloc (__mgr, sizeof **__mgr);
    if (__nvr_iserr (r))
        return r;
    (*__mgr)->__tidmfl = __fl;
    (*__mgr)->__tidml = __NV_NULL_TID;
    if (__fl & __NV_TIDSYS_RECYCLING) {
        __nv_tidmgr_recycling_t *__rmgr;
        r = __nv_os_smalloc (&__rmgr, sizeof *__rmgr);
        if (__nvr_iserr (r)) {
            __nv_os_smdealloc (__mgr, sizeof **__mgr);
            return r;
        }
        __rmgr->__recyls = NULL;
        __nv_lock_init (&__rmgr->__recylslck);
        __rmgr->__recylsmono = 0;
        (*__mgr)->__tidmun = (void *)__rmgr;
        return __NVR_OK;
    } else {
        __nv_tidmgr_monotonic_t *__mmgr;
        r = __nv_os_smalloc (&__mmgr, sizeof *__mmgr);
        if (__nvr_iserr (r)) {
            __nv_os_smdealloc (__mgr, sizeof **__mgr);
            return r;
        }
        __nv_lock_init (&__mmgr->__monolck);
        __mmgr->__mono = 0;
        return __NVR_OK;
    }
}

__nvr_t __nv_tid_sys_destroy (__nv_tidmgr_t *__mgr)
{
    if (__mgr->__tidmfl & __NV_TIDSYS_RECYCLING) {
        /* destroy free lists */
        __nv_tidmgr_recylsch_t *__crecylsch = ((__nv_tidmgr_recycling_t *)(__mgr->__tidmun))->__recyls,
                               *__swap;
        while (__crecylsch != NULL) {
            __swap = __crecylsch->__tidrecylschnx;
            munmap (__crecylsch, 0x4000);
            __crecylsch = __swap;
        }
        __nv_lock_destroy(&((__nv_tidmgr_recycling_t*)__mgr->__tidmun)->__recylslck);
        __nv_os_smdealloc (__mgr->__tidmun, sizeof (__nv_tidmgr_recycling_t));
        return __NVR_OK;
    } else {
        __nv_lock_destroy(&((__nv_tidmgr_recycling_t*)__mgr->__tidmun)->__recylslck);
        __nv_os_smdealloc (__mgr->__tidmun, sizeof (__nv_tidmgr_monotonic_t));
        return __NVR_OK;
    }
}

uint64_t __nv_tid (__nv_tidmgr_t *__mgr)
{
    if (__mgr->__tidmfl & __NV_TIDSYS_LAZY && __mgr->__tidml == __NV_NULL_TID) {
        __nv_tid_thread_init (__mgr);
    }
    return __mgr->__tidml;
}
__nvr_t __nv_tid_state (__nv_tidmgr_t *__mgr)
{
    if (__mgr->__tidml == __NV_NULL_TID) {
        return __NVR_TID_THR_UNINITIALIZED;
    }
}

size_t __nv_chunk_nblocks(__nv_allocator_t * __alloc, size_t __meml) {
    return __meml / __alloc->__al_blksz;
}

__nvr_t __nv_chunk_new (__nv_allocator_t *__alloc, __nv_chunk_t **__chunk)
{
    void *__chmem = NULL;
    __nvr_t r = __nv_os_challoc (__alloc, &__chmem, __alloc->__al_chsz);
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
    uint8_t *const __chbase = (uint8_t *)__chunk + sizeof (__nv_chunk_t);
    uint8_t *__choff = __chbase;
    /* \perftarget tight pack v. 8-aligned v. pagealign
     * My intuition is that tight or 8-aligned will give the best result
     */
    for (size_t __bli = 0; __bli < __chnblk; ++__bli) {
        /* In-place block bind.
         */
        __nv_block_bind (__alloc, __choff);
        /* Next block.
         */
        __choff += sizeof (__nv_block_header_t);
        __choff += __alloc->__al_blksz;
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
    __nv_chunk_t *__tmp__chlsnx;
    do {
        __tmp__chlsnx = __chunk->__ch_chlsnx;
        __nvr_t r = __nv_os_dealloc (__alloc, __chunk, __chunk->__ch_nbyt);
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

__nvr_t __nv_block_bind (__nv_allocator_t *__alloc, void *__blmem)
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

__nvr_t __nv_lkg_req_block_from_heap (__nv_allocator_t *__alloc, __nv_heap_t *__heap, size_t __lkg_idx, __nv_block_header_t **__blockhp)
{
    /* %self __heap */
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
        /* STRICT_INLINE forcibly combines simply-chained functions:
         *  - functions where following function's return value is immediately returned, OR
         *  - functions with a fixed or no return value */
#if !__NV_STRICT_INLINE
    return __nv_lkg_alloc_object (__alloc, &__heap->__hp_lkgs[__lkgi], __heap, __obj);
}

/* __nv_lkg_alloc_object (lkg.alloc-object) is the central allocation routine.
 * 
 * Fast path goes: mov, test, branch (ignored), call, cmp, branch (?), ret.
 */
__nvr_t __nv_lkg_alloc_object (__nv_allocator_t *__alloc, __nv_lkg_t *__lkg, __nv_heap_t *__heap, void **__obj)
{
#endif
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
            __nvr_t r = __nv_lkg_req_block_from_heap (__alloc, __heap, __lkg->lkg_idx, &__tmpbl);
            if (__nvr_iserr (r))
                return r;
            __atomic_or_fetch (&__tmpbl->a__bh_flag, __NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->a__bh_tid, __nv_tid (), __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->gl__bh_lkg, __lkg, __ATOMIC_SEQ_CST);
            __tmpbl->ll__bh_chnx = NULL;
            __tmpbl->ll__bh_chpr = NULL;
            __atomic_store_n (&__lkg->lla__lkg_active, __tmpbl, __ATOMIC_SEQ_CST);
        }
        __nv_unlock (&__tmpbl->__bh_glck);
        __nv_unlock (&__lkg->lkg_ll);
        /* Guarantee: freshly dropped block is always alloc-able */
#if __NVC_PROMOTE_GUARANTEE_FAILURES
        __nvr_t r = __nv_block_alloc_object (__alloc, __tmpbl, __obj);
        if (r == __NVR_FAIL)
            return __NVR_ALLOC_SPOILED_PROMOTEE;
        return r;
#else
        return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
#endif
    }

    /* STANDARD CASE */

    if (__NV_LIKELY (!__nvr_iserr (__nv_block_alloc_object (__alloc, __blcache, __obj)))) {
        return __NVR_OK;
    }

    /* SLIDE CASE */

    __nv_lock (&__lkg->lkg_ll);
    __blcache = __atomic_load_n (&__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);
    /* random thought: does having both branches start with the same instruction help eliminate branch cost? */
    if (__blcache->ll__bh_chnx != NULL) {
        __nv_lock (&__blcache->__bh_glck);
        /* Manually check if we're slideable; along the way, remove and cauterize any blocks that are getting lifted.
         */
        _Bool __slideable = YES;
        while (1) {
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
                if (__blcache->ll__bh_chnx == NULL) {
                    __slideable = NO;
                    break;
                }
                /* otherwise continue until we find a block that isn't getting lifted... */
            } else {
                break;
            }
        }
        if (__slideable == YES) {
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
#if __NVC_PROMOTE_GUARANTEE_FAILURES
            __nvr_t r = __nv_block_alloc_object (__alloc, __blcache->ll__bh_chnx, __obj);
            if (r == __NVR_FAIL)
                return __NVR_ALLOC_SPOILED_PROMOTEE;
            return r;
#else
            return __nv_block_alloc_object (__alloc, __blcache->ll__bh_chnx, __obj);
#endif
        }
    }

    /* PULL CASE */

    __nv_lock (&__blcache->__bh_glck);
    __nv_block_header_t *__tmpbl;
    {
        __nvr_t r = __nv_lkg_req_block_from_heap (__alloc, __heap, __lkg->lkg_idx, &__tmpbl);
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
        __nv_unlock (&__tmpbl->__bh_glck);
    }
    __nv_unlock (&__blcache->__bh_glck);
    __nv_unlock (&__lkg->lkg_ll);

    /* Guarantee: freshly promoted block is always alloc-able */
#if __NVC_PROMOTE_GUARANTEE_FAILURES
    __nvr_t r = __nv_block_alloc_object (__alloc, __tmpbl, __obj);
    if (r == __NVR_FAIL)
        return __NVR_ALLOC_SPOILED_PROMOTEE;
    return r;
#else
    return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
#endif
}

/* |mark-complete: __NV_STRICT_INLINE */
__nvr_t __nv_block_alloc_object (__nv_allocator_t *__alloc, __nv_block_header_t *__blockh, void **__obj)
{
#if __NVC_NULLCHECK
    if (__alloc == NULL || __blockh == NULL || __obj == NULL)
        return __NVR_INVALP;
#endif
    if (__NV_LIKELY (__blockh->__bh_fpl != NULL)) {
        return __nv_block_alloc_object_inner (__alloc, __blockh, __obj);
    } else {
        __nv_lock (&__blockh->__bh_glck);
        /* works on x86-64 */
        /* note: we don't actually want this to be atomic; luckily enough,
         * this just evaluates to a pair of `mov`s and an `xchg` */
        __blockh->__bh_fpl = __atomic_exchange_n (&__blockh->gl__bh_fpg, NULL, __ATOMIC_SEQ_CST);
        __nv_unlock (&__blockh->__bh_glck);
        if (__NV_LIKELY (__blockh->__bh_fpl != NULL)) {
            return __nv_block_alloc_object_inner (__alloc, __blockh, __obj);
        }
        return __NVR_FAIL;
    }
}

__nvr_t __nv_block_alloc_object_inner (__nv_allocator_t *__alloc, __nv_block_header_t *__blockh, void **__obj)
{
    (*__obj) = __blockh->__bh_fpl;
    uint16_t __nxoff = *(uint16_t *)(*__obj);
    if (__NV_LIKELY (__nxoff != 0xffff)) {
        __blockh->__bh_fpl = (uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET + __nxoff;
    } else /* % __nxoff == 0xffff */ {
        __blockh->__bh_fpl = NULL;
    }
    /* inner section, no checking */
    __atomic_add_fetch (&__blockh->a__bh_acnt, 1, __ATOMIC_SEQ_CST);

    /* FIXED-STATE RETURN (given predicate) */
    return __NVR_OK;
}

__nvr_t __nv_block_dealloc_object (__nv_allocator_t *__alloc, __nv_block_header_t *__blockh, void *__obj)
{
    /* TID invalidation is a non-problem. */
    if (__NV_LIKELY (__nv_tid () == __atomic_load_n (&__blockh->a__bh_tid, __ATOMIC_SEQ_CST))) {
        /* % local */
        if (__NV_LIKELY (__blockh->__bh_fpl != NULL)) {
            *(uint16_t *)__obj = (uint8_t *)__blockh->__bh_fpl - ((uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET);
            __blockh->__bh_fpl = __obj;
        } else {
            *(uint16_t *)__obj = 0xffff;
            __blockh->__bh_fpl = __obj;
        }
    } else {
        /* % global */
        __nv_lock (&__blockh->__bh_glck);
        {
            if (__NV_LIKELY (__blockh->gl__bh_fpg != NULL)) {
                *(uint16_t *)__obj = (uint8_t *)__blockh->gl__bh_fpg - ((uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET);
                __blockh->gl__bh_fpg = __obj;
            } else {
                *(uint16_t *)__obj = 0xffff;
                __blockh->gl__bh_fpg = __obj;
            }
        }
        __nv_unlock (&__blockh->__bh_glck);
    }

    if (__atomic_sub_fetch (&__blockh->a__bh_acnt, 1, __ATOMIC_SEQ_CST) == 0) {
        __nv_lock (&__blockh->__bh_glck);
        if (0 == (__atomic_load_n (&__blockh->a__bh_flag, __ATOMIC_SEQ_CST) & __NV_BLHDRFL_LKGHD)) {
            if (__atomic_load_n (&__blockh->a__bh_acnt, __ATOMIC_SEQ_CST) == 0) {
                void *__cfpl = __blockh->__bh_fpl, *__cfpg = __blockh->gl__bh_fpg;
                __blockh->__bh_fpl = NULL;
                __blockh->gl__bh_fpg = NULL;
                __nv_unlock (&__blockh->__bh_glck);
                /* the gap */
                __nv_lkg_t *__lkgcache = __atomic_load_n (&__blockh->gl__bh_lkg, __ATOMIC_SEQ_CST);
                __nv_lock (&__lkgcache->lkg_ll);
                __nv_lock (&__blockh->__bh_glck);
                __blockh->__bh_fpl = __cfpl;
                __blockh->gl__bh_fpg = __cfpg;
                /* BRL takes care of unlocking procedures */
                return __nv_block_requests_lift (__alloc, __lkgcache, __blockh);
            } else {
                __nv_unlock (&__blockh->__bh_glck);
                return __NVR_OK;
            }
        } else {
            __nv_unlock (&__blockh->__bh_glck);
            return __NVR_OK;
        }
    }

    /* half-empty? */

    return __NVR_OK;
}
