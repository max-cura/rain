//
// file nova-tid.c
// author Maximilien M. Cura
//

/* FILE nova-tid.c
 * DESC Facilities for working with thread identification to enable thread
 * differentiation in the allocator.
 */

#include "nova.h"
#include <sys/mman.h>
#include <string.h>

/* VAR __nv_tidml__@nova-tid.c
 * DESC Thread-local variable containing the current thread's thread id, or
 *  __NV_NULL_TID
 */
static _Thread_local uint64_t __nv_tidml__ = __NV_NULL_TID;
/* VAR __nv_tidmgr__@nova-tid.c
 * DESC Default thread id manager.
 */
static __nv_tidmgr_t __nv_tidmgr__;

/* FUNC __nv_tid_thread_init_mono
 * DESC Per-thread monotonic thread ID initialization.
 */
__nvr_t __nv_tid_thread_init_mono ()
{
#if __NV_TRACE
    printf ("[trace]\ttid thread-init:mono\n");
#endif
    __nv_lock (&__nv_tidmgr__.__tidlck);
    /* If the monotonic counter is in an exhausted state, then ignore. */
    if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_SEQ_CST)
        & __NV_TIDSYS_MONOEXHAUSTED) {
#if __NV_TRACE
        printf ("[trace]\t> monotonic counter exhausted, counterstate: %llu\n",
                __atomic_load_n (
                    &(((__nv_tidmgr_monotonic_t *)(__nv_tidmgr__.__tidmun))
                          ->__mono),
                    __ATOMIC_SEQ_CST));
#endif
        __nv_unlock (&__nv_tidmgr__.__tidlck);
        return __NVR_TID_INSUFFICIENT_TIDS;
    }
    __nv_tidml__ = __atomic_add_fetch (
        &(((__nv_tidmgr_monotonic_t *)(__nv_tidmgr__.__tidmun))->__mono), 1lu,
        __ATOMIC_SEQ_CST);
    /* Overflow check */
    if (__NV_UNLIKELY (__nv_tidml__ == 0lu)) {
        /* so that subsequent calls also error, put the manager into an
         * exhausted state*/
        __atomic_or_fetch (&__nv_tidmgr__.__tidmfl, __NV_TIDSYS_MONOEXHAUSTED,
                           __ATOMIC_SEQ_CST);
        __nv_unlock (&__nv_tidmgr__.__tidlck);
        return __NVR_TID_INSUFFICIENT_TIDS;
    }

    __nv_unlock (&__nv_tidmgr__.__tidlck);
    return __NVR_OK;
}

__nvr_t __nv_tid_thread_destroy_mono ()
{
#if 0
    /* cheap, so why not? */
    __atomic_compare_exchange_n (
        &((__nv_tidmgr_monotonic_t *)(__nv_tidmgr__.__tidmun))
             ->__mono,
        &__nv_tidml__,
        __nv_tidml__ - 1,
        0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
    __nv_tidml__ = __NV_NULL_TID;
    return __NVR_OK;
}

__nvr_t __nv_tidrecyls_newch (__nv_tidmgr_recylsch_t **__recyls)
{
    (*__recyls)
        = mmap (NULL, 0x4000, (unsigned)PROT_READ | (unsigned)PROT_WRITE,
                MAP_ANON, /* spec'd */ -1, 0);
    if ((*__recyls) == MAP_FAILED) { return __NVR_OS_ALLOC; }
    (*__recyls)->__tidrecylschidx = 0;
    memset (&(*__recyls)->__tidrecylschtids, 0,
            sizeof (*__recyls)->__tidrecylschtids);
    (*__recyls)->__tidrecylschnx = NULL;
    return __NVR_OK;
}

__nvr_t __nv_tid_thread_init_recy ()
{
    __nv_tidmgr_recycling_t *__rmgr
        = (__nv_tidmgr_recycling_t *)(__nv_tidmgr__.__tidmun);
    __nv_lock (&__nv_tidmgr__.__tidlck);
    if (__rmgr->__recyls != NULL) {
        __nv_tidml__
            = __rmgr->__recyls
                  ->__tidrecylschtids[__rmgr->__recyls->__tidrecylschidx--];
        if (__rmgr->__recyls->__tidrecylschidx == 0) {
            /* chop-at-end */
            __nv_tidmgr_recylsch_t *__swap = __rmgr->__recyls->__tidrecylschnx;
            munmap (__rmgr->__recyls, 0x4000);
            __rmgr->__recyls = __swap;
        }
        __nv_unlock (&__nv_tidmgr__.__tidlck);
        return __NVR_OK;
    } else {
        __nv_tidml__
            = __atomic_add_fetch (&__rmgr->__recylsmono, 1, __ATOMIC_SEQ_CST);
        if (__nv_tidml__ == 0) {
            __nv_unlock (&__nv_tidmgr__.__tidlck);
            return __NVR_TID_INSUFFICIENT_TIDS;
        }
        __nv_unlock (&__nv_tidmgr__.__tidlck);
        return __NVR_OK;
    }
}

__nvr_t __nv_tid_thread_destroy_recy ()
{
    __nv_tidmgr_recycling_t *__rmgr
        = (__nv_tidmgr_recycling_t *)(__nv_tidmgr__.__tidmun);
    __nv_lock (&__nv_tidmgr__.__tidlck);
    if (__rmgr->__recyls == NULL) {
        __nvr_t r = __nv_tidrecyls_newch (&__rmgr->__recyls);
        if (__nvr_iserr (r)) {
            __nv_unlock (&__nv_tidmgr__.__tidlck);
            return r;
        }
    } else if (__rmgr->__recyls->__tidrecylschidx == 0x7fe) {
        __nv_tidmgr_recylsch_t *__swap = __rmgr->__recyls;
        __nvr_t r = __nv_tidrecyls_newch (&__rmgr->__recyls);
        if (__nvr_iserr (r)) {
            __nv_unlock (&__nv_tidmgr__.__tidlck);
            return r;
        }
        __rmgr->__recyls->__tidrecylschnx = __swap;
    }
    uint64_t __returning = __nv_tidml__;
    __rmgr->__recyls->__tidrecylschtids[__rmgr->__recyls->__tidrecylschidx++]
        = __returning;
    __nv_tidml__ = __NV_NULL_TID;

    __nv_unlock (&__nv_tidmgr__.__tidlck);
    return __NVR_OK;
}

__nvr_t __nv_tid_thread_init ()
{
    if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_RELAXED)
        & __NV_TIDSYS_LAZY) {
        return __NVR_OK;
    } else {
        if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_RELAXED)
            & __NV_TIDSYS_RECYCLING) {
            return __nv_tid_thread_init_recy ();
        } else {
            return __nv_tid_thread_init_mono ();
        }
    }
}

__nvr_t __nv_tid_thread_destroy ()
{
    if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_RELAXED)
        & __NV_TIDSYS_RECYCLING) {
        return __nv_tid_thread_destroy_recy ();
    } else {
        return __nv_tid_thread_destroy_mono ();
    }
}

__nvr_t __nv_tid_sys_init (uint64_t __fl)
{
    __nv_tidmgr__.__tidmfl = __fl;
    __nv_tidml__ = __NV_NULL_TID;
    if (__fl & __NV_TIDSYS_RECYCLING) {
        __nv_tidmgr_recycling_t *__rmgr;
        const __nvr_t r = __nv_os_smalloc ((void **)&__rmgr, sizeof *__rmgr);
        if (__nvr_iserr (r)) { return r; }
        __rmgr->__recyls = NULL;
        __nv_lock_init (&__nv_tidmgr__.__tidlck);
        __rmgr->__recylsmono = 0;
        __nv_tidmgr__.__tidmun = (void *)__rmgr;
        return __NVR_OK;
    } else {
        __nv_tidmgr_monotonic_t *__mmgr;
        const __nvr_t r = __nv_os_smalloc ((void **)&__mmgr, sizeof *__mmgr);
        if (__nvr_iserr (r)) { return r; }
        __nv_lock_init (&__nv_tidmgr__.__tidlck);
        __mmgr->__mono = 0;
        __nv_tidmgr__.__tidmun = (void *)__mmgr;
        return __NVR_OK;
    }
}

__nvr_t __nv_tid_sys_destroy ()
{
    if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_RELAXED)
        & __NV_TIDSYS_RECYCLING) {
        /* destroy free lists */
        __nv_tidmgr_recylsch_t *__crecylsch
            = ((__nv_tidmgr_recycling_t *)(__nv_tidmgr__.__tidmun))->__recyls,
            *__swap;
        while (__crecylsch != NULL) {
            __swap = __crecylsch->__tidrecylschnx;
            munmap (__crecylsch, 0x4000);
            __crecylsch = __swap;
        }
        __nv_lock_destroy (&__nv_tidmgr__.__tidlck);
        __nv_os_smdealloc (__nv_tidmgr__.__tidmun,
                           sizeof (__nv_tidmgr_recycling_t));
        return __NVR_OK;
    } else {
        __nv_lock_destroy (&__nv_tidmgr__.__tidlck);
        __nv_os_smdealloc (__nv_tidmgr__.__tidmun,
                           sizeof (__nv_tidmgr_monotonic_t));
        return __NVR_OK;
    }
}


uint64_t __nv_tid ()
{
    if (__nv_tidml__ == __NV_NULL_TID
        && __atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_RELAXED)
               & __NV_TIDSYS_LAZY) {
        __nv_tid_thread_init ();
    }
    return __nv_tidml__;
}
__nvr_t __nv_tid_state ()
{
    if (__nv_tidml__ == __NV_NULL_TID) {
        if (__atomic_load_n (&__nv_tidmgr__.__tidmfl, __ATOMIC_SEQ_CST)
            & __NV_TIDSYS_MONOEXHAUSTED) {
            return __NVR_TID_INSUFFICIENT_TIDS;
        } else {
            return __NVR_TID_THR_UNINITIALIZED;
        }
    }
    return __NVR_OK;
}
