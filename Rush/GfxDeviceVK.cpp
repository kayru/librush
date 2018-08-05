#include "GfxDeviceVK.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_VK

#include "GfxRef.h"
#include "UtilLog.h"
#include "Window.h"

#include <algorithm>
#include <string>

#ifdef __GNUC__
#include <limits.h>
#define _strdup strdup
#endif

#include <stdlib.h>

namespace Rush
{

class GfxDevice;
class GfxContext;

static GfxDevice*  g_device       = nullptr;
static GfxContext* g_context      = nullptr;
static VkDevice    g_vulkanDevice = VK_NULL_HANDLE;

static PFN_vkDebugMarkerSetObjectTagEXT      vkDebugMarkerSetObjectTag         = VK_NULL_HANDLE;
static PFN_vkDebugMarkerSetObjectNameEXT     vkDebugMarkerSetObjectName        = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerBeginEXT          vkCmdDebugMarkerBegin             = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerEndEXT            vkCmdDebugMarkerEnd               = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerInsertEXT         vkCmdDebugMarkerInsert            = VK_NULL_HANDLE;
static PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = VK_NULL_HANDLE;

#define VK_STRUCTURE_TYPE_WAVE_LIMIT_AMD ((VkStructureType)(1000045000ull))
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WAVE_LIMIT_PROPERTIES_AMD ((VkStructureType)(1000045001ull))

typedef struct VkPipelineShaderStageCreateInfoWaveLimitAMD
{
	VkStructureType sType;
	const void*     pNext;
	float           wavesPerCu;
	uint32_t*       cuEnableMask;
} VkPipelineShaderStageCreateInfoWaveLimitAMD;

typedef struct VkPhysicalDeviceWaveLimitPropertiesAMD
{
	VkStructureType sType;
	void*           pNext;
	uint32_t        cuCount;
	uint32_t        maxWavesPerCu;
} VkPhysicalDeviceWaveLimitPropertiesAMD;

#define V(x)                                                                                                           \
	{                                                                                                                  \
		auto s = x;                                                                                                    \
		RUSH_UNUSED(s);                                                                                                \
		RUSH_ASSERT_MSG(s == VK_SUCCESS, #x " call failed");                                                           \
	}

inline void* offsetPtr(void* p, size_t offset) { return static_cast<char*>(p) + offset; }

static GfxContext* allocateContext(GfxContextType type);
static GfxContext* getUploadContext();
static u32         aspectFlagsFromFormat(GfxFormat format);

template <typename ObjectType, typename HandleType>
HandleType retainResource(DynamicResourcePool<ObjectType, HandleType>& pool, const ObjectType& object)
{
	RUSH_ASSERT(object.id != 0);
	auto handle = pool.push(object);
	Gfx_Retain(handle);
	Gfx_Retain(g_device);
	return handle;
}

template <typename ObjectType, typename HandleType>
void releaseResource(DynamicResourcePool<ObjectType, HandleType>& pool, HandleType handle)
{
	if (!handle.valid())
		return;

	auto& t = pool[handle];
	if (t.removeReference() > 1)
		return;

	t.destroy();

	pool.remove(handle);

	Gfx_Release(g_device);
}

static std::vector<VkSurfaceFormatKHR> enumerateSurfaceFormats(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	u32 count = 0;

	V(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr));
	std::vector<VkSurfaceFormatKHR> enumerated(count);

	V(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, enumerated.data()));

	return enumerated;
}

static std::vector<VkLayerProperties> enumarateInstanceLayers()
{
	u32 count = 0;

	V(vkEnumerateInstanceLayerProperties(&count, nullptr));
	std::vector<VkLayerProperties> enumerated(count);

	V(vkEnumerateInstanceLayerProperties(&count, enumerated.data()));

	return enumerated;
}

static std::vector<VkExtensionProperties> enumerateInstanceExtensions()
{
	u32 count = 0;

	V(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
	std::vector<VkExtensionProperties> enumerated(count);

	V(vkEnumerateInstanceExtensionProperties(nullptr, &count, enumerated.data()));

	return enumerated;
}

static std::vector<VkLayerProperties> enumerateDeviceLayers(VkPhysicalDevice physicalDevice)
{
	u32 count = 0;

	V(vkEnumerateDeviceLayerProperties(physicalDevice, &count, nullptr));
	std::vector<VkLayerProperties> enumerated(count);

	V(vkEnumerateDeviceLayerProperties(physicalDevice, &count, enumerated.data()));

	return enumerated;
}

static std::vector<VkExtensionProperties> enumerateDeviceExtensions(VkPhysicalDevice physicalDevice)
{
	u32 count = 0;

	V(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr));
	std::vector<VkExtensionProperties> enumerated(count);

	V(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, enumerated.data()));

	return enumerated;
}

static VkMemoryAllocateInfo getStagingMemoryAllocateInfo(const VkMemoryRequirements& stagingMemoryReq)
{
	VkMemoryAllocateInfo stagingAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	stagingAllocInfo.allocationSize       = stagingMemoryReq.size;
	stagingAllocInfo.memoryTypeIndex      = g_device->memoryTypeFromProperties(
        stagingMemoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Some hardware (i.e. Intel) will fail with device local exclusion
	// Retry without this bit.
	if (stagingAllocInfo.memoryTypeIndex == 0xFFFFFFFF)
	{
		stagingAllocInfo.memoryTypeIndex =
		    g_device->memoryTypeFromProperties(stagingMemoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	return stagingAllocInfo;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT                                           objectType,
    uint64_t                                                             object,
    size_t                                                               location,
    int32_t                                                              messageCode,
    const char*                                                          pLayerPrefix,
    const char*                                                          pMessage,
    void*                                                                pUserData)
{
	enum Severity
	{
		Severity_Ignore,
		Severity_Message,
		Severity_Warning,
		Severity_Error,
	};

	Severity severity = Severity_Ignore;

	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		severity = Severity_Message;
	}
	else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
	{
		severity = Severity_Warning;
		if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) &&
		    (strstr(pMessage, "but no vertex buffers are attached to this Pipeline State Object") ||
		        strstr(pMessage, "not consumed by vertex shader")))
		{
			severity = Severity_Ignore;
		}
	}
	else
	{
		severity = Severity_Error;
		if (strstr(pMessage, "SPIR-V module not valid: OpStore Pointer <id>") ||
		    strstr(pMessage, "SPIR-V module not valid: OpReturnValue Value <id>") ||
		    strstr(pMessage, "MemberDecorate requires one of these capabilities: ClipDistance"))
		{
			severity = Severity_Warning;
		}
	}

	switch (severity)
	{
	default:
	case Severity_Ignore: break;
	case Severity_Message: RUSH_LOG("Vulkan debug: %s", pMessage); break;
	case Severity_Warning: RUSH_LOG_WARNING("Vulkan debug: %s", pMessage); break;
	case Severity_Error: RUSH_LOG_ERROR("Vulkan debug: %s", pMessage); break;
	}

	return VK_FALSE;
}

static VkImageLayout convertImageLayout(GfxResourceState state)
{
	switch (state)
	{
	default:
	case GfxResourceState_Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
	case GfxResourceState_General: return VK_IMAGE_LAYOUT_GENERAL;
	case GfxResourceState_RenderTarget: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case GfxResourceState_DepthStencilTarget: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case GfxResourceState_DepthStencilTargetReadOnly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case GfxResourceState_ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case GfxResourceState_TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case GfxResourceState_TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case GfxResourceState_Preinitialized: return VK_IMAGE_LAYOUT_PREINITIALIZED;
	case GfxResourceState_Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	case GfxResourceState_SharedPresent: return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
	}
}

static VkCompareOp convertCompareFunc(GfxCompareFunc compareFunc)
{
	switch (compareFunc)
	{
	case GfxCompareFunc::Less: return VK_COMPARE_OP_LESS;
	case GfxCompareFunc::Equal: return VK_COMPARE_OP_EQUAL;
	case GfxCompareFunc::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
	case GfxCompareFunc::Greater: return VK_COMPARE_OP_GREATER;
	case GfxCompareFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
	case GfxCompareFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case GfxCompareFunc::Always: return VK_COMPARE_OP_ALWAYS;
	case GfxCompareFunc::Never: return VK_COMPARE_OP_NEVER;
	default: RUSH_LOG_ERROR("Unexpected compare function"); return VK_COMPARE_OP_NEVER;
	}
}

static VkFilter convertFilter(GfxTextureFilter filter)
{
	switch (filter)
	{
	case GfxTextureFilter::Point: return VK_FILTER_NEAREST;
	case GfxTextureFilter::Linear: return VK_FILTER_LINEAR;
	default: RUSH_LOG_ERROR("Unexpected filter"); return VK_FILTER_NEAREST;
	}
}

static VkSamplerMipmapMode convertMipmapMode(GfxTextureFilter mip)
{
	switch (mip)
	{
	case GfxTextureFilter::Point: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case GfxTextureFilter::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	default: RUSH_LOG_ERROR("Unexpected mipmap mode"); return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
}

static VkSamplerAddressMode convertSamplerAddressMode(GfxTextureWrap mode)
{
	switch (mode)
	{
	case GfxTextureWrap::Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case GfxTextureWrap::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case GfxTextureWrap::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	default: RUSH_LOG_ERROR("Unexpected wrap mode"); return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

static VkBlendFactor convertBlendParam(GfxBlendParam blendParam)
{
	switch (blendParam)
	{
	default: RUSH_LOG_ERROR("Unexpected blend factor"); return VK_BLEND_FACTOR_ZERO;
	case GfxBlendParam::Zero: return VK_BLEND_FACTOR_ZERO;
	case GfxBlendParam::One: return VK_BLEND_FACTOR_ONE;
	case GfxBlendParam::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
	case GfxBlendParam::InvSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case GfxBlendParam::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
	case GfxBlendParam::InvSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case GfxBlendParam::DestAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
	case GfxBlendParam::InvDestAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case GfxBlendParam::DestColor: return VK_BLEND_FACTOR_DST_COLOR;
	case GfxBlendParam::InvDestColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	}
}

static VkBlendOp convertBlendOp(GfxBlendOp blendOp)
{
	switch (blendOp)
	{
	default: RUSH_LOG_ERROR("Unexpected blend operation"); return VK_BLEND_OP_ADD;
	case GfxBlendOp::Add: return VK_BLEND_OP_ADD;
	case GfxBlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
	case GfxBlendOp::RevSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
	case GfxBlendOp::Min: return VK_BLEND_OP_MIN;
	case GfxBlendOp::Max: return VK_BLEND_OP_MAX;
	}
}

static VkPrimitiveTopology convertPrimitiveType(GfxPrimitive primitiveType)
{
	switch (primitiveType)
	{
	default: RUSH_LOG_ERROR("Unexpected primitive type"); return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	case GfxPrimitive::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case GfxPrimitive::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case GfxPrimitive::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case GfxPrimitive::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case GfxPrimitive::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	}
}

static VkFormat convertFormat(GfxFormat format)
{
	switch (format)
	{
	case GfxFormat_R8_Unorm: return VK_FORMAT_R8_UNORM;
	case GfxFormat_RG8_Unorm: return VK_FORMAT_R8G8_UNORM;
	case GfxFormat_R16_Uint: return VK_FORMAT_R16_UINT;
	case GfxFormat_RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case GfxFormat_RGBA32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case GfxFormat_BGRA8_Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
	case GfxFormat_RGBA8_Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
	case GfxFormat_BGRA8_sRGB: return VK_FORMAT_B8G8R8A8_SRGB;
	case GfxFormat_RGBA8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
	case GfxFormat_D32_Float_S8_Uint: return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case GfxFormat_D24_Unorm_S8_Uint:
		// return VK_FORMAT_D24_UNORM_S8_UINT;
		return VK_FORMAT_D32_SFLOAT_S8_UINT; // TODO: detect VK_FORMAT_D24_UNORM_S8_UINT support
	case GfxFormat_D32_Float: return VK_FORMAT_D32_SFLOAT;
	case GfxFormat_R32_Float: return VK_FORMAT_R32_SFLOAT;
	case GfxFormat_R32_Uint: return VK_FORMAT_R32_UINT;
	case GfxFormat_BC1_Unorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case GfxFormat_BC1_Unorm_sRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case GfxFormat_BC3_Unorm: return VK_FORMAT_BC3_UNORM_BLOCK;
	case GfxFormat_BC3_Unorm_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
	case GfxFormat_BC4_Unorm: return VK_FORMAT_BC4_UNORM_BLOCK;
	case GfxFormat_BC6H_SFloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case GfxFormat_BC6H_UFloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case GfxFormat_BC7_Unorm: return VK_FORMAT_BC7_UNORM_BLOCK;
	case GfxFormat_BC7_Unorm_sRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
	default: RUSH_LOG_ERROR("Unsupported format"); return VK_FORMAT_UNDEFINED;
	}
}

static VkShaderStageFlags convertStageFlags(GfxStageFlags flags)
{
	VkShaderStageFlags res = VkShaderStageFlags(0);

	if (!!(flags & GfxStageFlags::Vertex))
	{
		res |= VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (!!(flags & GfxStageFlags::Geometry))
	{
		res |= VK_SHADER_STAGE_GEOMETRY_BIT;
	}
	if (!!(flags & GfxStageFlags::Pixel))
	{
		res |= VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	if (!!(flags & GfxStageFlags::Hull))
	{
		res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	}
	if (!!(flags & GfxStageFlags::Domain))
	{
		res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	}
	if (!!(flags & GfxStageFlags::Compute))
	{
		res |= VK_SHADER_STAGE_COMPUTE_BIT;
	}

	return res;
}

static bool isLayerSupported(const std::vector<VkLayerProperties>& layers, const char* name)
{
	for (const auto& it : layers)
	{
		if (!strcmp(it.layerName, name))
		{
			return true;
		}
	}
	return false;
}

static bool isExtensionSupported(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
	for (const auto& it : extensions)
	{
		if (!strcmp(it.extensionName, name))
		{
			return true;
		}
	}
	return false;
}

static bool enableLayer(std::vector<const char*>& layers, const std::vector<VkLayerProperties>& availableLayers,
    const char* name, bool required = false)
{
	if (isLayerSupported(availableLayers, name))
	{
		layers.push_back(name);
		return true;
	}
	else if (required)
	{
		RUSH_LOG_ERROR("Required Vulkan layer '%s' is not supported.", name);
	}

	return false;
}

static bool enableExtension(std::vector<const char*>& extensions,
    const std::vector<VkExtensionProperties>& availableExtensions, const char* name, bool required = false)
{
	if (isExtensionSupported(availableExtensions, name))
	{
		extensions.push_back(name);
		return true;
	}
	else if (required)
	{
		RUSH_LOG_ERROR("Required Vulkan extension '%s' is not supported.", name);
	}

	return false;
}

inline void validateBufferUse(const BufferVK& buffer)
{
	if (!!(buffer.desc.flags & GfxBufferFlags::Transient))
	{
		RUSH_ASSERT_MSG(buffer.lastUpdateFrame == g_device->m_frameCount,
		    "Trying to use stale buffer. Temporary buffers must be updated/filled and used on the same frame.");
	}
}

GfxDevice::GfxDevice(Window* window, const GfxConfig& cfg)
{
	g_device = this;

	m_window = window;
	m_window->retain();

	m_refs = 1;

	auto enumeratedInstanceLayers     = enumarateInstanceLayers();
	auto enumeratedInstanceExtensions = enumerateInstanceExtensions();

	std::vector<const char*> enabledInstanceLayers;
	std::vector<const char*> enabledInstanceExtensions;

	enableExtension(enabledInstanceExtensions, enumeratedInstanceExtensions, VK_KHR_SURFACE_EXTENSION_NAME, true);

#if defined(RUSH_PLATFORM_WINDOWS)
	enableExtension(enabledInstanceExtensions, enumeratedInstanceExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true);
#elif defined(RUSH_PLATFORM_LINUX)
	enableExtension(enabledInstanceExtensions, enumeratedInstanceExtensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME, true);
#else
	RUSH_LOG_FATAL("Vulkan surface extension is not implemented for this platform");
#endif

	if (cfg.debug)
	{
		enableLayer(enabledInstanceLayers, enumeratedInstanceLayers, "VK_LAYER_LUNARG_standard_validation", false);
		enableExtension(enabledInstanceExtensions, enumeratedInstanceExtensions, "VK_EXT_debug_report", false);
	}

	enableExtension(enabledInstanceExtensions, enumeratedInstanceExtensions, "VK_KHR_get_physical_device_properties2");

	VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	appInfo.pApplicationName  = "Rush";
	appInfo.pEngineName       = "Rush";
	appInfo.apiVersion        = VK_MAKE_VERSION(1, 0, VK_HEADER_VERSION);

	VkInstanceCreateInfo instanceCreateInfo    = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	instanceCreateInfo.pApplicationInfo        = &appInfo;
	instanceCreateInfo.enabledLayerCount       = (u32)enabledInstanceLayers.size();
	instanceCreateInfo.ppEnabledLayerNames     = enabledInstanceLayers.data();
	instanceCreateInfo.enabledExtensionCount   = (u32)enabledInstanceExtensions.size();
	instanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();
	V(vkCreateInstance(&instanceCreateInfo, nullptr, &m_vulkanInstance));

	vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(
	    vkGetInstanceProcAddr(m_vulkanInstance, "vkGetPhysicalDeviceProperties2KHR"));

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
	    reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
	        vkGetInstanceProcAddr(m_vulkanInstance, "vkCreateDebugReportCallbackEXT"));

	if (vkCreateDebugReportCallbackEXT)
	{
		VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
		callbackCreateInfo.pNext                              = nullptr;
		callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
		                           VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackCreateInfo.pfnCallback = &debugReportCallback;
		callbackCreateInfo.pUserData   = nullptr;
		V(vkCreateDebugReportCallbackEXT(m_vulkanInstance, &callbackCreateInfo, nullptr, &m_debugReportCallbackExt));
	}

	u32 gpuCount = 0;

	V(vkEnumeratePhysicalDevices(m_vulkanInstance, &gpuCount, nullptr));
	RUSH_ASSERT(gpuCount);

	m_physicalDevices.resize(gpuCount);
	V(vkEnumeratePhysicalDevices(m_vulkanInstance, &gpuCount, m_physicalDevices.data()));

	m_physicalDevice = m_physicalDevices[0];

	enum VendorID
	{
		VendorID_AMD    = 0x1002,
		VendorID_NVIDIA = 0x10DE,
		VendorID_Intel  = 0x8086,
	};

	std::memset(&m_physicalDeviceProps, 0, sizeof(m_physicalDeviceProps));
	vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProps);

	vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_physicalDeviceFeatures);
	RUSH_ASSERT(m_physicalDeviceFeatures.shaderClipDistance);

	auto enumeratedDeviceLayers     = enumerateDeviceLayers(m_physicalDevice);
	auto enumeratedDeviceExtensions = enumerateDeviceExtensions(m_physicalDevice);

	u32 queueCount = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);
	RUSH_ASSERT(queueCount >= 1);

	m_queueProps.resize(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, m_queueProps.data());
	RUSH_ASSERT(queueCount >= 1);

	const u32 invalidIndex       = 0xFFFFFFFF;
	u32       graphicsQueueIndex = invalidIndex;
	u32       computeQueueIndex  = invalidIndex;
	u32       transferQueueIndex = invalidIndex;

	for (u32 queueIt = 0; queueIt < queueCount; queueIt++)
	{
		if ((m_queueProps[queueIt].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsQueueIndex == invalidIndex)
		{
			graphicsQueueIndex = queueIt;
			continue;
		}

		if ((m_queueProps[queueIt].queueFlags & VK_QUEUE_COMPUTE_BIT) && computeQueueIndex == invalidIndex)
		{
			computeQueueIndex = queueIt;
			continue;
		}

		if ((m_queueProps[queueIt].queueFlags & VK_QUEUE_TRANSFER_BIT) && transferQueueIndex == invalidIndex)
		{
			transferQueueIndex = queueIt;
			continue;
		}
	}

	RUSH_ASSERT(graphicsQueueIndex != invalidIndex);

	float queuePriorities[] = {0.0f};

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(queueCount);

	{
		m_graphicsQueueIndex = graphicsQueueIndex;

		VkDeviceQueueCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};

		info.queueFamilyIndex = graphicsQueueIndex;
		info.queueCount       = 1;
		info.pQueuePriorities = queuePriorities;

		queueCreateInfos.push_back(info);
	}

	if (computeQueueIndex != invalidIndex)
	{
		m_computeQueueIndex = computeQueueIndex;

		VkDeviceQueueCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};

		info.queueFamilyIndex = computeQueueIndex;
		info.queueCount       = 1;
		info.pQueuePriorities = queuePriorities;

		queueCreateInfos.push_back(info);
	}

	if (transferQueueIndex != invalidIndex)
	{
		m_transferQueueIndex = transferQueueIndex;

		VkDeviceQueueCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};

		info.queueFamilyIndex = transferQueueIndex;
		info.queueCount       = 1;
		info.pQueuePriorities = queuePriorities;

		queueCreateInfos.push_back(info);
	}

	std::vector<const char*> enabledDeviceLayers;
	std::vector<const char*> enabledDeviceExtensions;

	enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);

	m_supportedExtensions.NV_geometry_shader_passthrough = enableExtension(
	    enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_NV_geometry_shader_passthrough", false);

	m_supportedExtensions.AMD_shader_explicit_vertex_parameter = enableExtension(
	    enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_AMD_shader_explicit_vertex_parameter", false);

	m_supportedExtensions.AMD_wave_limits =
	    enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_AMD_wave_limits", false);

	m_supportedExtensions.EXT_shader_subgroup_vote =
	    enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_EXT_shader_subgroup_vote", false);

	m_supportedExtensions.EXT_shader_subgroup_ballot =
	    enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_EXT_shader_subgroup_ballot", false);

	m_supportedExtensions.KHR_maintenance1 =
	    enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, VK_KHR_MAINTENANCE1_EXTENSION_NAME, false);

	std::memset(&m_physicalDeviceProps2, 0, sizeof(m_physicalDeviceProps2));

	VkPhysicalDeviceWaveLimitPropertiesAMD waveLimitProps = {
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WAVE_LIMIT_PROPERTIES_AMD};
	if (vkGetPhysicalDeviceProperties2KHR)
	{
		m_physicalDeviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		m_physicalDeviceProps2.pNext = &waveLimitProps;
		vkGetPhysicalDeviceProperties2KHR(m_physicalDevice, &m_physicalDeviceProps2);
	}

	if (!m_supportedExtensions.KHR_maintenance1)
	{
		m_supportedExtensions.AMD_negative_viewport_height = enableExtension(
		    enabledDeviceExtensions, enumeratedDeviceExtensions, "VK_AMD_negative_viewport_height", false);
	}

	bool debugMerkersAvailable =
	    enableExtension(enabledDeviceExtensions, enumeratedDeviceExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false);

	if (cfg.debug)
	{
		if (!enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_standard_validation"))
		{
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_GOOGLE_threading");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_parameter_validation");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_device_limits");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_object_tracker");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_image");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_core_validation");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_LUNARG_swapchain");
			enableLayer(enabledDeviceLayers, enumeratedDeviceLayers, "VK_LAYER_GOOGLE_unique_objects");
		}
	}

	VkPhysicalDeviceFeatures enabledDeviceFeatures = m_physicalDeviceFeatures;
	enabledDeviceFeatures.robustBufferAccess       = false;

	VkDeviceCreateInfo deviceCreateInfo      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	deviceCreateInfo.queueCreateInfoCount    = (u32)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos       = queueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount       = (u32)enabledDeviceLayers.size();
	deviceCreateInfo.ppEnabledLayerNames     = enabledDeviceLayers.data();
	deviceCreateInfo.enabledExtensionCount   = (u32)enabledDeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures        = &enabledDeviceFeatures;

	V(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_vulkanDevice));
	g_vulkanDevice = m_vulkanDevice;

	if (debugMerkersAvailable)
	{
		vkDebugMarkerSetObjectTag =
		    (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(m_vulkanDevice, "vkDebugMarkerSetObjectTagEXT");
		vkDebugMarkerSetObjectName =
		    (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(m_vulkanDevice, "vkDebugMarkerSetObjectNameEXT");
		vkCmdDebugMarkerBegin =
		    (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(m_vulkanDevice, "vkCmdDebugMarkerBeginEXT");
		vkCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(m_vulkanDevice, "vkCmdDebugMarkerEndEXT");
		vkCmdDebugMarkerInsert =
		    (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(m_vulkanDevice, "vkCmdDebugMarkerInsertEXT");
	}

	vkGetDeviceQueue(m_vulkanDevice, graphicsQueueIndex, 0, &m_graphicsQueue);
	RUSH_ASSERT(m_graphicsQueue);

	if (m_computeQueueIndex != invalidIndex)
	{
		vkGetDeviceQueue(m_vulkanDevice, m_computeQueueIndex, 0, &m_computeQueue);
		if (m_physicalDeviceProps.vendorID == VendorID_NVIDIA && m_computeQueue)
		{
			// Workaround for NVIDIA compute queue creation.
			// Compute work on graphics queue is broken without this.
			vkQueueWaitIdle(m_computeQueue);
		}
		RUSH_ASSERT(m_computeQueue);
	}

	if (m_transferQueueIndex != invalidIndex)
	{
		vkGetDeviceQueue(m_vulkanDevice, m_transferQueueIndex, 0, &m_transferQueue);
		RUSH_ASSERT(m_transferQueue);
	}

	// Memory types

	std::memset(&m_deviceMemoryProps, 0, sizeof(m_deviceMemoryProps));
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProps);
	m_memoryTraits.resize(m_deviceMemoryProps.memoryTypeCount);
	for (u32 i = 0; i < m_deviceMemoryProps.memoryTypeCount; ++i)
	{
		u32 bits               = m_deviceMemoryProps.memoryTypes[i].propertyFlags;
		m_memoryTraits[i].bits = bits;
	}

	m_memoryTypes.local = memoryTypeFromProperties(0xFFFFFFFF, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_memoryTypes.hostVisible =
	    memoryTypeFromProperties(0xFFFFFFFF, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	if (m_memoryTypes.hostVisible == 0xFFFFFFFF)
	{
		m_memoryTypes.hostVisible = memoryTypeFromProperties(0xFFFFFFFF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	m_memoryTypes.hostOnly = memoryTypeFromProperties(0xFFFFFFFF,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	    VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (m_memoryTypes.hostOnly == 0xFFFFFFFF)
	{
		m_memoryTypes.hostOnly = memoryTypeFromProperties(
		    0xFFFFFFFF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	}

	if (m_memoryTypes.hostOnly == 0xFFFFFFFF)
	{
		m_memoryTypes.hostOnly = memoryTypeFromProperties(0xFFFFFFFF, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	m_memoryTypes.hostOnlyCached = memoryTypeFromProperties(0xFFFFFFFF,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	RUSH_ASSERT(m_memoryTypes.local != 0xFFFFFFFF);
	RUSH_ASSERT(m_memoryTypes.hostVisible != 0xFFFFFFFF);
	RUSH_ASSERT(m_memoryTypes.hostOnly != 0xFFFFFFFF);

	if (m_memoryTypes.hostOnlyCached == 0xFFFFFFFF)
	{
		// Host-only cached is unavailable (Intel).
		// Fallback to host visible.
		m_memoryTypes.hostOnlyCached = m_memoryTypes.hostVisible;
	}

	// Swap chain

	m_desiredSwapChainImageCount = cfg.minimizeLatency ? 1 : 2;

	VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	V(vkCreateSemaphore(m_vulkanDevice, &semaphoreCreateInfo, nullptr, &m_presentCompleteSemaphore));

	createSwapChain();

	// Command pool

	{
		VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cmdPoolInfo.queueFamilyIndex        = graphicsQueueIndex;
		cmdPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		V(vkCreateCommandPool(m_vulkanDevice, &cmdPoolInfo, nullptr, &m_graphicsCommandPool));
	}

	if (m_computeQueueIndex != invalidIndex)
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cmdPoolInfo.queueFamilyIndex        = computeQueueIndex;
		cmdPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		V(vkCreateCommandPool(m_vulkanDevice, &cmdPoolInfo, nullptr, &m_computeCommandPool));
	}

	if (m_transferQueueIndex != invalidIndex)
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cmdPoolInfo.queueFamilyIndex        = transferQueueIndex;
		cmdPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		V(vkCreateCommandPool(m_vulkanDevice, &cmdPoolInfo, nullptr, &m_transferCommandPool));
	}

	// Pipeline cache

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
	V(vkCreatePipelineCache(m_vulkanDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));

	// Frame data (descriptor pools, timing pools, memory allocators)

	for (FrameData& it : m_frameData)
	{
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		descriptorPoolCreateInfo.maxSets                    = 65536; // TODO: create descriptor pools on demand

		VkDescriptorPoolSize poolSizes[] = {
		    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 65536},
		    {VK_DESCRIPTOR_TYPE_SAMPLER, 65536},
		    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 65536},
		    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 65536},
		    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 65536},
		    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 65536},
		    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 65536},
		};
		descriptorPoolCreateInfo.poolSizeCount = RUSH_COUNTOF(poolSizes);
		descriptorPoolCreateInfo.pPoolSizes    = poolSizes;

		V(vkCreateDescriptorPool(m_vulkanDevice, &descriptorPoolCreateInfo, nullptr, &it.descriptorPool));

		VkQueryPoolCreateInfo timestampPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		timestampPoolCreateInfo.queryCount =
		    2 * (GfxStats::MaxCustomTimers + 1); // 2 slots per custom timer + 2 slots for total frame time
		timestampPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;

		V(vkCreateQueryPool(m_vulkanDevice, &timestampPoolCreateInfo, nullptr, &it.timestampPool));
		it.timestampPoolData.resize(timestampPoolCreateInfo.queryCount);
		it.timestampSlotMap.resize(timestampPoolCreateInfo.queryCount);

		it.localOnlyAllocator.init(m_memoryTypes.local, false);
		it.hostVisibleAllocator.init(m_memoryTypes.hostVisible, true);
		it.hostOnlyAllocator.init(m_memoryTypes.hostOnly, true);
		it.hostOnlyCachedAllocator.init(m_memoryTypes.hostOnlyCached, true);
	}

	m_currentFrame                         = &m_frameData.back();
	m_currentFrame->presentSemaphoreWaited = true;

	BlendStateVK& defaultBlendState = m_blendStates[InvalidResourceHandle{}];
	defaultBlendState.id            = generateId();
	defaultBlendState.desc.src      = GfxBlendParam::One;
	defaultBlendState.desc.alphaSrc = GfxBlendParam::One;

	// Setup resize event notifications

	m_resizeEvents.mask = WindowEventMask_Resize;
	m_resizeEvents.setOwner(window);

	// Done!

	m_caps.looseConstants  = false;
	m_caps.constantBuffers = true;

	if (cfg.preferredCoordinateSystem == GfxConfig::PreferredCoordinateSystem_Direct3D &&
	    (m_supportedExtensions.AMD_negative_viewport_height || m_supportedExtensions.KHR_maintenance1))
	{
		m_useNegativeViewport  = true;
		m_caps.projectionFlags = ProjectionFlags::Default;
		m_caps.deviceTopLeft   = Vec2(-1.0f, 1.0f);
	}
	else
	{
		m_caps.projectionFlags = ProjectionFlags::FlipVertical;
		m_caps.deviceTopLeft   = Vec2(-1.0f, -1.0f);
	}

	m_caps.textureTopLeft = Vec2(0.0f, 0.0f);
	m_caps.shaderTypeMask |= 1 << GfxShaderSourceType_SPV;
	m_caps.compute          = true;
	m_caps.instancing       = true;
	m_caps.drawIndirect     = true;
	m_caps.dispatchIndirect = true;
	m_caps.shaderInt16      = !!enabledDeviceFeatures.shaderInt16;
	m_caps.shaderInt64      = !!enabledDeviceFeatures.shaderInt64;
	m_caps.asyncCompute     = m_computeQueueIndex != invalidIndex;
	m_caps.shaderWaveIntrinsics =
	    m_supportedExtensions.EXT_shader_subgroup_ballot && m_supportedExtensions.EXT_shader_subgroup_vote;
	m_caps.geometryShaderPassthroughNV = m_supportedExtensions.NV_geometry_shader_passthrough;
	m_caps.explicitVertexParameterAMD  = m_supportedExtensions.AMD_shader_explicit_vertex_parameter;

	switch (m_physicalDeviceProps.vendorID)
	{
	case VendorID_NVIDIA: m_caps.threadGroupSize = 32; break;
	case VendorID_AMD: m_caps.threadGroupSize = 64; break;
	default:
		m_caps.threadGroupSize = 64; // TODO: what's best for Intel?
		break;
	}

	m_caps.apiName = "Vulkan";

	m_techniques.data.reserve(256);
	m_techniques.empty.reserve(256);
}

GfxDevice::~GfxDevice()
{
	m_resizeEvents.setOwner(nullptr);

	m_window->release();
	m_window = nullptr;

	V(vkQueueWaitIdle(m_graphicsQueue));
	if (m_computeQueue)
	{
		V(vkQueueWaitIdle(m_computeQueue));
	}
	if (m_transferQueue)
	{
		V(vkQueueWaitIdle(m_transferQueue));
	}

	for (auto& it : m_pipelines)
	{
		vkDestroyPipeline(m_vulkanDevice, it.second, nullptr);
	}

	for (FrameData& it : m_frameData)
	{
		it.destructionQueue.flush(m_vulkanDevice);
		vkDestroyDescriptorPool(m_vulkanDevice, it.descriptorPool, nullptr);
		vkDestroyQueryPool(m_vulkanDevice, it.timestampPool, nullptr);
	}

	for (auto& contextPool : m_freeContexts)
	{
		for (GfxContext* p : contextPool)
		{
			Gfx_Release(p);
		}
	}

	for (auto& it : m_renderPasses)
	{
		vkDestroyRenderPass(m_vulkanDevice, it.second, nullptr);
	}

	for (auto& it : m_frameBuffers)
	{
		vkDestroyFramebuffer(m_vulkanDevice, it.second, nullptr);
	}

	vkDestroyPipelineCache(m_vulkanDevice, m_pipelineCache, nullptr);

	vkDestroySwapchainKHR(m_vulkanDevice, m_swapChain, nullptr);
	vkDestroyCommandPool(m_vulkanDevice, m_graphicsCommandPool, nullptr);
	if (m_computeCommandPool)
	{
		vkDestroyCommandPool(m_vulkanDevice, m_computeCommandPool, nullptr);
	}
	vkDestroySurfaceKHR(m_vulkanInstance, m_swapChainSurface, nullptr);
	vkDestroySemaphore(m_vulkanDevice, m_presentCompleteSemaphore, nullptr);
	vkDestroyDevice(m_vulkanDevice, nullptr);

	if (m_debugReportCallbackExt)
	{
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
		    reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
		        vkGetInstanceProcAddr(m_vulkanInstance, "vkDestroyDebugReportCallbackEXT"));

		if (vkDestroyDebugReportCallbackEXT)
		{
			vkDestroyDebugReportCallbackEXT(m_vulkanInstance, m_debugReportCallbackExt, nullptr);
		}
	}

	vkDestroyInstance(m_vulkanInstance, nullptr);
}

VkPipeline GfxDevice::createPipeline(const PipelineInfoVK& info)
{
	RUSH_ASSERT(info.techniqueHandle.valid());

	PipelineKey key = {};
	key.techniqueId = g_device->m_techniques[info.techniqueHandle].id;

	if (!g_device->m_techniques[info.techniqueHandle].cs.valid())
	{
		key.blendStateId        = g_device->m_blendStates[info.blendStateHandle].id;
		key.depthStencilStateId = g_device->m_depthStencilStates[info.depthStencilStateHandle].id;
		key.rasterizerStateId   = g_device->m_rasterizerStates[info.rasterizerStateHandle].id;
		for (u32 i = 0; i < RUSH_COUNTOF(key.vertexBufferStride); ++i)
		{
			key.vertexBufferStride[i] = info.vertexBufferStride[i];
		}
		key.primitiveType        = info.primitiveType;
		key.colorAttachmentCount = info.colorAttachmentCount;
	}

	auto existingPipeline = m_pipelines.find(key);
	if (existingPipeline != m_pipelines.end())
	{
		return existingPipeline->second;
	}

	const TechniqueVK& technique = m_techniques[info.techniqueHandle];

	VkPipeline pipeline = VK_NULL_HANDLE;

	if (technique.cs.valid())
	{
		VkComputePipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

		createInfo.stage  = technique.shaderStages[0];
		createInfo.layout = technique.pipelineLayout;

		// setup done

		V(vkCreateComputePipelines(m_vulkanDevice, m_pipelineCache, 1, &createInfo, nullptr, &pipeline));
	}
	else
	{
		VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

		createInfo.stageCount = (u32)technique.shaderStages.size();
		createInfo.pStages    = technique.shaderStages.data();
		createInfo.layout     = technique.pipelineLayout;

		// vertex buffers

		const size_t                      maxCustomVertexAttributes = 32;
		VkVertexInputAttributeDescription customVertexAttributes[maxCustomVertexAttributes];

		VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		createInfo.pVertexInputState            = &vi;

		if (technique.vf.valid())
		{
			const VertexFormatVK& vertexFormat = m_vertexFormats[technique.vf.get()];
			const ShaderVK&       vertexShader = m_vertexShaders[technique.vs.get()];

			if (vertexShader.inputMappings.empty())
			{
				// default vertex input mappings (vertex shader must match vertex format perfectly)
				vi.vertexAttributeDescriptionCount = (u32)vertexFormat.attributes.size();
				vi.pVertexAttributeDescriptions    = vertexFormat.attributes.data();
			}
			else
			{
				u32 customAttributeCount = 0;

				for (const auto& inputMapping : vertexShader.inputMappings)
				{
					u32  elementIndex = 0;
					bool inputFound   = false;
					for (const auto& element : vertexFormat.desc)
					{
						if (element.semantic == inputMapping.semantic && element.index == inputMapping.semanticIndex)
						{
							RUSH_ASSERT(customAttributeCount < maxCustomVertexAttributes);

							VkVertexInputAttributeDescription& attrib = customVertexAttributes[customAttributeCount];
							attrib                                    = vertexFormat.attributes[elementIndex];
							attrib.location                           = inputMapping.location;
							inputFound                                = true;
							customAttributeCount++;
							break;
						}
						elementIndex++;
					}
					if (!inputFound)
					{
						RUSH_LOG_ERROR("Vertex shader input '%s%d' not found in vertex format declaration.",
						    toString(inputMapping.semantic), inputMapping.semanticIndex);
					}
				}

				vi.vertexAttributeDescriptionCount = customAttributeCount;
				vi.pVertexAttributeDescriptions    = customVertexAttributes;
			}
		}

		VkVertexInputBindingDescription vd[PipelineInfoVK::MaxVertexStreams] = {};
		vi.vertexBindingDescriptionCount                                     = 0;
		vi.pVertexBindingDescriptions                                        = vd;

		for (u32 i = 0; i < technique.vertexStreamCount; ++i)
		{
			vd[i].binding = i;
			vd[i].stride  = info.vertexBufferStride[i];
			vd[i].inputRate =
			    technique.instanceDataStream == i ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
			vi.vertexBindingDescriptionCount++;
		}

		// input assembly

		VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		createInfo.pInputAssemblyState            = &ia;
		ia.topology                               = convertPrimitiveType(info.primitiveType);
		ia.primitiveRestartEnable                 = false;

		// tessellation (not supported)

		// VkPipelineTessellationStateCreateInfo ts = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		// createInfo.pTessellationState = &ts;
		// ts.patchControlPoints = 0;

		// viewport

		VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		createInfo.pViewportState            = &vp;
		vp.viewportCount                     = 1;
		vp.pViewports                        = nullptr; // dynamic viewport state is used
		vp.scissorCount                      = 1;
		vp.pScissors                         = nullptr; // dynamic scissor state is used

		// rasterizer

		const GfxRasterizerDesc& rasterizerDesc = g_device->m_rasterizerStates[info.rasterizerStateHandle].desc;

		VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		createInfo.pRasterizationState            = &rs;
		rs.depthClampEnable                       = false;
		rs.rasterizerDiscardEnable                = false;
		rs.polygonMode = rasterizerDesc.fillMode == GfxFillMode::Solid ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE;
		rs.cullMode    = rasterizerDesc.cullMode == GfxCullMode::None ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
		rs.frontFace =
		    rasterizerDesc.cullMode == GfxCullMode::CCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
		rs.depthBiasEnable         = rasterizerDesc.depthBias != 0;
		rs.depthBiasConstantFactor = rasterizerDesc.depthBias;
		rs.depthBiasClamp          = 0.0f;
		rs.depthBiasSlopeFactor    = rasterizerDesc.depthBiasSlopeScale;
		rs.lineWidth               = 1.0f;

		// multisample

		VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		createInfo.pMultisampleState            = &ms;
		ms.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
		ms.sampleShadingEnable                  = false;
		ms.minSampleShading                     = 0.0f;
		ms.pSampleMask                          = nullptr;
		ms.alphaToCoverageEnable                = false;
		ms.alphaToOneEnable                     = false;

		// depth stencil

		const GfxDepthStencilDesc& depthStencilDesc = g_device->m_depthStencilStates[info.depthStencilStateHandle].desc;

		VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		createInfo.pDepthStencilState            = &ds;
		ds.depthTestEnable                       = depthStencilDesc.enable;
		ds.depthWriteEnable                      = depthStencilDesc.writeEnable;
		ds.depthCompareOp                        = convertCompareFunc(depthStencilDesc.compareFunc);
		ds.depthBoundsTestEnable                 = false;
		ds.stencilTestEnable                     = false;
		ds.back.failOp                           = VK_STENCIL_OP_KEEP;
		ds.back.passOp                           = VK_STENCIL_OP_KEEP;
		ds.back.compareOp                        = VK_COMPARE_OP_ALWAYS;
		ds.front                                 = ds.back;
		ds.minDepthBounds                        = 0.0f;
		ds.maxDepthBounds                        = 1.0f;

		// color blend

		VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		createInfo.pColorBlendState            = &cb;
		VkPipelineColorBlendAttachmentState colorAttachments[GfxPassDesc::MaxTargets] = {};

		BlendStateVK& blendState = g_device->m_blendStates[info.blendStateHandle];
		// TODO: support separate target blend states
		for (u32 i = 0; i < info.colorAttachmentCount; ++i)
		{
			colorAttachments[i].blendEnable         = blendState.desc.enable;
			colorAttachments[i].colorBlendOp        = convertBlendOp(blendState.desc.op);
			colorAttachments[i].srcColorBlendFactor = convertBlendParam(blendState.desc.src);
			colorAttachments[i].dstColorBlendFactor = convertBlendParam(blendState.desc.dst);
			colorAttachments[i].alphaBlendOp        = convertBlendOp(blendState.desc.alphaOp);
			colorAttachments[i].srcAlphaBlendFactor = convertBlendParam(blendState.desc.alphaSrc);
			colorAttachments[i].dstAlphaBlendFactor = convertBlendParam(blendState.desc.alphaDst);
			colorAttachments[i].colorWriteMask      = 0xF; // TODO: support color write mask
		}

		cb.attachmentCount = info.colorAttachmentCount;
		cb.pAttachments    = colorAttachments;

		// dynamic state

		VkPipelineDynamicStateCreateInfo dyn = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		createInfo.pDynamicState             = &dyn;

		VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		dyn.dynamicStateCount          = RUSH_COUNTOF(dynamicStates);
		dyn.pDynamicStates             = dynamicStates;

		// render pass

		RUSH_ASSERT(info.renderPass);

		createInfo.renderPass = info.renderPass;
		createInfo.subpass    = 0;

		// setup done

		V(vkCreateGraphicsPipelines(m_vulkanDevice, m_pipelineCache, 1, &createInfo, nullptr, &pipeline));
	}

	RUSH_ASSERT(pipeline);

	m_pipelines.insert(std::make_pair(key, pipeline));

	return pipeline;
}

VkRenderPass GfxDevice::createRenderPass(const GfxPassDesc& desc)
{
	RenderPassKey key = {};

	key.flags              = desc.flags;
	key.depthStencilFormat = g_device->m_textures[desc.depth].desc.format;

	u32 colorTargetCount = 0;
	for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
	{
		if (desc.color[i].valid())
		{
			colorTargetCount++;
			key.colorFormats[i] = g_device->m_textures[desc.color[i]].desc.format;
		}
		else
		{
			break;
		}
	}

	auto existingPass = m_renderPasses.find(key);
	if (existingPass != m_renderPasses.end())
	{
		return existingPass->second;
	}

	bool discardColor      = !!(desc.flags & GfxPassFlags::DiscardColor);
	bool clearColor        = !!(desc.flags & GfxPassFlags::ClearColor);
	bool clearDepthStencil = !!(desc.flags & GfxPassFlags::ClearDepthStencil);

	VkAttachmentDescription attachmentDesc[1 + GfxPassDesc::MaxTargets] = {};
	u32                     attachmentCount                             = 0;

	VkAttachmentReference depthAttachmentReference = {};

	if (desc.depth.valid())
	{
		auto& attachment = attachmentDesc[attachmentCount];

		attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp         = clearDepthStencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp  = clearDepthStencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachment.format         = convertFormat(g_device->m_textures[desc.depth].desc.format);

		depthAttachmentReference.attachment = attachmentCount;
		depthAttachmentReference.layout     = attachment.initialLayout;

		attachmentCount++;
	}

	VkAttachmentReference colorAttachmentReferences[GfxPassDesc::MaxTargets] = {};
	for (u32 i = 0; i < colorTargetCount; ++i)
	{
		auto& attachment = attachmentDesc[attachmentCount];

		attachment.format  = convertFormat(g_device->m_textures[desc.color[i]].desc.format);
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		if (discardColor)
		{
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}
		else if (clearColor)
		{
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}
		else
		{
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachmentReferences[i].attachment = attachmentCount;
		colorAttachmentReferences[i].layout     = attachment.initialLayout;

		attachmentCount++;
	}

	VkSubpassDescription subpassDesc    = {};
	subpassDesc.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount    = colorTargetCount;
	subpassDesc.pColorAttachments       = colorAttachmentReferences;
	subpassDesc.pDepthStencilAttachment = desc.depth.valid() ? &depthAttachmentReference : nullptr;

	VkRenderPassCreateInfo renderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	renderPassCreateInfo.attachmentCount        = attachmentCount;
	renderPassCreateInfo.pAttachments           = attachmentDesc;
	renderPassCreateInfo.subpassCount           = 1;
	renderPassCreateInfo.pSubpasses             = &subpassDesc;

	// setup done
	VkRenderPass renderPass = VK_NULL_HANDLE;

	V(vkCreateRenderPass(m_vulkanDevice, &renderPassCreateInfo, nullptr, &renderPass));

	m_renderPasses.insert(std::make_pair(key, renderPass));

	return renderPass;
}

VkFramebuffer GfxDevice::createFrameBuffer(const GfxPassDesc& desc, VkRenderPass renderPass)
{
	FrameBufferKey key = {};

	key.renderPass    = renderPass;
	key.depthBufferId = g_device->m_textures[desc.depth].id;
	for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
	{
		if (desc.color[i].valid())
		{
			key.colorBufferId[i] = g_device->m_textures[desc.color[i]].id;
		}
		else
		{
			break;
		}
	}

	auto existingFrameBuffer = m_frameBuffers.find(key);
	if (existingFrameBuffer != m_frameBuffers.end())
	{
		return existingFrameBuffer->second;
	}

	u32 width  = UINT_MAX;
	u32 height = UINT_MAX;

	u32         attachmentCount = 0;
	VkImageView frameBufferAttachments[1 + GfxPassDesc::MaxTargets];

	if (desc.depth.valid())
	{
		TextureVK& texture                      = g_device->m_textures[desc.depth];
		width                                   = min(width, texture.desc.width);
		height                                  = min(height, texture.desc.height);
		frameBufferAttachments[attachmentCount] = texture.depthStencilImageView;
		attachmentCount++;
	}

	for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
	{
		if (desc.color[i].valid())
		{
			TextureVK& texture                      = g_device->m_textures[desc.color[i]];
			width                                   = min(width, texture.desc.width);
			height                                  = min(height, texture.desc.height);
			frameBufferAttachments[attachmentCount] = texture.imageView;
			attachmentCount++;
		}
		else
		{
			break;
		}
	}

	RUSH_ASSERT(attachmentCount);

	VkFramebufferCreateInfo frameBufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};

	frameBufferCreateInfo.renderPass      = renderPass;
	frameBufferCreateInfo.attachmentCount = attachmentCount;
	frameBufferCreateInfo.pAttachments    = frameBufferAttachments;
	frameBufferCreateInfo.width           = width;
	frameBufferCreateInfo.height          = height;
	frameBufferCreateInfo.layers          = 1;

	// setup done

	VkFramebuffer frameBuffer = VK_NULL_HANDLE;

	V(vkCreateFramebuffer(m_vulkanDevice, &frameBufferCreateInfo, nullptr, &frameBuffer));

	m_frameBuffers.insert(std::make_pair(key, frameBuffer));

	return frameBuffer;
}

void GfxDevice::createSwapChain()
{
	V(vkQueueWaitIdle(m_graphicsQueue));

	if (!m_swapChainSurface)
	{
#if defined(RUSH_PLATFORM_WINDOWS)
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
		surfaceCreateInfo.hinstance                   = (HINSTANCE)GetModuleHandle(nullptr);
		surfaceCreateInfo.hwnd                        = *(HWND*)m_window->nativeHandle();
		V(vkCreateWin32SurfaceKHR(m_vulkanInstance, &surfaceCreateInfo, nullptr, &m_swapChainSurface));
#elif defined(RUSH_PLATFORM_LINUX)
		VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
		surfaceCreateInfo.connection                = (xcb_connection_t*)m_window->nativeConnection();
		surfaceCreateInfo.window                    = (xcb_window_t)(uintptr_t(m_window->nativeHandle()) & 0xFFFFFFFF);
		V(vkCreateXcbSurfaceKHR(m_vulkanInstance, &surfaceCreateInfo, nullptr, &m_swapChainSurface));
#else
		RUSH_LOG_FATAL("GfxDevice::createSwapChain() is not implemented");
#endif
	}

	VkBool32 graphicsQueueSupportsPresent = false;

	vkGetPhysicalDeviceSurfaceSupportKHR(
	    m_physicalDevice, m_graphicsQueueIndex, m_swapChainSurface, &graphicsQueueSupportsPresent);

	RUSH_ASSERT(graphicsQueueSupportsPresent); // TODO: support configurations when this is not the case

	VkSurfaceCapabilitiesKHR surfCaps;
	V(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_swapChainSurface, &surfCaps));

	m_swapChainExtent = surfCaps.currentExtent;

	u32 presentModeCount = 0;
	V(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_swapChainSurface, &presentModeCount, nullptr));

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	V(vkGetPhysicalDeviceSurfacePresentModesKHR(
	    m_physicalDevice, m_swapChainSurface, &presentModeCount, presentModes.data()));

	RUSH_ASSERT(
	    std::find(presentModes.begin(), presentModes.end(), m_pendingSwapChainPresentMode) != presentModes.end());

	u32 desiredSwapChainImageCount = max<u32>(m_desiredSwapChainImageCount, surfCaps.minImageCount);

	auto enumeratedSurfaceFormats = enumerateSurfaceFormats(m_physicalDevice, m_swapChainSurface);

	size_t preferredFormatIndex = 0;
	for (size_t i = 0; i < enumeratedSurfaceFormats.size(); ++i)
	{
		if (enumeratedSurfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
		    enumeratedSurfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			preferredFormatIndex = i;
			break;
		}
	}

	const VkFormat        swapChainColorFormat = enumeratedSurfaceFormats[preferredFormatIndex].format;
	const VkColorSpaceKHR swapChainColorSpace  = enumeratedSurfaceFormats[preferredFormatIndex].colorSpace;

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	swapChainCreateInfo.surface                  = m_swapChainSurface;
	swapChainCreateInfo.minImageCount            = desiredSwapChainImageCount;
	swapChainCreateInfo.imageFormat              = swapChainColorFormat;
	swapChainCreateInfo.imageColorSpace          = swapChainColorSpace;
	swapChainCreateInfo.imageExtent              = m_swapChainExtent;
	swapChainCreateInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	swapChainCreateInfo.preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.imageArrayLayers      = 1;
	swapChainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 0;
	swapChainCreateInfo.pQueueFamilyIndices   = nullptr;
	swapChainCreateInfo.presentMode           = m_pendingSwapChainPresentMode;
	swapChainCreateInfo.oldSwapchain          = m_swapChain;
	swapChainCreateInfo.clipped               = true;
	swapChainCreateInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	GfxTextureDesc depthBufferDesc = GfxTextureDesc::make2D(
	    m_swapChainExtent.width, m_swapChainExtent.height, GfxFormat_D32_Float_S8_Uint, GfxUsageFlags::DepthStencil);

	Gfx_Release(m_depthBufferTexture);
	m_depthBufferTexture = retainResource(m_textures, TextureVK::create(depthBufferDesc, nullptr, 0));

	auto oldSwapChain = m_swapChain;

	V(vkCreateSwapchainKHR(m_vulkanDevice, &swapChainCreateInfo, nullptr, &m_swapChain));

	if (oldSwapChain)
	{
		vkDestroySwapchainKHR(m_vulkanDevice, oldSwapChain, nullptr);
	}

	u32 swapChainImageCount = 0;
	V(vkGetSwapchainImagesKHR(m_vulkanDevice, m_swapChain, &swapChainImageCount, nullptr));

	RUSH_ASSERT(m_frameData.empty() || m_frameData.size() == swapChainImageCount);

	m_frameData.resize(swapChainImageCount);
	m_swapChainImages.resize(swapChainImageCount);

	for (auto it : m_swapChainTextures)
	{
		Gfx_Release(it);
	}
	m_swapChainTextures.resize(swapChainImageCount);

	V(vkGetSwapchainImagesKHR(m_vulkanDevice, m_swapChain, &swapChainImageCount, m_swapChainImages.data()));

	for (u32 i = 0; i < swapChainImageCount; ++i)
	{
		GfxFormat swapChainFormat;
		switch (swapChainColorFormat)
		{
		default: RUSH_BREAK;

		case VK_FORMAT_B8G8R8A8_UNORM: swapChainFormat = GfxFormat_BGRA8_Unorm; break;
		case VK_FORMAT_R8G8B8A8_UNORM: swapChainFormat = GfxFormat_RGBA8_Unorm; break;
		}

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(
		    m_swapChainExtent.width, m_swapChainExtent.height, swapChainFormat, GfxUsageFlags::RenderTarget);
		m_swapChainTextures[i] =
		    retainResource(m_textures, TextureVK::create(textureDesc, m_swapChainImages[i], VK_IMAGE_LAYOUT_UNDEFINED));
	}

	m_swapChainPresentMode = m_pendingSwapChainPresentMode;
}

inline void recycleContext(GfxContext* context)
{
	GfxContext* temp = allocateContext(context->m_type);

	std::swap(context->m_commandBuffer, temp->m_commandBuffer);
	std::swap(context->m_fence, temp->m_fence);

	g_device->enqueueDestroyContext(temp);
}

void GfxDevice::beginFrame()
{
	V(vkAcquireNextImageKHR(
	    m_vulkanDevice, m_swapChain, UINT64_MAX, m_presentCompleteSemaphore, VK_NULL_HANDLE, &m_swapChainIndex));

	m_currentFrame             = &m_frameData[m_swapChainIndex];
	m_currentFrame->frameIndex = g_device->m_frameCount;

	if (m_currentFrame->lastGraphicsFence)
	{
		V(vkWaitForFences(g_vulkanDevice, 1, &m_currentFrame->lastGraphicsFence, true, UINT64_MAX));
		m_currentFrame->lastGraphicsFence = VK_NULL_HANDLE;
	}

	m_currentFrame->destructionQueue.flush(m_vulkanDevice);

	m_currentFrame->localOnlyAllocator.reset();
	m_currentFrame->hostVisibleAllocator.reset();
	m_currentFrame->hostOnlyAllocator.reset();
	m_currentFrame->hostOnlyCachedAllocator.reset();

	m_currentFrame->presentSemaphoreWaited = false;

	V(vkResetDescriptorPool(m_vulkanDevice, m_currentFrame->descriptorPool, 0));
}

void GfxDevice::endFrame() {}

u32 GfxDevice::memoryTypeFromProperties(u32 memoryTypeBits, VkFlags requiredFlags, VkFlags incompatibleFlags)
{
	static_assert(VK_MAX_MEMORY_TYPES == 32,
	    "VK_MAX_MEMORY_TYPES is expected to be 32 to match VkMemoryRequirements::memoryTypeBits");
	for (u32 i = 0; i < 32; i++)
	{
		if ((memoryTypeBits & 1) == 1)
		{
			if (m_deviceMemoryProps.memoryTypes[i].propertyFlags & incompatibleFlags)
			{
				continue;
			}

			if ((m_deviceMemoryProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)
			{
				return i;
			}
		}
		memoryTypeBits >>= 1;
	}

	return 0xFFFFFFFF;
}

void GfxDevice::enqueueDestroyMemory(VkDeviceMemory object)
{
	m_currentFrame->destructionQueue.memory.push_back(object);
}

void GfxDevice::enqueueDestroyBuffer(VkBuffer object) { m_currentFrame->destructionQueue.buffers.push_back(object); }

void GfxDevice::enqueueDestroyImage(VkImage object) { m_currentFrame->destructionQueue.images.push_back(object); }

void GfxDevice::enqueueDestroyImageView(VkImageView object)
{
	m_currentFrame->destructionQueue.imageViews.push_back(object);
}

void GfxDevice::enqueueDestroyBufferView(VkBufferView object)
{
	m_currentFrame->destructionQueue.bufferViews.push_back(object);
}

void GfxDevice::enqueueDestroyContext(GfxContext* object)
{
	m_currentFrame->destructionQueue.contexts.push_back(object);
}

void GfxDevice::enqueueDestroySampler(VkSampler object) { m_currentFrame->destructionQueue.samplers.push_back(object); }

u32 GfxDevice::generateId() { return m_uniqueResourceCounter++; }

void MemoryAllocatorVK::init(u32 memoryType, bool hostVisible)
{
	RUSH_ASSERT(m_availableBlocks.empty() && m_fullBlocks.empty());
	m_memoryType  = memoryType;
	m_hostVisible = hostVisible;
}

MemoryBlockVK MemoryAllocatorVK::alloc(u64 size, u64 alignment)
{
	for (;;)
	{
		if (m_availableBlocks.empty())
		{
			u64 blockSize = max<u64>(size, m_defaultBlockSize);
			m_availableBlocks.push_back(allocBlock(blockSize));
		}

		MemoryBlockVK& currentBlock  = m_availableBlocks.back();
		u64            alignedOffset = alignCeiling(currentBlock.offset, alignment);

		if (alignedOffset + size > currentBlock.size)
		{
			m_fullBlocks.push_back(currentBlock);
			m_availableBlocks.pop_back();
			continue;
		}

		MemoryBlockVK result;

		result.memory       = currentBlock.memory;
		result.offset       = alignedOffset;
		result.size         = size;
		result.buffer       = currentBlock.buffer;
		result.mappedBuffer = offsetPtr(currentBlock.mappedBuffer, (size_t)alignedOffset);

		currentBlock.offset = alignedOffset + size;

		return result;
	}
}

void MemoryAllocatorVK::reset()
{
	if (!m_availableBlocks.empty())
	{
		m_availableBlocks.back().offset = 0;
	}

	for (MemoryBlockVK& block : m_fullBlocks)
	{
		block.offset = 0;
		m_availableBlocks.push_back(block);
	}
	m_fullBlocks.clear();
}

void MemoryAllocatorVK::releaseBlocks()
{
	for (MemoryBlockVK& block : m_fullBlocks)
	{
		freeBlock(block);
	}

	for (MemoryBlockVK& block : m_availableBlocks)
	{
		freeBlock(block);
	}

	m_fullBlocks.clear();
	m_availableBlocks.clear();
}

MemoryBlockVK MemoryAllocatorVK::allocBlock(u64 blockSize)
{
	MemoryBlockVK block = {};
	block.size          = blockSize;

	VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
	                         VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
	                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
	                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
	                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	bufferCreateInfo.size = blockSize;

	V(vkCreateBuffer(g_vulkanDevice, &bufferCreateInfo, nullptr, &block.buffer));

	VkMemoryRequirements memoryReq = {};
	vkGetBufferMemoryRequirements(g_vulkanDevice, block.buffer, &memoryReq);

	VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize       = memoryReq.size;
	allocInfo.memoryTypeIndex      = m_memoryType;

	V(vkAllocateMemory(g_vulkanDevice, &allocInfo, nullptr, &block.memory));
	V(vkBindBufferMemory(g_vulkanDevice, block.buffer, block.memory, 0));

	if (m_hostVisible)
	{
		V(vkMapMemory(g_vulkanDevice, block.memory, 0, blockSize, 0, &block.mappedBuffer));
	}

	return block;
}

void MemoryAllocatorVK::freeBlock(MemoryBlockVK block)
{
	if (m_hostVisible)
	{
		vkUnmapMemory(g_vulkanDevice, block.memory);
	}

	g_device->enqueueDestroyBuffer(block.buffer);
	g_device->enqueueDestroyMemory(block.memory);
}

inline VkCommandPool getCommandPoolByContextType(GfxContextType contextType)
{
	switch (contextType)
	{
	default: return VK_NULL_HANDLE;
	case GfxContextType::Graphics: return g_device->m_graphicsCommandPool;
	case GfxContextType::Compute: return g_device->m_computeCommandPool;
	case GfxContextType::Transfer: return g_device->m_transferCommandPool;
	}
}

GfxContext::GfxContext(GfxContextType contextType) : m_type(contextType)
{
	VkCommandPool commandPool = getCommandPoolByContextType(contextType);

	RUSH_ASSERT(g_vulkanDevice);
	RUSH_ASSERT(commandPool);

	std::memset(&m_currentRenderRect, 0, sizeof(m_currentRenderRect));
	std::memset(&m_pending.constantBufferOffsets, 0, sizeof(m_pending.constantBufferOffsets));
	std::memset(&m_pending.vertexBufferStride, 0, sizeof(m_pending.vertexBufferStride));

	VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fenceCreateInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(g_vulkanDevice, &fenceCreateInfo, nullptr, &m_fence);

	VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocateInfo.commandPool                 = commandPool;
	allocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount          = 1;

	V(vkAllocateCommandBuffers(g_vulkanDevice, &allocateInfo, &m_commandBuffer));

	VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	V(vkCreateSemaphore(g_vulkanDevice, &semaphoreCreateInfo, nullptr, &m_completionSemaphore));
}

GfxContext::~GfxContext()
{
	V(vkWaitForFences(g_vulkanDevice, 1, &m_fence, true, UINT64_MAX));

	VkCommandPool commandPool = getCommandPoolByContextType(m_type);

	RUSH_ASSERT(!m_isActive);
	vkFreeCommandBuffers(g_vulkanDevice, commandPool, 1, &m_commandBuffer);

	vkDestroyFence(g_vulkanDevice, m_fence, nullptr);
	vkDestroySemaphore(g_vulkanDevice, m_completionSemaphore, nullptr);
}

void GfxContext::beginBuild()
{
	RUSH_ASSERT(!m_isActive);

	m_isActive = true;

	m_lastUsedFrame = g_device->m_frameCount;

	V(vkWaitForFences(g_vulkanDevice, 1, &m_fence, true, UINT64_MAX));
	V(vkResetFences(g_vulkanDevice, 1, &m_fence));

	VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	V(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));

	m_activePipeline     = VK_NULL_HANDLE;
	m_dirtyState         = 0xFFFFFFFF;
	m_isRenderPassActive = false;

	m_currentFrameBuffer          = VK_NULL_HANDLE;
	m_currentRenderPass           = VK_NULL_HANDLE;
	m_currentColorAttachmentCount = 0;

	m_waitSemaphores.clear();
	m_waitDstStageMasks.clear();

	m_useCompletionSemaphore = false;
}

void GfxContext::endBuild()
{
	flushBarriers();

	RUSH_ASSERT(m_isActive);
	m_isActive = false;

	V(vkEndCommandBuffer(m_commandBuffer));

	m_pending.technique.reset();
	m_pending.indexBuffer.reset();
	m_pending.blendState.reset();
	m_pending.depthStencilState.reset();
	m_pending.rasterizerState.reset();

	for (auto& it : m_pending.vertexBuffer)
		it.reset();
	for (auto& it : m_pending.constantBuffers)
		it.reset();
	for (auto& it : m_pending.textures)
		it.reset();
	for (auto& it : m_pending.samplers)
		it.reset();
	for (auto& it : m_pending.storageImages)
		it.reset();
	for (auto& it : m_pending.storageBuffers)
		it.reset();

	m_pending = PendingState();
}

void GfxContext::addDependency(VkSemaphore waitSemaphore, VkPipelineStageFlags waitDstStageMask)
{
	m_waitSemaphores.push_back(waitSemaphore);
	m_waitDstStageMasks.push_back(waitDstStageMask);
}

void GfxContext::split()
{
	RUSH_ASSERT_MSG(m_type == GfxContextType::Graphics, "Splitting only implemented for graphics contexts.");

	endBuild();

	submit(g_device->m_graphicsQueue);

	recycleContext(this);

	beginBuild();
}

void GfxContext::submit(VkQueue queue)
{
	if (!m_pendingBufferUploads.empty())
	{
		GfxContext* uploadContext = getUploadContext();
		for (const BufferCopyCommand& cmd : m_pendingBufferUploads)
		{
			vkCmdCopyBuffer(uploadContext->m_commandBuffer, cmd.srcBuffer, cmd.dstBuffer, 1, &cmd.region);
		}
		g_device->flushUploadContext(this);
		m_pendingBufferUploads.clear();
	}

	if (queue == g_device->m_graphicsQueue && !g_device->m_currentFrame->presentSemaphoreWaited)
	{
		addDependency(g_device->m_presentCompleteSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		g_device->m_currentFrame->presentSemaphoreWaited = true;
	}

	VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

	submitInfo.waitSemaphoreCount   = (u32)m_waitSemaphores.size();
	submitInfo.pWaitSemaphores      = m_waitSemaphores.data();
	submitInfo.pSignalSemaphores    = &m_completionSemaphore;
	submitInfo.signalSemaphoreCount = m_useCompletionSemaphore ? 1 : 0;
	submitInfo.commandBufferCount   = 1;
	submitInfo.pCommandBuffers      = &m_commandBuffer;
	submitInfo.pWaitDstStageMask    = m_waitDstStageMasks.data();

	V(vkQueueSubmit(queue, 1, &submitInfo, m_fence));
}

void GfxContext::addImageBarrier(VkImage image, VkImageLayout nextLayout, VkImageLayout currentLayout,
    VkImageSubresourceRange* subresourceRange, bool force)
{
	if (currentLayout == nextLayout && !force)
	{
		return;
	}

	// TODO: track subresource states
	// TODO: initialize default subresource range to cover entire image
	VkImageSubresourceRange defaultSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	VkImageMemoryBarrier barrierDesc = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrierDesc.srcAccessMask        = 0;
	barrierDesc.dstAccessMask        = 0;
	barrierDesc.oldLayout            = currentLayout;
	barrierDesc.newLayout            = nextLayout;
	barrierDesc.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
	barrierDesc.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
	barrierDesc.subresourceRange     = subresourceRange ? *subresourceRange : defaultSubresourceRange;
	barrierDesc.image                = image;

	// conservative default flags
	VkPipelineStageFlagBits srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlagBits dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// current layout

	// TODO: move layout->flags conversion into a function

	switch (currentLayout)
	{
	default: RUSH_LOG_ERROR("Unexpected layout"); break;
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// nothing
		break;
	case VK_IMAGE_LAYOUT_PREINITIALIZED: barrierDesc.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT; break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrierDesc.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
		srcStageMask =
		    VkPipelineStageFlagBits(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrierDesc.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: barrierDesc.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT; break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: barrierDesc.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT; break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrierDesc.srcAccessMask |=
		    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		srcStageMask = VkPipelineStageFlagBits(
		    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrierDesc.srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
		srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		barrierDesc.srcAccessMask |= VK_ACCESS_MEMORY_READ_BIT;
		srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	}

	switch (nextLayout)
	{
	default: RUSH_LOG_ERROR("Unexpected layout"); break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrierDesc.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
		// TODO: vertex shader read?
		dstStageMask =
		    VkPipelineStageFlagBits(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrierDesc.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: barrierDesc.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT; break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrierDesc.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrierDesc.dstAccessMask |=
		    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		dstStageMask = VkPipelineStageFlagBits(
		    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		barrierDesc.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		barrierDesc.dstAccessMask |= VK_ACCESS_MEMORY_READ_BIT;
		dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	}

	if (m_type == GfxContextType::Transfer)
	{
		srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	m_pendingBarriers.srcStageMask |= srcStageMask;
	m_pendingBarriers.dstStageMask |= dstStageMask;
	m_pendingBarriers.imageBarriers.push_back(barrierDesc);

	// flushBarriers();
}

void GfxContext::addBufferBarrier(GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
    VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage)
{
	auto& buffer = g_device->m_buffers[h];

	VkBufferMemoryBarrier barrierDesc = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	barrierDesc.srcAccessMask         = srcAccess;
	barrierDesc.dstAccessMask         = dstAccess;
	barrierDesc.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
	barrierDesc.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
	barrierDesc.buffer                = buffer.info.buffer;
	barrierDesc.offset                = buffer.info.offset;
	barrierDesc.size                  = buffer.info.range;

	m_pendingBarriers.bufferBarriers.push_back(barrierDesc);

	m_pendingBarriers.srcStageMask |= srcStage;
	m_pendingBarriers.dstStageMask |= dstStage;
}

void GfxContext::flushBarriers()
{
	if (m_pendingBarriers.srcStageMask == 0 && m_pendingBarriers.dstStageMask == 0)
	{
		return;
	}

	vkCmdPipelineBarrier(m_commandBuffer, m_pendingBarriers.srcStageMask, m_pendingBarriers.dstStageMask,
	    0,          // dependency flags
	    0, nullptr, // memory barriers
	    u32(m_pendingBarriers.bufferBarriers.size()), m_pendingBarriers.bufferBarriers.data(),
	    u32(m_pendingBarriers.imageBarriers.size()), m_pendingBarriers.imageBarriers.data());

	m_pendingBarriers.imageBarriers.clear();
	m_pendingBarriers.bufferBarriers.clear();

	m_pendingBarriers.srcStageMask = 0;
	m_pendingBarriers.dstStageMask = 0;
}

void GfxContext::beginRenderPass(const GfxPassDesc& desc)
{
	RUSH_ASSERT(m_type == GfxContextType::Graphics);
	RUSH_ASSERT(!m_isRenderPassActive);
	RUSH_ASSERT(m_currentRenderPass == VK_NULL_HANDLE);

	m_pendingClear.color   = desc.clearColors[0];
	m_pendingClear.depth   = desc.clearDepth;
	m_pendingClear.stencil = desc.clearStencil;

	m_pendingClear.flags = GfxClearFlags::None;

	if (!!(desc.flags & GfxPassFlags::ClearColor))
	{
		m_pendingClear.flags = m_pendingClear.flags | GfxClearFlags::Color;
	}

	if (!!(desc.flags & GfxPassFlags::ClearDepthStencil))
	{
		m_pendingClear.flags = m_pendingClear.flags | GfxClearFlags::DepthStencil;
	}

	m_currentRenderRect.extent.width  = UINT_MAX;
	m_currentRenderRect.extent.height = UINT_MAX;

	u32          clearValueCount = 0;
	VkClearValue clearValues[1 + GfxPassDesc::MaxTargets];
	if (desc.depth.valid())
	{
		TextureVK& texture                = g_device->m_textures[desc.depth];
		m_currentRenderRect.extent.width  = min(m_currentRenderRect.extent.width, texture.desc.width);
		m_currentRenderRect.extent.height = min(m_currentRenderRect.extent.height, texture.desc.height);

		// TODO: track subresource states
		// TODO: initialize default subresource range to cover entire image
		VkImageSubresourceRange subresourceRange = {
		    aspectFlagsFromFormat(texture.desc.format), 0, texture.desc.mips, 0, 1};

		addImageBarrier(
		    texture.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, texture.currentLayout, &subresourceRange);
		texture.currentLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		clearValues[clearValueCount] = m_pendingClear.getClearDepthStencil();
		++clearValueCount;
	}

	const bool shouldClearColor = !!(desc.flags & GfxPassFlags::ClearColor);

	for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
	{
		if (desc.color[i].valid())
		{
			TextureVK& texture                = g_device->m_textures[desc.color[i]];
			m_currentRenderRect.extent.width  = min(m_currentRenderRect.extent.width, texture.desc.width);
			m_currentRenderRect.extent.height = min(m_currentRenderRect.extent.height, texture.desc.height);
			addImageBarrier(texture.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, texture.currentLayout);
			texture.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (shouldClearColor)
			{
				clearValues[clearValueCount] = m_pendingClear.getClearColor();
				++clearValueCount;
			}
		}
		else
		{
			break;
		}
	}

	VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	renderPassBeginInfo.renderPass            = g_device->createRenderPass(desc);
	renderPassBeginInfo.framebuffer           = g_device->createFrameBuffer(desc, renderPassBeginInfo.renderPass);
	renderPassBeginInfo.renderArea            = m_currentRenderRect;
	if (m_pendingClear.flags != GfxClearFlags::None)
	{
		renderPassBeginInfo.clearValueCount = clearValueCount;
		renderPassBeginInfo.pClearValues    = clearValues;
	}

	flushBarriers();

	vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_currentRenderPassDesc       = desc;
	m_currentRenderPass           = renderPassBeginInfo.renderPass;
	m_currentColorAttachmentCount = desc.getColorTargetCount();

	m_isRenderPassActive = true;

	Gfx_SetViewport(this, Tuple2u{m_currentRenderRect.extent.width, m_currentRenderRect.extent.height});
	Gfx_SetScissorRect(this, Tuple2u{m_currentRenderRect.extent.width, m_currentRenderRect.extent.height});
}

void GfxContext::endRenderPass()
{
	RUSH_ASSERT(m_isRenderPassActive);
	vkCmdEndRenderPass(m_commandBuffer);
	m_isRenderPassActive = false;
	m_currentRenderPass  = VK_NULL_HANDLE;
}

void GfxContext::applyState()
{
	if (m_dirtyState == 0)
	{
		return;
	}

	if (m_dirtyState & DirtyStateFlag_Pipeline)
	{
		PipelineInfoVK info = {};

		info.techniqueHandle         = m_pending.technique.get();
		info.primitiveType           = m_pending.primitiveType;
		info.depthStencilStateHandle = m_pending.depthStencilState.get();
		info.rasterizerStateHandle   = m_pending.rasterizerState.get();
		info.blendStateHandle        = m_pending.blendState.get();
		for (u32 i = 0; i < RUSH_COUNTOF(info.vertexBufferStride); ++i)
		{
			info.vertexBufferStride[i] = m_pending.vertexBufferStride[i];
		}
		info.renderPass           = m_currentRenderPass;
		info.colorAttachmentCount = m_currentColorAttachmentCount;

		m_activePipeline = g_device->createPipeline(info);
	}

	RUSH_ASSERT(m_pending.technique.valid());
	TechniqueVK& technique = g_device->m_techniques[m_pending.technique.get()];

	VkPipelineBindPoint bindPoint =
	    technique.cs.valid() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	if (m_dirtyState & DirtyStateFlag_Pipeline)
	{
		vkCmdBindPipeline(m_commandBuffer, bindPoint, m_activePipeline);
	}

	if (m_dirtyState & DirtyStateFlag_VertexBuffer)
	{
		for (u32 i = 0; i < technique.vertexStreamCount; ++i)
		{
			RUSH_ASSERT(m_pending.vertexBuffer[i].valid());

			BufferVK& buffer = g_device->m_buffers[m_pending.vertexBuffer[i].get()];
			validateBufferUse(buffer);

			VkDeviceSize bufferOffset = buffer.info.offset;
			vkCmdBindVertexBuffers(m_commandBuffer, i, 1, &buffer.info.buffer, &bufferOffset);
		}
	}

	if (m_pending.indexBuffer.valid() && (m_dirtyState & DirtyStateFlag_IndexBuffer))
	{
		BufferVK& buffer = g_device->m_buffers[m_pending.indexBuffer.get()];
		validateBufferUse(buffer);

		VkDeviceSize offset = buffer.info.offset;
		vkCmdBindIndexBuffer(m_commandBuffer, buffer.info.buffer, offset,
		    buffer.desc.stride == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
	}

	static const VkWriteDescriptorSet defaultDescriptor  = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	static const u32                  maxDescriptorCount = GfxContext::MaxTextures + GfxContext::MaxConstantBuffers +
	                                      GfxContext::MaxStorageImages + GfxContext::MaxStorageBuffers;

	VkWriteDescriptorSet descriptors[maxDescriptorCount];
	u32                  descriptorCount = 0;

	// allocate descriptor set
	// assume the technique will be used many times and there will be a benefit from batching the allocations
	// todo: cache descriptors by descriptor set layout, not simply by technique

	if (technique.descriptorSetCacheFrame != g_device->m_frameCount)
	{
		technique.descriptorSetCache.clear();
		technique.descriptorSetCacheFrame = g_device->m_frameCount;
	}

	if (technique.descriptorSetCache.empty())
	{
		static const u32 cacheBatchSize = 16;

		VkDescriptorSet       cachedDescriptorSets[cacheBatchSize];
		VkDescriptorSetLayout setLayouts[cacheBatchSize];
		for (u32 i = 0; i < cacheBatchSize; ++i)
		{
			setLayouts[i] = technique.descriptorSetLayout;
		}

		VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocInfo.descriptorPool              = g_device->m_currentFrame->descriptorPool;
		allocInfo.descriptorSetCount          = cacheBatchSize;
		allocInfo.pSetLayouts                 = setLayouts;
		V(vkAllocateDescriptorSets(g_vulkanDevice, &allocInfo, cachedDescriptorSets));

		for (VkDescriptorSet it : cachedDescriptorSets)
		{
			technique.descriptorSetCache.push_back(it);
		}
	}

	VkDescriptorSet descriptorSet = technique.descriptorSetCache.back();
	technique.descriptorSetCache.pop_back();

	// fill descriptors

	u32 bindingCount = 0;

	VkDescriptorBufferInfo pendingConstantBufferInfo[MaxConstantBuffers];

	for (u32 i = 0; i < technique.constantBufferCount; ++i)
	{
		RUSH_ASSERT(m_pending.constantBuffers[i].valid());

		RUSH_ASSERT(descriptorCount < maxDescriptorCount);

		BufferVK& buffer = g_device->m_buffers[m_pending.constantBuffers[i].get()];
		validateBufferUse(buffer);

		pendingConstantBufferInfo[i] = buffer.info;
		pendingConstantBufferInfo[i].offset += m_pending.constantBufferOffsets[i];
		pendingConstantBufferInfo[i].range = buffer.info.range - m_pending.constantBufferOffsets[i];

		descriptors[descriptorCount] = defaultDescriptor;

		descriptors[descriptorCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptors[descriptorCount].dstSet          = descriptorSet;
		descriptors[descriptorCount].descriptorCount = 1;
		descriptors[descriptorCount].dstBinding      = bindingCount++;
		descriptors[descriptorCount].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptors[descriptorCount].pBufferInfo     = &pendingConstantBufferInfo[i];
		++descriptorCount;
	}

	static const VkDescriptorImageInfo defaultImageInfo = {};

	static const u32 maxImageInfoCount  = GfxContext::MaxStorageImages + GfxContext::MaxTextures;
	static const u32 maxBufferInfoCount = GfxContext::MaxStorageBuffers;

	VkDescriptorImageInfo imageInfos[maxImageInfoCount];
	u32                   imageInfoCount = 0;

	// Samplers

	for (u32 i = 0; i < technique.samplerCount; ++i)
	{
		RUSH_ASSERT(m_pending.samplers[i].valid());
		RUSH_ASSERT(descriptorCount < maxDescriptorCount);
		RUSH_ASSERT(imageInfoCount < maxImageInfoCount);

		SamplerVK& sampler = g_device->m_samplers[m_pending.samplers[i].get()];

		imageInfos[imageInfoCount]         = defaultImageInfo;
		imageInfos[imageInfoCount].sampler = sampler.native;

		descriptors[descriptorCount] = defaultDescriptor;

		descriptors[descriptorCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptors[descriptorCount].dstSet          = descriptorSet;
		descriptors[descriptorCount].descriptorCount = 1;
		descriptors[descriptorCount].dstBinding      = bindingCount++;
		descriptors[descriptorCount].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptors[descriptorCount].pImageInfo      = &imageInfos[imageInfoCount];

		++descriptorCount;
		++imageInfoCount;
	}

	// Textures (sampled images)

	for (u32 i = 0; i < technique.sampledImageCount; ++i)
	{
		RUSH_ASSERT(m_pending.textures[i].valid());
		RUSH_ASSERT(descriptorCount < maxDescriptorCount);
		RUSH_ASSERT(imageInfoCount < maxImageInfoCount);

		TextureVK& texture = g_device->m_textures[m_pending.textures[i].get()];

		imageInfos[imageInfoCount] = defaultImageInfo;

		imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[imageInfoCount].imageView   = texture.imageView;

		// TODO: track subresource states
		VkImageSubresourceRange subresourceRange = {
		    aspectFlagsFromFormat(texture.desc.format), 0, texture.desc.mips, 0, 1};

		addImageBarrier(
		    texture.image, imageInfos[imageInfoCount].imageLayout, texture.currentLayout, &subresourceRange);
		texture.currentLayout = imageInfos[imageInfoCount].imageLayout;

		descriptors[descriptorCount] = defaultDescriptor;

		descriptors[descriptorCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptors[descriptorCount].dstSet          = descriptorSet;
		descriptors[descriptorCount].descriptorCount = 1;
		descriptors[descriptorCount].dstBinding      = bindingCount++;
		descriptors[descriptorCount].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptors[descriptorCount].pImageInfo      = &imageInfos[imageInfoCount];

		++descriptorCount;
		++imageInfoCount;
	}

	// RWTextures (storage images)

	for (u32 i = 0; i < technique.storageImageCount; ++i)
	{
		RUSH_ASSERT(m_pending.storageImages[i].valid());
		RUSH_ASSERT(descriptorCount < maxDescriptorCount);
		RUSH_ASSERT(imageInfoCount < maxImageInfoCount);

		TextureVK& texture = g_device->m_textures[m_pending.storageImages[i].get()];

		imageInfos[imageInfoCount] = defaultImageInfo;

		imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfos[imageInfoCount].imageView   = texture.imageView;
		imageInfos[imageInfoCount].sampler     = VK_NULL_HANDLE;

		addImageBarrier(texture.image, imageInfos[imageInfoCount].imageLayout, texture.currentLayout, nullptr, true);
		texture.currentLayout = imageInfos[imageInfoCount].imageLayout;

		descriptors[descriptorCount] = defaultDescriptor;

		descriptors[descriptorCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptors[descriptorCount].dstSet          = descriptorSet;
		descriptors[descriptorCount].descriptorCount = 1;
		descriptors[descriptorCount].dstBinding      = bindingCount++;
		descriptors[descriptorCount].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptors[descriptorCount].pImageInfo      = &imageInfos[imageInfoCount];

		++descriptorCount;
		++imageInfoCount;
	}

	// RWBuffers (storage buffers)

	for (u32 i = 0; i < technique.storageBufferCount + technique.typedStorageBufferCount; ++i)
	{
		RUSH_ASSERT(m_pending.storageBuffers[i].valid());
		RUSH_ASSERT(descriptorCount < maxDescriptorCount);

		BufferVK& buffer = g_device->m_buffers[m_pending.storageBuffers[i].get()];
		validateBufferUse(buffer);

		const bool isTyped = i > technique.storageBufferCount;

		descriptors[descriptorCount] = defaultDescriptor;

		descriptors[descriptorCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptors[descriptorCount].dstSet          = descriptorSet;
		descriptors[descriptorCount].descriptorCount = 1;
		descriptors[descriptorCount].dstBinding      = bindingCount++;

		if (isTyped)
		{
			RUSH_ASSERT(buffer.bufferView != VK_NULL_HANDLE);
			descriptors[descriptorCount].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
			descriptors[descriptorCount].pTexelBufferView = &buffer.bufferView;
		}
		else
		{
			RUSH_ASSERT(buffer.info.buffer != VK_NULL_HANDLE);
			descriptors[descriptorCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptors[descriptorCount].pBufferInfo    = &buffer.info;
		}

		++descriptorCount;
	}

	vkUpdateDescriptorSets(g_vulkanDevice, descriptorCount, descriptors, 0, nullptr);

	vkCmdBindDescriptorSets(m_commandBuffer, bindPoint, technique.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	m_dirtyState = 0;
}

static GfxContext* allocateContext(GfxContextType type)
{
	GfxContext* result = nullptr;

	auto& pool = g_device->m_freeContexts[u32(type)];

	if (pool.empty())
	{
		result = new GfxContext(type);
		Gfx_Retain(result);
	}
	else
	{
		result = pool.back();
		pool.pop_back();
	}

	return result;
}

// device
GfxDevice* Gfx_CreateDevice(Window* window, const GfxConfig& cfg)
{
	RUSH_ASSERT(g_device == nullptr);
	GfxDevice* dev = new GfxDevice(window, cfg);
	RUSH_ASSERT(dev == g_device);

	return dev;
}

void Gfx_Release(GfxDevice* dev)
{
	if (dev->removeReference() > 1)
		return;

	Gfx_Release(dev->m_depthBufferTexture);
	for (GfxTexture it : dev->m_swapChainTextures)
	{
		Gfx_Release(it);
	}

	delete dev;

	g_device = nullptr;
}

static void getQueryPoolResults(VkDevice device, VkQueryPool pool, u32 count, std::vector<u64>& output)
{
	output.resize(count);
	u32 stride = sizeof(*output.data());
	vkGetQueryPoolResults(device,
	    pool,
	    0,
	    (u32)output.size(),
	    (u32)output.size() * stride,
	    output.data(),
	    stride,
	    VK_QUERY_RESULT_64_BIT);
}

static GfxContext* getUploadContext()
{
	if (g_device->m_currentUploadContext == nullptr)
	{
		GfxContextType contextType = g_device->m_transferQueue ? GfxContextType::Transfer : GfxContextType::Graphics;

		g_device->m_currentUploadContext = allocateContext(contextType);
		g_device->m_currentUploadContext->beginBuild();
	}
	return g_device->m_currentUploadContext;
}

void GfxDevice::flushUploadContext(GfxContext* dependentContext, bool waitForCompletion)
{
	if (!m_currentUploadContext)
		return;

	m_currentUploadContext->endBuild();

	if (dependentContext)
	{
		m_currentUploadContext->m_useCompletionSemaphore = true;
		dependentContext->addDependency(
		    m_currentUploadContext->m_completionSemaphore, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	VkQueue queue = g_device->m_transferQueue ? g_device->m_transferQueue : g_device->m_graphicsQueue;

	m_currentUploadContext->submit(queue);

	enqueueDestroyContext(m_currentUploadContext);
	m_currentUploadContext = nullptr;

	if (waitForCompletion)
	{
		V(vkQueueWaitIdle(queue));
	}
}

static void writeTimestamp(GfxContext* context, u32 slotIndex, VkPipelineStageFlagBits stageFlags)
{
	g_device->m_currentFrame->timestampSlotMap[slotIndex] = u16(g_device->m_currentFrame->timestampIssuedCount);
	vkCmdWriteTimestamp(context->m_commandBuffer,
	    stageFlags,
	    g_device->m_currentFrame->timestampPool,
	    g_device->m_currentFrame->timestampIssuedCount++);
}

void Gfx_BeginFrame()
{
	if (!g_device->m_resizeEvents.empty() ||
	    g_device->m_pendingSwapChainPresentMode != g_device->m_swapChainPresentMode)
	{
		g_device->createSwapChain();
		g_device->m_resizeEvents.clear();
	}

	if (g_device->m_frameCount == 0 && g_device->m_currentUploadContext)
	{
		// Upload initial data (before first frame)
		g_device->flushUploadContext(nullptr, true);
	}

	g_device->beginFrame();

	recycleContext(g_context);

#if 0
	if (vkGetFenceStatus(g_vulkanDevice, g_context->m_fence) == VK_NOT_READY)
	{
		RUSH_LOG_ERROR("Allocated immediage context is still in used by GPU. This is likely a bug in command buffer management implementation.");
	}
#endif

	g_context->beginBuild();

	static constexpr u16 InvalidTimestampSlotIndex = 0xFFFF;

	if (g_device->m_currentFrame->timestampIssuedCount)
	{
		getQueryPoolResults(g_vulkanDevice,
		    g_device->m_currentFrame->timestampPool,
		    g_device->m_currentFrame->timestampIssuedCount,
		    g_device->m_currentFrame->timestampPoolData);

		double nanoSecondsPerTick = g_device->m_physicalDeviceProps.limits.timestampPeriod;
		double secondsPerTick     = 1e-9 * nanoSecondsPerTick;

		for (u32 i = 0; i < GfxStats::MaxCustomTimers; ++i)
		{
			u16 slotBegin = g_device->m_currentFrame->timestampSlotMap[2 * i];
			u16 slotEnd   = g_device->m_currentFrame->timestampSlotMap[2 * i + 1];

			u64 timestampDelta = 0;

			if (slotBegin != InvalidTimestampSlotIndex && slotEnd != InvalidTimestampSlotIndex)
			{
				timestampDelta = g_device->m_currentFrame->timestampPoolData[slotEnd] -
				                 g_device->m_currentFrame->timestampPoolData[slotBegin];
				g_device->m_stats.customTimer[i] = timestampDelta * secondsPerTick;
			}
			else
			{
				g_device->m_stats.customTimer[i] = 0;
			}
		}

		u32 frameBeginSlot = g_device->m_currentFrame->timestampSlotMap[2 * GfxStats::MaxCustomTimers];
		u32 frameEndSlot   = g_device->m_currentFrame->timestampSlotMap[2 * GfxStats::MaxCustomTimers + 1];
		u64 timestampDelta = g_device->m_currentFrame->timestampPoolData[frameEndSlot] -
		                     g_device->m_currentFrame->timestampPoolData[frameBeginSlot];
		g_device->m_stats.lastFrameGpuTime = timestampDelta * secondsPerTick;

		g_device->m_currentFrame->timestampIssuedCount = 0;
	}

	for (u16& it : g_device->m_currentFrame->timestampSlotMap)
	{
		it = InvalidTimestampSlotIndex;
	}

	writeTimestamp(g_context, 2 * GfxStats::MaxCustomTimers, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
}

void GfxDevice::captureScreenshot()
{
	const size_t screenshotSizeInBytes = 4 * m_swapChainExtent.width * m_swapChainExtent.height; // assume 8888 format

	VkDeviceMemory memory       = VK_NULL_HANDLE;
	VkBuffer       buffer       = VK_NULL_HANDLE;
	void*          mappedBuffer = nullptr;

	VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferCreateInfo.size               = screenshotSizeInBytes;

	V(vkCreateBuffer(m_vulkanDevice, &bufferCreateInfo, nullptr, &buffer));

	VkMemoryRequirements memoryReq = {};
	vkGetBufferMemoryRequirements(m_vulkanDevice, buffer, &memoryReq);

	VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize       = memoryReq.size;
	allocInfo.memoryTypeIndex      = m_memoryTypes.hostOnly;

	V(vkAllocateMemory(m_vulkanDevice, &allocInfo, nullptr, &memory));
	V(vkBindBufferMemory(m_vulkanDevice, buffer, memory, 0));
	V(vkMapMemory(m_vulkanDevice, memory, 0, memoryReq.size, 0, &mappedBuffer));

	GfxContext* context = allocateContext(GfxContextType::Graphics);
	context->beginBuild();

	// copy image to buffer

	VkBufferImageCopy bufferImageCopy;
	bufferImageCopy.bufferOffset      = 0;
	bufferImageCopy.bufferRowLength   = 0;
	bufferImageCopy.bufferImageHeight = 0;
	bufferImageCopy.imageSubresource  = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	bufferImageCopy.imageOffset       = VkOffset3D{0, 0, 0};
	bufferImageCopy.imageExtent       = VkExtent3D{m_swapChainExtent.width, m_swapChainExtent.height, 1};

	VkImage swapChainImage = m_swapChainImages[m_swapChainIndex];

	context->addImageBarrier(swapChainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	context->flushBarriers();

	vkCmdCopyImageToBuffer(
	    context->m_commandBuffer, swapChainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &bufferImageCopy);

	context->addImageBarrier(swapChainImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	context->endBuild();
	context->submit(m_graphicsQueue);

	V(vkWaitForFences(m_vulkanDevice, 1, &context->m_fence, true, UINT64_MAX));

	Tuple2u imageSize = {m_swapChainExtent.width, m_swapChainExtent.height};
	m_pendingScreenshotCallback(reinterpret_cast<ColorRGBA8*>(mappedBuffer), imageSize, m_pendingScreenshotUserData);

	Gfx_Release(context);

	vkUnmapMemory(m_vulkanDevice, memory);
	vkDestroyBuffer(m_vulkanDevice, buffer, nullptr);
	vkFreeMemory(m_vulkanDevice, memory, nullptr);
}

void Gfx_EndFrame()
{
	RUSH_ASSERT(!g_context->m_isRenderPassActive);

	writeTimestamp(g_context, 2 * GfxStats::MaxCustomTimers + 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	TextureVK& backBufferTexture = g_device->m_textures[g_device->m_swapChainTextures[g_device->m_swapChainIndex]];
	g_context->addImageBarrier(
	    backBufferTexture.image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, backBufferTexture.currentLayout);
	backBufferTexture.currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	g_context->endBuild();

	if (g_device->m_pendingScreenshotCallback)
	{
		g_device->captureScreenshot();

		g_device->m_pendingScreenshotCallback = nullptr;
		g_device->m_pendingScreenshotUserData = nullptr;
	}

	g_device->flushUploadContext(g_context);

	g_device->endFrame();
}

void Gfx_RequestScreenshot(GfxScreenshotCallback callback, void* userData)
{
	g_device->m_pendingScreenshotCallback = callback;
	g_device->m_pendingScreenshotUserData = userData;
}

void Gfx_Present()
{
	g_context->submit(g_device->m_graphicsQueue);

	g_device->m_currentFrame->lastGraphicsFence = g_context->m_fence;

	VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.swapchainCount   = 1;
	presentInfo.pSwapchains      = &g_device->m_swapChain;
	presentInfo.pImageIndices    = &g_device->m_swapChainIndex;

	V(vkQueuePresentKHR(g_device->m_graphicsQueue, &presentInfo));

	g_device->m_frameCount++;
}

void Gfx_SetPresentInterval(u32 interval)
{
	g_device->m_presentInterval = interval;
	if (interval == 0)
	{
		g_device->m_pendingSwapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
	else
	{
		g_device->m_pendingSwapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	}
}

void Gfx_Finish()
{
	g_device->flushUploadContext(g_context);

	g_context->split();

	vkDeviceWaitIdle(g_vulkanDevice);
}

const GfxStats& Gfx_Stats() { return g_device->m_stats; }

void Gfx_ResetStats() { g_device->m_stats = GfxStats(); }

// vertex format

static VkFormat convertFormat(const GfxVertexFormatDesc::Element& vertexElement)
{
	switch (vertexElement.type)
	{
	case GfxVertexFormatDesc::DataType::Float1: return VK_FORMAT_R32_SFLOAT;
	case GfxVertexFormatDesc::DataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
	case GfxVertexFormatDesc::DataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
	case GfxVertexFormatDesc::DataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case GfxVertexFormatDesc::DataType::Color: return VK_FORMAT_R8G8B8A8_UNORM;
	case GfxVertexFormatDesc::DataType::UInt: return VK_FORMAT_R32_UINT;
	case GfxVertexFormatDesc::DataType::Short2N: return VK_FORMAT_R16G16_UNORM;
	default: RUSH_LOG_ERROR("Unsupported vertex element format type"); return VK_FORMAT_UNDEFINED;
	}
}

GfxVertexFormat Gfx_CreateVertexFormat(const GfxVertexFormatDesc& desc)
{
	VertexFormatVK format;

	format.id   = g_device->generateId();
	format.desc = desc;
	format.attributes.resize(desc.elementCount());

	for (u32 i = 0; i < (u32)desc.elementCount(); ++i)
	{
		const auto& element = desc.element(i);
		RUSH_ASSERT(element.stream < GfxContext::MaxVertexStreams);

		if (element.semantic == GfxVertexFormatDesc::Semantic::InstanceData)
		{
			RUSH_ASSERT_MSG(format.instanceDataStream == 0xFFFFFFFF, "Only one instance data element is supported.");
			format.instanceDataStream         = element.stream;
			format.instanceDataAttributeIndex = i;
		}

		format.attributes[i].binding  = element.stream;
		format.attributes[i].location = i;
		format.attributes[i].format   = convertFormat(element);
		format.attributes[i].offset   = element.offset;
		format.vertexStreamCount      = max<u32>(format.vertexStreamCount, element.stream + 1);
	}

	return retainResource(g_device->m_vertexFormats, format);
}

void Gfx_DestroyVertexFormat(GfxVertexFormat h) { releaseResource(g_device->m_vertexFormats, h); }

ShaderVK createShader(VkDevice device, const GfxShaderSource& code)
{
	ShaderVK result;

	result.entry = _strdup(code.entry);

	VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	moduleCreateInfo.codeSize                 = code.size();
	moduleCreateInfo.pCode                    = (const u32*)code.data();

	V(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &result.module));

	return result;
}

static ShaderVK::InputMapping parseVertexInputMapping(const std::string& vertexAttributeName)
{
	ShaderVK::InputMapping result;
	result.location      = 0;
	result.semantic      = GfxVertexFormatDesc::Semantic::Unused;
	result.semanticIndex = 0;

	size_t attrLen = vertexAttributeName.length();

	result.semanticIndex = vertexAttributeName[attrLen - 1] - '0';

	if (strstr(vertexAttributeName.c_str(), "a_pos"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Position;
	}
	else if (strstr(vertexAttributeName.c_str(), "a_tex"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Texcoord;
	}
	else if (strstr(vertexAttributeName.c_str(), "a_col"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Color;
	}
	else if (strstr(vertexAttributeName.c_str(), "a_nor"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Normal;
	}
	else if (strstr(vertexAttributeName.c_str(), "a_tan") || strstr(vertexAttributeName.c_str(), "a_tau"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Tangent;
	}
	else if (strstr(vertexAttributeName.c_str(), "a_bin") || strstr(vertexAttributeName.c_str(), "a_bit") ||
	         strstr(vertexAttributeName.c_str(), "a_tav"))
	{
		result.semantic = GfxVertexFormatDesc::Semantic::Bitangent;
	}

	return result;
}

// vertex shader
GfxVertexShader Gfx_CreateVertexShader(const GfxShaderSource& code)
{
	RUSH_ASSERT(code.type == GfxShaderSourceType_SPV);

	ShaderVK res = createShader(g_vulkanDevice, code);
	res.id       = g_device->generateId();

	if (res.module)
	{
		return retainResource(g_device->m_vertexShaders, res);
	}
	else
	{
		return InvalidResourceHandle();
	}
}

void Gfx_DestroyVertexShader(GfxVertexShader h) { releaseResource(g_device->m_vertexShaders, h); }

// pixel shader
GfxPixelShader Gfx_CreatePixelShader(const GfxShaderSource& code)
{
	RUSH_ASSERT(code.type == GfxShaderSourceType_SPV);

	ShaderVK res = createShader(g_vulkanDevice, code);
	res.id       = g_device->generateId();

	if (res.module)
	{
		return retainResource(g_device->m_pixelShaders, res);
	}
	else
	{
		return InvalidResourceHandle();
	}
}

void Gfx_DestroyPixelShader(GfxPixelShader h) { releaseResource(g_device->m_pixelShaders, h); }

// geometry shader
GfxGeometryShader Gfx_CreateGeometryShader(const GfxShaderSource& code)
{
	RUSH_ASSERT(code.type == GfxShaderSourceType_SPV);

	ShaderVK res = createShader(g_vulkanDevice, code);
	res.id       = g_device->generateId();

	if (res.module)
	{
		return retainResource(g_device->m_geometryShaders, res);
	}
	else
	{
		return InvalidResourceHandle();
	}
}

void Gfx_DestroyGeometryShader(GfxGeometryShader h) { releaseResource(g_device->m_geometryShaders, h); }

// compute shader
GfxComputeShader Gfx_CreateComputeShader(const GfxShaderSource& code)
{
	RUSH_ASSERT(code.type == GfxShaderSourceType_SPV);

	ShaderVK res = createShader(g_vulkanDevice, code);
	res.id       = g_device->generateId();

	if (res.module)
	{
		return retainResource(g_device->m_computeShaders, res);
	}
	else
	{
		return InvalidResourceHandle();
	}
}

void Gfx_DestroyComputeShader(GfxComputeShader h) { releaseResource(g_device->m_computeShaders, h); }

// technique
GfxTechnique Gfx_CreateTechnique(const GfxTechniqueDesc& desc)
{
	TechniqueVK res;

	res.id = g_device->generateId();

	if (g_device->m_supportedExtensions.AMD_wave_limits)
	{
		res.waveLimits = new VkPipelineShaderStageCreateInfoWaveLimitAMD[u32(GfxStage::count)];
		for (u32 i = 0; i < u32(GfxStage::count); ++i)
		{
			res.waveLimits[i].sType      = VK_STRUCTURE_TYPE_WAVE_LIMIT_AMD;
			res.waveLimits[i].wavesPerCu = 1.0;
		}
		res.waveLimits[u32(GfxStage::Pixel)].wavesPerCu   = desc.psWaveLimit;
		res.waveLimits[u32(GfxStage::Vertex)].wavesPerCu  = desc.vsWaveLimit;
		res.waveLimits[u32(GfxStage::Compute)].wavesPerCu = desc.csWaveLimit;
	}

	if (desc.specializationData)
	{
		// todo: batch all allocations into one

		size_t specializationEntriesSize = sizeof(VkSpecializationMapEntry) * desc.specializationConstantCount;
		void*  specializationEntriesCopy = malloc(specializationEntriesSize);
		memcpy(specializationEntriesCopy, desc.specializationConstants, specializationEntriesSize);

		void* specializationDataCopy = malloc(desc.specializationDataSize);
		memcpy(specializationDataCopy, desc.specializationData, desc.specializationDataSize);

		res.specializationInfo = (VkSpecializationInfo*)malloc(sizeof(VkSpecializationInfo));

		res.specializationInfo->mapEntryCount = desc.specializationConstantCount;
		res.specializationInfo->pMapEntries   = reinterpret_cast<VkSpecializationMapEntry*>(specializationEntriesCopy);
		res.specializationInfo->dataSize      = desc.specializationDataSize;
		res.specializationInfo->pData         = specializationDataCopy;
	}

	if (desc.cs.valid())
	{
		VkPipelineShaderStageCreateInfo stageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stageInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
		stageInfo.module                          = g_device->m_computeShaders[desc.cs].module;
		stageInfo.pName                           = g_device->m_computeShaders[desc.cs].entry;
		stageInfo.pSpecializationInfo             = res.specializationInfo;

		if (g_device->m_supportedExtensions.AMD_wave_limits && desc.csWaveLimit != 1.0f)
		{
			res.waveLimits[u32(GfxStage::Compute)].pNext = stageInfo.pNext;
			stageInfo.pNext                              = &res.waveLimits[u32(GfxStage::Compute)];
		}

		res.shaderStages.push_back(stageInfo);
		res.cs.retain(desc.cs);
	}
	else
	{
		RUSH_ASSERT(desc.vf.valid());
		RUSH_ASSERT(desc.vs.valid());

		res.vf.retain(desc.vf);

		if (res.vf.valid())
		{
			const VertexFormatVK& vertexFormat = g_device->m_vertexFormats[res.vf.get()];
			res.instanceDataStream             = vertexFormat.instanceDataStream;
			res.vertexStreamCount              = vertexFormat.vertexStreamCount;
		}

		if (desc.vs.valid())
		{
			VkPipelineShaderStageCreateInfo stageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			stageInfo.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
			stageInfo.module                          = g_device->m_vertexShaders[desc.vs].module;
			stageInfo.pName                           = g_device->m_vertexShaders[desc.vs].entry;
			stageInfo.pSpecializationInfo             = res.specializationInfo;

			if (g_device->m_supportedExtensions.AMD_wave_limits && desc.vsWaveLimit != 1.0f)
			{
				res.waveLimits[u32(GfxStage::Vertex)].pNext = stageInfo.pNext;
				stageInfo.pNext                             = &res.waveLimits[u32(GfxStage::Vertex)];
			}

			res.shaderStages.push_back(stageInfo);
			res.vs.retain(desc.vs);
		}

		if (desc.gs.valid())
		{
			VkPipelineShaderStageCreateInfo stageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			stageInfo.stage                           = VK_SHADER_STAGE_GEOMETRY_BIT;
			stageInfo.module                          = g_device->m_geometryShaders[desc.gs].module;
			stageInfo.pName                           = g_device->m_geometryShaders[desc.gs].entry;
			stageInfo.pSpecializationInfo             = res.specializationInfo;

			res.shaderStages.push_back(stageInfo);
			res.gs.retain(desc.gs);
		}

		if (desc.ps.valid())
		{
			VkPipelineShaderStageCreateInfo stageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			stageInfo.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
			stageInfo.module                          = g_device->m_pixelShaders[desc.ps].module;
			stageInfo.pName                           = g_device->m_pixelShaders[desc.ps].entry;
			stageInfo.pSpecializationInfo             = res.specializationInfo;

			if (g_device->m_supportedExtensions.AMD_wave_limits && desc.psWaveLimit != 1.0f)
			{
				res.waveLimits[u32(GfxStage::Pixel)].pNext = stageInfo.pNext;
				stageInfo.pNext                            = &res.waveLimits[u32(GfxStage::Pixel)];
			}

			res.shaderStages.push_back(stageInfo);
			res.ps.retain(desc.ps);
		}
	}

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
	    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};

	res.bindings = desc.bindings;

	res.pushConstantStageFlags  = convertStageFlags(desc.bindings.pushConstantStageFlags);
	res.pushConstantsSize       = desc.bindings.pushConstants;
	res.samplerCount            = desc.bindings.samplers;
	res.sampledImageCount       = desc.bindings.textures;
	res.constantBufferCount     = desc.bindings.constantBuffers;
	res.storageImageCount       = desc.bindings.rwImages;
	res.storageBufferCount      = desc.bindings.rwBuffers;
	res.typedStorageBufferCount = desc.bindings.rwTypedBuffers;

	RUSH_ASSERT(res.storageBufferCount <= GfxContext::MaxStorageBuffers);
	RUSH_ASSERT(res.constantBufferCount <= GfxContext::MaxConstantBuffers);

	u32 resourceStageFlags = 0;
	if (desc.cs.valid())
		resourceStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
	if (desc.vs.valid())
		resourceStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
	if (desc.gs.valid())
		resourceStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
	if (desc.ps.valid())
		resourceStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

	u32 bindingSlot = 0;

	for (u32 i = 0; i < res.constantBufferCount; ++i)
	{
		VkDescriptorSetLayoutBinding uniformBinding = {};
		uniformBinding.binding                      = bindingSlot++;
		uniformBinding.descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBinding.descriptorCount              = 1;
		uniformBinding.stageFlags                   = resourceStageFlags;
		layoutBindings.push_back(uniformBinding);
	}

	for (u32 i = 0; i < res.samplerCount; ++i)
	{
		VkDescriptorSetLayoutBinding samplerBinding = {};
		samplerBinding.binding                      = bindingSlot++;
		samplerBinding.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
		samplerBinding.descriptorCount              = 1;
		samplerBinding.stageFlags                   = resourceStageFlags;
		layoutBindings.push_back(samplerBinding);
	}

	for (u32 i = 0; i < res.sampledImageCount; ++i)
	{
		VkDescriptorSetLayoutBinding imageBinding = {};
		imageBinding.binding                      = bindingSlot++;
		imageBinding.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		imageBinding.descriptorCount              = 1;
		imageBinding.stageFlags                   = resourceStageFlags;
		layoutBindings.push_back(imageBinding);
	}

	for (u32 i = 0; i < res.storageImageCount; ++i)
	{
		VkDescriptorSetLayoutBinding imageBinding = {};
		imageBinding.binding                      = bindingSlot++;
		imageBinding.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageBinding.descriptorCount              = 1;
		imageBinding.stageFlags                   = resourceStageFlags;
		layoutBindings.push_back(imageBinding);
	}

	for (u32 i = 0; i < res.storageBufferCount + res.typedStorageBufferCount; ++i)
	{
		const bool isTyped = i > res.storageBufferCount;

		VkDescriptorSetLayoutBinding bufferBinding = {};
		bufferBinding.binding                      = bindingSlot++;
		if (isTyped)
		{
			bufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		}
		else
		{
			bufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}

		bufferBinding.descriptorCount = 1;
		bufferBinding.stageFlags      = resourceStageFlags;
		layoutBindings.push_back(bufferBinding);
	}

	descriptorSetLayoutCreateInfo.bindingCount = (u32)layoutBindings.size();
	descriptorSetLayoutCreateInfo.pBindings    = layoutBindings.data();

	V(vkCreateDescriptorSetLayout(g_vulkanDevice, &descriptorSetLayoutCreateInfo, nullptr, &res.descriptorSetLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipelineLayoutCreateInfo.setLayoutCount             = 1;
	pipelineLayoutCreateInfo.pSetLayouts                = &res.descriptorSetLayout;

	VkPushConstantRange pushConstantRange;
	if (res.pushConstantsSize)
	{
		pushConstantRange.offset                        = 0;
		pushConstantRange.size                          = res.pushConstantsSize;
		pushConstantRange.stageFlags                    = res.pushConstantStageFlags;
		pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstantRange;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	}

	V(vkCreatePipelineLayout(g_vulkanDevice, &pipelineLayoutCreateInfo, nullptr, &res.pipelineLayout));

	return retainResource(g_device->m_techniques, res);
}

void TechniqueVK::destroy()
{
	if (specializationInfo)
	{
		free(const_cast<void*>(specializationInfo->pData));
		free(const_cast<VkSpecializationMapEntry*>(specializationInfo->pMapEntries));
		free(specializationInfo);
	}

	delete[] waveLimits;

	// TODO: queue-up destruction
	vkDestroyDescriptorSetLayout(g_vulkanDevice, descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(g_vulkanDevice, pipelineLayout, nullptr);
}

void Gfx_DestroyTechnique(GfxTechnique h) { releaseResource(g_device->m_techniques, h); }

// texture
GfxTexture Gfx_CreateTextureFromFile(const char* filename, TextureType type)
{
	RUSH_LOG_ERROR("Not implemented");
	return GfxTexture();
}

static u32 aspectFlagsFromFormat(GfxFormat format)
{
	u32 aspectFlags = 0;

	if (getGfxFormatComponent(format) & GfxFormatComponent_Stencil)
	{
		aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	if (getGfxFormatComponent(format) & GfxFormatComponent_Depth)
	{
		aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	if (getGfxFormatComponent(format) & GfxFormatComponent_RGBA)
	{
		aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
	}

	return aspectFlags;
}

VkDeviceMemory allocDeviceMemory(size_t size, u32 memoryType)
{
	VkDeviceMemory result = VK_NULL_HANDLE;

	VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize       = size;
	allocInfo.memoryTypeIndex      = memoryType;

	V(vkAllocateMemory(g_vulkanDevice, &allocInfo, nullptr, &result));

	return result;
}

inline VkDeviceMemory allocLocalMemory(size_t size) { return allocDeviceMemory(size, g_device->m_memoryTypes.local); }

inline VkDeviceMemory allocHostOnlyMemory(size_t size)
{
	return allocDeviceMemory(size, g_device->m_memoryTypes.hostOnly);
}

inline VkDeviceMemory allocHostVisibleMemory(size_t size)
{
	return allocDeviceMemory(size, g_device->m_memoryTypes.hostVisible);
}

inline VkImageViewType getDefaultImageViewType(TextureType type)
{
	switch (type)
	{
	default: return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	case TextureType::Tex1D: return VK_IMAGE_VIEW_TYPE_1D;
	case TextureType::Tex1DArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	case TextureType::Tex2D: return VK_IMAGE_VIEW_TYPE_2D;
	case TextureType::Tex2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case TextureType::Tex3D: return VK_IMAGE_VIEW_TYPE_3D;
	case TextureType::TexCube: return VK_IMAGE_VIEW_TYPE_CUBE;
	case TextureType::TexCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
}

inline u32 getTextureArrayLayerCount(const GfxTextureDesc& desc)
{
	switch (desc.type)
	{
	default: RUSH_LOG_ERROR("Unsupported texture type"); return 0;
	case TextureType::Tex1D:
	case TextureType::Tex1DArray:
	case TextureType::Tex2D:
	case TextureType::Tex2DArray: return desc.depth;
	case TextureType::TexCube:
	case TextureType::TexCubeArray: return desc.depth * 6;
	case TextureType::Tex3D: return 1;
	}
}

TextureVK TextureVK::create(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count)
{
	const bool isDepthBuffer  = !!(desc.usage & GfxUsageFlags::DepthStencil);
	const bool isRenderTarget = !!(desc.usage & GfxUsageFlags::RenderTarget);
	const bool isStorageImage = !!(desc.usage & GfxUsageFlags::StorageImage);

	RUSH_ASSERT(!isDepthBuffer || (getGfxFormatComponent(desc.format) & GfxFormatComponent_Depth));
	RUSH_ASSERT(data || isDepthBuffer || isRenderTarget || isStorageImage);
	RUSH_ASSERT(desc.type == TextureType::Tex2D || desc.type == TextureType::TexCube);
	RUSH_ASSERT(desc.mips != 0);
	RUSH_ASSERT(desc.depth != 0);
	RUSH_ASSERT(desc.usage != GfxUsageFlags::None);

	TextureVK res;
	res.id            = g_device->generateId();
	res.desc          = desc;
	res.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

	switch (desc.type)
	{
	default: RUSH_LOG_ERROR("Unsupported texture type");
	case TextureType::Tex1D:
	case TextureType::Tex1DArray:
		imageCreateInfo.imageType   = VK_IMAGE_TYPE_1D;
		imageCreateInfo.arrayLayers = desc.depth;
		break;
	case TextureType::Tex2D:
	case TextureType::Tex2DArray:
		imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
		imageCreateInfo.arrayLayers = desc.depth;
		break;
	case TextureType::TexCube:
	case TextureType::TexCubeArray:
		imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
		imageCreateInfo.arrayLayers = desc.depth * 6;
		imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		break;
	case TextureType::Tex3D:
		imageCreateInfo.imageType   = VK_IMAGE_TYPE_3D;
		imageCreateInfo.arrayLayers = 1;
		break;
	}

	imageCreateInfo.format        = convertFormat(desc.format);
	imageCreateInfo.extent.width  = desc.width;
	imageCreateInfo.extent.height = desc.height;
	imageCreateInfo.extent.depth  = desc.depth;
	imageCreateInfo.mipLevels     = desc.mips;
	imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;

	GfxFormat viewFormat = desc.format;

	if (!!(desc.usage & GfxUsageFlags::DepthStencil))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	if (!!(desc.usage & GfxUsageFlags::ShaderResource))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		switch (desc.format)
		{
		case GfxFormat_D32_Float:
			// viewFormat = GfxFormat_R32_Float;
			break;
		case GfxFormat_D24_Unorm_S8_Uint:
		case GfxFormat_D24_Unorm_X8:
		case GfxFormat_D32_Float_S8_Uint: RUSH_LOG_ERROR("Unsupported resource format"); break;
		default:
			// nothing
			break;
		}
	}

	if (!!(desc.usage & GfxUsageFlags::RenderTarget))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (!!(desc.usage & GfxUsageFlags::StorageImage))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	if (data)
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	imageCreateInfo.initialLayout = res.currentLayout;

	V(vkCreateImage(g_vulkanDevice, &imageCreateInfo, nullptr, &res.image));
	res.ownsImage = true;

	VkMemoryRequirements memoryReq = {};
	vkGetImageMemoryRequirements(g_vulkanDevice, res.image, &memoryReq);

	VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize       = memoryReq.size;
	allocInfo.memoryTypeIndex =
	    g_device->memoryTypeFromProperties(memoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	V(vkAllocateMemory(g_vulkanDevice, &allocInfo, nullptr, &res.memory));
	res.ownsMemory = true;

	V(vkBindImageMemory(g_vulkanDevice, res.image, res.memory, 0));

	u32 aspectFlags = aspectFlagsFromFormat(desc.format);

	VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};

	imageViewCreateInfo.viewType   = getDefaultImageViewType(desc.type);
	imageViewCreateInfo.format     = convertFormat(viewFormat);
	imageViewCreateInfo.components = {
	    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
	imageViewCreateInfo.subresourceRange = {aspectFlags, 0, desc.mips, 0, imageCreateInfo.arrayLayers};
	imageViewCreateInfo.image            = res.image;

	V(vkCreateImageView(g_vulkanDevice, &imageViewCreateInfo, nullptr, &res.imageView));

	if (!!(desc.usage & GfxUsageFlags::DepthStencil))
	{
		VkImageViewCreateInfo depthViewCreateInfo = imageViewCreateInfo;
		depthViewCreateInfo.format                = imageCreateInfo.format;
		V(vkCreateImageView(g_vulkanDevice, &depthViewCreateInfo, nullptr, &res.depthStencilImageView));
	}

	if (data)
	{
		GfxContext* uploadContext = getUploadContext();

		VkImageSubresourceRange subresourceRange = {
		    VK_IMAGE_ASPECT_COLOR_BIT, 0, desc.mips, 0, imageCreateInfo.arrayLayers};
		uploadContext->addImageBarrier(
		    res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, res.currentLayout, &subresourceRange);
		uploadContext->flushBarriers();

		const size_t bitsPerPixel   = getBitsPerPixel(desc.format);
		const size_t bitsPerElement = (isGfxFormatBlockCompressed(desc.format) ? 16 * bitsPerPixel : bitsPerPixel);

		size_t stagingBufferSize = 0;

		for (u32 i = 0; i < count; ++i)
		{
			const u32 mipLevel  = data[i].mip;
			const u32 mipWidth  = data[i].width ? data[i].width : max<u32>(1, (desc.width >> mipLevel));
			const u32 mipHeight = data[i].height ? data[i].height : max<u32>(1, (desc.height >> mipLevel));
			const u32 mipDepth  = data[i].depth ? data[i].depth : max<u32>(1, (desc.depth >> mipLevel));

			const size_t levelSize = (size_t(mipWidth * mipHeight * mipDepth) * bitsPerPixel) / 8;
			const size_t alignedLevelSize =
			    alignCeiling((u64(mipWidth * mipHeight * mipDepth) * bitsPerPixel), bitsPerElement) / 8;

			stagingBufferSize += alignedLevelSize;
		}

		VkBufferCreateInfo stagingBufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		stagingBufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferCreateInfo.size               = stagingBufferSize;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		V(vkCreateBuffer(g_vulkanDevice, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		VkMemoryRequirements stagingMemoryReq = {};
		vkGetBufferMemoryRequirements(g_vulkanDevice, stagingBuffer, &stagingMemoryReq);

		VkMemoryAllocateInfo stagingAllocInfo = getStagingMemoryAllocateInfo(stagingMemoryReq);

		// TODO: allocate memory from a pool, rather than do individual allocations
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		V(vkAllocateMemory(g_vulkanDevice, &stagingAllocInfo, nullptr, &stagingMemory));
		V(vkBindBufferMemory(g_vulkanDevice, stagingBuffer, stagingMemory, 0));

		// TODO: keep upload / staging memory persistently mapped
		void* mappedStagingImage = nullptr;
		V(vkMapMemory(g_vulkanDevice, stagingMemory, 0, stagingAllocInfo.allocationSize, 0, &mappedStagingImage));

		size_t stagingImageOffset = 0;
		u8*    stagingImagePixels = (u8*)mappedStagingImage;

		for (u32 i = 0; i < count; ++i)
		{
			RUSH_ASSERT(data[i].pixels);

			const u32 mipLevel  = data[i].mip;
			const u32 mipWidth  = data[i].width ? data[i].width : max<u32>(1, (desc.width >> mipLevel));
			const u32 mipHeight = data[i].height ? data[i].height : max<u32>(1, (desc.height >> mipLevel));
			const u32 mipDepth  = data[i].depth ? data[i].depth : max<u32>(1, (desc.depth >> mipLevel));

			const size_t levelSize = (size_t(mipWidth * mipHeight * mipDepth) * bitsPerPixel) / 8;
			const size_t alignedLevelSize =
			    alignCeiling((u64(mipWidth * mipHeight * mipDepth) * bitsPerPixel), bitsPerElement) / 8;

			const u8* srcPixels = reinterpret_cast<const u8*>(data[i].pixels);
			memcpy(stagingImagePixels, srcPixels, levelSize);

			VkBufferImageCopy bufferImageCopy;
			bufferImageCopy.bufferOffset      = stagingImageOffset;
			bufferImageCopy.bufferRowLength   = 0;
			bufferImageCopy.bufferImageHeight = 0;
			bufferImageCopy.imageSubresource =
			    VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, data[i].slice, 1};
			bufferImageCopy.imageOffset = VkOffset3D{0, 0, 0};
			bufferImageCopy.imageExtent = VkExtent3D{mipWidth, mipHeight, mipDepth};

			vkCmdCopyBufferToImage(uploadContext->m_commandBuffer, stagingBuffer, res.image,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

			stagingImagePixels += alignedLevelSize;
			stagingImageOffset += alignedLevelSize;
		}

		uploadContext->addImageBarrier(res.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &subresourceRange);
		res.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkUnmapMemory(g_vulkanDevice, stagingMemory);

		g_device->enqueueDestroyMemory(stagingMemory);
		g_device->enqueueDestroyBuffer(stagingBuffer);
	}

	return res;
}

TextureVK TextureVK::create(const GfxTextureDesc& desc, VkImage image, VkImageLayout initialLayout)
{
	RUSH_ASSERT(desc.type == TextureType::Tex2D);

	TextureVK res;

	res.id   = g_device->generateId();
	res.desc = desc;

	res.ownsMemory = false;
	res.memory     = VK_NULL_HANDLE;

	res.image     = image;
	res.ownsImage = false;

	res.currentLayout = initialLayout;

	u32 aspectFlags = aspectFlagsFromFormat(desc.format); // TODO: calculate from usage instead of just format

	VkImageViewCreateInfo viewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	viewCreateInfo.image                 = res.image;
	viewCreateInfo.format                = convertFormat(desc.format);
	viewCreateInfo.components            = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
	viewCreateInfo.subresourceRange = {aspectFlags, 0, desc.mips, 0, 1};
	viewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;

	V(vkCreateImageView(g_vulkanDevice, &viewCreateInfo, nullptr, &res.imageView));

	return res;
}

GfxTexture Gfx_CreateTexture(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count)
{
	return retainResource(g_device->m_textures, TextureVK::create(desc, data, count));
}

const GfxTextureDesc& Gfx_GetTextureDesc(GfxTexture h)
{
	if (h.valid())
	{
		return g_device->m_textures[h].desc;
	}
	else
	{
		static const GfxTextureDesc desc = GfxTextureDesc::make2D(1, 1, GfxFormat_Unknown);
		return desc;
	}
}

void TextureVK::destroy()
{
	if (imageView)
	{
		g_device->enqueueDestroyImageView(imageView);
		imageView = VK_NULL_HANDLE;
	}

	if (depthStencilImageView)
	{
		g_device->enqueueDestroyImageView(depthStencilImageView);
		depthStencilImageView = VK_NULL_HANDLE;
	}

	if (image && ownsImage)
	{
		g_device->enqueueDestroyImage(image);
		image = VK_NULL_HANDLE;
	}

	if (memory && ownsMemory)
	{
		g_device->enqueueDestroyMemory(memory);
		memory = VK_NULL_HANDLE;
	}
}

void Gfx_DestroyTexture(GfxTexture h) { releaseResource(g_device->m_textures, h); }

// blend state
GfxBlendState Gfx_CreateBlendState(const GfxBlendStateDesc& desc)
{
	BlendStateVK res;
	res.id   = g_device->generateId();
	res.desc = desc;

	return retainResource(g_device->m_blendStates, res);
}

void Gfx_DestroyBlendState(GfxBlendState h) { releaseResource(g_device->m_blendStates, h); }

// sampler state
GfxSampler Gfx_CreateSamplerState(const GfxSamplerDesc& desc)
{
	SamplerVK res;

	res.id   = g_device->generateId();
	res.desc = desc;

	VkSamplerCreateInfo samplerCreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	samplerCreateInfo.magFilter           = convertFilter(desc.filterMag);
	samplerCreateInfo.minFilter           = convertFilter(desc.filterMin);
	samplerCreateInfo.mipmapMode          = convertMipmapMode(desc.filterMip);
	samplerCreateInfo.addressModeU        = convertSamplerAddressMode(desc.wrapU);
	samplerCreateInfo.addressModeV        = convertSamplerAddressMode(desc.wrapV);
	samplerCreateInfo.addressModeW        = convertSamplerAddressMode(desc.wrapW);
	samplerCreateInfo.mipLodBias          = 0.0f;
	samplerCreateInfo.anisotropyEnable    = desc.anisotropy > 1.0f;
	samplerCreateInfo.maxAnisotropy = min(desc.anisotropy, g_device->m_physicalDeviceProps.limits.maxSamplerAnisotropy);
	samplerCreateInfo.compareOp     = convertCompareFunc(desc.compareFunc);
	samplerCreateInfo.compareEnable = desc.compareEnable;
	samplerCreateInfo.minLod        = 0.0f;
	samplerCreateInfo.maxLod        = FLT_MAX;
	samplerCreateInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerCreateInfo.unnormalizedCoordinates = false;

	V(vkCreateSampler(g_vulkanDevice, &samplerCreateInfo, nullptr, &res.native));

	return retainResource(g_device->m_samplers, res);
}

void SamplerVK::destroy() { g_device->enqueueDestroySampler(native); }

void Gfx_DestroySamplerState(GfxSampler h) { releaseResource(g_device->m_samplers, h); }

// depth stencil state
GfxDepthStencilState Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc)
{
	DepthStencilStateVK res;
	res.id   = g_device->generateId();
	res.desc = desc;

	return retainResource(g_device->m_depthStencilStates, res);
}

void Gfx_DestroyDepthStencilState(GfxDepthStencilState h) { releaseResource(g_device->m_depthStencilStates, h); }

// rasterizer state
GfxRasterizerState Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc)
{
	RasterizerStateVK res;
	res.id   = g_device->generateId();
	res.desc = desc;

	return retainResource(g_device->m_rasterizerStates, res);
}

void Gfx_DestroyRasterizerState(GfxRasterizerState h) { releaseResource(g_device->m_rasterizerStates, h); }

static VkBufferCreateInfo makeBufferCreateInfo(const GfxBufferDesc& desc)
{
	VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

	if (!!(desc.flags & GfxBufferFlags::Vertex))
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}

	if (!!(desc.flags & GfxBufferFlags::Index))
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}

	if (!!(desc.flags & GfxBufferFlags::Constant))
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	if (!!(desc.flags & GfxBufferFlags::Storage))
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (desc.format != GfxFormat_Unknown)
		{
			bufferCreateInfo.usage |=
			    VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		}
	}

	if (!!(desc.flags & GfxBufferFlags::Texel))
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
	}

	if (!!(desc.flags & GfxBufferFlags::IndirectArgs))
	{
		bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	bufferCreateInfo.size = desc.count * desc.stride;

	return bufferCreateInfo;
}

// buffers
GfxBuffer Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data)
{
	BufferVK res;

	res.id   = g_device->generateId();
	res.desc = desc;

	VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(desc);

	// Always allow buffers to be copied to and from
	bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	const bool isStatic = !(desc.flags & GfxBufferFlags::Transient);

	if (data || isStatic)
	{
		V(vkCreateBuffer(g_vulkanDevice, &bufferCreateInfo, nullptr, &res.info.buffer));
		res.ownsBuffer = true;

		res.size        = (u32)bufferCreateInfo.size;
		res.info.offset = 0;
		res.info.range  = bufferCreateInfo.size;
	}

	if (data)
	{
		VkBufferCreateInfo stagingBufferCreateInfo = bufferCreateInfo;
		stagingBufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		V(vkCreateBuffer(g_vulkanDevice, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		VkMemoryRequirements stagingMemoryReq = {};
		vkGetBufferMemoryRequirements(g_vulkanDevice, stagingBuffer, &stagingMemoryReq);
		VkMemoryAllocateInfo stagingAllocInfo = getStagingMemoryAllocateInfo(stagingMemoryReq);

		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		V(vkAllocateMemory(g_vulkanDevice, &stagingAllocInfo, nullptr, &stagingMemory));

		void* mappedBuffer = nullptr;
		V(vkMapMemory(g_vulkanDevice, stagingMemory, 0, stagingAllocInfo.allocationSize, 0, &mappedBuffer));

		memcpy(mappedBuffer, data, (size_t)stagingAllocInfo.allocationSize);

		vkUnmapMemory(g_vulkanDevice, stagingMemory);

		V(vkBindBufferMemory(g_vulkanDevice, stagingBuffer, stagingMemory, 0));

		V(vkCreateBuffer(g_vulkanDevice, &bufferCreateInfo, nullptr, &res.info.buffer));
		res.ownsBuffer = true;

		VkMemoryRequirements memoryReq = {};
		vkGetBufferMemoryRequirements(g_vulkanDevice, res.info.buffer, &memoryReq);

		VkMemoryAllocateInfo allocInfo = stagingAllocInfo;
		allocInfo.allocationSize       = memoryReq.size;
		allocInfo.memoryTypeIndex =
		    g_device->memoryTypeFromProperties(memoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		V(vkAllocateMemory(g_vulkanDevice, &allocInfo, nullptr, &res.memory));
		res.ownsMemory = true;

		V(vkBindBufferMemory(g_vulkanDevice, res.info.buffer, res.memory, 0));

		GfxContext*  uploadContext = getUploadContext();
		VkBufferCopy region        = {};
		region.srcOffset           = 0;
		region.dstOffset           = 0;
		region.size                = bufferCreateInfo.size;
		vkCmdCopyBuffer(uploadContext->m_commandBuffer, stagingBuffer, res.info.buffer, 1, &region);

		g_device->enqueueDestroyBuffer(stagingBuffer);
		g_device->enqueueDestroyMemory(stagingMemory);

		res.lastUpdateFrame = g_device->m_frameCount;
	}
	else if (isStatic)
	{
		VkMemoryRequirements memoryReq = {};
		vkGetBufferMemoryRequirements(g_vulkanDevice, res.info.buffer, &memoryReq);

		VkFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		if (desc.hostVisible)
		{
			memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}

		VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocInfo.allocationSize       = max(memoryReq.size, bufferCreateInfo.size);
		allocInfo.memoryTypeIndex      = g_device->memoryTypeFromProperties(memoryReq.memoryTypeBits, memoryProperties);

		V(vkAllocateMemory(g_vulkanDevice, &allocInfo, nullptr, &res.memory));
		res.ownsMemory = true;

		V(vkBindBufferMemory(g_vulkanDevice, res.info.buffer, res.memory, 0));

		if (desc.hostVisible)
		{
			V(vkMapMemory(g_vulkanDevice, res.memory, 0, allocInfo.allocationSize, 0, &res.mappedMemory));
			RUSH_ASSERT(res.mappedMemory);
		}
	}

	if (desc.format != GfxFormat_Unknown && !!(desc.flags & GfxBufferFlags::Storage) && (data || isStatic))
	{
		VkBufferViewCreateInfo bufferViewCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
		bufferViewCreateInfo.buffer                 = res.info.buffer;
		bufferViewCreateInfo.format                 = convertFormat(desc.format);
		bufferViewCreateInfo.offset                 = res.info.offset;
		bufferViewCreateInfo.range                  = res.info.range;
		V(vkCreateBufferView(g_vulkanDevice, &bufferViewCreateInfo, nullptr, &res.bufferView));
	}

	return retainResource(g_device->m_buffers, res);
}

void Gfx_vkFlushBarriers(GfxContext* ctx) { ctx->flushBarriers(); }

void Gfx_vkFillBuffer(GfxContext* ctx, GfxBuffer h, u32 value)
{
	auto& buffer = g_device->m_buffers[h];
	validateBufferUse(buffer);

	vkCmdFillBuffer(ctx->m_commandBuffer, buffer.info.buffer, buffer.info.offset, buffer.info.range, value);
}

void Gfx_vkFullPipelineBarrier(GfxContext* ctx)
{
	ctx->flushBarriers();

	VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};

	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

	barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
	                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
	                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(ctx->m_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	    0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void Gfx_vkExecutionBarrier(GfxContext* ctx, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage)
{
	ctx->m_pendingBarriers.srcStageMask |= srcStage;
	ctx->m_pendingBarriers.dstStageMask |= dstStage;
}

void Gfx_vkBufferMemoryBarrier(GfxContext* ctx, GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
    VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage)
{
	ctx->addBufferBarrier(h, srcAccess, dstAccess, srcStage, dstStage);
}

void Gfx_vkFlushMappedBuffer(GfxBuffer h)
{
	auto& buffer = g_device->m_buffers[h];

	VkMappedMemoryRange memoryRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
	memoryRange.memory              = buffer.memory;
	memoryRange.offset              = buffer.info.offset;
	memoryRange.size                = buffer.info.range;
	vkInvalidateMappedMemoryRanges(g_vulkanDevice, 1, &memoryRange);
}

GfxMappedBuffer Gfx_MapBuffer(GfxBuffer vb, u32 offset, u32 size)
{
	const auto& desc = g_device->m_buffers[vb].desc;

	RUSH_ASSERT_MSG(!(desc.flags & GfxBufferFlags::Transient),
	    "Transient buffers can't be mapped, as their memory is frequently recycled.")

	RUSH_ASSERT(offset == 0 && size == 0);
	RUSH_ASSERT(desc.hostVisible);

	GfxMappedBuffer result;
	result.data   = g_device->m_buffers[vb].mappedMemory;
	result.size   = desc.stride * desc.count;
	result.handle = vb;

	return result;
}

void Gfx_UnmapBuffer(GfxMappedBuffer&)
{
	// nothing to do
}

static void markDirtyIfBound(GfxContext* rc, BufferVK& buffer)
{
	for (u32 i = 0; i < GfxContext::MaxVertexStreams; ++i)
	{
		if (buffer.id == g_device->m_buffers[rc->m_pending.vertexBuffer[i].get()].id)
		{
			rc->m_dirtyState |= GfxContext::DirtyStateFlag_VertexBuffer;
		}
	}

	if (buffer.id == g_device->m_buffers[rc->m_pending.indexBuffer.get()].id)
	{
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_IndexBuffer;
	}

	for (u32 i = 0; i < GfxContext::MaxStorageBuffers; ++i)
	{
		if (buffer.id == g_device->m_buffers[rc->m_pending.storageBuffers[i].get()].id)
		{
			rc->m_dirtyState |= GfxContext::DirtyStateFlag_StorageBuffer;
		}
	}

	for (u32 i = 0; i < GfxContext::MaxConstantBuffers; ++i)
	{
		if (buffer.id == g_device->m_buffers[rc->m_pending.constantBuffers[i].get()].id)
		{
			rc->m_dirtyState |= GfxContext::DirtyStateFlag_ConstantBuffer;
		}
	}
}

static VkDeviceSize getBufferAlignmentRequirements(GfxBufferFlags flags)
{
	VkDeviceSize alignment = 1;

	if (!!(flags & GfxBufferFlags::Vertex))
	{
		alignment = 16; // What's a sensible alignment?
	}

	if (!!(flags & GfxBufferFlags::Index))
	{
		alignment = 16; // What's a sensible alignment?
	}

	if (!!(flags & GfxBufferFlags::Constant))
	{
		alignment = max(alignment, g_device->m_physicalDeviceProps.limits.minUniformBufferOffsetAlignment);
	}

	if (!!(flags & GfxBufferFlags::Storage))
	{
		alignment = max(alignment, g_device->m_physicalDeviceProps.limits.minStorageBufferOffsetAlignment);
	}

	if (!!(flags & GfxBufferFlags::Texel))
	{
		alignment = max(alignment, g_device->m_physicalDeviceProps.limits.minTexelBufferOffsetAlignment);
	}

	if (!!(flags & GfxBufferFlags::IndirectArgs))
	{
		alignment = max(alignment, g_device->m_physicalDeviceProps.limits.minStorageBufferOffsetAlignment);
	}

	return alignment;
}

#if 0
static VkMemoryRequirements getBufferMemoryRequirements(const BufferVK& buffer)
{
	VkMemoryRequirements memoryReq;
	vkGetBufferMemoryRequirements(g_vulkanDevice, buffer.info.buffer, &memoryReq);

	memoryReq.alignment = max(memoryReq.alignment, getBufferAlignmentRequirements(buffer.desc.type));

	return memoryReq;
}
#endif

void Gfx_UpdateBuffer(GfxContext* rc, GfxBuffer h, const void* data, u32 size)
{
	RUSH_ASSERT(data);

	void* mappedBuffer = Gfx_BeginUpdateBuffer(rc, h, size);
	memcpy(mappedBuffer, data, size);
	Gfx_EndUpdateBuffer(rc, h);
}

void* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBuffer h, u32 size)
{
	if (!h.valid())
	{
		return nullptr;
	}

	BufferVK& buffer = g_device->m_buffers[h];
	markDirtyIfBound(rc, buffer);

	RUSH_ASSERT_MSG(!!(buffer.desc.flags & GfxBufferFlags::Transient),
	    "Only temporary buffers can be dynamically updated/renamed.");

	buffer.lastUpdateFrame = g_device->m_frameCount;

	const VkDeviceSize alignment = getBufferAlignmentRequirements(buffer.desc.flags);

	size = alignCeiling(size, u32(alignment));

	// Rename device local buffer
	{
		if (buffer.bufferView)
		{
			g_device->enqueueDestroyBufferView(buffer.bufferView);
		}

		if (buffer.memory && buffer.ownsMemory)
		{
			g_device->enqueueDestroyMemory(buffer.memory);
		}

		if (buffer.info.buffer && buffer.ownsBuffer)
		{
			g_device->enqueueDestroyBuffer(buffer.info.buffer);
		}

		MemoryBlockVK block = g_device->m_currentFrame->localOnlyAllocator.alloc(size, alignment);

		buffer.memory     = block.memory;
		buffer.ownsMemory = false;

		buffer.ownsBuffer  = false;
		buffer.info.buffer = block.buffer;
		buffer.info.offset = block.offset;
		buffer.info.range  = size;

		if (buffer.desc.format != GfxFormat_Unknown && !!(buffer.desc.flags & GfxBufferFlags::Storage))
		{
			VkBufferViewCreateInfo bufferViewCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
			bufferViewCreateInfo.buffer                 = buffer.info.buffer;
			bufferViewCreateInfo.format                 = convertFormat(buffer.desc.format);
			bufferViewCreateInfo.offset                 = buffer.info.offset;
			bufferViewCreateInfo.range                  = buffer.info.range;
			V(vkCreateBufferView(g_vulkanDevice, &bufferViewCreateInfo, nullptr, &buffer.bufferView));
		}
	}

	VkBufferCreateInfo stagingBufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	stagingBufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferCreateInfo.size               = size;

	VkMemoryRequirements memoryReq = {};
	memoryReq.alignment            = alignment;
	memoryReq.size                 = size;

	MemoryBlockVK block = g_device->m_currentFrame->hostOnlyAllocator.alloc(memoryReq.size, memoryReq.alignment);
	RUSH_ASSERT(block.buffer);
	RUSH_ASSERT(block.mappedBuffer);

	VkBufferCopy region = {};
	region.srcOffset    = block.offset;
	region.dstOffset    = buffer.info.offset;
	region.size         = size;

	GfxContext::BufferCopyCommand cmd;
	cmd.srcBuffer = block.buffer;
	cmd.dstBuffer = buffer.info.buffer;
	cmd.region    = region;

	bool isNewCommand = true;

	if (!rc->m_pendingBufferUploads.empty())
	{
		auto& lastCmd = rc->m_pendingBufferUploads.back();
		if (cmd.srcBuffer == lastCmd.srcBuffer && cmd.dstBuffer == lastCmd.dstBuffer &&
		    cmd.region.srcOffset == (lastCmd.region.srcOffset + lastCmd.region.size) &&
		    cmd.region.dstOffset == (lastCmd.region.dstOffset + lastCmd.region.size))
		{
			lastCmd.region.size += region.size;
			isNewCommand = false;
		}
	}

	if (isNewCommand)
	{
		rc->m_pendingBufferUploads.push_back(cmd);
	}

	return block.mappedBuffer;
}

void Gfx_EndUpdateBuffer(GfxContext* rc, GfxBuffer h) {}

void Gfx_DestroyBuffer(GfxBuffer h) { releaseResource(g_device->m_buffers, h); }

// context

GfxContext* Gfx_BeginAsyncCompute(GfxContext* parentContext)
{
	RUSH_ASSERT_MSG(parentContext->m_type == GfxContextType::Graphics,
	    "Kicking async compute is only implemented on graphics contexts.");

	g_device->flushUploadContext(parentContext);

	GfxContext* asyncContext = allocateContext(GfxContextType::Compute);

#if 0
	if (vkGetFenceStatus(g_vulkanDevice, asyncContext->m_fence) == VK_NOT_READY)
	{
		RUSH_LOG_ERROR("Allocated async compute context is still in used by GPU. This is likely a bug in command buffer management implementation.");
	}
#endif

	asyncContext->beginBuild();

	// TODO: can a more precise pipeline stage flag be provided here?
	asyncContext->addDependency(parentContext->m_completionSemaphore, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

	parentContext->m_useCompletionSemaphore = true;
	parentContext->split();

	return asyncContext;
}

void Gfx_EndAsyncCompute(GfxContext* parentContext, GfxContext* asyncContext)
{
	RUSH_ASSERT_MSG(parentContext->m_type == GfxContextType::Graphics,
	    "Waiting for async compute is only implemented on graphics contexts.");

	asyncContext->m_useCompletionSemaphore = true;
	asyncContext->endBuild();

	asyncContext->submit(g_device->m_computeQueue);

	parentContext->split();

	parentContext->addDependency(asyncContext->m_completionSemaphore, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	g_device->enqueueDestroyContext(asyncContext);
}

GfxContext* Gfx_AcquireContext()
{
	if (g_context == nullptr)
	{
		g_context         = new GfxContext(GfxContextType::Graphics);
		g_context->m_refs = 1;
	}
	else
	{
		Gfx_Retain(g_context);
	}
	return g_context;
}

void Gfx_Release(GfxContext* rc)
{
	if (rc->removeReference() > 1)
		return;

	if (rc == g_context)
	{
		g_context = nullptr;
	}

	delete rc;
}

void Gfx_Clear(GfxContext* rc, ColorRGBA8 color, GfxClearFlags clearFlags, float depth, u32 stencil)
{
	// TODO: handle depth and stencil clears here as well

	RUSH_ASSERT(rc->m_isRenderPassActive);

	ColorRGBA         colorFloat = (ColorRGBA)color;
	VkClearAttachment clearAttachment;
	clearAttachment.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
	clearAttachment.clearValue.color.float32[0] = colorFloat.r;
	clearAttachment.clearValue.color.float32[1] = colorFloat.g;
	clearAttachment.clearValue.color.float32[2] = colorFloat.b;
	clearAttachment.clearValue.color.float32[3] = colorFloat.a;
	clearAttachment.colorAttachment             = 0;

	VkClearRect clearRect = {};
	clearRect.rect        = rc->m_currentRenderRect;
	vkCmdClearAttachments(rc->m_commandBuffer, 1, &clearAttachment, 1, &clearRect);
}

void Gfx_SetViewport(GfxContext* rc, const GfxViewport& viewport)
{
	RUSH_ASSERT(rc->m_isRenderPassActive);

	VkViewport vp = {};

	vp.x        = viewport.x;
	vp.y        = viewport.y;
	vp.width    = viewport.w;
	vp.height   = viewport.h;
	vp.minDepth = viewport.depthMin;
	vp.maxDepth = viewport.depthMax;

	if (g_device->m_useNegativeViewport)
	{
		vp.height = -vp.height;
		if (g_device->m_supportedExtensions.KHR_maintenance1)
		{
			vp.y = rc->m_currentRenderRect.extent.height - vp.y;
		}
	}

	vkCmdSetViewport(rc->m_commandBuffer, 0, 1, &vp);
}

void Gfx_SetScissorRect(GfxContext* rc, const GfxRect& rect)
{
	VkRect2D scissor      = {};
	scissor.offset.x      = rect.left;
	scissor.offset.y      = rect.top;
	scissor.extent.width  = rect.right - rect.left;
	scissor.extent.height = rect.bottom - rect.top;

	if (scissor.offset.x < 0)
	{
		scissor.extent.width += scissor.offset.x;
		scissor.offset.x = 0;
	}

	if (scissor.offset.y < 0)
	{
		scissor.extent.height += scissor.offset.y;
		scissor.offset.y = 0;
	}

	vkCmdSetScissor(rc->m_commandBuffer, 0, 1, &scissor);
}

void Gfx_SetTechnique(GfxContext* rc, GfxTechnique h)
{
	if (rc->m_pending.technique != h)
	{
		rc->m_pending.technique.retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_Technique;
	}
}

void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type)
{
	if (rc->m_pending.primitiveType != type)
	{
		rc->m_pending.primitiveType = type;
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_PrimitiveType;
	}
}

void Gfx_SetIndexStream(GfxContext* rc, GfxBuffer h)
{
	if (rc->m_pending.indexBuffer != h)
	{
		rc->m_pending.indexBuffer.retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_IndexBuffer;
	}
}

void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBuffer h)
{
	RUSH_ASSERT(idx < GfxContext::MaxVertexStreams);

	if (rc->m_pending.vertexBuffer[idx] != h)
	{
		rc->m_pending.vertexBuffer[idx].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_VertexBuffer;

		if (h.valid())
		{
			rc->m_pending.vertexBufferStride[idx] = g_device->m_buffers[h].desc.stride;
		}
		else
		{
			rc->m_pending.vertexBufferStride[idx] = 0;
		}
	}
}

void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTexture h)
{
	RUSH_ASSERT(idx < RUSH_COUNTOF(GfxContext::m_pending.storageImages));

	if (rc->m_pending.storageImages[idx] != h)
	{
		if (h.valid())
		{
			const auto& desc = g_device->m_textures[h].desc;
			RUSH_ASSERT(!!(desc.usage & GfxUsageFlags::StorageImage));
		}

		rc->m_pending.storageImages[idx].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_StorageImage;
	}
}

void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBuffer h)
{
	RUSH_ASSERT(idx < RUSH_COUNTOF(GfxContext::m_pending.storageBuffers));

	if (rc->m_pending.storageBuffers[idx] != h)
	{
		rc->m_pending.storageBuffers[idx].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_StorageBuffer;
	}
}

void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTexture h)
{
	RUSH_ASSERT(stage == GfxStage::Pixel || stage == GfxStage::Compute);
	RUSH_ASSERT(idx < RUSH_COUNTOF(GfxContext::m_pending.textures));

	if (rc->m_pending.textures[idx] != h)
	{
		rc->m_pending.textures[idx].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_Texture;
	}
}

void Gfx_SetSampler(GfxContext* rc, GfxStage stage, u32 idx, GfxSampler h)
{
	RUSH_ASSERT(stage == GfxStage::Pixel || stage == GfxStage::Compute);
	RUSH_ASSERT(idx < RUSH_COUNTOF(GfxContext::m_pending.samplers));

	if (rc->m_pending.samplers[idx] != h)
	{
		rc->m_pending.samplers[idx].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_Sampler;
	}
}

void Gfx_SetBlendState(GfxContext* rc, GfxBlendState nextState)
{
	if (rc->m_pending.blendState != nextState)
	{
		rc->m_pending.blendState.retain(nextState);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_BlendState;
	}
}

void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilState nextState)
{
	if (rc->m_pending.depthStencilState != nextState)
	{
		rc->m_pending.depthStencilState.retain(nextState);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_DepthStencilState;
	}
}

void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerState nextState)
{
	if (rc->m_pending.rasterizerState != nextState)
	{
		rc->m_pending.rasterizerState.retain(nextState);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_RasterizerState;
	}
}

void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBuffer h, size_t offset)
{
	RUSH_ASSERT(index < GfxContext::MaxConstantBuffers);
	RUSH_ASSERT(index < RUSH_COUNTOF(GfxContext::m_pending.constantBuffers));

	if (rc->m_pending.constantBuffers[index] != h || rc->m_pending.constantBufferOffsets[index] != offset)
	{
		rc->m_pending.constantBufferOffsets[index] = offset;
		rc->m_pending.constantBuffers[index].retain(h);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_ConstantBuffer;
	}
}

GfxTexture Gfx_GetBackBufferColorTexture() { return g_device->m_swapChainTextures[g_device->m_swapChainIndex]; }

GfxTexture Gfx_GetBackBufferDepthTexture() { return g_device->m_depthBufferTexture; }

void Gfx_AddImageBarrier(
    GfxContext* rc, GfxTexture textureHandle, GfxResourceState desiredState, GfxSubresourceRange* subresourceRange)
{
	auto& texture = g_device->m_textures[textureHandle];

	VkImageLayout desiredLayout = convertImageLayout(desiredState);

	VkImageSubresourceRange subresourceRangeVk = {
	    aspectFlagsFromFormat(texture.desc.format), 0, texture.desc.mips, 0, getTextureArrayLayerCount(texture.desc)};

	if (subresourceRange)
	{
		RUSH_LOG_ERROR("Gfx_AddImageBarrier with subresource range is not implemented");
		return;
	}

	rc->addImageBarrier(texture.image, desiredLayout, texture.currentLayout, &subresourceRangeVk);
	texture.currentLayout = desiredLayout;
}

void Gfx_BeginPass(GfxContext* rc, const GfxPassDesc& desc)
{
	if (!desc.depth.valid() && !desc.color[0].valid())
	{
		GfxPassDesc backBufferPassDesc = desc;
		backBufferPassDesc.depth       = Gfx_GetBackBufferDepthTexture();
		backBufferPassDesc.color[0]    = Gfx_GetBackBufferColorTexture();
		rc->beginRenderPass(backBufferPassDesc);
	}
	else
	{
		rc->beginRenderPass(desc);
	}
}

void Gfx_EndPass(GfxContext* rc) { rc->endRenderPass(); }

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ)
{
	Gfx_Dispatch(rc, sizeX, sizeY, sizeZ, nullptr, 0);
}

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ, const void* pushConstants, u32 pushConstantsSize)
{
	rc->applyState();

	if (pushConstants)
	{
		RUSH_ASSERT(rc->m_pending.technique.valid());
		TechniqueVK& technique = g_device->m_techniques[rc->m_pending.technique.get()];
		RUSH_ASSERT(technique.pushConstantsSize == pushConstantsSize);
		vkCmdPushConstants(rc->m_commandBuffer, technique.pipelineLayout, technique.pushConstantStageFlags, 0,
		    pushConstantsSize, pushConstants);
	}

	rc->flushBarriers();

	vkCmdDispatch(rc->m_commandBuffer, sizeX, sizeY, sizeZ);
}

void Gfx_DispatchIndirect(
    GfxContext* rc, GfxBuffer argsBuffer, size_t argsBufferOffset, const void* pushConstants, u32 pushConstantsSize)
{
	rc->applyState();

	if (pushConstants)
	{
		RUSH_ASSERT(rc->m_pending.technique.valid());
		TechniqueVK& technique = g_device->m_techniques[rc->m_pending.technique.get()];
		RUSH_ASSERT(technique.pushConstantsSize == pushConstantsSize);
		vkCmdPushConstants(rc->m_commandBuffer, technique.pipelineLayout, technique.pushConstantStageFlags, 0,
		    pushConstantsSize, pushConstants);
	}

	const auto& buffer = g_device->m_buffers[argsBuffer];

	rc->flushBarriers();

	// TODO: insert buffer barrier (VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
	vkCmdDispatchIndirect(rc->m_commandBuffer, buffer.info.buffer, buffer.info.offset + argsBufferOffset);
}

inline u32 computeTriangleCount(GfxPrimitive primitiveType, u32 vertexCount)
{
	switch (primitiveType)
	{
	case GfxPrimitive::TriangleList: return vertexCount / 3;
	case GfxPrimitive::TriangleStrip: return vertexCount - 2;
	default: return 0; ;
	}
}

void Gfx_Draw(GfxContext* rc, u32 firstVertex, u32 vertexCount)
{
	RUSH_ASSERT(rc->m_isRenderPassActive);
	rc->applyState();

	rc->flushBarriers();

	vkCmdDraw(rc->m_commandBuffer, vertexCount, 1, firstVertex, 0);

	g_device->m_stats.drawCalls++;
	g_device->m_stats.vertices += vertexCount;

	g_device->m_stats.triangles += computeTriangleCount(rc->m_pending.primitiveType, vertexCount);
}

static void drawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    u32 instanceCount, u32 instanceOffset, const void* pushConstants, u32 pushConstantsSize)
{
	RUSH_ASSERT(rc->m_isRenderPassActive);
	rc->applyState();

	if (pushConstants)
	{
		RUSH_ASSERT(rc->m_pending.technique.valid());
		TechniqueVK& technique = g_device->m_techniques[rc->m_pending.technique.get()];
		RUSH_ASSERT(technique.pushConstantsSize == pushConstantsSize);
		vkCmdPushConstants(rc->m_commandBuffer, technique.pipelineLayout, technique.pushConstantStageFlags, 0,
		    pushConstantsSize, pushConstants);
	}

	rc->flushBarriers();

	vkCmdDrawIndexed(rc->m_commandBuffer, indexCount, instanceCount, firstIndex, baseVertex, instanceOffset);

	g_device->m_stats.drawCalls++;
	g_device->m_stats.vertices += indexCount * instanceCount;

	g_device->m_stats.triangles += computeTriangleCount(rc->m_pending.primitiveType, indexCount);
}

void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    const void* pushConstants, u32 pushConstantsSize)
{
	drawIndexed(rc, indexCount, firstIndex, baseVertex, vertexCount, 1, 0, pushConstants, pushConstantsSize);
}

void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount)
{
	drawIndexed(rc, indexCount, firstIndex, baseVertex, vertexCount, 1, 0, nullptr, 0);
}

void Gfx_DrawIndexedInstanced(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    u32 instanceCount, u32 instanceOffset)
{
	drawIndexed(rc, indexCount, firstIndex, baseVertex, vertexCount, instanceCount, instanceOffset, nullptr, 0);
}

void Gfx_DrawIndexedIndirect(GfxContext* rc, GfxBuffer argsBuffer, size_t argsBufferOffset, u32 drawCount)
{
	RUSH_ASSERT(rc->m_isRenderPassActive);
	rc->applyState();

	const auto& buffer = g_device->m_buffers[argsBuffer];

	rc->flushBarriers();

	// TODO: insert buffer barrier (VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
	vkCmdDrawIndexedIndirect(
	    rc->m_commandBuffer, buffer.info.buffer, buffer.info.offset + argsBufferOffset, drawCount, buffer.desc.stride);

	g_device->m_stats.drawCalls++;
}

void Gfx_PushMarker(GfxContext* rc, const char* marker)
{
	if (!vkCmdDebugMarkerBegin)
		return;

	VkDebugMarkerMarkerInfoEXT markerInfo = {VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT};
	markerInfo.pMarkerName                = marker;
	markerInfo.pNext                      = nullptr;
	markerInfo.color[0]                   = 0;
	markerInfo.color[1]                   = 0;
	markerInfo.color[2]                   = 0;
	markerInfo.color[3]                   = 0;

	vkCmdDebugMarkerBegin(rc->m_commandBuffer, &markerInfo);
}

void Gfx_PopMarker(GfxContext* rc)
{
	if (!vkCmdDebugMarkerEnd)
		return;

	vkCmdDebugMarkerEnd(rc->m_commandBuffer);
}

void Gfx_BeginTimer(GfxContext* rc, u32 timestampId)
{
	RUSH_ASSERT(timestampId < GfxStats::MaxCustomTimers);
	writeTimestamp(g_context, timestampId * 2, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
}

void Gfx_EndTimer(GfxContext* rc, u32 timestampId)
{
	RUSH_ASSERT(timestampId < GfxStats::MaxCustomTimers);
	writeTimestamp(g_context, timestampId * 2 + 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void Gfx_Retain(GfxDevice* dev) { dev->addReference(); }

void Gfx_Retain(GfxContext* rc) { rc->addReference(); }

void Gfx_Retain(GfxVertexFormat h) { g_device->m_vertexFormats[h].addReference(); }

void Gfx_Retain(GfxVertexShader h) { g_device->m_vertexShaders[h].addReference(); }

void Gfx_Retain(GfxPixelShader h) { g_device->m_pixelShaders[h].addReference(); }

void Gfx_Retain(GfxGeometryShader h) { g_device->m_geometryShaders[h].addReference(); }

void Gfx_Retain(GfxComputeShader h) { g_device->m_computeShaders[h].addReference(); }

void Gfx_Retain(GfxTechnique h) { g_device->m_techniques[h].addReference(); }

void Gfx_Retain(GfxTexture h) { g_device->m_textures[h].addReference(); }

void Gfx_Retain(GfxBlendState h) { g_device->m_blendStates[h].addReference(); }

void Gfx_Retain(GfxSampler h) { g_device->m_samplers[h].addReference(); }

void Gfx_Retain(GfxDepthStencilState h) { g_device->m_depthStencilStates[h].addReference(); }

void Gfx_Retain(GfxRasterizerState h) { g_device->m_rasterizerStates[h].addReference(); }

void Gfx_Retain(GfxBuffer h) { g_device->m_buffers[h].addReference(); }

void ShaderVK::destroy()
{
	// TODO: queue-up destruction
	vkDestroyShaderModule(g_vulkanDevice, module, nullptr);
	free(const_cast<char*>(entry));
}

void BufferVK::destroy()
{
	if (info.buffer && ownsBuffer)
	{
		g_device->enqueueDestroyBuffer(info.buffer);
		info.buffer = VK_NULL_HANDLE;
	}

	if (mappedMemory)
	{
		vkUnmapMemory(g_vulkanDevice, memory);
		mappedMemory = nullptr;
	}

	if (memory && ownsMemory)
	{
		g_device->enqueueDestroyMemory(memory);
		memory = VK_NULL_HANDLE;
	}

	if (bufferView)
	{
		g_device->enqueueDestroyBufferView(bufferView);
		bufferView = VK_NULL_HANDLE;
	}
}

const GfxCapability& Gfx_GetCapability() { return g_device->m_caps; }

void GfxDevice::DestructionQueue::flush(VkDevice vulkanDevice)
{
	for (VkSampler s : samplers)
	{
		vkDestroySampler(vulkanDevice, s, nullptr);
	}
	samplers.clear();

	for (VkDeviceMemory p : memory)
	{
		vkFreeMemory(vulkanDevice, p, nullptr);
	}
	memory.clear();

	for (VkBuffer p : buffers)
	{
		vkDestroyBuffer(vulkanDevice, p, nullptr);
	}
	buffers.clear();

	for (VkImage p : images)
	{
		vkDestroyImage(vulkanDevice, p, nullptr);
	}
	images.clear();

	for (VkImageView p : imageViews)
	{
		vkDestroyImageView(vulkanDevice, p, nullptr);
	}
	imageViews.clear();

	for (VkBufferView p : bufferViews)
	{
		vkDestroyBufferView(vulkanDevice, p, nullptr);
	}
	bufferViews.clear();

	for (GfxContext* p : contexts)
	{
		g_device->m_freeContexts[u32(p->m_type)].push_back(p);
	}
	contexts.clear();
}
}

#undef V

#else  // RUSH_RENDER_API==RUSH_RENDER_API_VK
	char _VKRenderDevice_cpp; // suppress linker warning
#endif // RUSH_RENDER_API==RUSH_RENDER_API_VK
