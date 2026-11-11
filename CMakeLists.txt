cmake_minimum_required(VERSION 3.24)
project(ets2_161_cabin_sound LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "eurotrucks2.exe is x64: use an x64 toolchain (vcvars64, or cmake/mingw-w64-x86_64.cmake)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # no VC++ redistributable needed

# MinHook: third_party/minhook if present, otherwise downloaded at a pinned commit (v1.3.4).
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/minhook/CMakeLists.txt)
  add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/minhook ${CMAKE_BINARY_DIR}/minhook EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        c3fcafdc10146beb5919319d0683e44e3c30d537)  # v1.3.4
  FetchContent_MakeAvailable(minhook)
endif()

add_library(ets2_161_cabin_sound SHARED src/plugin.cpp src/mem.cpp)
target_include_directories(ets2_161_cabin_sound PRIVATE src)
target_link_libraries(ets2_161_cabin_sound PRIVATE minhook)
target_compile_definitions(ets2_161_cabin_sound PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
  target_compile_options(ets2_161_cabin_sound PRIVATE /W4 /permissive- /utf-8)
  target_link_options(ets2_161_cabin_sound PRIVATE /OPT:REF /OPT:ICF)
else()  # MinGW-w64: link the runtime statically so the game needs no MinGW DLLs
  target_compile_options(ets2_161_cabin_sound PRIVATE -Wall -Wextra -Wno-cast-function-type)
  target_link_options(ets2_161_cabin_sound PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -s)
  set_target_properties(ets2_161_cabin_sound PROPERTIES PREFIX "")
endif()

set_target_properties(ets2_161_cabin_sound PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/build/bin
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/build/bin
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/build/bin)
