cmake_minimum_required(VERSION 3.10)

project(MetaCallPoC VERSION 1.0)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Debug
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    if(WIN32)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address)
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=address)
        add_link_options(-fsanitize=undefined)
    endif()
endif()

# Test
include(CTest)

# Windows exports
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# PLTHook
include(FetchContent)

set(PLTHook_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/plthook")

FetchContent_Declare(PLTHook
    GIT_REPOSITORY https://github.com/metacall/plthook.git
    GIT_TAG master
    SOURCE_DIR ${PLTHook_SOURCE_DIR}
)

FetchContent_MakeAvailable(PLTHook)

set(PLTHook_INCLUDE_DIR "${PLTHook_SOURCE_DIR}")

if(APPLE)
    set(PLTHOOK_SOURCE "${PLTHook_SOURCE_DIR}/plthook_osx.c")
elseif(WIN32 OR MINGW)
    set(PLTHOOK_SOURCE "${PLTHook_SOURCE_DIR}/plthook_win32.c")
else()
    set(PLTHOOK_SOURCE "${PLTHook_SOURCE_DIR}/plthook_elf.c")
endif()

# MetaCall Library
add_library(metacall SHARED source/metacall.c ${PLTHOOK_SOURCE})
target_include_directories(metacall PRIVATE ${PLTHook_INCLUDE_DIR})
target_link_libraries(metacall ${CMAKE_DL_LIBS})

# NodeJS library of NodeJS static
add_library(libnode-static source/libnode.c)
target_compile_definitions(libnode-static PUBLIC NODE_STATIC=1) # Add definition for compiling as "node-static"

# NodeJS static
add_executable(node-static source/node-static.c)
target_link_libraries(node-static libnode-static ${CMAKE_DL_LIBS} metacall)
set_property(TARGET node-static PROPERTY ENABLE_EXPORTS ON) # Required to export the symbols on the executable
add_test(NAME node-static
    COMMAND $<TARGET_FILE:node-static>
)

# NodeJS library of NodeJS dynamic
add_library(libnode SHARED source/libnode.c)

# NodeJS dynamic
add_executable(node-dynamic source/node-dynamic.c)
target_link_libraries(node-dynamic ${CMAKE_DL_LIBS} metacall)
add_test(NAME node-dynamic
    COMMAND $<TARGET_FILE:node-dynamic>
)

# Normal Executable
add_executable(normal-executable source/normal-executable.c)
target_link_libraries(normal-executable ${CMAKE_DL_LIBS} metacall)
add_test(NAME normal-executable
    COMMAND $<TARGET_FILE:normal-executable>
)
set_property(TEST normal-executable PROPERTY ENVIRONMENT "NORMAL_EXECUTABLE=1")

# NodeJS Loader is linked weakly to libnode2
add_library(node_loader SHARED source/node_loader.c)
target_link_libraries(node_loader
    PRIVATE

    # Delay load for MSVC
    $<$<CXX_COMPILER_ID:MSVC>:libnode2>
    $<$<CXX_COMPILER_ID:MSVC>:delayimp>
)
target_link_options(node_loader
    PRIVATE
    # TODO: Add flags for gcc?
    $<$<AND:$<BOOL:${APPLE}>,$<CXX_COMPILER_ID:AppleClang,Clang>>:-Wl,-undefined,dynamic_lookup>
    $<$<CXX_COMPILER_ID:MSVC>:/DELAYLOAD:$<TARGET_FILE_BASE_NAME:libnode2>.dll>
)

# NodeJS library of NodeJS Loader
add_library(libnode2 SHARED source/libnode2.c source/libnode2.cpp)

# Additional dlinfo test (unrelated to the PoC)
if(NOT WIN32 AND NOT APPLE)
    add_executable(dlinfo-test source/dlinfo-test.c)
    target_link_libraries(dlinfo-test ${CMAKE_DL_LIBS})
endif()
