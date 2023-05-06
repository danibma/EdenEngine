#include "DynamicRHI.h"
#include "RHI/D3D12/D3D12DynamicRHI.h"
#include "Core/CommandLine.h"
#include "Core/Window.h"

namespace Eden
{
	void RHICreate(Window* window)
	{
		if (CommandLine::HasArg("d3d12"))
		{
			GRHI = enew D3D12DynamicRHI();
		}
		else
		{
			GRHI = enew D3D12DynamicRHI();
			ED_LOG_INFO("No Rendering was set through the command line arguments, choosing D3D12 by default!");
		}

		GRHI->Init(window);
	}

	void RHIShutdown()
	{
		GRHI->Shutdown();
		edelete GRHI;
	}
}