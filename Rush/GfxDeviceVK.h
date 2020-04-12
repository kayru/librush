#pragma once

#include "GfxDevice.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_VK

#ifndef VK_ENABLE_BETA_EXTENSIONS
#define VK_ENABLE_BETA_EXTENSIONS
#endif

#include "Window.h"
#include "UtilArray.h"
#include "UtilHash.h"

#include <unordered_map>

#include <volk.h>

namespace Rush
{

struct VkPipelineShaderStageCreateInfoWaveLimitAMD;
struct DescriptorPoolVK;

union MemoryTraitsVK {
	struct
	{
		bool deviceLocal : 1;
		bool hostVisible : 1;
		bool hostCoherent : 1;
		bool hostCached : 1;
		bool lazilyAllocated : 1;
	};
	u32 bits;
};

struct ShaderVK : GfxResourceBase
{
	VkShaderModule module = VK_NULL_HANDLE;
	const char*    entry  = nullptr;

	struct InputMapping
	{
		GfxVertexFormatDesc::Semantic semantic = GfxVertexFormatDesc::Semantic::Unused;
		u8                            semanticIndex = 0;
		u8                            location = 0;
	};

	DynamicArray<InputMapping> inputMappings;

	void destroy();
};

using DescriptorSetLayoutArray = StaticArray<VkDescriptorSetLayout, GfxShaderBindingDesc::MaxDescriptorSets>;

struct PipelineBaseVK : GfxResourceBase
{
	GfxShaderBindingDesc bindings;

	DescriptorSetLayoutArray setLayouts; // lifetime managed by device
	VkPipelineLayout         pipelineLayout = VK_NULL_HANDLE;

	DynamicArray<VkDescriptorSet> descriptorSetCache;
	u32                           descriptorSetCacheFrame = 0;

};

struct TechniqueVK : PipelineBaseVK
{
	DynamicArray<VkPipelineShaderStageCreateInfo> shaderStages;
	GfxRef<GfxVertexFormat>                       vf;
	GfxRef<GfxVertexShader>                       vs;
	GfxRef<GfxGeometryShader>                     gs;
	GfxRef<GfxPixelShader>                        ps;
	GfxRef<GfxComputeShader>                      cs;
	GfxRef<GfxMeshShader>                         ms;

	VkShaderStageFlags pushConstantStageFlags = VkShaderStageFlags(0);
	u32                pushConstantsSize      = 0;

	u32 instanceDataStream = 0xFFFFFFFF;
	u32 vertexStreamCount  = 0;

	VkSpecializationInfo* specializationInfo = nullptr;

	VkPipelineShaderStageCreateInfoWaveLimitAMD* waveLimits = nullptr;

	void destroy();
};

struct VertexFormatVK : GfxResourceBase
{
	GfxVertexFormatDesc                            desc;
	DynamicArray<VkVertexInputAttributeDescription> attributes;
	u32                                            vertexStreamCount          = 0;
	u32                                            instanceDataStream         = 0xFFFFFFFF;
	u32                                            instanceDataAttributeIndex = 0xFFFFFFFF;
	void                                           destroy(){};
};

struct BufferVK : GfxResourceBase
{
	GfxBufferDesc             desc;
	VkDeviceMemory            memory          = VK_NULL_HANDLE;
	VkDescriptorBufferInfo    info            = {};
	VkBufferView              bufferView      = VK_NULL_HANDLE;
	bool                      ownsBuffer      = false;
	bool                      ownsMemory      = false;
	void*                     mappedMemory    = nullptr;
	u32                       size            = 0;
	u32                       lastUpdateFrame = ~0u;

	void destroy();
};

struct DepthStencilStateVK : GfxResourceBase
{
	GfxDepthStencilDesc desc;

	void destroy(){};
};

struct RasterizerStateVK : GfxResourceBase
{
	GfxRasterizerDesc desc;

	void destroy(){};
};

struct BlendStateVK : GfxResourceBase
{
	GfxBlendStateDesc desc;

	void destroy(){};
};

struct TextureVK : GfxResourceBase
{
	GfxTextureDesc desc;
	u32            aspectFlags = 0;

	bool           ownsMemory = false;
	VkDeviceMemory memory     = VK_NULL_HANDLE;

	VkImage image     = VK_NULL_HANDLE;
	bool    ownsImage = false;

	VkImageView   imageView             = VK_NULL_HANDLE;
	VkImageView   depthStencilImageView = VK_NULL_HANDLE;
	VkImageLayout currentLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

	static TextureVK create(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count, const void* pixels);
	static TextureVK create(const GfxTextureDesc& desc, VkImage image, VkImageLayout initialLayout);

	void destroy();
};

struct SamplerVK : GfxResourceBase
{
	GfxSamplerDesc desc;
	VkSampler      native = VK_NULL_HANDLE;

	void destroy();
};

struct DescriptorSetVK : GfxResourceBase
{
	GfxDescriptorSetDesc     desc;
	VkDescriptorSetLayout    layout = VK_NULL_HANDLE; // lifetime managed by device
	VkDescriptorSet          native = VK_NULL_HANDLE;
	DescriptorPoolVK*        pool = nullptr;

	void destroy();
};

struct RayTracingPipelineVK : PipelineBaseVK
{
	VkShaderModule rayGen     = VK_NULL_HANDLE;
	VkShaderModule miss       = VK_NULL_HANDLE;
	VkShaderModule closestHit = VK_NULL_HANDLE;
	VkShaderModule anyHit     = VK_NULL_HANDLE;

	VkPipeline pipeline = VK_NULL_HANDLE;

	u32 maxRecursionDepth = 1;

	BufferVK systemSbt;

	DynamicArray<u8> shaderHandles;

	u32 rayGenOffset   = 0;
	u32 missOffset     = 0;
	u32 hitGroupOffset = 0;

	void destroy();
};

struct AccelerationStructureVK : GfxResourceBase
{
	VkAccelerationStructureNV native = VK_NULL_HANDLE;
	u64                       handle = 0;

	VkAccelerationStructureInfoNV info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };

	GfxAccelerationStructureType            type = GfxAccelerationStructureType::BottomLevel;
	DynamicArray<GfxRayTracingGeometryDesc> geometries;
	DynamicArray<VkGeometryNV>              nativeGeometries;

	// TODO: pool allocations
	VkDeviceMemory memory = VK_NULL_HANDLE;

	void destroy();
};

struct PipelineInfoVK
{
	enum
	{
		MaxVertexStreams = 2
	};

	GfxTechnique         techniqueHandle;
	GfxPrimitive         primitiveType;
	GfxDepthStencilState depthStencilStateHandle;
	GfxRasterizerState   rasterizerStateHandle;
	GfxBlendState        blendStateHandle;
	u32                  vertexBufferStride[MaxVertexStreams];
	u32                  instanceBufferStride;
	VkRenderPass         renderPass;
	u32                  colorAttachmentCount;
	u32                  colorSampleCount;
	u32                  depthSampleCount;
};

struct MemoryBlockVK
{
	VkDeviceMemory memory       = VK_NULL_HANDLE;
	u64            offset       = 0;
	u64            size         = 0;
	VkBuffer       buffer       = VK_NULL_HANDLE;
	void*          mappedBuffer = nullptr;
};

class MemoryAllocatorVK
{
public:

	void          init(u32 memoryType, bool hostVisible);
	MemoryBlockVK alloc(u64 size, u64 alignment);
	void          reset();
	void          releaseBlocks(bool immediate);

	void addBlock(const MemoryBlockVK& block);

	MemoryBlockVK allocBlock(u64 blockSize);
	void          freeBlock(MemoryBlockVK block, bool immediate);

	u32                         m_memoryType = 0;
	DynamicArray<MemoryBlockVK> m_availableBlocks;
	DynamicArray<MemoryBlockVK> m_fullBlocks;
	bool                        m_hostVisible = false;

	static const u32 m_defaultBlockSize = 16 * 1024 * 1024;
};

struct DescriptorPoolVK
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(DescriptorPoolVK)

	struct DescriptorsPerSetDesc
	{
		u16 staticUniformBuffers = 0;
		u16 dynamicUniformBuffers = 0;
		u16 samplers = 0;
		u16 sampledImages = 0;
		u16 storageImages = 0;
		u16 storageBuffers = 0;
		u16 storageTexelBuffers = 0;
		u16 accelerationStructures = 0;
	};

	DescriptorPoolVK() = default;
	DescriptorPoolVK(VkDevice vulkanDevice, const DescriptorsPerSetDesc& desc, u32 maxSets);
	DescriptorPoolVK(DescriptorPoolVK&& other) noexcept;
	~DescriptorPoolVK();
	void reset();

	VkDevice m_vulkanDevice = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
};

class GfxDevice : public GfxRefCount
{
public:
	GfxDevice(Window* window, const GfxConfig& cfg);
	~GfxDevice();

	VkPipeline    createPipeline(const PipelineInfoVK& info);
	VkRenderPass  createRenderPass(const GfxPassDesc& desc);
	VkFramebuffer createFrameBuffer(const GfxPassDesc& desc, VkRenderPass renderPass);

	VkDescriptorSetLayout createDescriptorSetLayout(const GfxDescriptorSetDesc& desc,
		u32 resourceStageFlags,
		bool useDynamicUniformBuffers);

	DescriptorSetLayoutArray createDescriptorSetLayouts(const GfxShaderBindingDesc& desc, u32 resourceStageFlags);

	void          createSwapChain();

	void beginFrame();
	void endFrame();

	u32 memoryTypeFromProperties(u32 memoryTypeBits, VkFlags requiredFlags, VkFlags incompatibleFlags = 0);

	void flushUploadContext(GfxContext* dependentContext = nullptr, bool waitForCompletion = false);

	// TODO: destruction queue shoult be tied to main queue command buffer execution rather than frames
	void euqneueDestroyPipeline(VkPipeline pipeline);
	void enqueueDestroyMemory(VkDeviceMemory object);
	void enqueueDestroyBuffer(VkBuffer object);
	void enqueueDestroyImage(VkImage object);
	void enqueueDestroyImageView(VkImageView object);
	void enqueueDestroyBufferView(VkBufferView object);
	void enqueueDestroySampler(VkSampler object);
	void enqueueDestroyContext(GfxContext* object);
	void enqueueDestroyDescriptorPool(DescriptorPoolVK* object);
	void enqueueDestroyAccelerationStructure(VkAccelerationStructureNV object);

	void captureScreenshot();

	struct FrameBufferKey
	{
		VkRenderPass renderPass;
		u32          depthBufferId;
		u32          colorBufferId[GfxPassDesc::MaxTargets];

		bool operator==(const FrameBufferKey& other) const
		{
			for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
			{
				if (colorBufferId[i] != other.colorBufferId[i])
				{
					return false;
				}
			}

			return depthBufferId == other.depthBufferId && renderPass == other.renderPass;
		}

		struct Hash
		{
			size_t operator()(const FrameBufferKey& k) const { return (size_t)k.renderPass; }
		};
	};

	struct RenderPassKey
	{
		GfxFormat    depthStencilFormat;
		GfxFormat    colorFormats[GfxPassDesc::MaxTargets];
		GfxPassFlags flags;
		u32 colorSampleCount = 0;
		u32 depthSampleCount = 0;

		bool operator==(const RenderPassKey& other) const
		{
			for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
			{
				if (colorFormats[i] != other.colorFormats[i])
				{
					return false;
				}
			}

			return flags == other.flags && depthStencilFormat == other.depthStencilFormat 
				&& colorSampleCount == other.colorSampleCount 
				&& depthSampleCount == other.depthSampleCount;
		}

		struct Hash
		{
			size_t operator()(const RenderPassKey& k) const
			{
				return (size_t)k.colorFormats[0] ^ (size_t)k.flags ^ (size_t)k.depthStencilFormat;
			}
		};
	};

	struct PipelineKey
	{
		u32          techniqueId;
		u32          blendStateId;
		u32          depthStencilStateId;
		u32          rasterizerStateId;
		u32          vertexBufferStride[PipelineInfoVK::MaxVertexStreams];
		u32          colorAttachmentCount;
		u32          colorSampleCount;
		u32          depthSampleCount;
		GfxPrimitive primitiveType;

		bool operator==(const PipelineKey& other) const
		{
			if (techniqueId != other.techniqueId || blendStateId != other.blendStateId ||
			    depthStencilStateId != other.depthStencilStateId || rasterizerStateId != other.rasterizerStateId ||
			    primitiveType != other.primitiveType || colorAttachmentCount != other.colorAttachmentCount ||
				colorSampleCount != other.colorSampleCount || depthSampleCount != other.depthSampleCount)
			{
				return false;
			}

			for (u32 i = 0; i < RUSH_COUNTOF(vertexBufferStride); ++i)
			{
				if (vertexBufferStride[i] != other.vertexBufferStride[i])
				{
					return false;
				}
			}

			return true;
		}

		struct Hash
		{
			size_t operator()(const PipelineKey& k) const
			{
				return (k.techniqueId) ^ (k.blendStateId << 16) ^ ((u32)k.primitiveType << 24);
			}
		};
	};

	struct DescriptorSetLayoutKey
	{
		GfxDescriptorSetDesc desc;
		u32 resourceStageFlags = 0;
		bool useDynamicUniformBuffers = 0;

		bool operator==(const DescriptorSetLayoutKey& other) const
		{
			return desc == other.desc
				&& resourceStageFlags == other.resourceStageFlags
				&& useDynamicUniformBuffers == other.useDynamicUniformBuffers;
		}

		struct Hash
		{
			size_t operator()(const DescriptorSetLayoutKey& k) const
			{
				return size_t(hashFnv1a64(&k.desc, sizeof(k.desc)));
			}
		};
	};

	GfxCapability m_caps;

	VkDebugReportCallbackEXT         m_debugReportCallbackExt;
	VkInstance                       m_vulkanInstance = VK_NULL_HANDLE;
	VkDevice                         m_vulkanDevice   = VK_NULL_HANDLE;
	VkPhysicalDevice                 m_physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties       m_physicalDeviceProps;
	VkPhysicalDeviceProperties2      m_physicalDeviceProps2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	VkPhysicalDeviceFeatures2        m_physicalDeviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	VkPhysicalDeviceDescriptorIndexingFeatures m_physicalDeviceDescriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
	VkPhysicalDeviceMeshShaderFeaturesNV m_nvMeshShaderFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV };
	VkPhysicalDeviceMemoryProperties m_deviceMemoryProps = {};
	VkPhysicalDeviceRayTracingPropertiesNV m_nvRayTracingProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
	VkPhysicalDeviceMeshShaderPropertiesNV m_nvMeshShaderProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV };
	VkPhysicalDeviceDescriptorIndexingProperties m_descriptorIndexingProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES };
	DynamicArray<MemoryTraitsVK>      m_memoryTraits;

	struct MemoryTypes
	{
		u32 local = ~0u;
		u32 host  = ~0u;
	} m_memoryTypes;

	VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;
	VkCommandPool m_computeCommandPool  = VK_NULL_HANDLE;
	VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;

	VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

	std::unordered_map<PipelineKey, VkPipeline, PipelineKey::Hash>          m_pipelines;
	std::unordered_map<RenderPassKey, VkRenderPass, RenderPassKey::Hash>    m_renderPasses;
	std::unordered_map<FrameBufferKey, VkFramebuffer, FrameBufferKey::Hash> m_frameBuffers;
	std::unordered_map<DescriptorSetLayoutKey, VkDescriptorSetLayout, DescriptorSetLayoutKey::Hash> m_descriptorSetLayouts;

	DynamicArray<VkPhysicalDevice>        m_physicalDevices;
	DynamicArray<VkQueueFamilyProperties> m_queueProps;

	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_computeQueue  = VK_NULL_HANDLE;
	VkQueue m_transferQueue = VK_NULL_HANDLE;

	u32 m_graphicsQueueIndex = 0xFFFFFFFF;
	u32 m_computeQueueIndex  = 0xFFFFFFFF;
	u32 m_transferQueueIndex = 0xFFFFFFFF;

	VkSemaphore m_presentCompleteSemaphore = VK_NULL_HANDLE;

	// swap chain

	VkSwapchainKHR        m_swapChain                   = VK_NULL_HANDLE;
	VkSurfaceKHR          m_swapChainSurface            = VK_NULL_HANDLE;
	VkExtent2D            m_swapChainExtent             = VkExtent2D{0, 0};
	VkPresentModeKHR      m_swapChainPresentMode        = VK_PRESENT_MODE_MAX_ENUM_KHR;
	DynamicArray<VkImage> m_swapChainImages;
	u32                   m_swapChainIndex = 0;
	bool                  m_swapChainValid = false;

	DynamicArray<VkPresentModeKHR> m_availablePresentModes;

	GfxOwn<GfxTexture>               m_depthBufferTexture;
	DynamicArray<GfxOwn<GfxTexture>> m_swapChainTextures;

	// resources

	ResourcePool<TechniqueVK, GfxTechnique>                         m_techniques;
	ResourcePool<ShaderVK, UntypedResourceHandle>                   m_shaders;
	ResourcePool<VertexFormatVK, GfxVertexFormat>                   m_vertexFormats;
	ResourcePool<BufferVK, GfxBuffer>                               m_buffers;
	ResourcePool<DepthStencilStateVK, GfxDepthStencilState>         m_depthStencilStates;
	ResourcePool<RasterizerStateVK, GfxRasterizerState>             m_rasterizerStates;
	ResourcePool<TextureVK, GfxTexture>                             m_textures;
	ResourcePool<BlendStateVK, GfxBlendState>                       m_blendStates;
	ResourcePool<SamplerVK, GfxSampler>                             m_samplers;
	ResourcePool<DescriptorSetVK, GfxDescriptorSet>                 m_descriptorSets;
	ResourcePool<RayTracingPipelineVK, GfxRayTracingPipeline>       m_rayTracingPipelines;
	ResourcePool<AccelerationStructureVK, GfxAccelerationStructure> m_accelerationStructures;

	template <typename HandleType>
	static GfxOwn<HandleType> makeOwn(HandleType h) { return GfxOwn<HandleType>(h); }

	struct DestructionQueue
	{
		DynamicArray<VkPipeline>        pipelines;
		DynamicArray<VkDeviceMemory>    memory;
		DynamicArray<VkBuffer>          buffers;
		DynamicArray<VkImage>           images;
		DynamicArray<VkImageView>       imageViews;
		DynamicArray<VkBufferView>      bufferViews;
		DynamicArray<GfxContext*>       contexts;
		DynamicArray<VkSampler>         samplers;
		DynamicArray<DescriptorPoolVK*> descriptorPools;
		DynamicArray<MemoryBlockVK>     transientHostMemory;
		DynamicArray<VkAccelerationStructureNV> accelerationStructures;

		void flush(GfxDevice* device);
	};

	struct FrameData
	{
		VkDescriptorPool currentDescriptorPool = VK_NULL_HANDLE;
		DynamicArray<DescriptorPoolVK> descriptorPools;
		DynamicArray<DescriptorPoolVK> availableDescriptorPools;

		VkQueryPool       timestampPool = VK_NULL_HANDLE;
		DynamicArray<u64> timestampPoolData;
		DynamicArray<u16> timestampSlotMap;
		u32               timestampIssuedCount = 0;

		DestructionQueue destructionQueue;

		u32     frameIndex             = ~0u;
		VkFence lastGraphicsFence      = VK_NULL_HANDLE;
		bool    presentSemaphoreWaited = false;
	};

	DynamicArray<FrameData> m_frameData;
	FrameData*             m_currentFrame = nullptr;

	MemoryAllocatorVK m_transientLocalAllocator;
	MemoryAllocatorVK m_transientHostAllocator;

	u32 m_uniqueResourceCounter = 1;
	u32 m_frameCount            = 0;

	GfxStats m_stats;

	GfxContext* m_currentUploadContext = nullptr;

	DynamicArray<GfxContext*> m_freeContexts[u32(GfxContextType::count)];

	u32 m_presentInterval            = 1;
	u32 m_desiredPresentInterval     = m_presentInterval;
	u32 m_desiredSwapChainImageCount = 2;

	Window*             m_window = nullptr;
	WindowEventListener m_resizeEvents;

	GfxScreenshotCallback m_pendingScreenshotCallback = nullptr;
	void*                 m_pendingScreenshotUserData = nullptr;

	struct SupportedExtensions
	{
		bool AMD_negative_viewport_height         = false;
		bool AMD_shader_explicit_vertex_parameter = false;
		bool AMD_wave_limits                      = false;
		bool EXT_sample_locations                 = false;
		bool KHR_maintenance1                     = false;
		bool NV_framebuffer_mixed_samples         = false;
		bool NV_geometry_shader_passthrough       = false;
		bool NV_ray_tracing                       = false;
		bool NV_mesh_shader                       = false;
	} m_supportedExtensions;

	bool m_useNegativeViewport = false;
};

struct ClearParamsVK
{
	GfxClearFlags flags   = GfxClearFlags::None;
	ColorRGBA     color   = ColorRGBA::Black();
	float         depth   = 1.0f;
	u32           stencil = 0;

	VkClearValue getClearColor() const
	{
		VkClearValue result;
		result.color.float32[0] = color.r;
		result.color.float32[1] = color.g;
		result.color.float32[2] = color.b;
		result.color.float32[3] = color.a;
		return result;
	}

	VkClearValue getClearDepthStencil() const
	{
		VkClearValue result;
		result.depthStencil.depth   = depth;
		result.depthStencil.stencil = stencil;
		return result;
	}
};

class GfxContext : public GfxRefCount
{
public:

	RUSH_DISALLOW_COPY_AND_ASSIGN(GfxContext)

	enum
	{
		MaxTextures        = 16,
		MaxStorageImages   = 8,
		MaxVertexStreams   = PipelineInfoVK::MaxVertexStreams,
		MaxConstantBuffers = 4,
		MaxStorageBuffers  = 6,
		MaxDescriptorSets  = GfxShaderBindingDesc::MaxDescriptorSets,
	};

	GfxContext(GfxContextType contextType = GfxContextType::Graphics);
	~GfxContext();

	void setName(const char* name) { m_name = name; }

	void beginBuild();
	void endBuild();
	void submit(VkQueue queue);
	void split();
	void addDependency(VkSemaphore waitSemaphore, VkPipelineStageFlags waitDstStageMask);

	// TODO: buffer barriers!
	VkImageLayout addImageBarrier(VkImage image, VkImageLayout nextLayout, VkImageLayout currentLayout,
	    VkImageSubresourceRange* subresourceRange = nullptr, bool force = false);
	void addBufferBarrier(GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
	    VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage);
	void flushBarriers();

	void beginRenderPass(const GfxPassDesc& desc);
	void endRenderPass();
	void resolveImage(GfxTextureArg src, GfxTextureArg dst);

	void applyState();

	VkFence             m_fence                = VK_NULL_HANDLE;
	VkCommandBuffer     m_commandBuffer        = VK_NULL_HANDLE;
	VkDescriptorSet     m_currentDescriptorSet = VK_NULL_HANDLE;
	VkPipelineBindPoint m_currentBindPoint     = VK_PIPELINE_BIND_POINT_MAX_ENUM;

	bool        m_isActive = false;
	const char* m_name     = "";

	enum DirtyStateFlag
	{
		DirtyStateFlag_Technique              = 1 << 0,
		DirtyStateFlag_PrimitiveType          = 1 << 1,
		DirtyStateFlag_VertexBuffer           = 1 << 2,
		DirtyStateFlag_IndexBuffer            = 1 << 3,
		DirtyStateFlag_Texture                = 1 << 4,
		DirtyStateFlag_BlendState             = 1 << 5,
		DirtyStateFlag_DepthStencilState      = 1 << 6,
		DirtyStateFlag_RasterizerState        = 1 << 7,
		DirtyStateFlag_Sampler                = 1 << 8,
		DirtyStateFlag_ConstantBuffer         = 1 << 9,
		DirtyStateFlag_StorageImage           = 1 << 10,
		DirtyStateFlag_StorageBuffer          = 1 << 11,
		DirtyStateFlag_AccelerationStructure  = 1 << 12,
		DirtyStateFlag_ConstantBufferOffset   = 1 << 13,
		DirtyStateFlag_DescriptorSet          = 1 << 14,

		DirtyStateFlag_Descriptors = DirtyStateFlag_ConstantBuffer | DirtyStateFlag_Texture | DirtyStateFlag_Sampler |
		                             DirtyStateFlag_StorageImage | DirtyStateFlag_StorageBuffer |
		                             DirtyStateFlag_ConstantBufferOffset | DirtyStateFlag_AccelerationStructure,
		DirtyStateFlag_Pipeline = DirtyStateFlag_Technique | DirtyStateFlag_PrimitiveType | DirtyStateFlag_BlendState |
		                          DirtyStateFlag_DepthStencilState | DirtyStateFlag_RasterizerState,
	};

	u32 m_dirtyState = 0xFFFFFFFF;

	struct PendingState
	{
		GfxPrimitive             primitiveType = GfxPrimitive::TriangleList;
		GfxTechnique             technique;
		GfxRayTracingPipeline    rayTracingPipeline;
		GfxBuffer                vertexBuffer[MaxVertexStreams];
		GfxBuffer                indexBuffer;
		GfxBuffer                constantBuffers[MaxConstantBuffers];
		GfxTexture               textures[MaxTextures];
		GfxSampler               samplers[MaxTextures];
		GfxTexture               storageImages[MaxStorageImages];
		GfxBuffer                storageBuffers[MaxStorageBuffers];
		GfxAccelerationStructure accelerationStructure;
		GfxBlendState            blendState;
		GfxDepthStencilState     depthStencilState;
		GfxRasterizerState       rasterizerState;
		u32                      constantBufferOffsets[MaxConstantBuffers] = {};
		u32                      vertexBufferStride[MaxVertexStreams]      = {};
		GfxDescriptorSet         descriptorSets[MaxDescriptorSets]         = {};
	} m_pending;

	VkPipeline m_activePipeline = VK_NULL_HANDLE;

	ClearParamsVK m_pendingClear;
	bool          m_isRenderPassActive = false;

	VkFramebuffer m_currentFrameBuffer = VK_NULL_HANDLE;
	VkRenderPass  m_currentRenderPass  = VK_NULL_HANDLE;
	GfxPassDesc   m_currentRenderPassDesc;
	u32           m_currentColorAttachmentCount = 0;
	u32           m_currentColorSampleCount = 0;
	u32           m_currentDepthSampleCount = 0;

	VkRect2D m_currentRenderRect = {};

	VkSemaphore m_completionSemaphore    = VK_NULL_HANDLE;
	bool        m_useCompletionSemaphore = false;

	DynamicArray<VkSemaphore>          m_waitSemaphores;
	DynamicArray<VkPipelineStageFlags> m_waitDstStageMasks;

	GfxContextType m_type = GfxContextType::Graphics;

	u32 m_lastUsedFrame = ~0u;

	struct PendingBarriers
	{
		u32                                 srcStageMask    = 0; // VkPipelineStageFlagBits
		u32                                 dstStageMask    = 0; // VkPipelineStageFlagBits
		u32                                 dependencyFlags = 0;
		DynamicArray<VkImageMemoryBarrier>  imageBarriers;
		DynamicArray<VkBufferMemoryBarrier> bufferBarriers;
	} m_pendingBarriers;

	struct BufferCopyCommand
	{
		VkBuffer     srcBuffer;
		VkBuffer     dstBuffer;
		VkBufferCopy region;
	};

	DynamicArray<BufferCopyCommand> m_pendingBufferUploads;
};

void Gfx_vkFillBuffer(GfxContext* ctx, GfxBuffer h, u32 value);

void Gfx_vkFullPipelineBarrier(GfxContext* ctx);

void Gfx_vkBufferMemoryBarrier(GfxContext* ctx, GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
    VkPipelineStageFlagBits srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

void Gfx_vkExecutionBarrier(GfxContext* ctx, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage);

void Gfx_vkFlushMappedBuffer(GfxBuffer h);
void Gfx_vkFlushBarriers(GfxContext* ctx);

VkFormat Gfx_vkConvertFormat(GfxFormat format);

const char* toString(VkResult value);

}

#endif
