QT       =
CONFIG   += c++17
TARGET    = ch04_04_blit_resolve
TEMPLATE  = app

DEFINES    += ENABLE_VALIDATION

SOURCES  += \
    ../../ch02/0202_allocator.cpp \
    04_04_blit_resolve.cpp \
    my_vulkan.cpp

HEADERS += \
    ../../ch02/0201_allocator.h \
    my_vulkan.h


# Keep a qmake command-line value; otherwise import the environment value.

isEmpty(MSYS2_DIR): MSYS2_DIR = $$(MSYS2_DIR)

isEmpty(MSYS2_DIR) {
    exists(D:/Programs/msys64/mingw64) {
        MSYS2_DIR = D:/Programs/msys64/mingw64
    } else {
        MSYS2_DIR = C:/Users/JohnAlamina/scoop/apps/msys2/2025-08-30/mingw64
    }
}

INCLUDEPATH += $$MSYS2_DIR/include
DEFINES      += VK_LAYER_PATH=\\\"$$MSYS2_DIR/bin\\\"

# Link the import libs by full path rather than -L$$MSYS2_DIR/lib -lglfw3
# -lvulkan-1: adding MSYS2's lib/ to the search path lets the unqualified
# -lmingw32 (added automatically by the mkspec) resolve to MSYS2's CRT import
# lib instead of the active toolchain's own, which breaks the link when this
# project is built with a different MinGW (e.g. Qt's bundled one) than the
# one MSYS2_DIR points at.
LIBS        += $$MSYS2_DIR/lib/libglfw3.dll.a $$MSYS2_DIR/lib/libvulkan-1.dll.a

# qmake puts the built exe in OUT_PWD/release for a release build but
# OUT_PWD/debug for a debug build (e.g. Qt Creator's default Debug config) -
# the copy steps below must land in whichever one is actually active.
CONFIG(debug, debug|release) {
    DEST_SUBDIR = debug
} else {
    DEST_SUBDIR = release
}

# Copy the GLFW/Vulkan runtime DLLs and validation layer next to the built
# executable so the app runs standalone, regardless of which MinGW kit
# (MSYS2 or a Qt-bundled one) built it and without needing MSYS2's bin/ on
# the run PATH.
dlls_copy.commands = $(COPY) \"$$shell_path($$MSYS2_DIR/bin/glfw3.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/vulkan-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.json)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libwinpthread-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libstdc++-6.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\"
QMAKE_EXTRA_TARGETS += dlls_copy
POST_TARGETDEPS     += dlls_copy
