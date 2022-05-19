#pragma once

// TODO(Daniel): In the future add support for PIX

#define ED_PROFILER_ENABLED

#ifdef ED_PROFILER_ENABLED
#include <optick.h>

/*------------------ - CPU Profiling-------------------*/
#define ED_PROFILE_FRAME(FRAME_NAME, ...)			OPTICK_FRAME(FRAME_NAME, __VA_ARGS__)
#define ED_PROFILE_FUNCTION(...)					OPTICK_EVENT(__VA_ARGS__)
#define ED_PROFILE_THREAD(...)						OPTICK_THREAD(__VA_ARGS__)
// A macro for attaching any custom data to the current scope. eg. ED_PROFILE_TAG("Health", 100);
#define ED_PROFILE_TAG(name, ...)					OPTICK_TAG(name, __VA_ARGS__)
#define ED_PROFILE_CATEGORY(name, ...)				OPTICK_CATEGORY(name, __VA_ARGS__)

/*------------------ - GPU Profiling-------------------*/
#define ED_PROFILE_GPU_INIT(DEVICE, CMD_QUEUES, NUM_CMD_QUEUS)	OPTICK_GPU_INIT_D3D12(DEVICE, CMD_QUEUES, NUM_CMD_QUEUS)
#define ED_PROFILE_GPU_CONTEXT(COMMAND_LIST)					OPTICK_GPU_CONTEXT(COMMAND_LIST)
#define ED_PROFILE_GPU_FUNCTION(NAME)							OPTICK_GPU_EVENT(NAME)
#define ED_PROFILE_GPU_FLIP(SWAPCHAIN)							OPTICK_GPU_FLIP(SWAPCHAIN)

#define ED_PROFILER_SHUTDOWN() OPTICK_SHUTDOWN()

#else

#define ED_PROFILE_FRAME
#define ED_PROFILE_FUNCTION
#define ED_PROFILE_THREAD
#define ED_PROFILE_TAG
#define ED_PROFILE_CATEGORY
#define ED_PROFILE_GPU_INIT
#define ED_PROFILE_GPU_CONTEXT
#define ED_PROFILE_GPU_FUNCTION

#endif // ED_PROFILER_ENABLED
