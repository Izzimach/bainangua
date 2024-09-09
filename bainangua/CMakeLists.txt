# CMakeList.txt : CMake project for GLFW_X, include source and define
# project specific logic here.

find_package(fmt REQUIRED)
find_package(immer REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS hana)
find_package(boost_asio CONFIG REQUIRED)


#
# basic library, used by the main executable and test executable
#

add_library(bainangua STATIC "OuterBoilerplate.cpp" "PresentationLayer.cpp" "Pipeline.cpp")
target_compile_features(bainangua PUBLIC cxx_std_20)

target_include_directories(bainangua PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(bainangua PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(bainangua PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(bainangua PUBLIC glfw)
target_link_libraries(bainangua PUBLIC immer)
target_link_libraries(bainangua PUBLIC Vulkan::Vulkan)
target_link_libraries(bainangua PUBLIC fmt::fmt)
target_link_libraries(bainangua INTERFACE Boost::hana Boost::asio)


#
# experimental stuff
#

add_library(experimental_lib "RowType.cpp")


target_compile_features(experimental_lib PUBLIC cxx_std_20)

target_include_directories(experimental_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(experimental_lib PUBLIC glfw)
target_link_libraries(experimental_lib PUBLIC immer)
target_link_libraries(experimental_lib PUBLIC Vulkan::Vulkan)
target_link_libraries(experimental_lib PUBLIC fmt::fmt)
target_link_libraries(experimental_lib INTERFACE Boost::hana Boost::asio)

#
# shader compilation
#

add_custom_target(shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders)

set(SHADER_FILES
    shaders/Basic.vert
    shaders/Basic.frag
    )

foreach (FILE ${SHADER_FILES})
    add_custom_command(
        TARGET shaders
        COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/$<PATH:REPLACE_EXTENSION,${FILE},$<JOIN:$<LIST:APPEND,$<PATH:GET_EXTENSION,${FILE}>,spv>,_>>
        COMMENT "Compiling HLSL ${FILE}"
        VERBATIM)
endforeach (FILE)

#
# main executable
#

add_executable (white_pumpkin "white_pumpkin.cpp")
target_compile_features(white_pumpkin PUBLIC cxx_std_20)

target_include_directories(white_pumpkin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(bainangua PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(bainangua PRIVATE -Wall -Wextra -Wpedantic)
endif()


target_link_libraries(white_pumpkin PRIVATE Vulkan::Vulkan)
target_link_libraries(white_pumpkin PRIVATE bainangua)

add_dependencies(white_pumpkin shaders)

