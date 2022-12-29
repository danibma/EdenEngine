#pragma once

#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(call) \
	{ \
		VkResult result_ = (call); \
		ensureMsgf(result_ == VK_SUCCESS, "Vulkan call \"%s\" failed and returned \"%s\"", #call, string_VkResult(result_)); \
	}

