
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "OuterBoilerplate.hpp"
#include "Pipeline.hpp"
#include "PresentationLayer.hpp"
#include "tanuki.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <vector>


using namespace std;
using namespace bainangua;


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
			.innerCode = [](OuterBoilerplateState& s) -> bool {
				PresentationLayer presenter;
				presenter.build(s);

				PipelineBundle pipeline(createPipeline(presenter, "shaders/Basic.frag_spv", "shaders/Basic.vert_spv"));

				s.endOfFrame();

				destroyPipeline(presenter, pipeline);

				presenter.teardown();

				return true;
			}
		}
	);


	return 0;
}
