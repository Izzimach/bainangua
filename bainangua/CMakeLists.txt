﻿# CMake file for make bainangua library and experimental executable.

# cmake packages - usually via vcpkg

find_package(immer REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(Boost REQUIRED COMPONENTS hana container_hash)
find_package(glm REQUIRED)

# vcpkg version of gtl doesn't use the normal CMake packaging...
find_path(GTL_INCLUDE_DIRS "gtl/adv_utils.hpp")


# some dependencies are acquired via FetchContent

include(FetchContent)

FetchContent_Declare(
  qlibsreflect
  GIT_REPOSITORY https://github.com/qlibs/reflect
  GIT_TAG v1.2.3
)
FetchContent_MakeAvailable(qlibsreflect)

FetchContent_Declare(
    libcoro
    GIT_REPOSITORY https://github.com/jbaldwin/libcoro.git
    GIT_TAG        v0.12.1
)
FetchContent_MakeAvailable(libcoro)


#
# basic library, used by the main executable and test executable
#

add_library(bainangua)
target_compile_features(bainangua PUBLIC cxx_std_20)

target_sources(bainangua PUBLIC "ReifyHeaders.cpp")

target_sources(bainangua
    PUBLIC
    FILE_SET bainangua_modules 
    TYPE CXX_MODULES 
    FILES "OneFrame.cppm" "VulkanContext.cppm" "PresentationLayer.cppm" "Pipeline.cppm" "Commands.cppm"
          "VertBuffer.cppm" "UniformBuffer.cppm" "DescriptorSets.cppm" "TextureImage.cppm"
          "resources/ResourceLoader.cppm" "resources/Shader.cppm" "resources/CommandQueue.cppm" "resources/StagingBuffer.cppm" "resources/VertexBuffer.cppm"
          "resources/PerFramePool.cppm" "resources/Buffers.cppm")


target_include_directories(bainangua PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_include_directories(bainangua PRIVATE
    ${qlibsreflect_SOURCE_DIR}
    ${GTL_INCLUDE_DIRS}
    )

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(bainangua PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(bainangua PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

target_link_libraries(bainangua PUBLIC glfw)
target_link_libraries(bainangua PUBLIC immer)
target_link_libraries(bainangua PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
target_link_libraries(bainangua INTERFACE Boost::hana)
target_link_libraries(bainangua PUBLIC libcoro)

#
# shader compilation - shaders get compiled and put into assets/shaders in the build tree
#

set(SHADER_FILES
    Basic.vert
    Basic.frag
    PosColor.vert
    PosColor.frag
    PosColorMVP.vert
    TexturedMVP.vert
    Textured.frag
    )

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${ASSETS_BINARY_DIR}/shaders")

add_custom_target(shaders ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR})

foreach (FILE ${SHADER_FILES})
    add_custom_command(
        TARGET shaders POST_BUILD
        COMMAND glslc ${SHADER_SOURCE_DIR}/${FILE} -o ${SHADER_BINARY_DIR}/$<PATH:REPLACE_EXTENSION,${FILE},$<JOIN:$<LIST:APPEND,$<PATH:GET_EXTENSION,${FILE}>,spv>,_>>
        COMMENT "Compiling HLSL ${FILE}"
        VERBATIM)
endforeach (FILE)

#
# texture copy
#

set (TEXTURE_FILES
     default.jpg)

set(TEXTURE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/textures")
set(TEXTURE_BINARY_DIR "${ASSETS_BINARY_DIR}/textures")

add_custom_target(textures ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${TEXTURE_BINARY_DIR})

foreach (FILE ${TEXTURE_FILES})
    add_custom_command(
        TARGET textures POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${TEXTURE_SOURCE_DIR}/${FILE}
                ${TEXTURE_BINARY_DIR}/${FILE}
    )
endforeach (FILE)

#
# experimental executable
#

add_executable (white_pumpkin "white_pumpkin.cpp")
target_compile_features(white_pumpkin PUBLIC cxx_std_20)

# No console window for non-debug builds on Windows
# from https://stackoverflow.com/questions/2752792/whats-the-equivalent-of-gccs-mwindows-option-in-cmake
#
set_target_properties(white_pumpkin PROPERTIES WIN32_EXECUTABLE
  $<AND:$<PLATFORM_ID:Windows>,$<NOT:$<CONFIG:Debug,RelWithDebInfo>>>
)

# figure out the ASSET_DIR and put it into the white_pumpkin.hpp file for use by C++ code
set(ASSETS_DIR ${ASSETS_BINARY_DIR})
cmake_path(RELATIVE_PATH ASSETS_DIR BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(white_pumpkin.hpp.in white_pumpkin.hpp)

target_include_directories(white_pumpkin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}   # the generated white_pumpkin.hpp gets put in the BUILD directory, so this makes sure we can find it
    ${qlibsreflect_SOURCE_DIR}
    ${GTL_INCLUDE_DIRS}
    )

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(white_pumpkin PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(white_pumpkin PRIVATE -Wall -Wextra -Wpedantic)
endif()


target_link_libraries(white_pumpkin PRIVATE Vulkan::Vulkan)
target_link_libraries(white_pumpkin PRIVATE bainangua)

add_dependencies(white_pumpkin shaders)
add_dependencies(white_pumpkin textures)
