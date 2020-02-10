#ifndef INCLUDED_RUSHC_H
#define INCLUDED_RUSHC_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdbool.h>
#include <stdint.h>

// Platform

typedef void (*rush_platform_callback_startup)  (void* user_data);
typedef void (*rush_platform_callback_update)   (void* user_data);
typedef void (*rush_platform_callback_shutdown) (void* user_data);

typedef struct
{
	const char* name;

	int vsync;

	int width;
	int height;

	int max_width;
	int max_height;

	bool fullscreen;
	bool resizable;
	bool debug;
	bool warp;
	bool minimize_latency;

	int    argc;
	char** argv;

	void* user_data;

	rush_platform_callback_startup  on_startup;
	rush_platform_callback_update   on_update;
	rush_platform_callback_shutdown on_shutdown;
} rush_app_config;

void rush_app_config_init(rush_app_config* out_cfg);

int rush_platform_main(const rush_app_config* cfg);

// Window

struct rush_window* rush_platform_get_window();

// Graphics

typedef struct { float x, y; }       rush_vec2;
typedef struct { float x, y, z; }    rush_vec3;
typedef struct { float x, y, z, w; } rush_vec4;
typedef struct { float r, g, b, a; } rush_color_rgba;

struct rush_gfx_device*  rush_platform_get_device();
struct rush_gfx_context* rush_platform_get_context();

typedef struct { uint16_t handle; } rush_gfx_vertex_format;
typedef struct { uint16_t handle; } rush_gfx_vertex_shader;
typedef struct { uint16_t handle; } rush_gfx_pixel_shader;
typedef struct { uint16_t handle; } rush_gfx_geometry_shader;
typedef struct { uint16_t handle; } rush_gfx_compute_shader;
typedef struct { uint16_t handle; } rush_gfx_mesh_shader;
typedef struct { uint16_t handle; } rush_gfx_ray_tracing_pipeline;
typedef struct { uint16_t handle; } rush_gfx_acceleration_structure;
typedef struct { uint16_t handle; } rush_gfx_texture;
typedef struct { uint16_t handle; } rush_gfx_buffer;
typedef struct { uint16_t handle; } rush_gfx_sampler;
typedef struct { uint16_t handle; } rush_gfx_blend_state;
typedef struct { uint16_t handle; } rush_gfx_depth_stencil_state;
typedef struct { uint16_t handle; } rush_gfx_rasterizer_state;
typedef struct { uint16_t handle; } rush_gfx_technique;
typedef struct { uint16_t handle; } rush_gfx_descriptor_set;

enum rush_gfx_pass_flags
{
	RUSH_GFX_PASS_NONE = 0,
	RUSH_GFX_PASS_CLEAR_COLOR = 1 << 0,
	RUSH_GFX_PASS_CLEAR_DEPTH_STENCIL = 1 << 1,
	RUSH_GFX_PASS_DISCARD_COLOR = 1 << 2,
};

enum rush_gfx_shader_source_type
{
	RUSH_GFX_SHADER_SOURCE_UNKNOWN = 0,
	RUSH_GFX_SHADER_SOURCE_SPV     = 1, // binary
	RUSH_GFX_SHADER_SOURCE_GLSL    = 2, // text
	RUSH_GFX_SHADER_SOURCE_HLSL    = 3, // text
	RUSH_GFX_SHADER_SOURCE_DXBC    = 4, // binary
	RUSH_GFX_SHADER_SOURCE_DXIL    = 5, // binary
	RUSH_GFX_SHADER_SOURCE_MSL     = 6, // text
};

enum rush_gfx_format
{
	RUSH_GFX_FORMAT_UNKNOWN,

	RUSH_GFX_FORMAT_D24_UNORM_S8_UINT,
	RUSH_GFX_FORMAT_D24_UNORM_X8,
	RUSH_GFX_FORMAT_D32_FLOAT,
	RUSH_GFX_FORMAT_D32_FLOAT_S8_UINT,
	RUSH_GFX_FORMAT_R8_UNORM,
	RUSH_GFX_FORMAT_R16_FLOAT,
	RUSH_GFX_FORMAT_R16_UINT,
	RUSH_GFX_FORMAT_R32_FLOAT,
	RUSH_GFX_FORMAT_R32_UINT,
	RUSH_GFX_FORMAT_RG8_UNORM,
	RUSH_GFX_FORMAT_RG16_FLOAT,
	RUSH_GFX_FORMAT_RG32_FLOAT,
	RUSH_GFX_FORMAT_RGB32_FLOAT,
	RUSH_GFX_FORMAT_RGB8_UNORM,
	RUSH_GFX_FORMAT_RGBA16_FLOAT,
	RUSH_GFX_FORMAT_RGBA16_UNORM,
	RUSH_GFX_FORMAT_RGBA32_FLOAT,
	RUSH_GFX_FORMAT_RGBA8_UNORM,
	RUSH_GFX_FORMAT_RGBA8_SRGB,
	RUSH_GFX_FORMAT_BGRA8_UNORM,
	RUSH_GFX_FORMAT_BGRA8_SRGB,

	RUSH_GFX_FORMAT_BC1_UNORM,
	RUSH_GFX_FORMAT_BC1_UNORM_SRGB,
	RUSH_GFX_FORMAT_BC3_UNORM,
	RUSH_GFX_FORMAT_BC3_UNORM_SRGB,
	RUSH_GFX_FORMAT_BC4_UNORM,
	RUSH_GFX_FORMAT_BC5_UNORM,
	RUSH_GFX_FORMAT_BC6H_UFLOAT,
	RUSH_GFX_FORMAT_BC6H_SFLOAT,
	RUSH_GFX_FORMAT_BC7_UNORM,
	RUSH_GFX_FORMAT_BC7_UNORM_SRGB,
};

typedef struct
{
	rush_gfx_texture target;
	rush_color_rgba  clear_color;
} rush_gfx_color_target;

typedef struct
{
	rush_gfx_texture target;
	float    clear_depth;
	uint8_t  clear_stencil;
} rush_gfx_depth_target;

void rush_gfx_begin_pass(
	struct rush_gfx_context* ctx, 
	uint32_t color_count,
	const rush_gfx_color_target* color,
	const rush_gfx_depth_target* depth,
	enum rush_gfx_pass_flags flags
);
void rush_gfx_end_pass(struct rush_gfx_context* ctx);

void rush_gfx_draw(struct rush_gfx_context* ctx, uint32_t first_vertex, uint32_t vertex_count);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // INCLUDED_RUSHC_H
