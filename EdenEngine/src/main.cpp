#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Core/Assertions.h"
#include "Core/Application.h"
#include "Core/Camera.h"
#include "Profiling/Profiler.h"
#include "Graphics/D3D12/D3D12RHI.h"
#include "Graphics/Skybox.h"
#include "Scene/MeshSource.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"
#include "Math/Math.h"
#include "Utilities/Utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>

using namespace Eden;

class EdenApplication : public Application
{
public:
	EdenApplication() = default;

	void OnInit() override
	{
	}

	void OnUpdate() override
	{
		
	}

	void OnDestroy() override
	{
		
	}

};
EdenApplication* app;

int EdenMain()
{
	app = enew EdenApplication();
	app->Run();
	edelete app;

	return 0;
}