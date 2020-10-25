//
// file Alloc.cc
// author Maximilien M. Cura
//
#include <Fin/Rain/Mem/Alloc.h>

#include <sys/mman.h>

void *rain::mem::Allocator::region_create (usize_t size)
{
    // temporary:
    return page_create(((size + 8 + page_size - 1) & ~(page_size - 1))/page_size);
}
void rain::mem::Allocator::region_free (void *memory, usize_t size)
{
    return page_free(memory, size / page_size);
}
void *rain::mem::Allocator::page_create (rain::usize_t number)
{
    void * mem = mmap(nullptr, number * page_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
    if(mem == MAP_FAILED) {
        return nullptr;
    }
    return mem;
}
void rain::mem::Allocator::page_free (void *page_memory, rain::usize_t number)
{
    munmap(page_memory, number * page_size);
}
