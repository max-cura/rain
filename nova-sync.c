//
// file nova-sync.cc
// author Maximilien M. Cura
//

#include "nova.h"

__nvr_t __nv_lock_init(__nv_lock_t *__lock)
{
    /* __lock->__l_inner = PTHREAD_MUTEX_INITIALIZER; */
    pthread_mutexattr_t __mattr;
    if(0 != pthread_mutexattr_init(&__mattr))
        return __NVR_SYNC_MUTEX_INIT;
    if(0 != pthread_mutexattr_settype(&__mattr, PTHREAD_MUTEX_NORMAL)) {
        pthread_mutexattr_destroy(&__mattr);
        return __NVR_SYNC_MUTEX_INIT;
    }
    if(0 != pthread_mutex_init(&__lock->__l_inner, &__mattr)) {
        pthread_mutexattr_destroy(&__mattr);
        return __NVR_SYNC_MUTEX_INIT;
    }
    pthread_mutexattr_destroy(&__mattr);
    return __NVR_OK;
}

__nvr_t __nv_lock_destroy(__nv_lock_t *__nv_lock)
{
    pthread_mutex_destroy(&__nv_lock->__l_inner);
    return __NVR_OK;
}

__nvr_t __nv_lock(__nv_lock_t *__lock)
{
    pthread_mutex_lock(&__lock->__l_inner);
    return __NVR_OK;
}

__nvr_t __nv_unlock(__nv_lock_t *__lock)
{
    pthread_mutex_unlock(&__lock->__l_inner);
    return __NVR_OK;
}