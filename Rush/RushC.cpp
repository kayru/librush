#include "RushC.h"
#include "Platform.h"
#include "Window.h"
#include "GfxDevice.h"
#include "GfxEmbeddedShaders.h"
#include "GfxBitmapFont.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

using namespace Rush;

namespace {

rush_app_config convert(const AppConfig* cfg)
{
	rush_app_config result;
	result.name             = cfg->name;
	result.vsync            = cfg->vsync;
	result.width            = cfg->width;
	result.height           = cfg->height;
	result.max_width        = cfg->maxWidth;
	result.max_height       = cfg->maxHeight;
	result.fullscreen       = cfg->fullScreen;
	result.resizable        = cfg->resizable;
	result.debug            = cfg->debug;
	result.warp             = cfg->warp;
	result.minimize_latency = cfg->minimizeLatency;
	result.argc             = cfg->argc;
	result.argv             = cfg->argv;
	return result;
}

AppConfig convert(const rush_app_config* cfg)
{
	AppConfig result;
	result.name            = cfg->name;
	result.vsync           = cfg->vsync;
	result.width           = cfg->width;
	result.height          = cfg->height;
	result.maxWidth        = cfg->max_width;
	result.maxHeight       = cfg->max_height;
	result.fullScreen      = cfg->fullscreen;
	result.resizable       = cfg->resizable;
	result.debug           = cfg->debug;
	result.warp            = cfg->warp;
	result.minimizeLatency = cfg->minimize_latency;
	result.argc            = cfg->argc;
	result.argv            = cfg->argv;
	return result;
}

GfxFormat convert(rush_gfx_format fmt)
{
	switch(fmt)
	{
	default:
	case RUSH_GFX_FORMAT_UNKNOWN          : return GfxFormat_Unknown;
	case RUSH_GFX_FORMAT_D24_UNORM_S8_UINT: return GfxFormat_D24_Unorm_S8_Uint;
	case RUSH_GFX_FORMAT_D24_UNORM_X8     : return GfxFormat_D24_Unorm_X8;
	case RUSH_GFX_FORMAT_D32_FLOAT        : return GfxFormat_D32_Float;
	case RUSH_GFX_FORMAT_D32_FLOAT_S8_UINT: return GfxFormat_D32_Float_S8_Uint;
	case RUSH_GFX_FORMAT_R8_UNORM         : return GfxFormat_R8_Unorm;
	case RUSH_GFX_FORMAT_R16_FLOAT        : return GfxFormat_R16_Float;
	case RUSH_GFX_FORMAT_R16_UINT         : return GfxFormat_R16_Uint;
	case RUSH_GFX_FORMAT_R32_FLOAT        : return GfxFormat_R32_Float;
	case RUSH_GFX_FORMAT_R32_UINT         : return GfxFormat_R32_Uint;
	case RUSH_GFX_FORMAT_RG8_UNORM        : return GfxFormat_RG8_Unorm;
	case RUSH_GFX_FORMAT_RG16_FLOAT       : return GfxFormat_RG16_Float;
	case RUSH_GFX_FORMAT_RG32_FLOAT       : return GfxFormat_RG32_Float;
	case RUSH_GFX_FORMAT_RGB32_FLOAT      : return GfxFormat_RGB32_Float;
	case RUSH_GFX_FORMAT_RGB8_UNORM       : return GfxFormat_RGB8_Unorm;
	case RUSH_GFX_FORMAT_RGBA16_FLOAT     : return GfxFormat_RGBA16_Float;
	case RUSH_GFX_FORMAT_RGBA16_UNORM     : return GfxFormat_RGBA16_Unorm;
	case RUSH_GFX_FORMAT_RGBA32_FLOAT     : return GfxFormat_RGBA32_Float;
	case RUSH_GFX_FORMAT_RGBA8_UNORM      : return GfxFormat_RGBA8_Unorm;
	case RUSH_GFX_FORMAT_RGBA8_SRGB       : return GfxFormat_RGBA8_sRGB;
	case RUSH_GFX_FORMAT_BGRA8_UNORM      : return GfxFormat_BGRA8_Unorm;
	case RUSH_GFX_FORMAT_BGRA8_SRGB       : return GfxFormat_BGRA8_sRGB;
	case RUSH_GFX_FORMAT_BC1_UNORM        : return GfxFormat_BC1_Unorm;
	case RUSH_GFX_FORMAT_BC1_UNORM_SRGB   : return GfxFormat_BC1_Unorm_sRGB;
	case RUSH_GFX_FORMAT_BC3_UNORM        : return GfxFormat_BC3_Unorm;
	case RUSH_GFX_FORMAT_BC3_UNORM_SRGB   : return GfxFormat_BC3_Unorm_sRGB;
	case RUSH_GFX_FORMAT_BC4_UNORM        : return GfxFormat_BC4_Unorm;
	case RUSH_GFX_FORMAT_BC5_UNORM        : return GfxFormat_BC5_Unorm;
	case RUSH_GFX_FORMAT_BC6H_UFLOAT      : return GfxFormat_BC6H_UFloat;
	case RUSH_GFX_FORMAT_BC6H_SFLOAT      : return GfxFormat_BC6H_SFloat;
	case RUSH_GFX_FORMAT_BC7_UNORM        : return GfxFormat_BC7_Unorm;
	case RUSH_GFX_FORMAT_BC7_UNORM_SRGB   : return GfxFormat_BC7_Unorm_sRGB;
	}
}

rush_gfx_format convert(GfxFormat fmt)
{
	switch(fmt)
	{
	default:
	case GfxFormat_Unknown          : return RUSH_GFX_FORMAT_UNKNOWN;
	case GfxFormat_D24_Unorm_S8_Uint: return RUSH_GFX_FORMAT_D24_UNORM_S8_UINT;
	case GfxFormat_D24_Unorm_X8     : return RUSH_GFX_FORMAT_D24_UNORM_X8;
	case GfxFormat_D32_Float        : return RUSH_GFX_FORMAT_D32_FLOAT;
	case GfxFormat_D32_Float_S8_Uint: return RUSH_GFX_FORMAT_D32_FLOAT_S8_UINT;
	case GfxFormat_R8_Unorm         : return RUSH_GFX_FORMAT_R8_UNORM;
	case GfxFormat_R16_Float        : return RUSH_GFX_FORMAT_R16_FLOAT;
	case GfxFormat_R16_Uint         : return RUSH_GFX_FORMAT_R16_UINT;
	case GfxFormat_R32_Float        : return RUSH_GFX_FORMAT_R32_FLOAT;
	case GfxFormat_R32_Uint         : return RUSH_GFX_FORMAT_R32_UINT;
	case GfxFormat_RG8_Unorm        : return RUSH_GFX_FORMAT_RG8_UNORM;
	case GfxFormat_RG16_Float       : return RUSH_GFX_FORMAT_RG16_FLOAT;
	case GfxFormat_RG32_Float       : return RUSH_GFX_FORMAT_RG32_FLOAT;
	case GfxFormat_RGB32_Float      : return RUSH_GFX_FORMAT_RGB32_FLOAT;
	case GfxFormat_RGB8_Unorm       : return RUSH_GFX_FORMAT_RGB8_UNORM;
	case GfxFormat_RGBA16_Float     : return RUSH_GFX_FORMAT_RGBA16_FLOAT;
	case GfxFormat_RGBA16_Unorm     : return RUSH_GFX_FORMAT_RGBA16_UNORM;
	case GfxFormat_RGBA32_Float     : return RUSH_GFX_FORMAT_RGBA32_FLOAT;
	case GfxFormat_RGBA8_Unorm      : return RUSH_GFX_FORMAT_RGBA8_UNORM;
	case GfxFormat_RGBA8_sRGB       : return RUSH_GFX_FORMAT_RGBA8_SRGB;
	case GfxFormat_BGRA8_Unorm      : return RUSH_GFX_FORMAT_BGRA8_UNORM;
	case GfxFormat_BGRA8_sRGB       : return RUSH_GFX_FORMAT_BGRA8_SRGB;
	case GfxFormat_BC1_Unorm        : return RUSH_GFX_FORMAT_BC1_UNORM;
	case GfxFormat_BC1_Unorm_sRGB   : return RUSH_GFX_FORMAT_BC1_UNORM_SRGB;
	case GfxFormat_BC3_Unorm        : return RUSH_GFX_FORMAT_BC3_UNORM;
	case GfxFormat_BC3_Unorm_sRGB   : return RUSH_GFX_FORMAT_BC3_UNORM_SRGB;
	case GfxFormat_BC4_Unorm        : return RUSH_GFX_FORMAT_BC4_UNORM;
	case GfxFormat_BC5_Unorm        : return RUSH_GFX_FORMAT_BC5_UNORM;
	case GfxFormat_BC6H_UFloat      : return RUSH_GFX_FORMAT_BC6H_UFLOAT;
	case GfxFormat_BC6H_SFloat      : return RUSH_GFX_FORMAT_BC6H_SFLOAT;
	case GfxFormat_BC7_Unorm        : return RUSH_GFX_FORMAT_BC7_UNORM;
	case GfxFormat_BC7_Unorm_sRGB   : return RUSH_GFX_FORMAT_BC7_UNORM_SRGB;
	}
}

ColorRGBA convert(const rush_color_rgba& c)
{
	return ColorRGBA(c.r, c.g, c.b, c.a);
}

GfxShaderSource convert(const rush_gfx_shader_source* code)
{
	return GfxShaderSource((GfxShaderSourceType)code->type, (const char*)code->data, code->size_bytes, code->entry);
}

GfxDescriptorSetDesc convert(const rush_gfx_descriptor_set_desc& desc)
{
	GfxDescriptorSetDesc result;
	result.constantBuffers        = desc.constant_buffers;
	result.samplers               = desc.samplers;
	result.textures               = desc.textures;
	result.rwImages               = desc.rw_images;
	result.rwBuffers              = desc.rw_buffers;
	result.rwTypedBuffers         = desc.rw_typed_buffers;
	result.accelerationStructures = desc.acceleration_structures;
	result.stageFlags             = (GfxStageFlags)desc.stage_flags;
	result.flags                  = (GfxDescriptorSetFlags)desc.flags;
	return result;
}

template <typename T, typename H>
T convertHandle(H h)
{
	return T(UntypedResourceHandle(h.handle));
}

GfxContext* convert(rush_gfx_context* context)
{
	return (GfxContext*)context;
}

rush_gfx_context* convert(GfxContext* context)
{
	return (rush_gfx_context*)context;
}

rush_window* convert(Window* window)
{
	return (rush_window*)window;
}

Window* convert(rush_window* window)
{
	return (Window*)window;
}

} // namespace

void rush_app_config_init(rush_app_config* out_cfg)
{
	AppConfig cfg;
	*out_cfg = convert(&cfg);
}

static rush_platform_context* g_platform_context = nullptr;

struct rush_platform_context* rush_platform_startup(const rush_app_config* in_cfg)
{
	if (g_platform_context != nullptr)
	{
		// rush_platform_startup can only be called once
		return nullptr;
	}

	rush_platform_context* context = new rush_platform_context;

	AppConfig cfg = convert(in_cfg);
	GfxConfig gfx_cfg(cfg);	
	gfx_cfg.preferredCoordinateSystem = GfxConfig::PreferredCoordinateSystem_Direct3D;
	cfg.gfxConfig = &gfx_cfg;

	Platform_Startup(cfg);

	context->window      = (rush_window*)Platform_GetWindow();
	context->gfx_device  = (rush_gfx_device*)Platform_GetGfxDevice();
	context->gfx_context = (rush_gfx_context*)Platform_GetGfxContext();

	g_platform_context = context;

	return context;
}

void rush_platform_run(rush_platform_callback_update on_update, void* user_data) 
{
	Platform_Run(on_update, user_data);
}

void rush_platform_shutdown()
{
	RUSH_ASSERT(g_platform_context != nullptr);
	Platform_Shutdown();
	delete g_platform_context;
	g_platform_context = nullptr;
}

struct rush_window* rush_platform_get_main_window()
{
	return convert(Platform_GetWindow());
}

const rush_window_keyboard_state* rush_window_get_keyboard_state(struct rush_window* in_window)
{
	Window* window = convert(in_window);
	return (const rush_window_keyboard_state*)(&window->getKeyboardState());
}

const rush_window_mouse_state* rush_window_get_mouse_state(struct rush_window* in_window)
{
	Window* window = convert(in_window);
	return (const rush_window_mouse_state*)(&window->getMouseState());
}

rush_vec2 rush_window_get_size(struct rush_window* window)
{
	Vec2 size = convert(window)->getSizeFloat();
	return {size.x, size.y};
}

rush_vec2 rush_window_get_framebuffer_size(struct rush_window* window)
{
	Tuple2i size = convert(window)->getFramebufferSize();
	return {float(size.x), float(size.y)};
}

rush_vec2 rush_window_get_resolution_sclae(struct rush_window* window)
{
	Vec2 scale = convert(window)->getResolutionScale();
	return {scale.x, scale.y};
}

float rush_window_get_aspect(struct rush_window* window)
{
	return convert(window)->getAspect();
}

struct rush_window_event_listener : public WindowEventListener
{
	rush_window_event_listener(rush_window* in_window, rush_window_event_mask in_mask)
	: WindowEventListener(convert(in_window), u32(in_mask))
	{
	}
};

struct rush_window_event_listener* rush_window_create_listener(struct rush_window* in_window, rush_window_event_mask event_mask)
{
	rush_window_event_listener* listener = new rush_window_event_listener(in_window, event_mask);
	return listener;
}

void rush_window_destroy_listener(struct rush_window_event_listener* listener)
{
	delete listener;
}

uint32_t rush_window_event_listener_count(struct rush_window_event_listener* listener)
{
	return uint32_t(listener->size());
}

uint32_t rush_window_event_listener_receive(struct rush_window_event_listener* listener, uint32_t max_count, rush_window_event* out_events)
{
	const u32 receive_count = min<u32>(max_count, u32(listener->size()));
	WindowEvent* events = listener->data();
	for (u32 i=0; i<receive_count; ++i)
	{
		out_events[i].event_type   = rush_window_event_type(events[i].type);
		out_events[i].key          = rush_key(events[i].code);
		out_events[i].character    = events[i].character;
		out_events[i].modifiers    = events[i].modifiers;
		out_events[i].width        = events[i].width;
		out_events[i].height       = events[i].height;
		out_events[i].pos[0]       = events[i].pos.x;
		out_events[i].pos[1]       = events[i].pos.y;
		out_events[i].button       = events[i].button;
		out_events[i].double_click = events[i].doubleClick;
		out_events[i].scroll[0]    = events[i].scroll.x;
		out_events[i].scroll[1]    = events[i].scroll.y;
	}
	std::move(events+receive_count, events+listener->size(), events);
	listener->resize(listener->size() - receive_count);
	return receive_count;
}

void rush_window_event_listener_clear(struct rush_window_event_listener* listener)
{
	listener->clear();
}

rush_gfx_device* rush_platform_get_device()
{
	return (rush_gfx_device*)Platform_GetGfxDevice();
}

rush_gfx_context* rush_platform_get_context()
{
	return (rush_gfx_context*)Platform_GetGfxContext();
}

void rush_gfx_release_vertex_format(rush_gfx_vertex_format h)
{
	Gfx_Release(convertHandle<GfxVertexFormat>(h));
}
void rush_gfx_release_vertex_shader(rush_gfx_vertex_shader h)
{
	Gfx_Release(convertHandle<GfxVertexShader>(h));
}
void rush_gfx_release_pixel_shader(rush_gfx_pixel_shader h)
{
	Gfx_Release(convertHandle<GfxPixelShader>(h));
}
void rush_gfx_release_geometry_shader(rush_gfx_geometry_shader h)
{
	Gfx_Release(convertHandle<GfxGeometryShader>(h));
}
void rush_gfx_release_compute_shader(rush_gfx_compute_shader h)
{
	Gfx_Release(convertHandle<GfxComputeShader>(h));
}
void rush_gfx_release_mesh_shader(rush_gfx_mesh_shader h)
{
	Gfx_Release(convertHandle<GfxMeshShader>(h));
}
void rush_gfx_release_ray_tracing_pipeline(rush_gfx_ray_tracing_pipeline h)
{
	Gfx_Release(convertHandle<GfxRayTracingPipeline>(h));
}
void rush_gfx_release_acceleration_structure(rush_gfx_acceleration_structure h)
{
	Gfx_Release(convertHandle<GfxAccelerationStructure>(h));
}
void rush_gfx_release_technique(rush_gfx_technique h)
{
	Gfx_Release(convertHandle<GfxTechnique>(h));
}
void rush_gfx_release_texture(rush_gfx_texture h)
{
	Gfx_Release(convertHandle<GfxTexture>(h));
}
void rush_gfx_release_blend_state(rush_gfx_blend_state h)
{
	Gfx_Release(convertHandle<GfxBlendState>(h));
}
void rush_gfx_release_sampler(rush_gfx_sampler h)
{
	Gfx_Release(convertHandle<GfxSampler>(h));
}
void rush_gfx_release_depth_stencil_state(rush_gfx_depth_stencil_state h)
{
	Gfx_Release(convertHandle<GfxDepthStencilState>(h));
}
void rush_gfx_release_rasterizer_state(rush_gfx_rasterizer_state h)
{
	Gfx_Release(convertHandle<GfxRasterizerState>(h));
}
void rush_gfx_release_buffer(rush_gfx_buffer h)
{
	Gfx_Release(convertHandle<GfxBuffer>(h));
}
void rush_gfx_release_descriptor_set(rush_gfx_descriptor_set h)
{
	Gfx_Release(convertHandle<GfxDescriptorSet>(h));
}
void rush_gfx_begin_pass(
	struct rush_gfx_context* ctx, 
	uint32_t color_count,
	const rush_gfx_color_target* color,
	const rush_gfx_depth_target* depth,
	rush_gfx_pass_flags flags
)
{
	GfxPassDesc desc;
	color_count = min<u32>(color_count, GfxPassDesc::MaxTargets);
	for (u32 i=0; i<color_count; ++i)
	{
		desc.color      [i] = convertHandle<GfxTexture>(color[i].target);
		desc.clearColors[i] = convert(color[i].clear_color);
	}
	if (depth)
	{
		desc.depth        = convertHandle<GfxTexture>(depth->target);
		desc.clearDepth   = depth->clear_depth;
		desc.clearStencil = depth->clear_stencil;
	}
	desc.flags = (GfxPassFlags)flags;
	Gfx_BeginPass(convert(ctx), desc);
}

void rush_gfx_end_pass(struct rush_gfx_context* ctx)
{
	Gfx_EndPass(convert(ctx));
}

void rush_gfx_set_viewport(struct rush_gfx_context* ctx, const rush_gfx_viewport* in_viewport)
{
	GfxViewport viewport;

	viewport.x        = in_viewport->x;
	viewport.y        = in_viewport->y;
	viewport.w        = in_viewport->w;
	viewport.h        = in_viewport->h;
	viewport.depthMin = in_viewport->depth_min;
	viewport.depthMax = in_viewport->depth_max;

	Gfx_SetViewport(convert(ctx), viewport);
}

void rush_gfx_set_scissor_rect(struct rush_gfx_context* ctx, const rush_gfx_rect* in_rect)
{
	GfxRect rect;

	rect.left   = in_rect->left;
	rect.top    = in_rect->top;
	rect.right  = in_rect->right;
	rect.bottom = in_rect->bottom;

	Gfx_SetScissorRect(convert(ctx), rect);
}

void rush_gfx_draw(struct rush_gfx_context* ctx, uint32_t first_vertex, uint32_t vertex_count)
{
	Gfx_Draw(convert(ctx), first_vertex, vertex_count);
}

void rush_gfx_draw_indexed(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count)
{
	Gfx_DrawIndexed(convert(ctx), index_count, first_index, base_vertex, vertex_count);
}

void rush_gfx_draw_indexed2(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count, const void* push_constants, uint32_t push_constants_size)
{
	Gfx_DrawIndexed(convert(ctx), index_count, first_index, base_vertex, vertex_count, push_constants, push_constants_size);
}

void rush_gfx_draw_indexed_instanced(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count, uint32_t instance_count, uint32_t instance_offset)
{
	Gfx_DrawIndexedInstanced(convert(ctx), index_count, first_index, base_vertex, vertex_count, instance_count, instance_offset);
}

void rush_gfx_draw_indexed_indirect(struct rush_gfx_context* ctx, rush_gfx_buffer args_buffer, uint32_t args_buffer_offset, uint32_t draw_count)
{
	Gfx_DrawIndexedIndirect(convert(ctx), convertHandle<GfxBuffer>(args_buffer), args_buffer_offset, draw_count);
}

void rush_gfx_dispatch_indirect(struct rush_gfx_context* ctx, rush_gfx_buffer args_buffer, uint32_t args_buffer_offset, const void* push_constants, uint32_t push_constants_size)
{
	Gfx_DispatchIndirect(convert(ctx), convertHandle<GfxBuffer>(args_buffer), args_buffer_offset);
}

void rush_gfx_push_marker(struct rush_gfx_context* ctx, const char* marker)
{
	Gfx_PushMarker(convert(ctx), marker);
}

void rush_gfx_pop_marker(struct rush_gfx_context* ctx)
{
	Gfx_PopMarker(convert(ctx));
}

void rush_gfx_begin_timer(struct rush_gfx_context* ctx, uint32_t timestamp_id)
{
	Gfx_BeginTimer(convert(ctx), timestamp_id);
}

void rush_gfx_end_timer(struct rush_gfx_context* ctx, uint32_t timestamp_id)
{
	Gfx_EndTimer(convert(ctx), timestamp_id);
}

rush_gfx_buffer rush_gfx_create_buffer(const rush_gfx_buffer_desc* in_desc, const void* data)
{
	GfxBufferDesc desc;
	desc.format      = convert(in_desc->format);
	desc.flags       = (GfxBufferFlags)in_desc->flags;
	desc.stride      = in_desc->stride;
	desc.count       = in_desc->count;
	desc.hostVisible = in_desc->host_visible;
	return {Gfx_CreateBuffer(desc, data).detach().index()};
}

rush_gfx_texture rush_gfx_create_texture(const rush_gfx_texture_desc* in_desc, const rush_gfx_texture_data* in_data, uint32_t count, const void* pixels)
{
	GfxTextureDesc desc;
	desc.width     = in_desc->width;
	desc.height    = in_desc->height;
	desc.depth     = in_desc->depth;
	desc.mips      = in_desc->mips;
	desc.samples   = in_desc->samples;
	desc.format    = convert(in_desc->format);
	desc.type      = TextureType(in_desc->texture_type);
	desc.usage     = GfxUsageFlags(in_desc->usage);
	desc.debugName = nullptr;
	
	GfxTextureData* data = (GfxTextureData*)alloca(sizeof(GfxTextureData) * count);
	for (uint32_t i=0; i<count; ++i)
	{
		if (pixels)
		{
			data[i].offset = in_data[i].offset;
		}
		else
		{
			data[i].pixels = in_data[i].pixels;
		}
		data[i].mip    = in_data[i].mip;
		data[i].slice  = in_data[i].slice;
		data[i].width  = in_data[i].width;
		data[i].height = in_data[i].height;
		data[i].depth  = in_data[i].depth;
	}

	return {Gfx_CreateTexture(desc, data, count, pixels).detach().index()};
}

rush_gfx_blend_state rush_gfx_create_blend_state(const rush_gfx_blend_state_desc* in_desc)
{
	GfxBlendStateDesc desc;

	desc.src           = GfxBlendParam(in_desc->src);
	desc.dst           = GfxBlendParam(in_desc->dst);
	desc.op            = GfxBlendOp(in_desc->op);
	desc.alphaSrc      = GfxBlendParam(in_desc->alpha_src);
	desc.alphaDst      = GfxBlendParam(in_desc->alpha_dst);
	desc.alphaOp       = GfxBlendOp(in_desc->alpha_op);
	desc.alphaSeparate = in_desc->alpha_separate;
	desc.enable        = in_desc->enable;

	return { Gfx_CreateBlendState(desc).detach().index() };
}

rush_gfx_sampler rush_gfx_create_sampler_state(const rush_gfx_sampler_desc* in_desc)
{
	GfxSamplerDesc desc;

	desc.filterMin     = GfxTextureFilter(in_desc->filter_min);
	desc.filterMag     = GfxTextureFilter(in_desc->filter_mag);
	desc.filterMip     = GfxTextureFilter(in_desc->filter_mip);
	desc.wrapU         = GfxTextureWrap(in_desc->wrap_u);
	desc.wrapV         = GfxTextureWrap(in_desc->wrap_v);
	desc.wrapW         = GfxTextureWrap(in_desc->wrap_w);
	desc.compareFunc   = GfxCompareFunc(in_desc->compare_func);
	desc.compareEnable = in_desc->compare_enable;
	desc.anisotropy    = in_desc->anisotropy;
	desc.mipLodBias    = in_desc->mip_lod_bias;

	return { Gfx_CreateSamplerState(desc).detach().index() };
}

rush_gfx_depth_stencil_state rush_gfx_create_depth_stencil_state(const rush_gfx_depth_stencil_desc* in_desc)
{
	GfxDepthStencilDesc desc;

	desc.compareFunc = GfxCompareFunc(in_desc->compare_func);
	desc.enable      = in_desc->enable;
	desc.writeEnable = in_desc->write_enable;

	return { Gfx_CreateDepthStencilState(desc).detach().index() };
}

rush_gfx_rasterizer_state rush_gfx_create_rasterizer_state(const rush_gfx_rasterizer_desc* in_desc)
{
	GfxRasterizerDesc desc;

	desc.fillMode            = GfxFillMode(in_desc->fill_mode);
	desc.cullMode            = GfxCullMode(in_desc->cull_mode);
	desc.depthBias           = in_desc->depth_bias;
	desc.depthBiasSlopeScale = in_desc->depth_bias_slope_scale;

	return { Gfx_CreateRasterizerState(desc).detach().index() };
}

namespace Rush { extern const char* MSL_EmbeddedShaders; }

rush_gfx_shader_source rush_gfx_get_embedded_shader(rush_gfx_embedded_shader_type type)
{
	switch(type)
	{
		default:
			return {RUSH_GFX_SHADER_SOURCE_UNKNOWN, "main", nullptr, 0u};
#if RUSH_RENDER_API==RUSH_RENDER_API_MTL
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_PLAIN_PS:
			return {RUSH_GFX_SHADER_SOURCE_MSL, "psMain", MSL_EmbeddedShaders, 0u};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_TEXTURED_PS:
			return {RUSH_GFX_SHADER_SOURCE_MSL, "psMainTextured", MSL_EmbeddedShaders, 0u};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_2D_VS:
			return {RUSH_GFX_SHADER_SOURCE_MSL, "vsMain2D", MSL_EmbeddedShaders, 0u};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_3D_VS:
			return {RUSH_GFX_SHADER_SOURCE_MSL, "vsMain3D", MSL_EmbeddedShaders, 0u};
#else // RUSH_RENDER_API==RUSH_RENDER_API_MTL
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_PLAIN_PS:
			return {RUSH_GFX_SHADER_SOURCE_SPV, "psMain", SPV_psMain_data, (uint32_t)SPV_psMain_size};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_TEXTURED_PS:
			return {RUSH_GFX_SHADER_SOURCE_SPV, "psMainTextured", SPV_psMainTextured_data, (uint32_t)SPV_psMainTextured_size};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_2D_VS:
			return {RUSH_GFX_SHADER_SOURCE_SPV, "vsMain2D", SPV_vsMain2D_data, (uint32_t)SPV_vsMain2D_size};
		case RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_3D_VS:
			return {RUSH_GFX_SHADER_SOURCE_SPV, "vsMain3D", SPV_vsMain3D_data, (uint32_t)SPV_vsMain3D_size};
#endif // RUSH_RENDER_API==RUSH_RENDER_API_MTL
	}
}

void rush_gfx_set_present_interval(uint32_t interval)
{
	Gfx_SetPresentInterval(interval);
}

void rush_gfx_finish()
{
	Gfx_Finish();
}

rush_gfx_capability rush_gfx_get_capability()
{
	const GfxCapability& caps = Gfx_GetCapability();
	rush_gfx_capability result = {};

	result.api_name = caps.apiName;

	return result;
}

rush_gfx_stats rush_gfx_get_stats()
{
	const GfxStats& stats = Gfx_Stats();
	rush_gfx_stats result = {};

	result.draw_calls = stats.drawCalls;

	return result;
}

void rush_gfx_reset_stats()
{
	Gfx_ResetStats();
}

rush_gfx_vertex_shader rush_gfx_create_vertex_shader(const rush_gfx_shader_source* in_code)
{
	return {Gfx_CreateVertexShader(convert(in_code)).detach().index()};
}

rush_gfx_pixel_shader rush_gfx_create_pixel_shader(const rush_gfx_shader_source* in_code)
{
	return {Gfx_CreatePixelShader(convert(in_code)).detach().index()};
}

rush_gfx_geometry_shader rush_gfx_create_geometry_shader(const rush_gfx_shader_source* in_code)
{
	return {Gfx_CreateGeometryShader(convert(in_code)).detach().index()};
}

rush_gfx_compute_shader rush_gfx_create_compute_shader(const rush_gfx_shader_source* in_code)
{
	return {Gfx_CreateComputeShader(convert(in_code)).detach().index()};
}

rush_gfx_vertex_format rush_gfx_create_vertex_format(const rush_gfx_vertex_element* elements, uint32_t count)
{
	GfxVertexFormatDesc desc;
	for (u32 i=0; i<count; ++i)
	{
		const rush_gfx_vertex_element& elem = elements[i];
		// TODO: just use GfxFormat everywhere instead of GfxVertexFormatDesc::DataType
		GfxVertexFormatDesc::DataType type = GfxVertexFormatDesc::DataType::Unused;
		switch(convert(elem.format))
		{
			default:
				RUSH_LOG_ERROR("Unsupported vertex format");
				type = GfxVertexFormatDesc::DataType::Unused;
			case GfxFormat_R32_Float:    type = GfxVertexFormatDesc::DataType::Float1; break;
			case GfxFormat_RG32_Float:   type = GfxVertexFormatDesc::DataType::Float2; break;
			case GfxFormat_RGB32_Float:  type = GfxVertexFormatDesc::DataType::Float3; break;
			case GfxFormat_RGBA32_Float: type = GfxVertexFormatDesc::DataType::Float4; break;
			case GfxFormat_RGBA8_Unorm:  type = GfxVertexFormatDesc::DataType::Color; break;
			case GfxFormat_R32_Uint:     type = GfxVertexFormatDesc::DataType::UInt; break;
		}
		desc.add(elem.stream, type, (GfxVertexFormatDesc::Semantic)elem.semantic, elem.index);
	}
	return {Gfx_CreateVertexFormat(desc).detach().index()};
}

rush_gfx_technique rush_gfx_create_technique(const rush_gfx_technique_desc* in_desc)
{
	GfxShaderBindingDesc bindings;
	bindings.useDefaultDescriptorSet = in_desc->bindings.use_default_descriptor_set;

	for (u32 i=0; i<in_desc->bindings.descriptor_set_count && i <GfxShaderBindingDesc::MaxDescriptorSets; ++i)
	{
		if (i==0 && bindings.useDefaultDescriptorSet)
		{
			(GfxDescriptorSetDesc&)bindings = convert(in_desc->bindings.descriptor_sets[0]);
		}
		else
		{
			bindings.descriptorSets[i] = convert(in_desc->bindings.descriptor_sets[i]);            
		}
		
	}

	GfxTechniqueDesc desc;
	desc.cs = convertHandle<GfxComputeShader>(in_desc->cs);
	desc.ps = convertHandle<GfxPixelShader>(in_desc->ps);
	desc.gs = convertHandle<GfxGeometryShader>(in_desc->gs);
	desc.vs = convertHandle<GfxVertexShader>(in_desc->vs);
	desc.ms = convertHandle<GfxMeshShader>(in_desc->ms);
	desc.vf = convertHandle<GfxVertexFormat>(in_desc->vf);
	desc.bindings = bindings;
	desc.workGroupSize.x = in_desc->work_group_size[0];
	desc.workGroupSize.y = in_desc->work_group_size[1];
	desc.workGroupSize.z = in_desc->work_group_size[2];
	desc.specializationConstantCount = in_desc->spec_constant_count;
	desc.specializationConstants     = (const GfxSpecializationConstant*)in_desc->spec_constants;
	desc.specializationData          = in_desc->spec_data;
	desc.specializationDataSize      = in_desc->spec_data_size;

	return {Gfx_CreateTechnique(desc).detach().index()};
}

void rush_gfx_set_technique(struct rush_gfx_context* ctx, rush_gfx_technique h)
{
	Gfx_SetTechnique(convert(ctx), convertHandle<GfxTechnique>(h));
}

void rush_gfx_set_primitive(struct rush_gfx_context* ctx, enum rush_gfx_primitive_type type)
{
	Gfx_SetPrimitive(convert(ctx), (GfxPrimitive)type);
}

void rush_gfx_set_index_stream(struct rush_gfx_context* ctx, rush_gfx_buffer h)
{
	Gfx_SetIndexStream(convert(ctx), convertHandle<GfxBuffer>(h));
}

void rush_gfx_set_vertex_stream(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h)
{
	Gfx_SetVertexStream(convert(ctx), idx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_set_texture(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_texture h)
{
	Gfx_SetTexture(convert(ctx), idx, convertHandle<GfxTexture>(h));
}

void rush_gfx_set_sampler(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_sampler h)
{
	Gfx_SetSampler(convert(ctx), idx, convertHandle<GfxSampler>(h));
}

void rush_gfx_set_storage_image(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_texture h)
{
	Gfx_SetStorageImage(convert(ctx), idx, convertHandle<GfxTexture>(h));
}

void rush_gfx_set_storage_buffer(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h)
{
	Gfx_SetStorageBuffer(convert(ctx), idx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_set_blend_state(struct rush_gfx_context* ctx, rush_gfx_blend_state h)
{
	Gfx_SetBlendState(convert(ctx), convertHandle<GfxBlendState>(h));
}

void rush_gfx_set_depth_stencil_state(struct rush_gfx_context* ctx, rush_gfx_depth_stencil_state h)
{
	Gfx_SetDepthStencilState(convert(ctx), convertHandle<GfxDepthStencilState>(h));
}

void rush_gfx_set_rasterizer_state(struct rush_gfx_context* ctx, rush_gfx_rasterizer_state h)
{
	Gfx_SetRasterizerState(convert(ctx), convertHandle<GfxRasterizerState>(h));
}

void rush_gfx_set_constant_buffer(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h, uint32_t offset)
{
	Gfx_SetConstantBuffer(convert(ctx), idx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_add_image_barrier(struct rush_gfx_context* ctx, rush_gfx_texture texture, rush_gfx_resource_state desired_state)
{
	Gfx_AddImageBarrier(convert(ctx), convertHandle<GfxTexture>(texture), GfxResourceState(desired_state), nullptr);
}

void rush_gfx_add_image_subresource_barrier(struct rush_gfx_context* ctx, rush_gfx_texture texture, rush_gfx_resource_state desired_state, rush_gfx_subresource_range subresource_range)
{
	GfxSubresourceRange range;
	range.aspectMask     = GfxImageAspectFlags(subresource_range.aspect_mask);
	range.baseMipLevel   = subresource_range.base_mip_level;
	range.levelCount     = subresource_range.layer_count;
	range.baseArrayLayer = subresource_range.base_array_layer;
	range.layerCount     = subresource_range.layer_count;
	Gfx_AddImageBarrier(convert(ctx), convertHandle<GfxTexture>(texture), GfxResourceState(desired_state), &range);
}

void rush_gfx_resolve_image(struct rush_gfx_context* ctx, rush_gfx_texture src, rush_gfx_texture dst)
{
	Gfx_ResolveImage(convert(ctx), convertHandle<GfxTexture>(src), convertHandle<GfxTexture>(dst));
}

void rush_gfx_dispatch(struct rush_gfx_context* ctx, uint32_t size_x, uint32_t size_y, uint32_t size_z)
{
	Gfx_Dispatch(convert(ctx), size_x, size_y, size_z);
}

void rush_gfx_dispatch2(struct rush_gfx_context* ctx, uint32_t size_x, uint32_t size_y, uint32_t size_z, const void* push_constants, uint32_t push_constants_size)
{
	Gfx_Dispatch(convert(ctx), size_x, size_y, size_z, push_constants, push_constants_size);
}

rush_gfx_texture_desc gfx_get_texture_desc(rush_gfx_texture h)
{
	rush_gfx_texture_desc result = {};

	const GfxTextureDesc& desc = Gfx_GetTextureDesc(convertHandle<GfxTexture>(h));

	result.width        = desc.width;
	result.height       = desc.height;
	result.depth        = desc.depth;
	result.mips         = desc.mips;
	result.samples      = desc.samples;
	result.format       = convert(desc.format);
	result.texture_type = (rush_gfx_texture_type)desc.type;
	result.usage        = (rush_gfx_usage_flags)desc.usage;
	result.debug_name   = desc.debugName;

	return result;
}

rush_gfx_texture gfx_get_back_buffer_color_texture()
{
	return { Gfx_GetBackBufferColorTexture().index() };
}

rush_gfx_texture gfx_get_back_buffer_depth_texture()
{
	return { Gfx_GetBackBufferDepthTexture().index() };
}

rush_gfx_mapped_buffer gfx_map_buffer(rush_gfx_buffer h, uint32_t offset, uint32_t size)
{
	rush_gfx_mapped_buffer result;
	GfxMappedBuffer mapped_buffer = Gfx_MapBuffer(convertHandle<GfxBuffer>(h), offset, size);

	result.data   = mapped_buffer.data;
	result.size   = mapped_buffer.size;
	result.handle = rush_gfx_buffer{mapped_buffer.handle.index()};

	return result;
}

void rush_gfx_unmap_buffer(const rush_gfx_mapped_buffer* in_mapped_buffer)
{
	GfxMappedBuffer mapped_buffer;

	mapped_buffer.data   = in_mapped_buffer->data;
	mapped_buffer.size   = in_mapped_buffer->size;
	mapped_buffer.handle = convertHandle<GfxBuffer>(in_mapped_buffer->handle);

	Gfx_UnmapBuffer(mapped_buffer);
}

void rush_gfx_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, const void* data, uint32_t size)
{
	Gfx_UpdateBuffer(convert(ctx), convertHandle<GfxBuffer>(h), data, size);
}

void* rush_gfx_begin_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, uint32_t size)
{
	return Gfx_BeginUpdateBuffer(convert(ctx), convertHandle<GfxBuffer>(h), size);
}

void rush_gfx_end_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h)
{
	Gfx_EndUpdateBuffer(convert(ctx), convertHandle<GfxBuffer>(h));
}

void rush_embedded_font_blit_6x8(uint32_t* output, uint32_t output_offset_pixels, uint32_t width, uint32_t color, const char* text)
{
	EmbeddedFont_Blit6x8(output+output_offset_pixels, width, color, text);
}
