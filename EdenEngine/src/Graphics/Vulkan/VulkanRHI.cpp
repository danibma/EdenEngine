#include "VulkanRHI.h"
#include "VkBootstrap.h"
#include "Core/Log.h"
#include "Core/Base.h"

namespace Eden
{
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, 
														const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			ED_LOG_WARN("{} : {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			// Fix to this error message: "Loader Message : loader_get_json: Failed to open JSON file C:\Users\stormwind\AppData\Local\Medal\recorder-3.432.0\Host\medal-vulkan64.json"
			if (!strstr(pCallbackData->pMessage, "medal"))
				ED_LOG_ERROR("{} : {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else
		{
			ED_LOG_INFO("{} : {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}

		return VK_FALSE;
	}

	void VulkanRHI::Init(Window* window)
	{
		m_CurrentAPI = Vulkan;

		vkb::InstanceBuilder builder;
		auto inst_ret = builder.set_app_name("eden")
								.request_validation_layers(true)
								.require_api_version(1, 3, 0)
								.set_minimum_instance_version(1, 3, 0)
								.set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
								.set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
								.set_debug_callback(DebugCallback)
								.build();

		vkb::Instance vkb_inst = inst_ret.value();

		//store the instance
		m_Instance = vkb_inst.instance;
		//store the debug messenger
		m_DebugMessenger = vkb_inst.debug_messenger;

		VkWin32SurfaceCreateInfoKHR surface_info = {};
		surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_info.pNext = nullptr;
		surface_info.hinstance = GetModuleHandle(nullptr);
		surface_info.hwnd = window->GetHandle();

		if (vkCreateWin32SurfaceKHR(m_Instance, &surface_info, nullptr, &m_Surface) != VK_SUCCESS)
			ED_ASSERT_MB(false, "Failed to create win32 surface!");

		vkb::PhysicalDeviceSelector selector(vkb_inst);
		vkb::PhysicalDevice physical_device = selector.set_minimum_version(1, 3).set_surface(m_Surface).select().value();

		vkb::DeviceBuilder device_builder(physical_device);
		vkb::Device vkb_device = device_builder.build().value();

		m_Device = vkb_device.device;
		m_GPU = physical_device.physical_device;

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(m_GPU, &properties);
		ED_LOG_INFO("Initialized Vulkan device on {}", properties.deviceName);

		vkb::SwapchainBuilder swapchain_builder(m_GPU, m_Device, m_Surface);
		vkb::Swapchain vkb_swapchain = swapchain_builder.use_default_format_selection()
														.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
														.set_desired_extent(window->GetWidth(), window->GetHeight())
														.build()
														.value();

		m_Swapchain = vkb_swapchain.swapchain;
		m_SwapchainImages = vkb_swapchain.get_images().value();
		m_SwapchainImageViews = vkb_swapchain.get_image_views().value();
		m_SwapchainImageFormat = vkb_swapchain.image_format;

		m_GraphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
		m_GraphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

		VkCommandPoolCreateInfo command_pool_info = {};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.pNext = nullptr;
		command_pool_info.queueFamilyIndex = m_GraphicsQueueFamily;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_Device, &command_pool_info, nullptr, &m_CommandPool) != VK_SUCCESS)
			ED_ASSERT_MB(false, "Failed to create command pool!");

		VkCommandBufferAllocateInfo cmd_buffer_info = {};
		cmd_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_buffer_info.pNext = nullptr;
		cmd_buffer_info.commandPool = m_CommandPool;
		cmd_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_buffer_info.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(m_Device, &cmd_buffer_info, &m_CommandBuffer) != VK_SUCCESS)
			ED_ASSERT_MB(false, "Failed to allocate command buffers!");

		// Init sync things
		VkFenceCreateInfo fence_create_info = {};
		fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_create_info.pNext = nullptr;
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(m_Device, &fence_create_info, nullptr, &m_RenderFence);

		VkSemaphoreCreateInfo semaphore_create_info = {};
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_create_info.pNext = nullptr;
		vkCreateSemaphore(m_Device, &semaphore_create_info, nullptr, &m_PresentSemaphore);
		vkCreateSemaphore(m_Device, &semaphore_create_info, nullptr, &m_RenderSemaphore);

		// Render pass
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = m_SwapchainImageFormat;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;
		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.pNext = nullptr;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;

		if (vkCreateRenderPass(m_Device, &render_pass_info, nullptr, &m_RenderPass) != VK_SUCCESS)
			ED_ASSERT_LOG(false, "Failed to create render pass!");

		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;
		fb_info.renderPass = m_RenderPass;
		fb_info.attachmentCount = 1;
		fb_info.width = window->GetWidth();
		fb_info.height = window->GetHeight();
		fb_info.layers = 1;

		auto swapchain_image_count = m_SwapchainImages.size();
		m_FrameBuffers.resize(swapchain_image_count);
		for (int i = 0; i < swapchain_image_count; ++i)
		{
			fb_info.pAttachments = &m_SwapchainImageViews[i];
			if (vkCreateFramebuffer(m_Device, &fb_info, nullptr, &m_FrameBuffers[i]) != VK_SUCCESS)
				ED_ASSERT_LOG(false, "Failed to create framebuffer!");
		}
	}

	void VulkanRHI::Shutdown()
	{
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
		for (auto& framebuffer : m_FrameBuffers)
			vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
	}

	std::shared_ptr<Buffer> VulkanRHI::CreateBuffer(BufferDesc* desc, const void* initial_data)
	{
		return nullptr;
	}

	std::shared_ptr<Pipeline> VulkanRHI::CreatePipeline(PipelineDesc* desc)
	{
		return nullptr;
	}

	std::shared_ptr<Texture> VulkanRHI::CreateTexture(std::string path)
	{
		return nullptr;
	}

	std::shared_ptr<Texture> VulkanRHI::CreateTexture(TextureDesc* desc)
	{
		return nullptr;
	}

	std::shared_ptr<RenderPass> VulkanRHI::CreateRenderPass(RenderPassDesc* desc)
	{
		return nullptr;
	}

	void VulkanRHI::UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count /*= 0*/)
	{
		
	}

	uint64_t VulkanRHI::GetTextureID(std::shared_ptr<Texture>& texture)
	{
		return 0;
	}

	void VulkanRHI::ReloadPipeline(std::shared_ptr<Pipeline>& pipeline)
	{
		
	}

	void VulkanRHI::EnableImGui()
	{
		
	}

	void VulkanRHI::ImGuiNewFrame()
	{
	
	}

	void VulkanRHI::BindPipeline(const std::shared_ptr<Pipeline>& pipeline)
	{
	
	}

	void VulkanRHI::BindVertexBuffer(std::shared_ptr<Buffer>& vertex_buffer)
	{
	
	}

	void VulkanRHI::BindIndexBuffer(std::shared_ptr<Buffer>& index_buffer)
	{
	}

	void VulkanRHI::BindParameter(const std::string& parameter_name, const std::shared_ptr<Buffer>& buffer)
	{
	}

	void VulkanRHI::BindParameter(const std::string& parameter_name, const std::shared_ptr<Texture>& texture)
	{
	}

	void VulkanRHI::BeginRender()
	{
		WaitGPU();
		vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_PresentSemaphore, nullptr, &m_FrameIndex);

		if (vkResetCommandBuffer(m_CommandBuffer, 0) != VK_SUCCESS)
			ED_ASSERT_LOG(false, "Failed to reset command buffer");

		VkCommandBufferBeginInfo cmd_begin_info = {};
		cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_begin_info.pNext = nullptr;
		cmd_begin_info.pInheritanceInfo = nullptr;
		cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(m_CommandBuffer, &cmd_begin_info);
	}

	void VulkanRHI::BeginRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		//make a clear-color from frame number. This will flash with a 120*pi frame period.
		VkClearValue clear_value;
		clear_value.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };

		//start the main renderpass.
		//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rp_info = {};
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.pNext = nullptr;

		rp_info.renderPass = m_RenderPass;
		rp_info.renderArea.offset.x = 0;
		rp_info.renderArea.offset.y = 0;
		rp_info.renderArea.extent = { 1920, 1080 };
		rp_info.framebuffer = m_FrameBuffers[m_FrameIndex];

		//connect clear values
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &clear_value;

		vkCmdBeginRenderPass(m_CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanRHI::SetRTRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
	}

	void VulkanRHI::EndRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		vkCmdEndRenderPass(m_CommandBuffer);
	}

	void VulkanRHI::EndRender()
	{
		vkEndCommandBuffer(m_CommandBuffer);

		// prepare the submission to the queue.
		// we want to wait on the m_PresentSemaphore, as that semaphore is signaled when the swapchain is ready
		// we will signal the m_RenderSemaphore, to signal that rendering has finished
		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.pNext = nullptr;

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit.pWaitDstStageMask = &waitStage;

		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &m_PresentSemaphore;

		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &m_RenderSemaphore;

		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_CommandBuffer;

		// submit command buffer to the queue and execute it.
		// m_RenderFence will now block until the graphic commands finish execution
		vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_RenderFence);

		// this will put the image we just rendered into the visible window.
		// we want to wait on the _renderSemaphore for that,
		// as it's necessary that drawing commands have finished before the image is displayed to the user
		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.pNext = nullptr;

		present_info.pSwapchains = &m_Swapchain;
		present_info.swapchainCount = 1;

		present_info.pWaitSemaphores = &m_RenderSemaphore;
		present_info.waitSemaphoreCount = 1;

		present_info.pImageIndices = &m_FrameIndex;

		vkQueuePresentKHR(m_GraphicsQueue, &present_info);

	}

	void VulkanRHI::Draw(uint32_t vertex_count, uint32_t instance_count /*= 1*/, uint32_t start_vertex_location /*= 0*/, uint32_t start_instance_location /*= 0*/)
	{
	}

	void VulkanRHI::DrawIndexed(uint32_t index_count, uint32_t instance_count /*= 1*/, uint32_t start_index_location /*= 0*/, uint32_t base_vertex_location /*= 0*/, uint32_t start_instance_location /*= 0*/)
	{
	}

	void VulkanRHI::Render()
	{
	}

	void VulkanRHI::Resize(uint32_t width, uint32_t height)
	{
	}

	void VulkanRHI::ResizeRenderPassTexture(std::shared_ptr<RenderPass>& render_pass, uint32_t width, uint32_t height)
	{
	}

	void VulkanRHI::WaitGPU()
	{
		vkWaitForFences(m_Device, 1, &m_RenderFence, true, 1000000000);
		vkResetFences(m_Device, 1, &m_RenderFence);
	}

}