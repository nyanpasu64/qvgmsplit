cmake_minimum_required(VERSION 3.5)

project(qvgmsplit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    # rtaudio_c.cpp uses strncpy and the like. I don't actually use that file lol
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    if (MSVC)
        add_compile_options(/EHsc)
        add_compile_options(/permissive-)
    endif ()
endif ()

if (MSVC)
    list(APPEND options-3rdparty "/W3")
endif ()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND options
        -Wall -Wextra -Wconversion -Wsign-conversion -Wtype-limits
        -Wmissing-declarations
        -Wno-unused-function -Wno-unused-parameter
        -Werror=return-type
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND options
            -Wmissing-variable-declarations
        )
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND options
        /Zc:preprocessor
        /W3
        # I want to use /W4, but I get *far* too many false-positive C4244 warnings
        # when passing integer literals into functions.
        # And /w34244 doesn't help; it means "enable C4244 at /W3",
        # not "enable the /W3 version of C4244".
        /wd4100  # allow unused parameters
        /wd4505  # allow unused functions
        /wd4456 /wd4457  # allow shadowing
    )
endif ()

# Strip bloat from release Windows MinGW builds.
if (
    WIN32
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_BUILD_TYPE STREQUAL "Release"
)
    add_link_options(-Wl,--strip-all -Wl,--insert-timestamp -Wl,--gc-sections)
endif()


## Add dependencies

include_directories(src)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
set(Qt Qt${QT_VERSION_MAJOR})
find_package(${Qt} COMPONENTS Widgets REQUIRED)

add_subdirectory("3rdparty/fmt")
add_subdirectory("3rdparty/GSL")

# vgm-audio fails to build with Clang on Windows, using either mingw or vc headers.
# So skip building it since we don't use it.
set(BUILD_LIBAUDIO FALSE CACHE BOOL "")
set(BUILD_TESTS FALSE CACHE BOOL "")
set(BUILD_PLAYER FALSE CACHE BOOL "")
set(BUILD_VGM2WAV FALSE CACHE BOOL "")
set(USE_SANITIZERS FALSE CACHE BOOL "")
if (WIN32)
    set(UTIL_CHARCNV_ICONV OFF CACHE BOOL "")
    set(UTIL_CHARCNV_WINAPI ON CACHE BOOL "")
endif ()
add_subdirectory("3rdparty/libvgm")

add_subdirectory("3rdparty/STX")

## Application

set(PROJECT_SOURCES
    src/lib/box_array.h
    src/lib/copy_move.h
    src/lib/defer.h
    src/lib/enumerate.h
    src/lib/format.cpp
    src/lib/format.h
    src/lib/gtr.h
    src/lib/hv_line.h
    src/lib/layout_macros.h
    src/lib/release_assert.h
    src/lib/trace.h
    src/lib/unwrap.h

    src/backend.cpp
    src/backend.h
    src/gui_app.cpp
    src/gui_app.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/options_dialog.cpp
    src/options_dialog.h
    src/render_dialog.cpp
    src/render_dialog.h
    src/settings.cpp
    src/settings.h
    src/vgm.cpp
    src/vgm.h
    src/wave_writer.cpp
    src/wave_writer.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(qvgmsplit
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    add_executable(qvgmsplit
        ${PROJECT_SOURCES}
    )
endif()

target_compile_options(qvgmsplit PRIVATE "${options}")
target_link_libraries(qvgmsplit PRIVATE
    ${Qt}::Widgets
    vgm-emu vgm-player vgm-utils
    fmt GSL stx
)

set_target_properties(qvgmsplit PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER qvgmsplit.nyanpasu64.example
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

# Generate .pdb files for release MSVC builds (https://stackoverflow.com/a/57479289)
# The majority of end-user crashes happen in release builds,
# and debugging their memory dumps is difficult without symbols.
# So generate symbols to aid debugging.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")

    # Tell linker to include symbol data
    set_target_properties(qvgmsplit PROPERTIES
        LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    )

    # Set file name & location
    set_target_properties(qvgmsplit PROPERTIES
        COMPILE_PDB_NAME qvgmsplit
        COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    )
endif()

# Generate .pdb files for release Windows Clang builds.
# If you're using MinGW Clang, you should remove this line to reduce binary size.
if(
    WIN32
    AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    AND CMAKE_BUILD_TYPE STREQUAL "Release"
)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qvgmsplit)
endif()
