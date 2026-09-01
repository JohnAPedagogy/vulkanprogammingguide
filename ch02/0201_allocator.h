#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <cstddef>
#include <vulkan/vulkan.h>

class vk_allocator
{
public:
    // Operator that allows an instance of this class to be used as a
    // VkAllocationCallbacks structure
    inline operator VkAllocationCallbacks() const
    {
        VkAllocationCallbacks result;

        result.pUserData = (void*)this;
        result.pfnAllocation = &Allocation;
        result.pfnReallocation = &Reallocation;
        result.pfnFree = &Free;
        result.pfnInternalAllocation = nullptr;
        result.pfnInternalFree = nullptr;

        return result;
    };

private:
    // Declare the allocator callbacks as static member functions.
    static void* VKAPI_CALL Allocation(
        void*                                       pUserData,
        size_t                                      size,
        size_t                                      alignment,
        VkSystemAllocationScope                      allocationScope);

    static void* VKAPI_CALL Reallocation(
        void*                                       pUserData,
        void*                                       pOriginal,
        size_t                                      size,
        size_t                                      alignment,
        VkSystemAllocationScope                      allocationScope);

    static void VKAPI_CALL Free(
        void*                                       pUserData,
        void*                                       pMemory);

    // Now declare the nonstatic member functions that will actually perform
    // the allocations.
    void* Allocation(

        size_t                                      size,
        size_t                                      alignment,
        VkSystemAllocationScope                      allocationScope);

    void* Reallocation(
        void*                                       pOriginal,
        size_t                                      size,
        size_t                                      alignment,
        VkSystemAllocationScope                      allocationScope);

    void Free(
        void*                                       pMemory);
};

#endif // _ALLOCATOR_H
