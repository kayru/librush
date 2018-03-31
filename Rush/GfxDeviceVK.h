#pragma once

#include "GfxDevice.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_VK

#include "GfxRef.h"
#include "Window.h"

#define VK_USE_PLATFORM_WIN32_KHR

#include <unordered_map>
#include <vulkan/vulkan.h>

namespace Rush
{

struct VkPipelineShaderStageCreateInfoWaveLimitAMD;

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

struct ShaderVK : GfxRefCount
{
	u32            id     = 0;
	VkShaderModule module = VK_NULL_HANDLE;
	const char*    entry  = nullptr;

	struct InputMapping
	{
		GfxVertexFormatDesc::Semantic semantic : 4;
		u8                            semanticIndex : 4;
		u8                            location = 0;
	};
	static_assert(sizeof(InputMapping) == 2, "InputMapping is expected to be exactly 2 bytes");
	std::vector<InputMapping> inputMappings;

	void destroy();
};

struct TechniqueVK : GfxRefCount
{
	u32                                          id = 0;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	GfxShaderBindings                            bindings;
	GfxRef<GfxVertexFormat>                      vf;
	GfxRef<GfxVertexShader>                      vs;
	GfxRef<GfxPixelShader>                       ps;
	GfxRef<GfxComputeShader>                     cs;
	VkDescriptorSetLayout                        descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout                             pipelineLayout      = VK_NULL_HANDLE;

	VkShaderStageFlags pushConstantStageFlags = VK_SHADER_STAGE_VERTEX_BIT; // TODO: support other stages
	u32                pushConstantsSize      = 0;

	u32 combinedSamplerCount   = 0;
	u32 samplerCount           = 0;
	u32 sampledImageCount      = 0;
	u32 constantBufferCount    = 0;
	u32 storageImageCount      = 0;
	u32 storageBufferCount     = 0;
	u32 typedStorageBufferMask = 0;

	u32 instanceDataStream = 0xFFFFFFFF;
	u32 vertexStreamCount  = 0;

	std::vector<VkDescriptorSet> descriptorSetCache;
	u32                          descriptorSetCacheFrame = 0;

	VkSpecializationInfo* specializationInfo = nullptr;

	VkPipelineShaderStageCreateInfoWaveLimitAMD* waveLimits = nullptr;

	void destroy();
};

struct VertexFormatVK : GfxRefCount
{
	u32                                            id = 0;
	GfxVertexFormatDesc                            desc;
	std::vector<VkVertexInputAttributeDescription> attributes;
	u32                                            vertexStreamCount          = 0;
	u32                                            instanceDataStream         = 0xFFFFFFFF;
	u32                                            instanceDataAttributeIndex = 0xFFFFFFFF;
	void                                           destroy(){};
};

struct BufferVK : GfxRefCount
{
	u32                    id = 0;
	GfxBufferDesc          desc;
	VkDeviceMemory         memory          = VK_NULL_HANDLE;
	VkDescriptorBufferInfo info            = {};
	VkBufferView           bufferView      = VK_NULL_HANDLE;
	bool                   ownsBuffer      = false;
	bool                   ownsMemory      = false;
	void*                  mappedMemory    = nullptr;
	u32                    size            = 0;
	u32                    lastUpdateFrame = ~0u;

	void destroy();
};

struct DepthStencilStateVK : GfxRefCount
{
	u32                 id = 0;
	GfxDepthStencilDesc desc;

	void destroy(){};
};

struct RasterizerStateVK : GfxRefCount
{
	u32               id = 0;
	GfxRasterizerDesc desc;

	void destroy(){};
};

struct BlendStateVK : GfxRefCount
{
	u32               id = 0;
	GfxBlendStateDesc desc;

	void destroy(){};
};

struct TextureVK : GfxRefCount
{
	u32            id = 0;
	GfxTextureDesc desc;

	bool           ownsMemory = false;
	VkDeviceMemory memory     = VK_NULL_HANDLE;

	VkImage image     = VK_NULL_HANDLE;
	bool    ownsImage = false;

	VkImageView   imageView             = VK_NULL_HANDLE;
	VkImageView   depthStencilImageView = VK_NULL_HANDLE;
	VkImageLayout currentLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

	static TextureVK create(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count);
	static TextureVK create(const GfxTextureDesc& desc, VkImage image, VkImageLayout initialLayout);

	void destroy();
};

struct SamplerVK : GfxRefCount
{
	u32            id = 0;
	GfxSamplerDesc desc;
	VkSampler      native = VK_NULL_HANDLE;

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
	void          releaseBlocks();

private:
	MemoryBlockVK allocBlock(u64 blockSize);
	void          freeBlock(MemoryBlockVK block);

	u32                        m_memoryType = 0;
	std::vector<MemoryBlockVK> m_availableBlocks;
	std::vector<MemoryBlockVK> m_fullBlocks;
	bool                       m_hostVisible = false;

	static const u32 m_defaultBlockSize = 16 * 1024 * 1024;
};

class GfxDevice : public GfxRefCount
{
public:
	GfxDevice(Window* window, const GfxConfig& cfg);
	~GfxDevice();

	VkPipeline    createPipeline(const PipelineInfoVK& info);
	VkRenderPass  createRenderPass(const GfxPassDesc& desc);
	VkFramebuffer createFrameBuffer(const GfxPassDesc& desc, VkRenderPass renderPass);
	void          createSwapChain();

	void beginFrame();
	void endFrame();

	u32 memoryTypeFromProperties(u32 memoryTypeBits, VkFlags requiredFlags, VkFlags incompatibleFlags = 0);

	void flushUploadContext(GfxContext* dependentContext = nullptr, bool waitForCompletion = false);

	void enqueueDestroyMemory(VkDeviceMemory object);
	void enqueueDestroyBuffer(VkBuffer object);
	void enqueueDestroyImage(VkImage object);
	void enqueueDestroyImageView(VkImageView object);
	void enqueueDestroyBufferView(VkBufferView object);
	void enqueueDestroySampler(VkSampler object);
	;
	void enqueueDestroyContext(GfxContext* object);

	u32 generateId();

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

		bool operator==(const RenderPassKey& other) const
		{
			for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
			{
				if (colorFormats[i] != other.colorFormats[i])
				{
					return false;
				}
			}

			return flags == other.flags && depthStencilFormat == other.depthStencilFormat;
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
		GfxPrimitive primitiveType;

		bool operator==(const PipelineKey& other) const
		{
			if (techniqueId != other.techniqueId || blendStateId != other.blendStateId ||
			    depthStencilStateId != other.depthStencilStateId || rasterizerStateId != other.rasterizerStateId ||
			    primitiveType != other.primitiveType || colorAttachmentCount != other.colorAttachmentCount)
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

	GfxCapability m_caps;

	VkDebugReportCallbackEXT         m_debugReportCallbackExt;
	VkInstance                       m_vulkanInstance = VK_NULL_HANDLE;
	VkDevice                         m_vulkanDevice   = VK_NULL_HANDLE;
	VkPhysicalDevice                 m_physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties       m_physicalDeviceProps;
	VkPhysicalDeviceProperties2KHR   m_physicalDeviceProps2;
	VkPhysicalDeviceFeatures         m_physicalDeviceFeatures;
	VkPhysicalDeviceMemoryProperties m_deviceMemoryProps;
	std::vector<MemoryTraitsVK>      m_memoryTraits;

	struct MemoryTypes
	{
		u32 local          = ~0u;
		u32 hostVisible    = ~0u;
		u32 hostOnly       = ~0u;
		u32 hostOnlyCached = ~0u;
	} m_memoryTypes;

	VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;
	VkCommandPool m_computeCommandPool  = VK_NULL_HANDLE;
	VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;

	VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

	std::unordered_map<PipelineKey, VkPipeline, PipelineKey::Hash>          m_pipelines;
	std::unordered_map<RenderPassKey, VkRenderPass, RenderPassKey::Hash>    m_renderPasses;
	std::unordered_map<FrameBufferKey, VkFramebuffer, FrameBufferKey::Hash> m_frameBuffers;

	std::vector<VkPhysicalDevice>        m_physicalDevices;
	std::vector<VkQueueFamilyProperties> m_queueProps;

	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_computeQueue  = VK_NULL_HANDLE;
	VkQueue m_transferQueue = VK_NULL_HANDLE;

	u32 m_graphicsQueueIndex = 0xFFFFFFFF;
	u32 m_computeQueueIndex  = 0xFFFFFFFF;
	u32 m_transferQueueIndex = 0xFFFFFFFF;

	VkSemaphore m_presentCompleteSemaphore = VK_NULL_HANDLE;

	// swap chain

	VkSwapchainKHR       m_swapChain                   = VK_NULL_HANDLE;
	VkSurfaceKHR         m_swapChainSurface            = VK_NULL_HANDLE;
	VkExtent2D           m_swapChainExtent             = VkExtent2D{0, 0};
	VkPresentModeKHR     m_swapChainPresentMode        = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR     m_pendingSwapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	std::vector<VkImage> m_swapChainImages;
	u32                  m_swapChainIndex = 0;

	GfxTexture              m_depthBufferTexture;
	std::vector<GfxTexture> m_swapChainTextures;

	// resources

	DynamicResourcePool<ShaderVK, GfxVertexShader>                 m_vertexShaders;
	DynamicResourcePool<ShaderVK, GfxPixelShader>                  m_pixelShaders;
	DynamicResourcePool<ShaderVK, GfxComputeShader>                m_computeShaders;
	DynamicResourcePool<VertexFormatVK, GfxVertexFormat>           m_vertexFormats;
	DynamicResourcePool<BufferVK, GfxBuffer>                       m_buffers;
	DynamicResourcePool<TechniqueVK, GfxTechnique>                 m_techniques;
	DynamicResourcePool<DepthStencilStateVK, GfxDepthStencilState> m_depthStencilStates;
	DynamicResourcePool<RasterizerStateVK, GfxRasterizerState>     m_rasterizerStates;
	DynamicResourcePool<TextureVK, GfxTexture>                     m_textures;
	DynamicResourcePool<BlendStateVK, GfxBlendState>               m_blendStates;
	DynamicResourcePool<SamplerVK, GfxSampler>                     m_samplers;

	struct DestructionQueue
	{
		std::vector<VkDeviceMemory> memory;
		std::vector<VkBuffer>       buffers;
		std::vector<VkImage>        images;
		std::vector<VkImageView>    imageViews;
		std::vector<VkBufferView>   bufferViews;
		std::vector<GfxContext*>    contexts;
		std::vector<VkSampler>      samplers;

		void flush(VkDevice vulkanDevice);
	};

	struct FrameData
	{
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		VkQueryPool      timestampPool = VK_NULL_HANDLE;
		std::vector<u64> timestampPoolData;
		std::vector<u8>  timestampSlotMap;
		u32              timestampIssuedCount = 0;

		DestructionQueue destructionQueue;

		MemoryAllocatorVK localOnlyAllocator;
		MemoryAllocatorVK hostVisibleAllocator;
		MemoryAllocatorVK hostOnlyAllocator;
		MemoryAllocatorVK hostOnlyCachedAllocator;

		u32     frameIndex             = ~0u;
		VkFence lastGraphicsFence      = VK_NULL_HANDLE;
		bool    presentSemaphoreWaited = false;
	};

	std::vector<FrameData> m_frameData;
	FrameData*             m_currentFrame = nullptr;

	u32 m_uniqueResourceCounter = 1;
	u32 m_frameCount            = 0;

	GfxStats m_stats;

	GfxContext* m_currentUploadContext = nullptr;

	std::vector<GfxContext*> m_freeContexts[u32(GfxContextType::count)];

	u32 m_presentInterval            = 1; // TODO: this should control the vsync period
	u32 m_desiredSwapChainImageCount = 2;

	Window*             m_window = nullptr;
	WindowEventListener m_resizeEvents;

	GfxScreenshotCallback m_pendingScreenshotCallback = nullptr;
	void*                 m_pendingScreenshotUserData = nullptr;

	struct SupportedExtensions
	{
		bool AMD_negative_viewport_height = false;
		bool KHR_maintenance1             = false;
		bool AMD_wave_limits              = false;
		bool EXT_shader_subgroup_vote     = false;
		bool EXT_shader_subgroup_ballot   = false;
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
	enum
	{
		MaxTextures        = 16,
		MaxStorageImages   = 8,
		MaxVertexStreams   = PipelineInfoVK::MaxVertexStreams,
		MaxConstantBuffers = 4,
		MaxStorageBuffers  = 6,
	};

	GfxContext(GfxContextType contextType = GfxContextType::Graphics);
	~GfxContext();

	void beginBuild();
	void endBuild();
	void submit(VkQueue queue);
	void split();
	void addDependency(VkSemaphore waitSemaphore, VkPipelineStageFlags waitDstStageMask);

	// TODO: buffer barriers!
	void addImageBarrier(VkImage image, VkImageLayout nextLayout, VkImageLayout currentLayout,
	    VkImageSubresourceRange* subresourceRange = nullptr, bool force = false);
	void addBufferBarrier(GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
	    VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage);
	void flushBarriers();

	void beginRenderPass(const GfxPassDesc& desc);
	void endRenderPass();

	void applyState();

	VkFence         m_fence;
	VkCommandBuffer m_commandBuffer;
	bool            m_isActive = false;

	enum DirtyStateFlag
	{
		DirtyStateFlag_Technique         = 1 << 0,
		DirtyStateFlag_PrimitiveType     = 1 << 1,
		DirtyStateFlag_VertexBuffer      = 1 << 2,
		DirtyStateFlag_IndexBuffer       = 1 << 3,
		DirtyStateFlag_Texture           = 1 << 4,
		DirtyStateFlag_BlendState        = 1 << 5,
		DirtyStateFlag_DepthStencilState = 1 << 6,
		DirtyStateFlag_RasterizerState   = 1 << 7,
		DirtyStateFlag_Sampler           = 1 << 8,
		DirtyStateFlag_ConstantBuffer    = 1 << 9,
		DirtyStateFlag_StorageImage      = 1 << 10,
		DirtyStateFlag_StorageBuffer     = 1 << 11,

		DirtyStateFlag_Descriptors = DirtyStateFlag_ConstantBuffer | DirtyStateFlag_Texture | DirtyStateFlag_Sampler |
		                             DirtyStateFlag_StorageImage | DirtyStateFlag_StorageBuffer,
		DirtyStateFlag_Pipeline = DirtyStateFlag_Technique | DirtyStateFlag_PrimitiveType | DirtyStateFlag_BlendState |
		                          DirtyStateFlag_DepthStencilState | DirtyStateFlag_RasterizerState,
	};

	u32 m_dirtyState = 0xFFFFFFFF;

	struct PendingState
	{
		GfxPrimitive                 primitiveType = GfxPrimitive::TriangleList;
		GfxRef<GfxTechnique>         technique;
		GfxRef<GfxBuffer>            vertexBuffer[MaxVertexStreams];
		GfxRef<GfxBuffer>            indexBuffer;
		GfxRef<GfxBuffer>            constantBuffers[MaxConstantBuffers];
		GfxRef<GfxTexture>           textures[MaxTextures];
		GfxRef<GfxSampler>           samplers[MaxTextures];
		GfxRef<GfxTexture>           storageImages[MaxStorageImages];
		GfxRef<GfxBuffer>            storageBuffers[MaxStorageBuffers];
		GfxRef<GfxBlendState>        blendState;
		GfxRef<GfxDepthStencilState> depthStencilState;
		GfxRef<GfxRasterizerState>   rasterizerState;
		size_t                       constantBufferOffsets[MaxConstantBuffers] = {};
		u32                          vertexBufferStride[MaxVertexStreams]      = {};
	} m_pending;

	VkPipeline m_activePipeline = VK_NULL_HANDLE;

	ClearParamsVK m_pendingClear;
	bool          m_isRenderPassActive = false;

	VkFramebuffer m_currentFrameBuffer = VK_NULL_HANDLE;
	VkRenderPass  m_currentRenderPass  = VK_NULL_HANDLE;
	GfxPassDesc   m_currentRenderPassDesc;
	u32           m_currentColorAttachmentCount = 0;

	VkRect2D m_currentRenderRect = {};

	VkSemaphore m_completionSemaphore    = VK_NULL_HANDLE;
	bool        m_useCompletionSemaphore = false;

	std::vector<VkSemaphore>          m_waitSemaphores;
	std::vector<VkPipelineStageFlags> m_waitDstStageMasks;

	GfxContextType m_type = GfxContextType::Graphics;

	u32 m_lastUsedFrame = ~0u;

	struct PendingBarriers
	{
		u32                                srcStageMask    = 0; // VkPipelineStageFlagBits
		u32                                dstStageMask    = 0; // VkPipelineStageFlagBits
		u32                                dependencyFlags = 0;
		std::vector<VkImageMemoryBarrier>  imageBarriers;
		std::vector<VkBufferMemoryBarrier> bufferBarriers;
	} m_pendingBarriers;

	struct BufferCopyCommand
	{
		VkBuffer     srcBuffer;
		VkBuffer     dstBuffer;
		VkBufferCopy region;
	};

	std::vector<BufferCopyCommand> m_pendingBufferUploads;
};

void Gfx_vkFillBuffer(GfxContext* ctx, GfxBuffer h, u32 value);

void Gfx_vkFullPipelineBarrier(GfxContext* ctx);

void Gfx_vkBufferMemoryBarrier(GfxContext* ctx, GfxBuffer h, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
    VkPipelineStageFlagBits srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlagBits dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

void Gfx_vkExecutionBarrier(GfxContext* ctx, VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage);

void Gfx_vkFlushMappedBuffer(GfxBuffer h);
void Gfx_vkFlushBarriers(GfxContext* ctx);
}

#endif
