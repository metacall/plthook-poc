cmake_minimum_required(VERSION 3.10)

project(MetaCallPoC VERSION 1.0)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# TODO: -O3 generates a segmentation fault on Linux
#
# METACALL load from normal executable
# should_not_call_init constructor has been called and it should not
# NORMAL EXECUTABLE enum symbols node_loader (1): fflush -> 0x4864018
# ...
# NORMAL EXECUTABLE enum symbols node_loader (11): __cxa_finalize -> 0x4863ff8
# INSIDE NODE LOADER (before string_function call): (nil)
# ==12635== Jump to the invalid address stated on the next line
# ==12635==    at 0x0: ???
# ==12635==    by 0x4861188: node_loader (in /metacall-plthook-poc/build/libnode_loader.so)
# ==12635==    by 0x4856638: load_normal_executable (in /metacall-plthook-poc/build/libmetacall.so)
# ==12635==    by 0x4896564: (below main) (libc-start.c:332)
# ==12635==  Address 0x0 is not stack'd, malloc'd or (recently) free'd
# ==12635==
# ==12635== Process terminating with default action of signal 11 (SIGSEGV)
# ==12635==  Bad permissions for mapped region at address 0x0
# ==12635==    at 0x0: ???
# ==12635==    by 0x4861188: node_loader (in /metacall-plthook-poc/build/libnode_loader.so)
# ==12635==    by 0x4856638: load_normal_executable (in /metacall-plthook-poc/build/libmetacall.so)
# ==12635==    by 0x4896564: (below main) (libc-start.c:332)
string(REGEX MATCH "Linux" PROJECT_OS_LINUX ${CMAKE_SYSTEM_NAME})

if(PROJECT_OS_LINUX)
    # -O3 not supported, use -O2 by now until we can find why string_function is NULL
    set(CMAKE_C_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
elseif(WIN32)
    # /O2 and /O2b not supported, use /O1 /Ob1 by now until we
    # can find why string_function is an non-NULL invalid pointer
    set(CMAKE_C_FLAGS_RELEASE "/O1")
    set(CMAKE_CXX_FLAGS_RELEASE "/O1")
endif()

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
