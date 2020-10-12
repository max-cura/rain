//
// file nova-fast.c
// author Maximilien M. Cura
//

#include "nova.h"

__nvr_t __nv_heap_alloc_object (__nv_allocator_t *__alloc, __nv_heap_t *__heap,
                                size_t __osize, void **__obj)
{
#if __NVC_NULLCHECK
    if (__alloc == NULL || __heap == NULL || __obj == NULL) return __NVR_INVALP;
#endif
    const size_t __lkgi = __nv_lslup (__alloc, __osize);
#if __NVC_LKGNRANGECHECK
    if (__lkgi >= __alloc->__al_hpnlkg) return __NVR_ALLOC_OSIZE;
    if (__lkgi == 0) {
        return __NVR_ALLOC_UNSIZED;
    } /* |lint-patch: clang-format */
#endif
    /* STRICT_INLINE forcibly combines simply-chained functions:
     *  - functions where following function's return value is immediately
     * returned, OR
     *  - functions with a fixed or no return value */
#if !__NV_STRICT_INLINE
    return __nv_lkg_alloc_object (__alloc, &__heap->__hp_lkgs[__lkgi], __heap,
                                  __obj);
}

/* __nv_lkg_alloc_object (lkg.alloc-object) is the central allocation routine.
 *
 * Fast path goes: mov, test, branch (ignored), call, cmp, branch (?), ret.
 */
__nvr_t __nv_lkg_alloc_object (__nv_allocator_t *__alloc, __nv_lkg_t *__lkg,
                               __nv_heap_t *__heap, void **__obj)
{
#else
    /* __lkg only exists as an rvalue in __nv_heap_object, so we need to pull
     * that out as an lvalue here */
    __nv_lkg_t *__lkg = &__heap->__hp_lkgs[__lkgi];
#endif
#if __NVC_NULLCHECK
    if (__alloc == NULL || __heap == NULL || __obj == NULL) return __NVR_INVALP;
#endif
    __nv_block_header_t *__blcache
        = __atomic_load_n (&__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);

    /* NULL HEAD CASE */

    if (__NV_UNLIKELY (__blcache == NULL)) {
        /* given lla__lkg_active == NULL, lla__lkg_active will not spuriously
         * change. */
        __nv_lock (&__lkg->__lkg_ll);
        __nv_block_header_t *__tmpbl;
        { /* <GL> impl by lkg_RBFH */
            __nvr_t r = __nv_lkg_req_block_from_heap (
                __alloc, __heap, __lkg->__lkg_idx, &__tmpbl);
            if (__nvr_iserr (r)) return r;
            __atomic_or_fetch (&__tmpbl->a__bh_flag, __NV_BLHDRFL_LKGHD,
                               __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->a__bh_tid, __nv_tid (),
                              __ATOMIC_SEQ_CST);
            __atomic_store_n (&__tmpbl->gl__bh_lkg, __lkg, __ATOMIC_SEQ_CST);
            __tmpbl->ll__bh_chnx = NULL;
            __tmpbl->ll__bh_chpr = NULL;
            __atomic_store_n (&__lkg->lla__lkg_active, __tmpbl,
                              __ATOMIC_SEQ_CST);
        }
        __nv_unlock (&__tmpbl->__bh_glck);
        __nv_unlock (&__lkg->__lkg_ll);
        /* Guarantee: freshly dropped block is always alloc-able */
#if __NVC_PROMOTE_GUARANTEE_FAILURES
        __nvr_t r = __nv_block_alloc_object (__alloc, __tmpbl, __obj);
        if (r == __NVR_FAIL) return __NVR_ALLOC_SPOILED_PROMOTEE;
        return r;
#else
        return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
#endif
    }

    /* STANDARD CASE */

    if (__NV_LIKELY (!__nvr_iserr (
            __nv_block_alloc_object (__alloc, __blcache, __obj)))) {
        return __NVR_OK;
    }

    /* SLIDE CASE */

    __nv_lock (&__lkg->__lkg_ll);
    __blcache = __atomic_load_n (&__lkg->lla__lkg_active, __ATOMIC_SEQ_CST);
    /* random thought: does having both branches start with the same instruction
     * help eliminate branch cost? */
    if (__blcache->ll__bh_chnx != NULL) {
        __nv_lock (&__blcache->__bh_glck);
        /* Manually check if we're slideable; along the way, remove and
         * cauterize any blocks that are getting lifted.
         *
         * SLIDER LIFT-GUARD
         */
        _Bool __slideable = YES;
        while (1) {
            __nv_lock (&__blcache->ll__bh_chnx->__bh_glck);
            /* Check next head for concurrent FPL/G clear from
             * block.dealloc-object */
            if (__atomic_load_n (&__blcache->ll__bh_chnx->gl__bh_fpg,
                                 __ATOMIC_SEQ_CST)
                    == NULL
                && __atomic_load_n (&__blcache->ll__bh_chnx->a__bh_acnt,
                                    __ATOMIC_SEQ_CST)
                       == 0
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
                /* otherwise continue until we find a block that isn't getting
                 * lifted... */
            } else {
                break;
            }
        }
        if (__slideable == YES) {
            /* Guarantee: __blcache->ll__bh_chnx exists, and is not being
             * lifted.
             */

            /* swap head flag */
            __atomic_and_fetch (&__blcache->a__bh_flag, ~__NV_BLHDRFL_LKGHD,
                                __ATOMIC_SEQ_CST);
            __atomic_or_fetch (&__blcache->ll__bh_chnx->a__bh_flag,
                               __NV_BLHDRFL_LKGHD, __ATOMIC_SEQ_CST);
            __blcache->gl__bh_lkg = __lkg;
            /* chain is preserved, just gotta move the active */
            __atomic_store_n (&__lkg->lla__lkg_active, __blcache->ll__bh_chnx,
                              __ATOMIC_SEQ_CST);

            /* the unlocking of the mutexes... */
            __nv_unlock (&__blcache->ll__bh_chnx->__bh_glck);
            __nv_unlock (&__blcache->__bh_glck);
            __nv_unlock (&__lkg->__lkg_ll);

            /* Guarantee: freshly promoted block is always alloc-able */
#if __NVC_PROMOTE_GUARANTEE_FAILURES
            __nvr_t r = __nv_block_alloc_object (__alloc,
                                                 __blcache->ll__bh_chnx, __obj);
            if (r == __NVR_FAIL) return __NVR_ALLOC_SPOILED_PROMOTEE;
            return r;
#else
            return __nv_block_alloc_object (__alloc, __blcache->ll__bh_chnx,
                                            __obj);
#endif
        }
    }

    /* PULL CASE */

    __nv_lock (&__blcache->__bh_glck);
    __nv_block_header_t *__tmpbl;
    {
        __nvr_t r = __nv_lkg_req_block_from_heap (__alloc, __heap,
                                                  __lkg->__lkg_idx, &__tmpbl);
        /* lkg RBFH will ignore lifted blocks */
        if (__nvr_iserr (r)) return r;
        __atomic_or_fetch (&__tmpbl->a__bh_flag, __NV_BLHDRFL_LKGHD,
                           __ATOMIC_SEQ_CST);
        __atomic_store_n (&__tmpbl->a__bh_tid, __nv_tid (), __ATOMIC_SEQ_CST);
        __atomic_store_n (&__tmpbl->gl__bh_lkg, __lkg, __ATOMIC_SEQ_CST);

        __atomic_and_fetch (&__blcache->a__bh_flag, ~__NV_BLHDRFL_LKGHD,
                            __ATOMIC_SEQ_CST);
        __tmpbl->ll__bh_chpr = __blcache;
        __tmpbl->ll__bh_chnx = __blcache->ll__bh_chnx;
        if (__tmpbl->ll__bh_chnx != NULL)
            __tmpbl->ll__bh_chnx->ll__bh_chpr = __tmpbl;
        __blcache->ll__bh_chnx = __tmpbl;
        __atomic_store_n (&__lkg->lla__lkg_active, __tmpbl, __ATOMIC_SEQ_CST);
        __nv_unlock (&__tmpbl->__bh_glck);
    }
    __nv_unlock (&__blcache->__bh_glck);
    __nv_unlock (&__lkg->__lkg_ll);

    /* Guarantee: freshly promoted block is always alloc-able */
#if __NVC_PROMOTE_GUARANTEE_FAILURES
    __nvr_t r = __nv_block_alloc_object (__alloc, __tmpbl, __obj);
    if (r == __NVR_FAIL) return __NVR_ALLOC_SPOILED_PROMOTEE;
    return r;
#else
    return __nv_block_alloc_object (__alloc, __tmpbl, __obj);
#endif
}

/* |mark-complete: __NV_STRICT_INLINE */
__nvr_t __nv_block_alloc_object (__nv_allocator_t *__alloc,
                                 __nv_block_header_t *__blockh, void **__obj)
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
        __blockh->__bh_fpl = __atomic_exchange_n (&__blockh->gl__bh_fpg, NULL,
                                                  __ATOMIC_SEQ_CST);
        __nv_unlock (&__blockh->__bh_glck);
        if (__NV_LIKELY (__blockh->__bh_fpl != NULL)) {
            return __nv_block_alloc_object_inner (__alloc, __blockh, __obj);
        }
        return __NVR_FAIL;
    }
}

__nvr_t
    __nv_block_alloc_object_inner (__nv_allocator_t *__attribute__ ((unused))
                                   __alloc,
                                   __nv_block_header_t *__blockh, void **__obj)
{
    (*__obj) = __blockh->__bh_fpl;
    __blockh->__bh_fpl = *(void **)(*__obj);
#if 0
    uint16_t __nxoff = *(uint16_t *)(*__obj);
    if (__NV_LIKELY (__nxoff != 0xffff)) {
        __blockh->__bh_fpl = (uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET + __nxoff;
    } else /* % __nxoff == 0xffff */ {
        __blockh->__bh_fpl = NULL;
    }
#endif
    /* inner section, no checking */
    __atomic_add_fetch (&__blockh->a__bh_acnt, 1, __ATOMIC_SEQ_CST);

    /* FIXED-STATE RETURN (given predicate) */
    return __NVR_OK;
}

__nvr_t __nv_block_dealloc_object (__nv_allocator_t *__alloc,
                                   __nv_block_header_t *__blockh, void *__obj)
{
    /* TID invalidation is a non-problem. */
    if (__NV_LIKELY (
            __nv_tid ()
            == __atomic_load_n (&__blockh->a__bh_tid, __ATOMIC_SEQ_CST))) {
        /* % local */
        *(void **)__obj = __blockh->__bh_fpl;
#if 0
        if (__NV_LIKELY (__blockh->__bh_fpl != NULL)) {
             *(uint16_t *)__obj = (uint8_t *)__blockh->__bh_fpl - ((uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET);
            __blockh->__bh_fpl = __obj;
        } else {
             *(uint16_t *)__obj = 0xffff;
            __blockh->__bh_fpl = __obj;
        }
#endif
    } else {
        /* % global */
        __nv_lock (&__blockh->__bh_glck);
        *(void **)__obj = __blockh->gl__bh_fpg;
#if 0
        {
            if (__NV_LIKELY (__blockh->gl__bh_fpg != NULL)) {
                *(uint16_t *)__obj = (uint8_t *)__blockh->gl__bh_fpg - ((uint8_t *)__blockh + __NV_BLOCK_MEMORY_OFFSET);
                __blockh->gl__bh_fpg = __obj;
            } else {
                *(uint16_t *)__obj = 0xffff;
                __blockh->gl__bh_fpg = __obj;
            }
        }
#endif
        __nv_unlock (&__blockh->__bh_glck);
    }

    if (__atomic_sub_fetch (&__blockh->a__bh_acnt, 1, __ATOMIC_SEQ_CST) == 0) {
        __nv_lock (&__blockh->__bh_glck);
        if (0
            == (__atomic_load_n (&__blockh->a__bh_flag, __ATOMIC_SEQ_CST)
                & __NV_BLHDRFL_LKGHD)) {
            if (__atomic_load_n (&__blockh->a__bh_acnt, __ATOMIC_SEQ_CST)
                == 0) {
                void *__cfpl = __blockh->__bh_fpl,
                     *__cfpg = __blockh->gl__bh_fpg;
                __blockh->__bh_fpl = NULL;
                __blockh->gl__bh_fpg = NULL;
                __nv_unlock (&__blockh->__bh_glck);
                /* the gap */
                __nv_lkg_t *__lkgcache
                    = __atomic_load_n (&__blockh->gl__bh_lkg, __ATOMIC_SEQ_CST);
                __nv_lock (&__lkgcache->__lkg_ll);
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
