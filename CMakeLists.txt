cmake_minimum_required(VERSION 3.10)
project(duckstation C CXX)

# Pull in modules.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")

# Platform detection.
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX TRUE)
  set(SUPPORTS_X11 TRUE)
endif()


# Global options.
if(NOT ANDROID)
  option(BUILD_SDL_FRONTEND "Build the SDL frontend" ON)
  option(BUILD_QT_FRONTEND "Build the Qt frontend" ON)
  option(BUILD_LIBRETRO_CORE "Build a libretro core" OFF)
  option(ENABLE_DISCORD_PRESENCE "Build with Discord Rich Presence support" ON)
  option(USE_SDL2 "Link with SDL2 for controller support" ON)
endif()


# OpenGL context creation methods.
if(SUPPORTS_X11)
  option(USE_X11 "Support X11 window system" ON)
endif()
if(LINUX OR ANDROID)
  option(USE_EGL "Support EGL OpenGL context creation" ON)
endif()

# When we're building for libretro, everything else is invalid because of PIC.
if(BUILD_LIBRETRO_CORE)
  if(BUILD_SDL_FRONTEND)
    message(WARNING "Building libretro core, disabling SDL frontend")
    set(BUILD_SDL_FRONTEND OFF)
  endif()
  if(BUILD_QT_FRONTEND)
    message(WARNING "Building libretro core, disabling Qt frontend")
    set(BUILD_QT_FRONTEND OFF)
  endif()
  if(ENABLE_DISCORD_PRESENCE)
    message("Building libretro core, disabling Discord Presence support")
    set(ENABLE_DISCORD_PRESENCE OFF)
  endif()
  if(USE_SDL2)
    message("Building libretro core, disabling SDL2 support")
    set(USE_SDL2 OFF)
  endif()
  if(USE_X11)
    set(USE_X11 OFF)
  endif()
  if(USE_EGL)
    set(USE_EGL OFF)
  endif()

  # Force PIC when compiling a libretro core.
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()


# Common include/library directories on Windows.
if(WIN32)
  set(SDL2_FOUND TRUE)
  set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/include")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/lib64/SDL2.lib")
    set(SDL2MAIN_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/lib64/SDL2main.lib")
    set(SDL2_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/bin64/SDL2.dll")
    set(Qt5_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/qt/5.14.0/msvc2017_64/lib/cmake/Qt5")
  else()
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/lib32/SDL2.lib")
    set(SDL2MAIN_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/lib32/SDL2main.lib")
    set(SDL2_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/sdl2/bin32/SDL2.dll")
    set(Qt5_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep/msvc/qt/5.14.0/msvc2017_32/lib/cmake/Qt5")
  endif()
endif()


# Required libraries.
if(NOT ANDROID)
  if(NOT WIN32 AND (BUILD_SDL_FRONTEND OR USE_SDL2))
    find_package(SDL2 REQUIRED)
  endif()
  if(BUILD_QT_FRONTEND)
    find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)
  endif()
endif()

if(USE_EGL)
  find_package(EGL REQUIRED)
endif()
if(USE_X11)
  find_package(X11 REQUIRED)
endif()

# Set _DEBUG macro for Debug builds.
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")


# Release build optimizations for MSVC.
if(MSVC)
  add_definitions("/D_CRT_SECURE_NO_WARNINGS")
  foreach(config CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    # Set warning level 3 instead of 4.
    string(REPLACE "/W3" "/W4" ${config} "${${config}}")

    # Enable intrinsic functions, disable minimal rebuild.
    set(${config} "${${config}} /Oi /Gm-")
  endforeach()

  # RelWithDebInfo is set to Ob1 instead of Ob2.   
  string(REPLACE "/Ob1" "/Ob2" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

  # Disable incremental linking in RelWithDebInfo.
  string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")

  # COMDAT folding/remove unused functions.
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /OPT:ICF")
endif()


# Detect C++ version support.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(CheckCXXFlag)
    check_cxx_flag(-Wall COMPILER_SUPPORTS_WALL)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch")
    if(NOT ANDROID)
      check_cxx_flag(-Wno-class-memaccess COMPILER_SUPPORTS_MEMACCESS)
      check_cxx_flag(-Wno-invalid-offsetof COMPILER_SUPPORTS_OFFSETOF)
    endif()
endif()


# Detect processor type.
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
  set(CPU_ARCH "x64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  # MSVC x86/x64
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CPU_ARCH "x64")
  else()
    set(CPU_ARCH "x86")
  endif()
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386" OR
       ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
  set(CPU_ARCH "x86")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(CPU_ARCH "aarch64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7-a")
  set(CPU_ARCH "arm")
else()
  message(FATAL_ERROR "Unknown system processor: " ${CMAKE_SYSTEM_PROCESSOR})
endif()


# Write binaries to a seperate directory.
if(WIN32)
  # For Windows, use the source directory.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CPU_ARCH}")
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()


# Enable threads everywhere.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


# Recursively include the source tree.
enable_testing()
add_subdirectory(dep)
add_subdirectory(src)

if(ANDROID)
  add_subdirectory(android/app/src/cpp)
endif()
