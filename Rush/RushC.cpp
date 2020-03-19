#include "RushC.h"
#include "Platform.h"
#include "GfxDevice.h"
#include "GfxEmbeddedShaders.h"
#include "GfxBitmapFont.h"

#include <stdio.h>
#include <string.h>

using namespace Rush;

namespace {

rush_app_config convert(const AppConfig* cfg)
{
    rush_app_config result;
    result.name = cfg->name;
    result.vsync = cfg->vsync;
    result.width = cfg->width;
    result.height = cfg->height;
    result.max_width = cfg->maxWidth;
    result.max_height = cfg->maxHeight;
    result.fullscreen = cfg->fullScreen;
    result.resizable = cfg->resizable;
    result.debug = cfg->debug;
    result.warp = cfg->warp;
    result.minimize_latency = cfg->minimizeLatency;
    result.argc = cfg->argc;
    result.argv = cfg->argv;
    result.user_data = cfg->userData;
    result.on_startup = cfg->onStartup;
    result.on_update = cfg->onUpdate;
    result.on_shutdown = cfg->onShutdown;
    return result;
}

AppConfig convert(const rush_app_config* cfg)
{
    AppConfig result;
    result.name = cfg->name;
    result.vsync = cfg->vsync;
    result.width = cfg->width;
    result.height = cfg->height;
    result.maxWidth = cfg->max_width;
    result.maxHeight = cfg->max_height;
    result.fullScreen = cfg->fullscreen;
    result.resizable = cfg->resizable;
    result.debug = cfg->debug;
    result.warp = cfg->warp;
    result.minimizeLatency = cfg->minimize_latency;
    result.argc = cfg->argc;
    result.argv = cfg->argv;
    result.userData = cfg->user_data;
    result.onStartup = cfg->on_startup;
    result.onUpdate = cfg->on_update;
    result.onShutdown = cfg->on_shutdown;
    return result;
}

GfxFormat convert(rush_gfx_format fmt)
{
    switch(fmt)
    {
    case RUSH_GFX_FORMAT_UNKNOWN: return GfxFormat_Unknown;
    case RUSH_GFX_FORMAT_D24_UNORM_S8_UINT: return GfxFormat_D24_Unorm_S8_Uint;
    case RUSH_GFX_FORMAT_D24_UNORM_X8: return GfxFormat_D24_Unorm_X8;
    case RUSH_GFX_FORMAT_D32_FLOAT: return GfxFormat_D32_Float;
    case RUSH_GFX_FORMAT_D32_FLOAT_S8_UINT: return GfxFormat_D32_Float_S8_Uint;
    case RUSH_GFX_FORMAT_R8_UNORM: return GfxFormat_R8_Unorm;
    case RUSH_GFX_FORMAT_R16_FLOAT: return GfxFormat_R16_Float;
    case RUSH_GFX_FORMAT_R16_UINT: return GfxFormat_R16_Uint;
    case RUSH_GFX_FORMAT_R32_FLOAT: return GfxFormat_R32_Float;
    case RUSH_GFX_FORMAT_R32_UINT: return GfxFormat_R32_Uint;
    case RUSH_GFX_FORMAT_RG8_UNORM: return GfxFormat_RGB8_Unorm;
    case RUSH_GFX_FORMAT_RG16_FLOAT: return GfxFormat_RG16_Float;
    case RUSH_GFX_FORMAT_RG32_FLOAT: return GfxFormat_RG32_Float;
    case RUSH_GFX_FORMAT_RGB32_FLOAT: return GfxFormat_RGB32_Float;
    case RUSH_GFX_FORMAT_RGB8_UNORM: return GfxFormat_RGB8_Unorm;
    case RUSH_GFX_FORMAT_RGBA16_FLOAT: return GfxFormat_RGBA16_Float;
    case RUSH_GFX_FORMAT_RGBA16_UNORM: return GfxFormat_RGBA16_Unorm;
    case RUSH_GFX_FORMAT_RGBA32_FLOAT: return GfxFormat_RGBA32_Float;
    case RUSH_GFX_FORMAT_RGBA8_UNORM: return GfxFormat_RGBA8_Unorm;
    case RUSH_GFX_FORMAT_RGBA8_SRGB: return GfxFormat_RGBA8_sRGB;
    case RUSH_GFX_FORMAT_BGRA8_UNORM: return GfxFormat_BGRA8_Unorm;
    case RUSH_GFX_FORMAT_BGRA8_SRGB: return GfxFormat_BGRA8_sRGB;
    case RUSH_GFX_FORMAT_BC1_UNORM: return GfxFormat_BC1_Unorm;
    case RUSH_GFX_FORMAT_BC1_UNORM_SRGB: return GfxFormat_BC1_Unorm_sRGB;
    case RUSH_GFX_FORMAT_BC3_UNORM: return GfxFormat_BC3_Unorm;
    case RUSH_GFX_FORMAT_BC3_UNORM_SRGB: return GfxFormat_BC3_Unorm_sRGB;
    case RUSH_GFX_FORMAT_BC4_UNORM: return GfxFormat_BC4_Unorm;
    case RUSH_GFX_FORMAT_BC5_UNORM: return GfxFormat_BC5_Unorm;
    case RUSH_GFX_FORMAT_BC6H_UFLOAT: return GfxFormat_BC6H_UFloat;
    case RUSH_GFX_FORMAT_BC6H_SFLOAT: return GfxFormat_BC6H_SFloat;
    case RUSH_GFX_FORMAT_BC7_UNORM: return GfxFormat_BC7_Unorm;
    case RUSH_GFX_FORMAT_BC7_UNORM_SRGB: return GfxFormat_BC7_Unorm_sRGB;
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

} // namespace

void rush_app_config_init(rush_app_config* out_cfg)
{
    AppConfig cfg;
    *out_cfg = convert(&cfg);
}

int rush_platform_main(const rush_app_config* in_cfg)
{
    AppConfig cfg = convert(in_cfg);
    GfxConfig gfx_cfg(cfg);
    gfx_cfg.preferredCoordinateSystem = GfxConfig::PreferredCoordinateSystem_Direct3D;
    cfg.gfxConfig = &gfx_cfg;
    return Platform_Main(cfg);
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
        desc.color[i] = convertHandle<GfxTexture>(color[i].target);
        desc.clearColors[i] = convert(color[i].clear_color);
    }
    if (depth)
    {
        desc.depth = convertHandle<GfxTexture>(depth->target);
        desc.clearDepth = depth->clear_depth;
        desc.clearStencil = depth->clear_stencil;
    }
    desc.flags = (GfxPassFlags)flags;
    Gfx_BeginPass((GfxContext*)ctx, desc);
}

void rush_gfx_end_pass(struct rush_gfx_context* ctx)
{
    Gfx_EndPass((GfxContext*)ctx);
}

void rush_gfx_draw(struct rush_gfx_context* ctx, uint32_t first_vertex, uint32_t vertex_count)
{
    Gfx_Draw((GfxContext*)ctx, first_vertex, vertex_count);
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

rush_gfx_vertex_shader rush_gfx_create_vertex_shader(const rush_gfx_shader_source* in_code)
{
    return {Gfx_CreateVertexShader(convert(in_code)).detach().index()};
}

rush_gfx_pixel_shader rush_gfx_create_pixel_shader(const rush_gfx_shader_source* in_code)
{
    return {Gfx_CreatePixelShader(convert(in_code)).detach().index()};
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
    Gfx_SetTechnique((GfxContext*)ctx, convertHandle<GfxTechnique>(h));
}

void rush_gfx_set_primitive(struct rush_gfx_context* ctx, enum rush_gfx_primitive_type type)
{
    Gfx_SetPrimitive((GfxContext*)ctx, (GfxPrimitive)type);
}

void rush_gfx_set_index_stream(struct rush_gfx_context* ctx, rush_gfx_buffer h)
{
    Gfx_SetIndexStream((GfxContext*)ctx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_set_vertex_stream(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h)
{
    Gfx_SetVertexStream((GfxContext*)ctx, idx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_set_constant_buffer(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h, uint32_t offset)
{
    Gfx_SetConstantBuffer((GfxContext*)ctx, idx, convertHandle<GfxBuffer>(h));
}

void rush_gfx_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, const void* data, uint32_t size)
{
    void* write_ptr = Gfx_BeginUpdateBuffer((GfxContext*) ctx, convertHandle<GfxBuffer>(h), size);
    memcpy(write_ptr, data, size);
    Gfx_EndUpdateBuffer((GfxContext*) ctx, convertHandle<GfxBuffer>(h));
}

void* rush_gfx_begin_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, uint32_t size)
{
    return Gfx_BeginUpdateBuffer((GfxContext*) ctx, convertHandle<GfxBuffer>(h), size);
}

void rush_gfx_end_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h)
{
    Gfx_EndUpdateBuffer((GfxContext*) ctx, convertHandle<GfxBuffer>(h));
}

void rush_embedded_font_blit_6x8(uint32_t* output, uint32_t output_offset_pixels, uint32_t width, uint32_t color, const char* text)
{
    EmbeddedFont_Blit6x8(output+output_offset_pixels, width, color, text);
}
