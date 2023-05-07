#include "Core/Base.h"
#include "Core/Application.h"
#include "Core/Memory/Memory.h"
#include "Core/CommandLine.h"

#include "Scene/MeshSource.h"
#include "Renderer/Renderer.h"
#include "Editor/Editor.h"

#include "RHI/Tests/RHIBaseTest.h"
#include "RHI/Tests/RHITriangleTest.h"

using namespace Eden;

int EdenMain()
{
	CommandLine::Init(cmdLine);

	ApplicationDescription appDescription = {};
	appDescription.Width  = 1600;
	appDescription.Height = 900;

#ifdef ED_DEBUG
	appDescription.Title = "Eden Engine[Debug]";
#elif defined(ED_PROFILING)
	appDescription.Title = "Eden Engine[Profiling]";
#else
	appDescription.Title = "Eden Engine";
#endif 

	EdenEd* editor = nullptr;
	Gfx::Tests::RHIBaseTest* gfxTest = nullptr;

	// Check if we are running any type of graphics test
	std::string gfxTestType;
	CommandLine::Parse("gfx_test", gfxTestType);
	bool bIsGfxTest = gfxTestType.size() > 0;

	// Check if it is a valid graphics test
	if (bIsGfxTest)
	{
#ifdef WITH_EDITOR
		bIsGfxTest = false;
		ED_LOG_WARN("Attempted running a graphics test with the editor, please run the gfx tests without the editor! Running normal editor!");
#else
		if (gfxTestType == "triangle")
		{
			gfxTest = enew Gfx::Tests::RHITriangleTest();
			appDescription.Title += "[gfx::triangle]";
		}
		else
		{
			ED_LOG_WARN("Attempted running '{0}' test, but this isn't a valid test! Ignoring test path!", gfxTestType);
			bIsGfxTest = false;
		}
#endif
	}

	// Create base application and setup delegates
	Application* app = enew Application(appDescription);

	if (bIsGfxTest)
	{
		gfxTest->Init(app->GetWindow());
		app->updateDelegate.AddRaw(gfxTest, &Gfx::Tests::RHIBaseTest::Update);
	}
	else
	{
		// Init renderer
		Renderer::Init(app->GetWindow());

		// Init editor
#ifdef WITH_EDITOR
		editor = enew EdenEd();
		editor->Init(app->GetWindow());
#endif

		// add the update functions that need to be called during runtime, when not minimized
		app->updateDelegate.AddStatic(Renderer::BeginRender);
		app->updateDelegate.AddStatic(Renderer::Render);
#ifdef WITH_EDITOR
		app->updateDelegate.AddRaw(editor, &EdenEd::Update);
#endif
		app->updateDelegate.AddStatic(Renderer::EndRender);
	}
	

	// Run application
	app->Run();

	// Start shutting down
	if (bIsGfxTest)
	{
		edelete gfxTest;
	}
	else
	{
#ifdef WITH_EDITOR
		editor->Shutdown();
		edelete editor;
#endif

		Renderer::Shutdown();
	}

	edelete app;

	return 0;
}
























