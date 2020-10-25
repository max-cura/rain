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
#include <chrono>
#include <cmath>

extern "C" int main (int, char **);

int main (int __attribute__ ((unused)) argc,
          char **__attribute__ ((unused)) argv)
{
    //    auto *chtbl = (__nv_chunktable_t *)malloc (sizeof (size_t)
    //                                               + 16 * sizeof (__nv_chunk_t
    //                                               *));
    //    chtbl->__chtbl_sz = 16;
    //    memset (&chtbl->__chtbl_chs, 0, 16 * sizeof (__nv_chunk_t *));
    using clock = std::chrono::high_resolution_clock;
    auto bench_startup_begin = clock::now ();
    __nv_allocator_t alloc = {
        .__al_chsz = 0x200000,
        .__al_blksz = 0x4000,
        .__al_hpnslkg = 19,
        .__al_permtrytplvalloc = 1,
        .__al_ghp = nullptr,
        .__al_chls = nullptr,
        .__als_nchunk = 0,
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
    __nv_tid_sys_init (0ull);
    __nv_tid_thread_init ();
    //    printf ("[main] TID: %llu\n", __nv_tid ());

    auto bench_startup_end = clock::now ();
    printf ("Nova startup took %lu us\n", (unsigned long)
                                              std::chrono::duration_cast<std::chrono::microseconds> (bench_startup_end - bench_startup_begin)
                                                  .count ());

    const size_t OBJSZMAX = 0x2000;
    const size_t NOBJ = 0x10000;
    const size_t NSTEP = log2(OBJSZMAX) + 1;
    [[maybe_unused]] const size_t NTHR = 1;
    size_t OBJSZ = 0x1;
    void *obj_[NSTEP][NOBJ];

    void *obj;

    std::vector<std::tuple<unsigned long, unsigned long, size_t>> timetable;

    size_t j = 0;
    while (OBJSZ <= OBJSZMAX) {
        alloc.__als_nchunk = 0;
        auto bench_loop_begin = clock::now ();
        for (size_t i = 0; i < NOBJ; ++i) {
            __nv_heap_alloc_object (&alloc, flhp, OBJSZ, &obj);
            //        printf ("\n[main] Allocated object [%p]\n\n", obj);
            obj_[j][i] = obj;
        }
//        for (size_t i = 0; i < NOBJ; ++i) {
//            //        printf("\n[main] Deallocating object [%p]\n\n", obj_[__nv_tid()][i]);
//            __nv_dealloc_object (&alloc, obj_[j][i]);
//        }
        auto bench_loop_end = clock::now ();
        unsigned long bench_loop_nova_time = (unsigned long)
                                                 std::chrono::duration_cast<std::chrono::microseconds> (bench_loop_end - bench_loop_begin)
                                                     .count ();
        bench_loop_begin = clock::now ();
        for (size_t i = 0; i < NOBJ; ++i) {
            obj_[j][i] = malloc (OBJSZ);
        }
//        for (size_t i = 0; i < NOBJ; ++i) {
//            free (obj_[j][i]);
//        }
        bench_loop_end = clock::now ();
        unsigned long bench_loop_malloc_time = (unsigned long)
                                                   std::chrono::duration_cast<std::chrono::microseconds> (bench_loop_end - bench_loop_begin)
                                                       .count ();
        timetable.emplace_back (bench_loop_nova_time, bench_loop_malloc_time, alloc.__als_nchunk);
        printf("%-4lu %-24lu %-24lu\n", OBJSZ, bench_loop_nova_time, bench_loop_malloc_time);
        ++j;
        OBJSZ *= 2;
    }

    printf("size nova (us)                malloc (us)\n");
    size_t i = 1;
    for(auto row : timetable) {
        printf("%-4lu %-24lu %-24lu\t%.4f\tchunks: %2lu\n", i, std::get<0>(row), std::get<1>(row),
               double(std::get<0>(row)) / double(std::get<1>(row)), std::get<2>(row));
        i *= 2;
    }

    return 0;
}
