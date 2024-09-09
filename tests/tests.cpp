
#include "gtest/gtest.h"
#include "include/bainangua.hpp"
#include "include/OuterBoilerplate.hpp"

using namespace bainangua;

namespace {

TEST(Boilerplate, BasicTest)
{
	EXPECT_NO_THROW(
		outerBoilerplate(
			OuterBoilerplateConfig{
				.AppName = "My Test App",
				.requiredExtensions = {
						VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
				.useValidation = false,
				.innerCode = [](auto s) { return false; }
			}
		)
	);
}

TEST(FactorialTest2, Negative)
{
	EXPECT_EQ(1, 1);
	ASSERT_EQ(2, 2);
}

}