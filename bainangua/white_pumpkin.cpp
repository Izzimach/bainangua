//
// GLFW_X.cpp : Defines the entry point for the application.
//

#include "bainangua.hpp"

#include <algorithm>
#include <vector>

import OuterBoilerplate;
import PresentationLayer;

using namespace std;

int main()
{
	outerBoilerplate(
		OuterBoilerplateConfig{
			.AppName = "My Test App",
			.requiredExtensions = {
				VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			},
#if NDEBUG
			.useValidation = false,
#else
			.useValidation = true,
#endif
			.innerCode = [](OuterBoilerplateState& s) {
				return false;
			}
		}
	);

	return 0;
}
