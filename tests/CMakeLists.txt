#
# test executable
#

find_package(immer REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Boost REQUIRED)
find_package(Catch2 3 REQUIRED)


add_executable(nangua_test "nangua_tests.cpp" "RowTypeTests.cpp" "resources/resourceloader_tests.cpp" "resources/shader_tests.cpp" "resources/commandbuffer_tests.cpp" "resources/stagingbuffer_tests.cpp" "resources/commandqueue_tests.cpp")
target_compile_features(nangua_test PUBLIC cxx_std_20)

set(ASSETS_DIR ${ASSETS_BINARY_DIR})
cmake_path(RELATIVE_PATH ASSETS_DIR BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file(nangua_tests.hpp.in nangua_tests.hpp)

target_include_directories(nangua_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(nangua_test PRIVATE /W4)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(nangua_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(nangua_test PRIVATE glfw)
target_link_libraries(nangua_test PRIVATE Vulkan::Vulkan)
target_link_libraries(nangua_test PRIVATE Catch2::Catch2WithMain)
target_link_libraries(nangua_test PRIVATE bainangua)

add_dependencies(nangua_test shaders)

include (CTest)
include(Catch)
catch_discover_tests(nangua_test)

