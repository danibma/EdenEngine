#include "Core/Base.h"
#include "Core/Application.h"
#include "Core/Memory/Memory.h"

#include "Scene/MeshSource.h"

using namespace Eden;

int EdenMain()
{
	Application* app = enew Application();

	app->Run();

	edelete app;

	return 0;
}