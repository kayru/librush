#ifndef INCLUDED_RUSHC_H
#define INCLUDED_RUSHC_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdbool.h>
#include <stdint.h>

// Common

typedef struct rush_vec2 { float x, y; }       rush_vec2;
typedef struct rush_vec3 { float x, y, z; }    rush_vec3;
typedef struct rush_vec4 { float x, y, z, w; } rush_vec4;
typedef struct rush_color_rgba { float r, g, b, a; } rush_color_rgba;

typedef struct rush_memory_view {
	const void* ptr;
	uint64_t size;
} rush_memory_view;

// Platform

typedef void (*rush_platform_callback_startup)  (void* user_data);
typedef void (*rush_platform_callback_update)   (void* user_data);
typedef void (*rush_platform_callback_shutdown) (void* user_data);

typedef struct rush_app_config {
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

typedef enum rush_key
{
	RUSH_KEY_UNKNOWN       = 0,
	RUSH_KEY_SPACE         = ' ',
	RUSH_KEY_APOSTROPHE    = '\'',
	RUSH_KEY_COMMA         = ',',
	RUSH_KEY_MINUS         = '-',
	RUSH_KEY_PERIOD        = '.',
	RUSH_KEY_SLASH         = '/',
	RUSH_KEY_0             = '0',
	RUSH_KEY_1             = '1',
	RUSH_KEY_2             = '2',
	RUSH_KEY_3             = '3',
	RUSH_KEY_4             = '4',
	RUSH_KEY_5             = '5',
	RUSH_KEY_6             = '6',
	RUSH_KEY_7             = '7',
	RUSH_KEY_8             = '8',
	RUSH_KEY_9             = '9',
	RUSH_KEY_SEMICOLON     = ';',
	RUSH_KEY_EQUAL         = '=',
	RUSH_KEY_A             = 'A',
	RUSH_KEY_B             = 'B',
	RUSH_KEY_C             = 'C',
	RUSH_KEY_D             = 'D',
	RUSH_KEY_E             = 'E',
	RUSH_KEY_F             = 'F',
	RUSH_KEY_G             = 'G',
	RUSH_KEY_H             = 'H',
	RUSH_KEY_I             = 'I',
	RUSH_KEY_J             = 'J',
	RUSH_KEY_K             = 'K',
	RUSH_KEY_L             = 'L',
	RUSH_KEY_M             = 'M',
	RUSH_KEY_N             = 'N',
	RUSH_KEY_O             = 'O',
	RUSH_KEY_P             = 'P',
	RUSH_KEY_Q             = 'Q',
	RUSH_KEY_R             = 'R',
	RUSH_KEY_S             = 'S',
	RUSH_KEY_T             = 'T',
	RUSH_KEY_U             = 'U',
	RUSH_KEY_V             = 'V',
	RUSH_KEY_W             = 'W',
	RUSH_KEY_X             = 'X',
	RUSH_KEY_Y             = 'Y',
	RUSH_KEY_Z             = 'Z',
	RUSH_KEY_LEFT_BRACKET  = '[',
	RUSH_KEY_BACKSLASH     = '\\',
	RUSH_KEY_RIGHT_BRACKET = ']',
	RUSH_KEY_BACKQUOTE     = '`',
	RUSH_KEY_ESCAPE,
	RUSH_KEY_ENTER,
	RUSH_KEY_TAB,
	RUSH_KEY_BACKSPACE,
	RUSH_KEY_INSERT,
	RUSH_KEY_DELETE,
	RUSH_KEY_RIGHT,
	RUSH_KEY_LEFT,
	RUSH_KEY_DOWN,
	RUSH_KEY_UP,
	RUSH_KEY_PAGE_UP,
	RUSH_KEY_PAGE_DOWN,
	RUSH_KEY_HOME,
	RUSH_KEY_END,
	RUSH_KEY_CAPS_LOCK,
	RUSH_KEY_SCROLL_LOCK,
	RUSH_KEY_NUM_LOCK,
	RUSH_KEY_PRINT_SCREEN,
	RUSH_KEY_PAUSE,
	RUSH_KEY_F1,
	RUSH_KEY_F2,
	RUSH_KEY_F3,
	RUSH_KEY_F4,
	RUSH_KEY_F5,
	RUSH_KEY_F6,
	RUSH_KEY_F7,
	RUSH_KEY_F8,
	RUSH_KEY_F9,
	RUSH_KEY_F10,
	RUSH_KEY_F11,
	RUSH_KEY_F12,
	RUSH_KEY_F13,
	RUSH_KEY_F14,
	RUSH_KEY_F15,
	RUSH_KEY_F16,
	RUSH_KEY_F17,
	RUSH_KEY_F18,
	RUSH_KEY_F19,
	RUSH_KEY_F20,
	RUSH_KEY_F21,
	RUSH_KEY_F22,
	RUSH_KEY_F23,
	RUSH_KEY_F24,
	RUSH_KEY_F25,
	RUSH_KEY_KP0,
	RUSH_KEY_KP1,
	RUSH_KEY_KP2,
	RUSH_KEY_KP3,
	RUSH_KEY_KP4,
	RUSH_KEY_KP5,
	RUSH_KEY_KP6,
	RUSH_KEY_KP7,
	RUSH_KEY_KP8,
	RUSH_KEY_KP9,
	RUSH_KEY_KP_DECIMAL,
	RUSH_KEY_KP_DIVIDE,
	RUSH_KEY_KP_MULTIPLY,
	RUSH_KEY_KP_SUBTRACT,
	RUSH_KEY_KP_ADD,
	RUSH_KEY_KP_ENTER,
	RUSH_KEY_KP_EQUAL,
	RUSH_KEY_LEFT_SHIFT,
	RUSH_KEY_LEFT_CONTROL,
	RUSH_KEY_LEFT_ALT,
	RUSH_KEY_RIGHT_SHIFT,
	RUSH_KEY_RIGHT_CONTROL,
	RUSH_KEY_RIGHT_ALT,

	RUSH_KEY_COUNT,
} rush_key;

typedef struct rush_window_keyboard_state
{
	bool keys[RUSH_KEY_COUNT];
} rush_window_keyboard_state;

typedef struct rush_window_mouse_state
{
	bool    buttons[10];
	bool    double_click;
	float   pos[2];
	int32_t wheel[2];
} rush_window_mouse_state;

typedef enum rush_window_event_type
{
	RUSH_WINDOW_EVENT_TYPE_UNKNOWN,
	RUSH_WINDOW_EVENT_TYPE_KEY_DOWN,
	RUSH_WINDOW_EVENT_TYPE_KEY_UP,
	RUSH_WINDOW_EVENT_TYPE_RESIZE,
	RUSH_WINDOW_EVENT_TYPE_CHAR,
	RUSH_WINDOW_EVENT_TYPE_MOUSE_DOWN,
	RUSH_WINDOW_EVENT_TYPE_MOUSE_UP,
	RUSH_WINDOW_EVENT_TYPE_MOUSE_MOVE,
	RUSH_WINDOW_EVENT_TYPE_SCROLL,
	RUSH_WINDOW_EVENT_TYPE_COUNT
} rush_window_event_type;

typedef enum rush_window_event_mask
{
	RUSH_WINDOW_EVENT_MASK_KEY_DOWN   = 1 << RUSH_WINDOW_EVENT_TYPE_KEY_DOWN,
	RUSH_WINDOW_EVENT_MASK_KEY_UP     = 1 << RUSH_WINDOW_EVENT_TYPE_KEY_UP,
	RUSH_WINDOW_EVENT_MASK_RESIZE     = 1 << RUSH_WINDOW_EVENT_TYPE_RESIZE,
	RUSH_WINDOW_EVENT_MASK_CHAR       = 1 << RUSH_WINDOW_EVENT_TYPE_CHAR,
	RUSH_WINDOW_EVENT_MASK_MOUSE_DOWN = 1 << RUSH_WINDOW_EVENT_TYPE_MOUSE_DOWN,
	RUSH_WINDOW_EVENT_MASK_MOUSE_UP   = 1 << RUSH_WINDOW_EVENT_TYPE_MOUSE_UP,
	RUSH_WINDOW_EVENT_MASK_MOUSE_MOVE = 1 << RUSH_WINDOW_EVENT_TYPE_MOUSE_MOVE,
	RUSH_WINDOW_EVENT_MASK_SCROLL     = 1 << RUSH_WINDOW_EVENT_TYPE_SCROLL,
	RUSH_WINDOW_EVENT_MASK_KEY        = RUSH_WINDOW_EVENT_MASK_KEY_DOWN | RUSH_WINDOW_EVENT_MASK_KEY_UP,
	RUSH_WINDOW_EVENT_MASK_MOUSE      = RUSH_WINDOW_EVENT_MASK_MOUSE_DOWN | RUSH_WINDOW_EVENT_MASK_MOUSE_UP | RUSH_WINDOW_EVENT_MASK_MOUSE_MOVE,
	RUSH_WINDOW_EVENT_MASK_ALL        = 0XFFFF
} rush_window_event_mask;

typedef struct rush_window_event
{
	rush_window_event_type event_type;
	rush_key key;
	uint32_t character;
	uint32_t modifiers;
	uint32_t width;
	uint32_t height;
	float pos[2];
	uint32_t  button;
	bool double_click;
	float scroll[2];
} rush_window_event;

struct rush_window* rush_platform_get_main_window();

const rush_window_keyboard_state* rush_window_get_keyboard_state(struct rush_window* window);
const rush_window_mouse_state*    rush_window_get_mouse_state(struct rush_window* window);
rush_vec2 rush_window_get_size(struct rush_window* window);
rush_vec2 rush_window_get_framebuffer_size(struct rush_window* window);
rush_vec2 rush_window_get_resolution_sclae(struct rush_window* window);
float rush_window_get_aspect(struct rush_window* window);

struct rush_window_event_listener* rush_window_create_listener(struct rush_window* window, rush_window_event_mask event_mask);
void rush_window_destroy_listener(struct rush_window_event_listener* listener);
uint32_t rush_window_event_listener_count(struct rush_window_event_listener* listener);
uint32_t rush_window_event_listener_receive(struct rush_window_event_listener* listener, uint32_t max_count, rush_window_event* out_events);
void rush_window_event_listener_clear(struct rush_window_event_listener* listener);

// Graphics

struct rush_gfx_device*  rush_platform_get_device();
struct rush_gfx_context* rush_platform_get_context();

typedef struct rush_gfx_vertex_format 
{ uint16_t handle; } rush_gfx_vertex_format;
typedef struct rush_gfx_vertex_shader 
{ uint16_t handle; } rush_gfx_vertex_shader;
typedef struct rush_gfx_pixel_shader 
{ uint16_t handle; } rush_gfx_pixel_shader;
typedef struct rush_gfx_geometry_shader 
{ uint16_t handle; } rush_gfx_geometry_shader;
typedef struct rush_gfx_compute_shader 
{ uint16_t handle; } rush_gfx_compute_shader;
typedef struct rush_gfx_mesh_shader 
{ uint16_t handle; } rush_gfx_mesh_shader;
typedef struct rush_gfx_ray_tracing_pipeline 
{ uint16_t handle; } rush_gfx_ray_tracing_pipeline;
typedef struct rush_gfx_acceleration_structure 
{ uint16_t handle; } rush_gfx_acceleration_structure;
typedef struct rush_gfx_texture 
{ uint16_t handle; } rush_gfx_texture;
typedef struct rush_gfx_buffer 
{ uint16_t handle; } rush_gfx_buffer;
typedef struct rush_gfx_sampler 
{ uint16_t handle; } rush_gfx_sampler;
typedef struct rush_gfx_blend_state 
{ uint16_t handle; } rush_gfx_blend_state;
typedef struct rush_gfx_depth_stencil_state 
{ uint16_t handle; } rush_gfx_depth_stencil_state;
typedef struct rush_gfx_rasterizer_state 
{ uint16_t handle; } rush_gfx_rasterizer_state;
typedef struct rush_gfx_technique 
{ uint16_t handle; } rush_gfx_technique;
typedef struct rush_gfx_descriptor_set 
{ uint16_t handle; } rush_gfx_descriptor_set;

typedef enum rush_gfx_blend_param
{
	RUSH_GFX_BLEND_PARAM_ZERO,
	RUSH_GFX_BLEND_PARAM_ONE,
	RUSH_GFX_BLEND_PARAM_SRC_COLOR,
	RUSH_GFX_BLEND_PARAM_INV_SRC_COLOR,
	RUSH_GFX_BLEND_PARAM_SRC_ALPHA,
	RUSH_GFX_BLEND_PARAM_INV_SRC_ALPHA,
	RUSH_GFX_BLEND_PARAM_DEST_ALPHA,
	RUSH_GFX_BLEND_PARAM_INV_DEST_ALPHA,
	RUSH_GFX_BLEND_PARAM_DEST_COLOR,
	RUSH_GFX_BLEND_PARAM_INV_DEST_COLOR,
} rush_gfx_blend_param;

typedef enum rush_gfx_blend_op
{
	RUSH_GFX_BLEND_OP_ADD,
	RUSH_GFX_BLEND_OP_SUBTRACT,
	RUSH_GFX_BLEND_OP_REV_SUBTRACT,
	RUSH_GFX_BLEND_OP_MIN,
	RUSH_GFX_BLEND_OP_MAX,
} rush_gfx_blend_op;

typedef enum rush_gfx_texture_filter
{
	RUSH_GFX_TEXTURE_FILTER_POINT,
	RUSH_GFX_TEXTURE_FILTER_LINEAR,
	RUSH_GFX_TEXTURE_FILTER_ANISOTROPIC,
} rush_gfx_texture_filter;

typedef enum rush_gfx_texture_wrap
{
	RUSH_GFX_TEXTURE_WRAP_REPEAT,
	RUSH_GFX_TEXTURE_WRAP_MIRROR,
	RUSH_GFX_TEXTURE_WRAP_CLAMP,
} rush_gfx_texture_wrap;

typedef enum rush_gfx_compare_func
{
	RUSH_GFX_COMPARE_FUNC_NEVER,
	RUSH_GFX_COMPARE_FUNC_LESS,
	RUSH_GFX_COMPARE_FUNC_EQUAL,
	RUSH_GFX_COMPARE_FUNC_LESS_EQUAL,
	RUSH_GFX_COMPARE_FUNC_GREATER,
	RUSH_GFX_COMPARE_FUNC_NOT_EQUAL,
	RUSH_GFX_COMPARE_FUNC_GREATER_EQUAL,
	RUSH_GFX_COMPARE_FUNC_ALWAYS,
} rush_gfx_compare_func;

typedef enum rush_gfx_fill_mode
{
	RUSH_GFX_FILL_MODE_SOLID,
	RUSH_GFX_FILL_MODE_WIREFRAME
} rush_gfx_fill_mode;

typedef enum rush_gfx_cull_mode
{
	RUSH_GFX_CULL_MODE_NONE,
	RUSH_GFX_CULL_MODE_CW,
	RUSH_GFX_CULL_MODE_CCW,
} rush_gfx_cull_mode;

typedef enum rush_gfx_embedded_shader_type 
{
	RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_PLAIN_PS,
	RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_TEXTURED_PS,
	RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_2D_VS,
	RUSH_GFX_EMBEDDED_SHADER_PRIMITIVE_3D_VS,
} rush_gfx_embedded_shader_type;

typedef enum rush_gfx_primitive_type 
{
	RUSH_GFX_PRIMITIVE_POINT_LIST,
	RUSH_GFX_PRIMITIVE_LINE_LIST,
	RUSH_GFX_PRIMITIVE_LINE_STRIP,
	RUSH_GFX_PRIMITIVE_TRIANGLE_LIST,
	RUSH_GFX_PRIMITIVE_TRIANGLE_STRIP,
} rush_gfx_primitive_type;

typedef enum rush_gfx_pass_flags 
{
	RUSH_GFX_PASS_NONE                = 0,
	RUSH_GFX_PASS_CLEAR_COLOR         = 1 << 0,
	RUSH_GFX_PASS_CLEAR_DEPTH_STENCIL = 1 << 1,
	RUSH_GFX_PASS_DISCARD_COLOR       = 1 << 2,
} rush_gfx_pass_flags;

typedef enum rush_gfx_shader_source_type
{
	RUSH_GFX_SHADER_SOURCE_UNKNOWN = 0,
	RUSH_GFX_SHADER_SOURCE_SPV     = 1, // binary
	RUSH_GFX_SHADER_SOURCE_GLSL    = 2, // text
	RUSH_GFX_SHADER_SOURCE_HLSL    = 3, // text
	RUSH_GFX_SHADER_SOURCE_DXBC    = 4, // binary
	RUSH_GFX_SHADER_SOURCE_DXIL    = 5, // binary
	RUSH_GFX_SHADER_SOURCE_MSL     = 6, // text
} rush_gfx_shader_source_type;

typedef enum rush_gfx_format
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
} rush_gfx_format;

typedef enum rush_gfx_resource_state
{
	RUSH_GFX_RESOURCE_STATE_UNDEFINED,
	RUSH_GFX_RESOURCE_STATE_GENERAL,
	RUSH_GFX_RESOURCE_STATE_RENDER_TARGET,
	RUSH_GFX_RESOURCE_STATE_DEPTH_STENCIL_TARGET,
	RUSH_GFX_RESOURCE_STATE_DEPTH_STENCIL_TARGET_READ_ONLY,
	RUSH_GFX_RESOURCE_STATE_SHADER_READ,
	RUSH_GFX_RESOURCE_STATE_TRANSFER_SRC,
	RUSH_GFX_RESOURCE_STATE_TRANSFER_DST,
	RUSH_GFX_RESOURCE_STATE_PREINITIALIZED,
	RUSH_GFX_RESOURCE_STATE_PRESENT,
	RUSH_GFX_RESOURCE_STATE_SHARED_PRESENT,
} rush_gfx_resource_state;

typedef struct rush_gfx_color_target
{
	rush_gfx_texture target;
	rush_color_rgba  clear_color;
} rush_gfx_color_target;

typedef struct rush_gfx_depth_target
{
	rush_gfx_texture target;
	float    clear_depth;
	uint8_t  clear_stencil;
} rush_gfx_depth_target;

typedef struct rush_gfx_capability
{
	const char* api_name;
} rush_gfx_capability;

typedef struct rush_gfx_stats
{
	uint32_t draw_calls;
} rush_gfx_stats;

typedef struct rush_gfx_viewport
{
	float x; // top left x
	float y; // top left y
	float w; // width
	float h; // height
	float depth_min;
	float depth_max;
} rush_gfx_viewport;

typedef struct rush_gfx_rect
{
	int left, top, right, bottom;
} rush_gfx_rect;

typedef struct rush_gfx_shader_source
{
	rush_gfx_shader_source_type type;
	const char*                 entry;
	const void*                 data;
	uint32_t                    size_bytes;
} rush_gfx_shader_source;

typedef enum rush_gfx_vertex_semantic
{
	RUSH_GFX_VERTEX_SEMANTIC_UNUSED       = 0,
	RUSH_GFX_VERTEX_SEMANTIC_POSITION     = 1,
	RUSH_GFX_VERTEX_SEMANTIC_TEXCOORD     = 2,
	RUSH_GFX_VERTEX_SEMANTIC_COLOR        = 3,
	RUSH_GFX_VERTEX_SEMANTIC_NORMAL       = 4,
	RUSH_GFX_VERTEX_SEMANTIC_TANGENTU     = 5,
	RUSH_GFX_VERTEX_SEMANTIC_TANGENTV     = 6,
	RUSH_GFX_VERTEX_SEMANTIC_INSTANCEDATA = 7,
	RUSH_GFX_VERTEX_SEMANTIC_BONEINDEX    = 8,
	RUSH_GFX_VERTEX_SEMANTIC_BONEWEIGHT   = 9,
} rush_gfx_vertex_semantic;

typedef struct rush_gfx_vertex_element
{
	rush_gfx_vertex_semantic semantic;
	uint32_t                 index;
	rush_gfx_format          format;
	uint32_t                 stream;
} rush_gfx_vertex_element;

typedef enum rush_gfx_usage_flags
{
	RUSH_GFX_USAGE_SHADER_RESOURCE = 1 << 0,
	RUSH_GFX_USAGE_RENDER_TARGET   = 1 << 1,
	RUSH_GFX_USAGE_DEPTH_STENCIL   = 1 << 2,
	RUSH_GFX_USAGE_STORAGE_IMAGE   = 1 << 3,
	RUSH_GFX_USAGE_TRANSFER_SRC    = 1 << 4,
	RUSH_GFX_USAGE_TRANSFER_DST    = 1 << 5,
} rush_gfx_usage_flags;

typedef enum rush_gfx_image_aspect_flags
{
	RUSH_GFX_IMAGE_ASPECT_COLOR         = 1 << 0,
	RUSH_GFX_IMAGE_ASPECT_DEPTH         = 1 << 1,
	RUSH_GFX_IMAGE_ASPECT_STENCIL       = 1 << 2,
	RUSH_GFX_IMAGE_ASPECT_METADATA      = 1 << 3,
	RUSH_GFX_IMAGE_ASPECT_DEPTH_STENCIL = RUSH_GFX_IMAGE_ASPECT_DEPTH | RUSH_GFX_IMAGE_ASPECT_STENCIL,
	RUSH_GFX_IMAGE_ASPECT_ALL           = RUSH_GFX_IMAGE_ASPECT_COLOR | RUSH_GFX_IMAGE_ASPECT_DEPTH_STENCIL | RUSH_GFX_IMAGE_ASPECT_METADATA,
} rush_gfx_image_aspect_flags;

typedef enum rush_gfx_texture_type
{
	RUSH_GFX_TEXTURE_TYPE_1D,
	RUSH_GFX_TEXTURE_TYPE_1D_ARRAY,
	RUSH_GFX_TEXTURE_TYPE_2D,
	RUSH_GFX_TEXTURE_TYPE_2D_ARRAY,
	RUSH_GFX_TEXTURE_TYPE_3D,
	RUSH_GFX_TEXTURE_TYPE_CUBE,
	RUSH_GFX_TEXTURE_TYPE_CUBE_ARRAY,
} rush_gfx_texture_type;

typedef struct rush_gfx_texture_desc
{
	uint32_t               width;
	uint32_t               height;
	uint32_t               depth;
	uint32_t               mips;
	uint32_t               samples;
	rush_gfx_format        format;
	rush_gfx_texture_type  texture_type;
	rush_gfx_usage_flags   usage;
	const char*            debug_name;
} rush_gfx_texture_desc;

typedef struct rush_gfx_depth_stencil_desc
{
	rush_gfx_compare_func compare_func;
	bool                  enable;
	bool                  write_enable;
} rush_gfx_depth_stencil_desc;

typedef struct rush_gfx_rasterizer_desc
{
	rush_gfx_fill_mode fill_mode;
	rush_gfx_cull_mode cull_mode;
	float              depth_bias;
	float              depth_bias_slope_scale;
} rush_gfx_rasterizer_desc;

typedef enum rush_gfx_buffer_flags
{
	RUSH_GFX_BUFFER_FLAG_NONE          = 0U,
	RUSH_GFX_BUFFER_FLAG_VERTEX        = 1U << 0,
	RUSH_GFX_BUFFER_FLAG_INDEX         = 1U << 1,
	RUSH_GFX_BUFFER_FLAG_CONSTANT      = 1U << 2,
	RUSH_GFX_BUFFER_FLAG_STORAGE       = 1U << 3,
	RUSH_GFX_BUFFER_FLAG_TEXEL         = 1U << 4,
	RUSH_GFX_BUFFER_FLAG_INDIRECT_ARGS = 1U << 5,
	RUSH_GFX_BUFFER_FLAG_RAYTRACING    = 1U << 6,
	RUSH_GFX_BUFFER_FLAG_TRANSIENT     = 1U << 30,
} rush_gfx_buffer_flags;

typedef struct rush_gfx_buffer_desc
{
	rush_gfx_buffer_flags flags;
	rush_gfx_format       format;
	uint32_t              stride;
	uint32_t              count;
	bool                  host_visible;
} rush_gfx_buffer_desc;

typedef struct rush_gfx_mapped_buffer
{
	void*           data;
	uint32_t        size;
	rush_gfx_buffer handle;
} rush_gfx_mapped_buffer;

typedef struct rush_gfx_mapped_texture
{
	void*            data;
	uint32_t         size;
	rush_gfx_texture handle;
} rush_gfx_mapped_texture;

typedef struct rush_gfx_spec_constant
{
	uint32_t id;
	uint32_t offset;
	uint32_t size;
} rush_gfx_spec_constant;

typedef enum rush_gfx_stage_type
{
	RUSH_GFX_STAGE_VERTEX      = 0,
	RUSH_GFX_STAGE_GEOMETRY    = 1,
	RUSH_GFX_STAGE_PIXEL       = 2,
	RUSH_GFX_STAGE_HULL        = 3,
	RUSH_GFX_STAGE_DOMAIN      = 4,
	RUSH_GFX_STAGE_COMPUTE     = 5,
	RUSH_GFX_STAGE_MESH        = 6,
	RUSH_GFX_STAGE_RAYTRACING  = 7,
} rush_gfx_stage_type;

typedef enum rush_gfx_stage_flags
{
	RUSH_GFX_STAGE_FLAG_NONE        = 0U,
	RUSH_GFX_STAGE_FLAG_VERTEX      = 1U << RUSH_GFX_STAGE_VERTEX,
	RUSH_GFX_STAGE_FLAG_GEOMETRY    = 1U << RUSH_GFX_STAGE_GEOMETRY,
	RUSH_GFX_STAGE_FLAG_PIXEL       = 1U << RUSH_GFX_STAGE_PIXEL,
	RUSH_GFX_STAGE_FLAG_HULL        = 1U << RUSH_GFX_STAGE_HULL,
	RUSH_GFX_STAGE_FLAG_DOMAIN      = 1U << RUSH_GFX_STAGE_DOMAIN,
	RUSH_GFX_STAGE_FLAG_COMPUTE     = 1U << RUSH_GFX_STAGE_COMPUTE,
	RUSH_GFX_STAGE_FLAG_MESH        = 1U << RUSH_GFX_STAGE_MESH,
	RUSH_GFX_STAGE_FLAG_RAYTRACING  = 1U << RUSH_GFX_STAGE_RAYTRACING,
} rush_gfx_stage_flags;

typedef enum rush_gfx_descriptor_set_flags
{
	RUSH_GFX_DESCRIPTOR_SET_FLAG_NONE  = 0,
	RUSH_GFX_DESCRIPTOR_SET_FLAG_TEXTURE_ARRAY = 1 << 0,
} rush_gfx_descriptor_set_flags;

typedef struct rush_gfx_descriptor_set_desc
{
	uint16_t                      constant_buffers;
	uint16_t                      samplers;
	uint16_t                      textures;
	uint16_t                      rw_images;
	uint16_t                      rw_buffers;
	uint16_t                      rw_typed_buffers;
	uint16_t                      acceleration_structures;
	rush_gfx_stage_flags          stage_flags;
	rush_gfx_descriptor_set_flags flags;
} rush_gfx_descriptor_set_desc;

typedef struct rush_gfx_shader_bindings_desc
{
	const rush_gfx_descriptor_set_desc* descriptor_sets;
	uint32_t descriptor_set_count;
	bool use_default_descriptor_set;
} rush_gfx_shader_bindings_desc;

typedef struct rush_gfx_technique_desc
{
	rush_gfx_compute_shader        cs;
	rush_gfx_pixel_shader          ps;
	rush_gfx_geometry_shader       gs;
	rush_gfx_vertex_shader         vs;
	rush_gfx_mesh_shader           ms;
	rush_gfx_vertex_format         vf;
	rush_gfx_shader_bindings_desc  bindings;
	uint16_t                       work_group_size[3];
	uint32_t                       spec_constant_count;
	const rush_gfx_spec_constant*  spec_constants;
	const void*                    spec_data;
	uint32_t                       spec_data_size;
} rush_gfx_technique_desc;

typedef struct rush_gfx_texture_data
{
	uint64_t    offset;
	const void* pixels;
	uint32_t    mip;
	uint32_t    slice;
	uint32_t    width;
	uint32_t    height;
	uint32_t    depth;
} rush_gfx_texture_data;

typedef struct rush_gfx_blend_state_desc
{
	rush_gfx_blend_param src;
	rush_gfx_blend_param dst;
	rush_gfx_blend_op    op;
	rush_gfx_blend_param alpha_src;
	rush_gfx_blend_param alpha_dst;
	rush_gfx_blend_op    alpha_op;
	bool                 alpha_separate;
	bool                 enable;
} rush_gfx_blend_state_desc;

typedef struct rush_gfx_sampler_desc
{
	rush_gfx_texture_filter filter_min;
	rush_gfx_texture_filter filter_mag;
	rush_gfx_texture_filter filter_mip;
	rush_gfx_texture_wrap   wrap_u;
	rush_gfx_texture_wrap   wrap_v;
	rush_gfx_texture_wrap   wrap_w;
	rush_gfx_compare_func   compare_func;
	bool                    compare_enable;
	float                   anisotropy;
	float                   mip_lod_bias;
} rush_gfx_sampler_desc;

typedef struct rush_gfx_subresource_range
{
	rush_gfx_image_aspect_flags aspect_mask;
	uint32_t                    base_mip_level;
	uint32_t                    level_count;
	uint32_t                    base_array_layer;
	uint32_t                    layer_count;
} rush_gfx_subresource_range;

void rush_gfx_set_present_interval(uint32_t interval);
void rush_gfx_finish();
rush_gfx_capability rush_gfx_get_capability();
rush_gfx_stats rush_gfx_get_stats();
void rush_gfx_reset_stats();

rush_gfx_vertex_format rush_gfx_create_vertex_format(const rush_gfx_vertex_element* elements, uint32_t count);
rush_gfx_vertex_shader rush_gfx_create_vertex_shader(const rush_gfx_shader_source* code);
rush_gfx_pixel_shader rush_gfx_create_pixel_shader(const rush_gfx_shader_source* code);
rush_gfx_geometry_shader rush_gfx_create_geometry_shader(const rush_gfx_shader_source* code);
rush_gfx_compute_shader rush_gfx_create_compute_shader(const rush_gfx_shader_source* code);
rush_gfx_technique rush_gfx_create_technique(const rush_gfx_technique_desc* desc);
rush_gfx_texture rush_gfx_create_texture(const rush_gfx_texture_desc* tex, const rush_gfx_texture_data* data, uint32_t count, const void* pixels);
rush_gfx_blend_state rush_gfx_create_blend_state(const rush_gfx_blend_state_desc* desc);
rush_gfx_sampler rush_gfx_create_sampler_state(const rush_gfx_sampler_desc* desc);
rush_gfx_depth_stencil_state rush_gfx_create_depth_stencil_state(const rush_gfx_depth_stencil_desc* desc);
rush_gfx_rasterizer_state rush_gfx_create_rasterizer_state(const rush_gfx_rasterizer_desc* desc);
rush_gfx_buffer rush_gfx_create_buffer(const rush_gfx_buffer_desc* desc, const void* data);
void rush_gfx_release_vertex_format(rush_gfx_vertex_format h);
void rush_gfx_release_vertex_shader(rush_gfx_vertex_shader h);
void rush_gfx_release_pixel_shader(rush_gfx_pixel_shader h);
void rush_gfx_release_geometry_shader(rush_gfx_geometry_shader h);
void rush_gfx_release_compute_shader(rush_gfx_compute_shader h);
void rush_gfx_release_mesh_shader(rush_gfx_mesh_shader h);
void rush_gfx_release_ray_tracing_pipeline(rush_gfx_ray_tracing_pipeline h);
void rush_gfx_release_acceleration_structure(rush_gfx_acceleration_structure h);
void rush_gfx_release_technique(rush_gfx_technique h);
void rush_gfx_release_texture(rush_gfx_texture h);
void rush_gfx_release_blend_state(rush_gfx_blend_state h);
void rush_gfx_release_sampler(rush_gfx_sampler h);
void rush_gfx_release_depth_stencil_state(rush_gfx_depth_stencil_state h);
void rush_gfx_release_rasterizer_state(rush_gfx_rasterizer_state h);
void rush_gfx_release_buffer(rush_gfx_buffer h);
void rush_gfx_release_descriptor_set(rush_gfx_descriptor_set h);
rush_gfx_texture_desc gfx_get_texture_desc(rush_gfx_texture h);
rush_gfx_texture gfx_get_back_buffer_color_texture();
rush_gfx_texture gfx_get_back_buffer_depth_texture();
rush_gfx_mapped_buffer gfx_map_buffer(rush_gfx_buffer h, uint32_t offset, uint32_t size);
void rush_gfx_unmap_buffer(const rush_gfx_mapped_buffer* in_mapped_buffer);
void rush_gfx_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, const void* data, uint32_t size);
void* rush_gfx_begin_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h, uint32_t size);
void rush_gfx_end_update_buffer(struct rush_gfx_context* ctx, rush_gfx_buffer h);
void rush_gfx_begin_pass(
	struct rush_gfx_context* ctx, 
	uint32_t color_count,
	const rush_gfx_color_target* color,
	const rush_gfx_depth_target* depth,
	rush_gfx_pass_flags flags
);
void rush_gfx_end_pass(struct rush_gfx_context* ctx);
void rush_gfx_set_viewport(struct rush_gfx_context* ctx, const rush_gfx_viewport* _viewport);
void rush_gfx_set_scissor_rect(struct rush_gfx_context* ctx, const rush_gfx_rect* rect);
void rush_gfx_set_technique(struct rush_gfx_context* ctx, rush_gfx_technique h);
void rush_gfx_set_primitive(struct rush_gfx_context* ctx, enum rush_gfx_primitive_type type);
void rush_gfx_set_index_stream(struct rush_gfx_context* ctx, rush_gfx_buffer h);
void rush_gfx_set_vertex_stream(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h);
void rush_gfx_set_texture(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_texture h);
void rush_gfx_set_sampler(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_sampler h);
void rush_gfx_set_storage_image(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_texture h);
void rush_gfx_set_storage_buffer(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h);
void rush_gfx_set_blend_state(struct rush_gfx_context* ctx, rush_gfx_blend_state h);
void rush_gfx_set_depth_stencil_state(struct rush_gfx_context* ctx, rush_gfx_depth_stencil_state h);
void rush_gfx_set_rasterizer_state(struct rush_gfx_context* ctx, rush_gfx_rasterizer_state h);
void rush_gfx_set_constant_buffer(struct rush_gfx_context* ctx, uint32_t idx, rush_gfx_buffer h, uint32_t offset);
void rush_gfx_add_image_barrier(struct rush_gfx_context* ctx, rush_gfx_texture texture, rush_gfx_resource_state desired_state);
void rush_gfx_add_image_subresource_barrier(struct rush_gfx_context* ctx, rush_gfx_texture texture, rush_gfx_resource_state desired_state, rush_gfx_subresource_range subresource_range);
void rush_gfx_resolve_image(struct rush_gfx_context* ctx, rush_gfx_texture src, rush_gfx_texture dst);
void rush_gfx_dispatch(struct rush_gfx_context* ctx, uint32_t size_x, uint32_t size_y, uint32_t size_z);
void rush_gfx_dispatch2(struct rush_gfx_context* ctx, uint32_t size_x, uint32_t size_y, uint32_t size_z, const void* push_constants, uint32_t push_constants_size);
void rush_gfx_draw(struct rush_gfx_context* ctx, uint32_t first_vertex, uint32_t vertex_count);
void rush_gfx_draw_indexed(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count);
void rush_gfx_draw_indexed2(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count, const void* push_constants, uint32_t push_constants_size);
void rush_gfx_draw_indexed_instanced(struct rush_gfx_context* ctx, uint32_t index_count, uint32_t first_index, uint32_t base_vertex, uint32_t vertex_count, uint32_t instance_count, uint32_t instance_offset);
void rush_gfx_draw_indexed_indirect(struct rush_gfx_context* ctx, rush_gfx_buffer args_buffer, uint32_t args_buffer_offset, uint32_t draw_count);
void rush_gfx_dispatch_indirect(struct rush_gfx_context* ctx, rush_gfx_buffer args_buffer, uint32_t args_buffer_offset, const void* push_constants, uint32_t push_constants_size);
void rush_gfx_push_marker(struct rush_gfx_context* ctx, const char* marker);
void rush_gfx_pop_marker(struct rush_gfx_context* ctx);
void rush_gfx_begin_timer(struct rush_gfx_context* ctx, uint32_t timestamp_id);
void rush_gfx_end_timer(struct rush_gfx_context* ctx, uint32_t timestamp_id);

// Embedded resources
rush_gfx_shader_source rush_gfx_get_embedded_shader(rush_gfx_embedded_shader_type type);
void rush_embedded_font_blit_6x8(uint32_t* output, uint32_t output_offset_pixels, uint32_t width, uint32_t color, const char* text);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // INCLUDED_RUSHC_H
