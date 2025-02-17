﻿#
# Root CMAKE file for bainangua
#

cmake_minimum_required (VERSION 3.28)

# Enable Hot Reload for MSVC compilers if supported.
# DISABLED so we can use address sanitizer
#if (POLICY CMP0141)
#  cmake_policy(SET CMP0141 NEW)
#  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
#endif()

# for boost - define windows version as windows 10
if (WIN32)
  add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

#
# main project and subdirectories

project ("bainangua")

# Assets such as shaders and textures are put into an "assets/" directory in the build directory.

set(ASSETS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets)


# library and example app

add_subdirectory ("bainangua")


# tests

enable_testing()
add_subdirectory("tests")
