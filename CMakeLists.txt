cmake_minimum_required(VERSION 3.10)

project(MetaCallPoC VERSION 1.0)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Required for the IDE
set(CMAKE_VERBOSE_MAKEFILE ON) # Debugging

string(REGEX MATCH "Linux" PROJECT_OS_LINUX ${CMAKE_SYSTEM_NAME})

if(MINGW OR CYGWIN)
    message("Building under MingW")
    set(PROJECT_OS_MINGW 1)
else()
    set(PROJECT_OS_MINGW 0)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND PROJECT_OS_MINGW)
    message("Building under Clang MSYS2")
    set(PROJECT_OS_CLANG_MSYS2 1)
    set(PROJECT_OS_MINGW 0)
else()
    set(PROJECT_OS_CLANG_MSYS2 0)
endif()

if(PROJECT_OS_MINGW)
    # Find gendef (required for delay load mechanism)
    find_program(GENDEF_EXECUTABLE gendef)

    if(NOT GENDEF_EXECUTABLE)
        message(FATAL_ERROR "Program gendef not found. Please ensure MinGW or Cygwin is installed and dlltool is available.")
    endif()

    # Find dlltool (required for delay load mechanism)
    find_program(DLLTOOL_EXECUTABLE dlltool)

    if(NOT DLLTOOL_EXECUTABLE)
        message(FATAL_ERROR "Program dlltool not found. Please ensure MinGW or Cygwin is installed and dlltool is available.")
    endif()
endif()

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

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
if(PROJECT_OS_LINUX)
    # -O3 not supported, use -O2 by now until we can find why string_function is NULL
    set(CMAKE_C_FLAGS_RELEASE "-O2")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
elseif(MSVC)
    # /O2 and /O2b not supported, use /O1 /Ob1 by now until we
    # can find why string_function is an non-NULL invalid pointer
    set(CMAKE_C_FLAGS_RELEASE "/O1")
    set(CMAKE_CXX_FLAGS_RELEASE "/O1")
endif()

# Debug
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    if(MSVC)
        # TODO: Not working, seems not been able to locate sanitizer library on ctest
        # add_compile_options(/fsanitize=address)
    elseif(PROJECT_OS_LINUX OR APPLE OR PROJECT_OS_CLANG_MSYS2)
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
    set(PLTHook_SOURCE "${PLTHook_SOURCE_DIR}/plthook_osx.c")
elseif(WIN32 OR MINGW)
    set(PLTHook_SOURCE "${PLTHook_SOURCE_DIR}/plthook_win32.c")
else()
    set(PLTHook_SOURCE "${PLTHook_SOURCE_DIR}/plthook_elf.c")
endif()

# MetaCall Library
add_library(metacall SHARED source/metacall.c ${PLTHook_SOURCE})
target_include_directories(metacall PRIVATE ${PLTHook_INCLUDE_DIR})
# target_compile_definitions(metacall PUBLIC PLTHOOK_DEBUG=1) # Add debug info for PLTHook
target_link_libraries(metacall
    PRIVATE
    # Dynamic Load libraries
    ${CMAKE_DL_LIBS}

    # MinGW required libraries for PLTHook
    $<$<BOOL:${PROJECT_OS_MINGW}>:dbghelp>
)

# NodeJS library of NodeJS static
add_library(libnode-static source/libnode.c)
target_compile_definitions(libnode-static PUBLIC NODE_STATIC=1) # Add definition for compiling as "node-static"

# NodeJS static
add_executable(node-static source/node-static.c)
target_link_libraries(node-static
    PRIVATE
    libnode-static
    ${CMAKE_DL_LIBS}
    metacall
)
set_property(TARGET node-static PROPERTY ENABLE_EXPORTS ON) # Required to export the symbols on the executable
add_test(NAME node-static
    COMMAND $<TARGET_FILE:node-static>
)

# NodeJS library of NodeJS dynamic
add_library(libnode SHARED source/libnode.c)

# NodeJS dynamic
add_executable(node-dynamic source/node-dynamic.c)
target_link_libraries(node-dynamic
    PRIVATE
    ${CMAKE_DL_LIBS}
    metacall
)
add_test(NAME node-dynamic
    COMMAND $<TARGET_FILE:node-dynamic>
)

# NodeJS library of NodeJS Loader
add_library(libnode2 SHARED source/libnode2.c source/libnode2.cpp)

if(PROJECT_OS_MINGW)
    set(libnode2_delay ${CMAKE_BINARY_DIR}/liblibnode2-delay.dll.a)
    set(libnode2_def ${CMAKE_BINARY_DIR}/liblibnode2.def)

    # Generate the delay loaded library
    add_custom_command(
        OUTPUT ${libnode2_delay}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${GENDEF_EXECUTABLE} $<TARGET_FILE:libnode2>
        COMMAND ${DLLTOOL_EXECUTABLE} --input-def ${libnode2_def} --output-delaylib ${libnode2_delay} --dllname $<TARGET_FILE:libnode2>
        DEPENDS libnode2
        COMMENT "Generating libnode2 delayed loaded DLL"
    )

    add_custom_target(libnode2-delay
        DEPENDS ${libnode2_delay}
    )
endif()

# Normal Executable
add_executable(normal-executable source/normal-executable.c)
target_link_libraries(normal-executable
    PRIVATE
    ${CMAKE_DL_LIBS}
    metacall
)
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

    # Delay load for MSVC & Clang on MSYS2
    $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${PROJECT_OS_CLANG_MSYS2}>>:delayimp>

    # Delay load for MingW
    $<$<BOOL:${PROJECT_OS_MINGW}>:libnode2-delay>
)
target_link_options(node_loader
    PRIVATE
    # TODO: Add flags for gcc?

    # Delay load for Clang on MacOS
    $<$<AND:$<BOOL:${APPLE}>,$<CXX_COMPILER_ID:AppleClang,Clang>>:-Wl,-undefined,dynamic_lookup>

    # Delay load for MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/DELAYLOAD:$<TARGET_FILE_BASE_NAME:libnode2>.dll>

    # Delay load for Clang on MSYS2
    $<$<BOOL:${PROJECT_OS_CLANG_MSYS2}>:-delayload:$<TARGET_FILE_BASE_NAME:libnode2>.dll>
)

if(PROJECT_OS_MINGW)
    # Make node_loader depend on delayed libnode2
    add_dependencies(node_loader libnode2-delay)
endif()

# List libraries of a process
add_executable(list-library-test source/list-library-test.cpp)
target_link_libraries(list-library-test
    PRIVATE
    ${CMAKE_DL_LIBS}
    libnode

    # MinGW required libraries
    $<$<BOOL:${PROJECT_OS_MINGW}>:psapi>
)
add_test(NAME list-library-test
    COMMAND $<TARGET_FILE:list-library-test>
)
