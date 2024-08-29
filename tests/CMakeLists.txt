
#
# test executable
#

add_executable(GLFW_test "tests/tests.cpp")

target_compile_features(GLFW_test PUBLIC cxx_std_20)

target_include_directories(GLFW_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(GLFW_test PRIVATE glfw)
target_link_libraries(GLFW_test PRIVATE Vulkan::Vulkan)
target_link_libraries(GLFW_test PRIVATE GTest::gtest_main)
target_link_libraries(GLFW_test PRIVATE glfw_wrappers)

gtest_discover_tests(GLFW_test)
