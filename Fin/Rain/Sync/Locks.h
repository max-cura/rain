#pragma once
//
// file Locks.h
// author Maximilien M. Cura
//

#include <Fin/Rain/Lang/Util.h>
#include <pthread.h>

namespace rain::sync {
    /* HotLock and ColdLock are special locks that are used by systems that are
     * not allowed to allocate memory through Rain-- they need to either go through
     * the OS or use non-heap memory instead.
     */

    struct ColdLock
    {
        pthread_mutex_t inner_ = PTHREAD_MUTEX_INITIALIZER;

        ColdLock (ColdLock const &) = delete;
        ColdLock (ColdLock &&) = delete;

        ColdLock &operator= (ColdLock const &) = delete;
        ColdLock &operator= (ColdLock &&) = delete;

        ColdLock ()
        {
            pthread_mutexattr_t normal_attr;
            pthread_mutexattr_init (&normal_attr);
            pthread_mutexattr_settype (&normal_attr, PTHREAD_MUTEX_NORMAL);
            pthread_mutex_init (&inner_, &normal_attr);
            pthread_mutexattr_destroy (&normal_attr);
        }
        ~ColdLock ()
        {
            pthread_mutex_destroy (&inner_);
        }

        struct Sentinel
        {
            ColdLock *creator_;
            Sentinel (Sentinel const &) = delete;
            Sentinel &operator= (Sentinel const &) = delete;
            Sentinel (Sentinel &&x)
                : creator_{ x.creator_ }
            {
                x.creator_ = nullptr;
            }
            Sentinel &operator= (Sentinel &&x)
            {
                this->~Sentinel ();
                creator_ = x.creator_;
                x.creator_ = nullptr;
                return *this;
            }
            Sentinel (ColdLock &creator)
                : creator_{ &creator }
            {
                creator_->lock();
            }
            ~Sentinel ()
            {
                if (creator_ != nullptr) {
                    creator_->unlock();
                    creator_ = nullptr;
                }
            }
        };

        Sentinel guard_lock()
        {
            return Sentinel{*this};
        }
        void lock()
        {
            pthread_mutex_lock(&inner_);
        }
        void unlock()
        {
            pthread_mutex_unlock(&inner_);
        }
    };

    using HotLock = ColdLock;
}
