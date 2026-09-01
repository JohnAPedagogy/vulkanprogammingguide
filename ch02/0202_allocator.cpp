#include "0201_allocator.h"
#define GLFW_INCLUDE_VULKAN

void* allocator::Allocation(
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return aligned_malloc(size, alignment);
}

void* VKAPI_CALL allocator::Allocation(
    void*                                       pUserData,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return static_cast<allocator*>(pUserData)->Allocation(size,
                                                            alignment,
                                                            allocationScope);
}

void* allocator::Reallocation(
    void*                                       pOriginal,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return aligned_realloc(pOriginal, size, alignment);
}
void* VKAPI_CALL allocator::Reallocation(
    void*                                       pUserData,
    void*                                       pOriginal,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocationScope                      allocationScope)
{
    return static_cast<allocator*>(pUserData)->Reallocation(pOriginal,
                                                              size,
                                                              alignment,
                                                              allocationScope);
}
void allocator::Free(
    void*                                       pMemory)
{
    aligned_free(pMemory);
}
void VKAPI_CALL allocator::Free(
    void*                                       pUserData,
    void*                                       pMemory)
{
    return static_cast<allocator*>(pUserData)->Free(pMemory);
}