#include "0201_allocator.h"
#include <malloc.h>

void* vk_allocator::Allocation(
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    (void)allocationScope;
    return _aligned_malloc(size, alignment);
}

void* VKAPI_CALL vk_allocator::Allocation(
    void*                                       pUserData,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return static_cast<vk_allocator*>(pUserData)->Allocation(size,
                                                            alignment,
                                                            allocationScope);
}

void* vk_allocator::Reallocation(
    void*                                       pOriginal,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    (void)allocationScope;
    return _aligned_realloc(pOriginal, size, alignment);
}
void* VKAPI_CALL vk_allocator::Reallocation(
    void*                                       pUserData,
    void*                                       pOriginal,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return static_cast<vk_allocator*>(pUserData)->Reallocation(pOriginal,
                                                              size,
                                                              alignment,
                                                              allocationScope);
}
void vk_allocator::Free(
    void*                                       pMemory)
{
    _aligned_free(pMemory);
}
void VKAPI_CALL vk_allocator::Free(
    void*                                       pUserData,
    void*                                       pMemory)
{
    return static_cast<vk_allocator*>(pUserData)->Free(pMemory);
}
