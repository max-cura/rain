#pragma once
//
// file Alloc.h
// author Maximilien M. Cura
//

#include <Rain/Compiler.h>
#include <Rain/Math/Integers.h>
#include <Rain/Math/Util.h>
#include <Rain/Lang/Maybe.h>
#include <Rain/Sync/Locks.h>

namespace rain::mem {
    struct Allocator
    {
        typedef sync::ColdLock ColdLockType;
        typedef sync::HotLock HotLockType;

        constexpr static inline usize_t page_size = 0x4000;

        void *region_create (usize_t size);
        void region_free (void *memory, usize_t size);

        void *page_create (usize_t number);
        void page_free (void *page_memory, usize_t number);
    };

    struct Region
    {
        usize_t size;
        usize_t marker;
        void *memory;
        Allocator &allocator;

        Region (Region const &) = delete;
        Region &operator= (Region const &) = delete;
        inline ~Region ()
        {
            allocator.region_free (memory, size);
        }

        template <typename T, usize_t _TAlign = 8>
        lang::Maybe<T &> alloc ()
        {
            static_assert (_TAlign > 0);
            usize_t aligned_marker = marker + _TAlign - 1;
            aligned_marker -= (aligned_marker % _TAlign);
            if (LIKELY (size - aligned_marker >= sizeof (T))) {
                void *memory = reinterpret_cast<u8_t *> (memory) + aligned_marker;
                marker = aligned_marker + sizeof (T);
                return { *reinterpret_cast<T *> (memory) };
            } else {
                return {};
            }
        }
    };

    template <typename T, usize_t _TAlign = 8>
    struct ColdPoolAllocator
    {
        static_assert (_TAlign >= 8 || sizeof (T) >= 8);

        struct Page
        {
            void *next;
            usize_t available_objects;

            Page (Page const &) = delete;
            Page (Page &&) = delete;
            Page &operator= (Page const &) = delete;
            Page &operator= (Page &&) = delete;
            ~Page () = delete;

            void *alloc ()
            {
                return static_cast<u8_t *> (this)
                       + page_memory_offset
                       + (--available_objects * aligned_object_size);
            }
        };
        constexpr static inline usize_t aligned_object_size = math::round_up (sizeof (T), _TAlign);
        constexpr static inline usize_t page_memory_offset
            = math::round_up (sizeof (Page), _TAlign);
        constexpr static inline usize_t objects_in_page
            = (Allocator::page_size - page_memory_offset) / aligned_object_size;

        usize_t num_pages;
        Page *page;
        void *free_list;
        Allocator::ColdLockType lock;
        Allocator &allocator;
        usize_t page_batch_size;

        ColdPoolAllocator (Allocator &alloc, usize_t page_batch_size)
            : num_pages{ 0 }
            , page{ nullptr }
            , free_list{ nullptr }
            , lock ()
            , allocator{ alloc }
            , page_batch_size{ page_batch_size }
        { }
        ~ColdPoolAllocator ()
        {
            auto guard = lock.guard_lock();
            while (page != nullptr) {
                Page *next_page = page->next;
                allocator.page_free (page, page_batch_size);
                page = next_page;
            }
        }

        lang::Maybe<T &> alloc ()
        {
            auto guard = lock.guard_lock ();
            if (free_list != nullptr) {
                void *object = free_list;
                free_list = *reinterpret_cast<void **> (object);
                return { *reinterpret_cast<T *> (object) };
            }
            if (page->available_objects) {
                Page *new_page = static_cast<Page *> (
                    allocator.page_create (page_batch_size));
                if (new_page == nullptr) return {};
                new (new_page) Page{
                    .next = page,
                    .available_objects = objects_in_page
                };
                page = new_page;
            }
            return { page->alloc () };
        }

        void dealloc (T &t)
        {
            void *tptr = static_cast<void *> (reinterpret_cast<T *> (t));
            auto guard = lock.guard_lock ();
            (*reinterpret_cast<void **> (tptr)) = free_list;
            free_list = tptr;
        }
    };
}