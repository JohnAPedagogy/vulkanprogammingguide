QT       =
CONFIG   += c++17
TARGET    = 11_02_semaphore
TEMPLATE  = app

DEFINES    += ENABLE_VALIDATION

SOURCES  += \
    ../../ch02/0202_allocator.cpp \
    11_02_semaphore.cpp \
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

LIBS        += $$MSYS2_DIR/lib/libglfw3.dll.a $$MSYS2_DIR/lib/libvulkan-1.dll.a

CONFIG(debug, debug|release) {
    DEST_SUBDIR = debug
} else {
    DEST_SUBDIR = release
}

dlls_copy.commands = $(COPY) \"$$shell_path($$MSYS2_DIR/bin/glfw3.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/vulkan-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.json)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libwinpthread-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libstdc++-6.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\"
QMAKE_EXTRA_TARGETS += dlls_copy
POST_TARGETDEPS     += dlls_copy
