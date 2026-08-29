Vulkan Programming Guide
========================

Example code for the Vulkan Programming Guide book.  The source listings
have been ported from the original C++ listings to a pure C interface.

## Windows setup with MSYS2

The current qmake projects use the 64-bit MSYS2 `mingw64` layout. Use the
**MSYS2 MINGW64** terminal for the commands below. Do not mix the MSYS shell,
the UCRT64 toolchain, and the MINGW64 libraries in one build.

MSYS2 currently recommends UCRT64 for new projects, but these examples and
their `.pro` files are configured for MINGW64. If you choose UCRT64 instead,
replace every `mingw-w64-x86_64-` package name with its
`mingw-w64-ucrt-x86_64-` equivalent and update `MSYS2_DIR` in both qmake files.

### Install MSYS2

1. Install MSYS2 from [msys2.org](https://www.msys2.org/), accepting the
   default location `C:\msys64`.
2. Open **MSYS2 MINGW64** from the Start menu.
3. Update the base installation. If pacman asks you to close the terminal,
   close it, reopen **MSYS2 MINGW64**, and run the update again until no further
   update is required:

```bash
pacman -Syu
```

### Install prerequisites

Run this in the **MSYS2 MINGW64** terminal:

```bash
pacman -S --needed \
  git \
  base-devel \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-qt6-base \
  mingw-w64-x86_64-glfw \
  mingw-w64-x86_64-vulkan-headers \
  mingw-w64-x86_64-vulkan-loader
```

The required packages provide:

| Package | Purpose |
|---|---|
| `base-devel` | `make` and common build tools |
| `mingw-w64-x86_64-toolchain` | GCC, G++, linker, and MinGW runtime |
| `mingw-w64-x86_64-qt6-base` | `qmake` and Qt's MinGW build integration |
| `mingw-w64-x86_64-glfw` | GLFW headers, import library, and runtime DLL |
| `mingw-w64-x86_64-vulkan-headers` | Vulkan C headers |
| `mingw-w64-x86_64-vulkan-loader` | Vulkan loader import library and DLL |

For validation and shader work, also install:

```bash
pacman -S --needed \
  mingw-w64-x86_64-vulkan-validation-layers \
  mingw-w64-x86_64-spirv-tools \
  mingw-w64-x86_64-glslang
```

The loader is not a GPU driver. Install a current Vulkan driver from the
vendor of the machine's NVIDIA, AMD, or Intel GPU. Confirm that the driver
supports Vulkan before running an example.

### Get the source

From the MINGW64 terminal, either clone the repository or change to an
existing Windows checkout. MSYS2 exposes `C:\work` as `/c/work`:

```bash
cd /path/to/vulkanprogammingguide
```

### Configure the MSYS2 prefix

Both `ch1.pro` files honor a qmake `MSYS2_DIR` variable first, then the
`MSYS2_DIR` environment variable, and finally fall back to the existing
machine-specific paths. For a standard MSYS2 installation, the qmake value is:

```qmake
MSYS2_DIR = C:/msys64/mingw64
```

Use the environment form with qmake:

```bash
export MSYS2_DIR=C:/msys64/mingw64
```

Use forward slashes in qmake paths. The project uses `QT =` with no Qt
modules; Qt is installed here because it supplies `qmake`, while the example
itself uses GLFW and the Vulkan C API.

### Build directly with G++

This is the smallest MSYS2 build path and does not require qmake:

```bash
cd /path/to/vulkanprogammingguide/ch01/01_01
g++ -std=c++17 -Wall -Wextra main.cpp -o ch01.exe -lglfw3 -lvulkan-1
./ch01.exe
```

Repeat with `ch01/01_02` to build the second example. Because the MINGW64
terminal places `/mingw64/bin` on `PATH`, the Vulkan loader and GLFW DLLs can
be found when the executable starts.

### Build with qmake

To use the checked-in qmake project, build out of source:

```bash
cd /path/to/vulkanprogammingguide/ch01/01_01
mkdir -p build-msys2
cd build-msys2
qmake ../ch1.pro CONFIG+=debug
mingw32-make -j$(nproc)
./debug/ch01.exe
```

You can also pass the prefix for one configuration without exporting it:

```bash
qmake MSYS2_DIR=C:/msys64/mingw64 ../ch1.pro CONFIG+=debug
```

Build `ch01/01_02` the same way. The qmake file copies `glfw3.dll` and
`vulkan-1.dll` beside the debug or release executable. If qmake cannot find a
library, check that `MSYS2_DIR` points to the same prefix used by the MINGW64
compiler and that the matching `mingw-w64-x86_64-*` packages are installed.

### Verify Vulkan

The examples enumerate physical devices. A successful run should report at
least one device. If an example reports that Vulkan initialization failed:

1. Confirm that it was built from the MINGW64 terminal.
2. Confirm `/mingw64/bin` is on `PATH` and contains `vulkan-1.dll`.
3. Confirm the GPU vendor's Vulkan driver is installed.
4. Run the example with the validation layer enabled when diagnosing API
   misuse; the layer package is included in the optional package command
   above.
