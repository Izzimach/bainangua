#
# test executable
#

find_package(fmt REQUIRED)
find_package(immer REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)

add_executable(nangua_test "tests.cpp" "RowTypeTests.cpp")
target_compile_features(nangua_test PUBLIC cxx_std_20)

target_include_directories(nangua_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(nangua_test PRIVATE glfw)
target_link_libraries(nangua_test PRIVATE Vulkan::Vulkan)
target_link_libraries(nangua_test PRIVATE GTest::gtest_main)
target_link_libraries(nangua_test PRIVATE bainangua)

gtest_discover_tests(nangua_test)
