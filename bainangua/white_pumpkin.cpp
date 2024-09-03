
// 白南瓜 test application main entry point

#include "bainangua.hpp"

#include <algorithm>
#include <vector>


#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/string.hpp>

#include <fmt/format.h>

import OuterBoilerplate;
import PresentationLayer;
import RowType;

using namespace std;
using namespace RowType;

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
