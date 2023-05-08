#pragma once

#include "Core/Window.h"
#include "RHI/DynamicRHI.h"

namespace Eden::Gfx::Tests
{
	class RHIBaseTest
	{
	public:
		virtual ~RHIBaseTest() {}

		virtual void Init(Window* window) = 0;
		virtual void Update()  = 0;
	};
}