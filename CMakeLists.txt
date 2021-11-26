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
    add_link_options(-Wl,--strip-all -Wl,--insert-timestamp -Wl,--gc-sections -Wl,--discard-all)
endif()


## Add dependencies

include_directories(src)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
set(Qt Qt${QT_VERSION_MAJOR})
find_package(${Qt} COMPONENTS Widgets REQUIRED)

add_subdirectory("3rdparty/GSL")

set(BUILD_TESTS FALSE CACHE BOOL "")
set(BUILD_PLAYER FALSE CACHE BOOL "")
set(BUILD_VGM2WAV FALSE CACHE BOOL "")
set(USE_SANITIZERS FALSE CACHE BOOL "")
add_subdirectory("3rdparty/libvgm")

add_subdirectory("3rdparty/STX")

## Application

set(PROJECT_SOURCES
    src/lib/box_array.h
    src/lib/copy_move.h
    src/lib/format.cpp
    src/lib/format.h
    src/lib/gtr.h
    src/lib/hv_line.h
    src/lib/layout_macros.h
    src/lib/trace.h

    src/backend.cpp
    src/backend.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/vgm.cpp
    src/vgm.h
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
    GSL
    vgm-audio vgm-player vgm-utils
    stx
)

set_target_properties(qvgmsplit PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER qvgmsplit.nyanpasu64.example
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qvgmsplit)
endif()
