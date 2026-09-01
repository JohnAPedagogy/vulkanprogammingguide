## Summary

This PR establishes the Vulkan Programming Guide implementation from an empty repository and advances the project through Chapter 1, Listing 01.03 of 5.

The implementation is organized chapter by chapter, with each listing building on the shared Vulkan initialization code from the previous listing.

## Progress

- Chapter 1, Listing 01.01: Vulkan instance creation and physical-device enumeration
- Chapter 1, Listing 01.02: Physical-device queue-family inspection
- Chapter 1, Listing 01.03: Physical-device feature querying and logical-device creation
- Chapter 1, Listing 01.04: Layer handling, currently work in progress
- Chapter 1, Listing 01.05: Extension handling, currently work in progress

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

## Listing 01.03

Listing 01.03 introduces logical-device creation.

The example:

1. Initializes Vulkan.
2. Enumerates physical devices.
3. Selects the first available device.
4. Queries supported physical-device features.
5. Builds a requested feature structure.
6. Creates a logical device with one queue.
7. Reports whether logical-device creation succeeded.
8. Releases Vulkan resources during cleanup.

The implementation continues using the shared helpers instead of duplicating instance and device discovery logic in every listing.

## Build Validation

The Chapter 1 projects are configured for C++17 and qmake.

Example build commands:

```bash
cd ch01/01_03
mkdir -p build-msys2
cd build-msys2
qmake ../01_03_features.pro CONFIG+=debug
mingw32-make -j$(nproc)
./debug/ch01.exe
```
The examples are intended to be built from the MSYS2 MINGW64 terminal with matching Vulkan and GLFW libraries.
