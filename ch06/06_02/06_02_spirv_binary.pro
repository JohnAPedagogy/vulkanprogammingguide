QT       =
CONFIG   += c++17
TARGET    = 06_02_spirv_binary
TEMPLATE  = app

DEFINES    += ENABLE_VALIDATION

SOURCES  += \
    ../../ch02/0202_allocator.cpp \
    06_02_spirv_binary.cpp \
    my_vulkan.cpp

HEADERS += \
    ../../ch02/0201_allocator.h \
    my_vulkan.h

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

GLSLC = $$MSYS2_DIR/bin/glslc.exe
shaders.input = SHADER_SOURCES
shaders.output = ${QMAKE_FILE_IN}.spv
shaders.commands = $$GLSLC ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
shaders.CONFIG += target_predeps no_link
SHADER_SOURCES += shaders/triangle.vert
QMAKE_EXTRA_COMPILERS += shaders

shaders_copy.commands = $(COPY_DIR) \"$$shell_path($$PWD/shaders)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR/shaders)\"
QMAKE_EXTRA_TARGETS += shaders_copy
POST_TARGETDEPS     += shaders_copy

dlls_copy.commands = $(COPY) \"$$shell_path($$MSYS2_DIR/bin/glfw3.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/vulkan-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/VkLayer_khronos_validation.json)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libwinpthread-1.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\" $$escape_expand(\\n\\t) \
                     $(COPY) \"$$shell_path($$MSYS2_DIR/bin/libstdc++-6.dll)\" \"$$shell_path($$OUT_PWD/$$DEST_SUBDIR)\"
QMAKE_EXTRA_TARGETS += dlls_copy
POST_TARGETDEPS     += dlls_copy
