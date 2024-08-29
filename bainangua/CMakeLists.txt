# CMakeList.txt : CMake project for GLFW_X, include source and define
# project specific logic here.

find_package(fmt REQUIRED)
find_package(immer CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)

#
# basic library, used by the main executable and test executable
#

add_library(bainangua STATIC "OuterBoilerplate.ixx" "PresentationLayer.ixx")
target_compile_features(bainangua PUBLIC cxx_std_20)

target_link_libraries(bainangua PRIVATE glfw)
#target_link_libraries(bainangua PRIVATE immer)
target_link_libraries(bainangua PRIVATE Vulkan::Vulkan)
target_link_libraries(bainangua PRIVATE fmt::fmt)
target_link_libraries(bainangua PRIVATE Boost::boost)

#
# main executable
#

add_executable (white_pumpkin "white_pumpkin.cpp")
target_compile_features(white_pumpkin PUBLIC cxx_std_20)

target_include_directories(white_pumpkin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(white_pumpkin PRIVATE Vulkan::Vulkan)
target_link_libraries(white_pumpkin PRIVATE bainangua)

