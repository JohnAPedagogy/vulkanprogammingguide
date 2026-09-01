
## Summary

This PR establishes the Vulkan Programming Guide implementation from an empty repository and advances the project through Chapter 2, Listing 02.04.

The implementation is organized chapter by chapter, with each listing building on the shared Vulkan initialization code from the previous listing.

## Progress

- Chapter 1, Listing 01.01: Vulkan instance creation and physical-device enumeration
- Chapter 1, Listing 01.02: Physical-device queue-family inspection
- Chapter 1, Listing 01.03: Physical-device feature querying and logical-device creation
- Chapter 1, Listing 01.04: Layer handling, currently work in progress
- Chapter 1, Listing 01.05: Extension handling, currently work in progress
- Chapter 2, Listing 02.01: Custom Vulkan allocator header
- Chapter 2, Listing 02.02: Custom Vulkan allocator implementation
- Chapter 2, Listing 02.03: Buffer creation and memory allocation
- Chapter 2, Listing 02.04: Image creation and memory allocation

## What Changed

- Created the repository structure and initial project documentation from zero source files.
- Added reusable Vulkan initialization and cleanup helpers in `my_vulkan.cpp` and `my_vulkan.h`.
- Added physical-device enumeration and storage.
- Added queue-family property inspection.
- Added physical-device feature querying.
- Added logical-device creation using `VkDeviceQueueCreateInfo` and `VkDeviceCreateInfo`.
- Added feature-count reporting for the selected physical device.
- Added qmake project files for the Chapter 1 listings.
- Added Windows/MSYS2 build instructions and dependency documentation.
- Added Vulkan, GLFW, MinGW, and Qt/qmake configuration guidance.
- Added custom allocator support via `vk_allocator` class (`0201_allocator.h`, `0202_allocator.cpp`).
- Added `VkBuffer` creation and memory binding helpers (`vk_create_buffer`, `vk_track_buffer`).
- Added `VkImage` creation and memory binding helpers (`vk_create_image`, `vk_track_image`).
- Added memory type selection utility (`find_memory_type`).
- Added debug messenger support for validation layer output (Listing 02.04).
- Added `VK_EXT_debug_utils` extension handling for validation callbacks.
- Added image format support checking before creation.
- Extended cleanup to handle buffers, images, and debug messenger.
- Added Chapter 2 qmake project files with DLL copy steps.

## Listing 02.03

Listing 02.03 introduces Vulkan buffer creation and device-local memory allocation.

The example:

1. Initializes Vulkan with validation layers.
2. Enumerates physical devices and selects the first available device.
3. Queries queue-family properties and creates a logical device.
4. Enumerates instance layers and extensions.
5. Creates a `VkBuffer` with transfer usage flags.
6. Queries memory requirements and selects a host-visible, host-coherent memory type.
7. Allocates device memory and binds it to the buffer.
8. Reports whether buffer creation succeeded.
9. Releases all Vulkan resources during cleanup.

The implementation uses the custom `vk_allocator` class for host allocations, routed through `VkAllocationCallbacks`.

## Listing 02.04

Listing 02.04 extends Listing 02.03 by adding image creation.

The example:

1. Performs the same initialization sequence as Listing 02.03.
2. Creates a `VkImage` with sampled and transfer-dst usage flags.
3. Checks format support for the requested tiling and usage combination.
4. Queries image memory requirements and selects a device-local memory type.
5. Allocates device memory and binds it to the image.
6. Reports whether image creation succeeded.
7. Releases all Vulkan resources, including the debug messenger, during cleanup.

The debug messenger callback routes validation messages through `VK_EXT_debug_utils` when `ENABLE_VALIDATION` is defined.

## Build Validation

The Chapter 1 and Chapter 2 projects are configured for C++17 and qmake.

Example build commands for Chapter 2:

```bash
cd ch02/02_03
mkdir -p build-msys2
cd build-msys2
qmake ../02_03_buffer.pro CONFIG+=debug
mingw32-make -j$(nproc)
./debug/ch01.exe
```

```bash
cd ch02/02_04
mkdir -p build-msys2
cd build-msys2
qmake ../02_04_image_buf.pro CONFIG+=debug
mingw32-make -j$(nproc)
./debug/ch02_04.exe
```

The examples are intended to be built from the MSYS2 MINGW64 terminal with matching Vulkan and GLFW libraries.
