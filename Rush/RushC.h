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

struct rush_gfx_device*  rush_platform_get_device();
struct rush_gfx_context* rush_platform_get_context();

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // INCLUDED_RUSHC_H
