#
# test executable
#

find_package(immer REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Boost REQUIRED)
find_package(benchmark REQUIRED)
find_package(Catch2 3 REQUIRED)


add_executable(nangua_test "nangua_tests.cpp" "RowTypeTests.cpp")
target_compile_features(nangua_test PUBLIC cxx_std_20)

set(ASSETS_DIR ${ASSETS_BINARY_DIR})
cmake_path(RELATIVE_PATH ASSETS_DIR BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file(nangua_tests.hpp.in nangua_tests.hpp)

target_include_directories(nangua_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(nangua_test PRIVATE glfw)
target_link_libraries(nangua_test PRIVATE Vulkan::Vulkan)
target_link_libraries(nangua_test PRIVATE Catch2::Catch2WithMain)
target_link_libraries(nangua_test PRIVATE bainangua)

add_dependencies(nangua_test shaders)

include (CTest)
include(Catch)
catch_discover_tests(nangua_test)

#
# benchmark executable
#

add_executable(nangua_benchmark "nangua_benchmark.cpp" "RowTypeBenchmarks.cpp" "ImmerBenchmarks.cpp")
target_compile_features(nangua_test PUBLIC cxx_std_20)

set(ASSETS_DIR ${ASSETS_BINARY_DIR})
cmake_path(RELATIVE_PATH ASSETS_DIR BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file(nangua_tests.hpp.in nangua_tests.hpp)

target_include_directories(nangua_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(nangua_benchmark PUBLIC immer)
target_link_libraries(nangua_benchmark PRIVATE glfw)
target_link_libraries(nangua_benchmark PRIVATE Vulkan::Vulkan)
target_link_libraries(nangua_benchmark PRIVATE bainangua)
target_link_libraries(nangua_benchmark PRIVATE benchmark::benchmark)
target_link_libraries(nangua_benchmark INTERFACE Boost::hana)

add_dependencies(nangua_benchmark shaders)
