//
// file nova-os.c
// author Maximilien M. Cura
//

#include "nova.h"

/* Handy tool for checking if address is 4k-aligned (default pagealigned).
 */
_Bool __nv_is_aligned_4k (size_t __s)
{
    return (__s & 0x3ffful) == 0;
}
/* Handy tool for checking if address is 2m-aligned (default hugetlb).
 */
_Bool __nv_is_aligned_2m (size_t __s)
{
    return (__s & 0x1ffffful) == 0;
}

/* Magic headers
 */
#if defined __APPLE__ && __NV_USE_MACOS_VM_CALLS
    #include <mach/vm_statistics.h>
    #include <mach/vm_map.h>
    #include <mach/mach.h>
#else
    #include <sys/mman.h>
#endif

/* Chunk allocator
 */
__nvr_t __nv_os_challoc (__nv_allocator_t *__alloc, void **__mem)
{
    *__mem = NULL;
#if defined __APPLE__ && __NV_USE_MACOS_VM_CALLS
    if (!__nv_is_aligned_4k (__alloc->__al_chsz)) {
        return __NVR_OS_ALIGN;
    }
    /* otherwise vm_allocate will try to do a fixed-place allocation */
    kern_return_t kr = KERN_SUCCESS;
    if ((__alloc->__al_chsz & 0x3ffffffff) == 0) {
        kr = vm_allocate ((vm_map_t)mach_task_self (),
                          (vm_address_t *)__mem,
                          __alloc->__al_chsz,
                          VM_MAKE_TAG (246) /* userspace tag */
                              | VM_FLAGS_ANYWHERE
                              | VM_FLAGS_4GB_CHUNK); /* 4gib chunking */
    } else {
        kr = vm_allocate ((vm_map_t)mach_task_self (),
                          (vm_address_t *)__mem,
                          __alloc->__al_chsz,
                          VM_MAKE_TAG (246) /* userspace tag */
                              | VM_FLAGS_ANYWHERE);
    }
    if (kr == KERN_NO_SPACE) {
        *__mem = NULL;
    } else {
        return __NVR_OK;
    }
#endif
    /* ok, if we got it */
#if defined MAP_HUGETLB
    if (__nv_is_aligned_2m (__alloc->__al_chsz)) {
    #if defined MAP_HUGE_2MB
        *__mem = mmap (NULL, __alloc->__al_chsz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRICATE | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);
    #else
        *__mem = mmap (NULL, __alloc->__al_chsz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
    #endif
    } else
#endif
        if (__nv_is_aligned_4k (__alloc->__al_chsz)) {
        *__mem = mmap (NULL, __alloc->__al_chsz,
                       PROT_READ | PROT_WRITE,
                       MAP_ANON | MAP_PRIVATE, -1, 0);
    } else {
        return __NVR_OS_ALIGN;
    }
    if (*__mem == MAP_FAILED) {
        *__mem = NULL;
        return __NVR_OS_ALLOC;
    }
    return __NVR_OK;
}
