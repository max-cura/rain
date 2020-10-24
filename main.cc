//
// file main.cc
// author Maximilien M. Cura
//

#include "nova.h"

size_t __nvh_drlslup (size_t __li)
{
    --__li;
    return (16ul << (__li >> 1)) + ((__li & 1) << ((__li >> 1) + 3));
}

size_t __nvh_dlslup (size_t __osz)
{
    if (__osz <= 16) return 1;
    size_t __nlz;
    size_t __res = (__nlz = 63 - __builtin_clzl (__osz))
                       ? (2 * __nlz) - !(~(1 << __nlz) & __osz)
                             + ((1 << (__nlz - 1)) & __osz
                                && ~(3 << (__nlz - 1)) & __osz)
                             - 7
                       : 0;
    return __res + 1;
}

bool __nvh_ctb (__nv_allocator_t *__attribute__ ((unused)) __alloc,
                __nv_lkg_t *__lkg)
{
    return __lkg->ll__lkg_nblk < 32;
}

#include <stdlib.h>
#include <thread>
#include <vector>

extern "C" int main (int, char **);

int main (int __attribute__ ((unused)) argc,
          char **__attribute__ ((unused)) argv)
{
    printf ("Saluto il nuovo mondo\n");
    //    auto *chtbl = (__nv_chunktable_t *)malloc (sizeof (size_t)
    //                                               + 16 * sizeof (__nv_chunk_t
    //                                               *));
    //    chtbl->__chtbl_sz = 16;
    //    memset (&chtbl->__chtbl_chs, 0, 16 * sizeof (__nv_chunk_t *));
    __nv_allocator_t alloc = { .__al_chsz = 0x100000,
                               .__al_blksz = 0x4000,
                               .__al_hpnslkg = 19,
                               .__al_permtrytplvalloc = 1,
                               .__al_ghp = nullptr,
                               .__al_chls = nullptr,
                               .__al_ht = {
                                   (void *)__nvh_dlslup,
                                   (void *)__nvh_drlslup,
                                   (void *)__nvh_ctb,
                               },
                               /* .__al_chtbl = chtbl, */
    };
    auto *tlhp = (__nv_heap_t *)malloc (sizeof (void *) + sizeof (size_t)
                                        + sizeof (__nv_lkg_t)
                                              * (alloc.__al_hpnslkg + 1));
    auto *flhp = (__nv_heap_t *)malloc (
        sizeof (size_t) + sizeof (__nv_lkg_t) * (alloc.__al_hpnslkg + 1));
    *(size_t *)tlhp = 1;
    tlhp = (__nv_heap_t *)(&((size_t *)tlhp)[1]);
    alloc.__al_ghp = tlhp;
    tlhp->__hp_parent = nullptr;
    flhp->__hp_parent = tlhp;
    for (size_t i = 0; i <= alloc.__al_hpnslkg; ++i) {
        tlhp->__hp_lkgs[i].lla__lkg_active = nullptr;
        __nv_lock_init (&tlhp->__hp_lkgs[i].__lkg_ll);
        tlhp->__hp_lkgs[i].__lkg_idx = i;
        tlhp->__hp_lkgs[i].ll__lkg_nblk = 0;

        flhp->__hp_lkgs[i].lla__lkg_active = nullptr;
        __nv_lock_init (&flhp->__hp_lkgs[i].__lkg_ll);
        flhp->__hp_lkgs[i].__lkg_idx = i;
        flhp->__hp_lkgs[i].ll__lkg_nblk = 0;
    }

    const size_t NOBJ = 5;
    const size_t NTHR = 2;
    void *obj_[NTHR][NOBJ];

    std::mutex m;
    __nv_tid_sys_init (0ull);
    __nv_tid_thread_init ();
    printf ("[main] TID: %llu\n", __nv_tid ());

    //    auto alloc_some = [&m, &alloc, &flhp, &obj_]() {
    //            __nv_tid_thread_init();
    //            m.lock();
    //            printf("TID: %llu\n", __nv_tid());
    //            m.unlock();
    //        };
    //    std::vector<std::thread> v;
    //    v.reserve(NTHR);
    //    for(size_t i = 0;i < NTHR;++i) {
    //        v.emplace_back(alloc_some);
    //    }
    //    for(size_t i = 0;i < NTHR;++i) {
    //        v[i].join();
    //    }
    void *obj;
    for (size_t i = 0; i < NOBJ; ++i) {
        __nv_heap_alloc_object (&alloc, flhp, 0x1000, &obj);
        printf ("\n[main] Allocated object [%p]\n\n", obj);
        obj_[__nv_tid()][i] = obj;
    }
    for(size_t i = 0; i < NOBJ;++i) {
        printf("\n[main] Deallocating object [%p]\n\n", obj_[__nv_tid()][i]);
        __nv_dealloc_object(&alloc, obj_[__nv_tid()][i]);
    }

    return 0;
}
