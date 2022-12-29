#define VOLK_IMPLEMENTATION
#include "VulkanRHI.h"
#include "VulkanHelper.h"
#include "Core/Assertions.h"

#ifdef ED_PLATFORM_WINDOWS
#include <vulkan/vulkan_win32.h>
#endif

namespace Eden
{
	VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			ED_LOG_WARN("{}", pCallbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			ED_LOG_ERROR("{}", pCallbackData->pMessage);
		}
		else
		{
			ED_LOG_INFO("{}", pCallbackData->pMessage);
		}

		return VK_FALSE;
	}

	GfxResult VulkanRHI::Init(Window* window)
	{
		m_CurrentAPI = kApi_Vulkan;

		VkResult result = volkInitialize();
		ensureMsgf(result == VK_SUCCESS, "Failed to initialize volk! \"%s\"", string_VkResult(result));
		
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "EdenEngine";
		appInfo.applicationVersion = 1u;
		appInfo.pEngineName = "Eden";
		appInfo.engineVersion = 1u;
		appInfo.apiVersion = VK_API_VERSION_1_3;

		// TODO: add validation to all these extensions/layers, for the device too
		std::vector<const char*> enabledInstanceExtensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		};

		std::vector<const char*> enabledDeviceLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		VkInstanceCreateInfo instanceInfo = {};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pNext = nullptr;
		instanceInfo.pApplicationInfo = &appInfo;
		instanceInfo.enabledLayerCount = static_cast<uint32_t>(enabledDeviceLayers.size());
		instanceInfo.ppEnabledLayerNames = enabledDeviceLayers.data();
		instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
		instanceInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();
		
		VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_Instance));
		volkLoadInstance(m_Instance);

		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {};
		messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerCreateInfo.pNext = nullptr;
		messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerCreateInfo.pfnUserCallback = VulkanDebugCallback;
		VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_Instance, &messengerCreateInfo, nullptr, &m_DebugMessenger));

		uint32_t physicalDevicesCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &physicalDevicesCount, nullptr);
		ensureMsg(physicalDevicesCount > 0, "Couldn't find GPUs with Vulkan support!");
		std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
		vkEnumeratePhysicalDevices(m_Instance, &physicalDevicesCount, physicalDevices.data());
		for (VkPhysicalDevice& gpu : physicalDevices)
		{
			if (IsGPUSuitable(gpu))
			{
				m_PhysicalDevice = gpu;
				break;
			}
		}
		ensureMsg(m_PhysicalDevice != nullptr, "Could not find a suitable GPU!");

		AcquireQueueFamilyIndicesFamily();
		ensure(m_GraphicsQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED);

		std::vector<const char*> enabledDeviceExtensions = 
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
			//VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		};

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo deviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		deviceQueueCreateInfo.pNext = nullptr;
		deviceQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamilyIndex;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		deviceCreateInfo.pNext = nullptr;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
		vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);

		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
			ED_LOG_INFO("Initialized Vulkan device on {}", deviceProperties.deviceName);
		}

		return GfxResult::kNoError;
	}

	void VulkanRHI::Shutdown()
	{
		vkDestroyInstance(m_Instance, nullptr);
	}

	GfxResult VulkanRHI::CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	GfxResult VulkanRHI::CreatePipeline(Pipeline* pipeline, PipelineDesc* desc)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	GfxResult VulkanRHI::CreateTexture(Texture* texture, std::string path, bool bGenerateMips)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	GfxResult VulkanRHI::CreateTexture(Texture* texture, TextureDesc* desc)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	GfxResult VulkanRHI::CreateRenderPass(RenderPass* renderPass, RenderPassDesc* desc)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	GfxResult VulkanRHI::CreateGPUTimer(GPUTimer* timer)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	void VulkanRHI::SetName(Resource* child, std::string& name)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BeginGPUTimer(GPUTimer* timer)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::EndGPUTimer(GPUTimer* timer)
	{
		// NOT IMPLEMENTED
	}

	GfxResult VulkanRHI::UpdateBufferData(Buffer* buffer, const void* data, uint32_t count /*= 0*/)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	void VulkanRHI::GenerateMips(Texture* texture)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::ChangeResourceState(Texture* resource, ResourceState currentState, ResourceState desiredState, int subresource /*= -1*/)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::EnsureMsgResourceState(Texture* resource, ResourceState destResourceState)
	{
		// NOT IMPLEMENTED
	}

	uint64_t VulkanRHI::GetTextureID(Texture* texture)
	{
		// NOT IMPLEMENTED
		return 1u;
	}

	GfxResult VulkanRHI::ReloadPipeline(Pipeline* pipeline)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	void VulkanRHI::EnableImGui()
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::ImGuiNewFrame()
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindPipeline(Pipeline* pipeline)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindVertexBuffer(Buffer* vertexBuffer)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindIndexBuffer(Buffer* indexBuffer)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindParameter(const std::string& parameterName, Buffer* buffer)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage /*= kReadOnly*/)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BindParameter(const std::string& parameterName, void* data, size_t size)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BeginRender()
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::BeginRenderPass(RenderPass* renderPass)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::SetSwapchainTarget(RenderPass* renderPass)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::EndRenderPass(RenderPass* renderPass)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::EndRender()
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::Draw(uint32_t vertexCount, uint32_t instanceCount /*= 1*/, uint32_t startVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /*= 1*/, uint32_t startIndexLocation /*= 0*/, uint32_t baseVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		// NOT IMPLEMENTED
	}

	void VulkanRHI::Render()
	{
		// NOT IMPLEMENTED
	}

	GfxResult VulkanRHI::Resize(uint32_t width, uint32_t height)
	{
		// NOT IMPLEMENTED
		return GfxResult::kNoError;
	}

	void VulkanRHI::ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel)
	{
		// NOT IMPLEMENTED
	}

	bool VulkanRHI::IsGPUSuitable(VkPhysicalDevice& gpu)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
		
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			return true;

		return false;
	}

	void VulkanRHI::AcquireQueueFamilyIndicesFamily()
	{
		uint32_t queueFamilyPropertiesNum;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertiesNum, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesNum);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertiesNum, queueFamilyProperties.data());
		for (uint32_t i = 0; i < queueFamilyPropertiesNum; ++i)
		{
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				m_GraphicsQueueFamilyIndex = i;
				break;
			}
		}
	}

}