#include "Core/Base.h"
#include "Core/Application.h"
#include "Core/Memory/Memory.h"
#include "Core/CommandLine.h"

#include "Scene/MeshSource.h"

using namespace Eden;

int EdenMain()
{
	CommandLine::Init(cmdLine);

	Application* app = enew Application();
	app->Run();
	edelete app;

	return 0;
}