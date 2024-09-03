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

add_library(bainangua STATIC "OuterBoilerplate.ixx" "PresentationLayer.ixx")
target_compile_features(bainangua PUBLIC cxx_std_20)

target_include_directories(bainangua INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(bainangua PUBLIC glfw)
target_link_libraries(bainangua PUBLIC immer)
target_link_libraries(bainangua PUBLIC Vulkan::Vulkan)
target_link_libraries(bainangua PUBLIC fmt::fmt)
target_link_libraries(bainangua INTERFACE Boost::hana Boost::asio)


#
# experimental stuff
#

add_library(experimental_lib "RowType.ixx")

target_compile_features(experimental_lib PUBLIC cxx_std_20)

target_include_directories(experimental_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(experimental_lib PUBLIC glfw)
target_link_libraries(experimental_lib PUBLIC immer)
target_link_libraries(experimental_lib PUBLIC Vulkan::Vulkan)
target_link_libraries(experimental_lib PUBLIC fmt::fmt)
target_link_libraries(experimental_lib INTERFACE Boost::hana Boost::asio)


#
# main executable
#

add_executable (white_pumpkin "white_pumpkin.cpp")
target_compile_features(white_pumpkin PUBLIC cxx_std_20)

target_include_directories(white_pumpkin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(white_pumpkin PRIVATE Vulkan::Vulkan)
target_link_libraries(white_pumpkin PRIVATE bainangua)


