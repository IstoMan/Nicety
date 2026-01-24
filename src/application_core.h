#pragma once
#include <SDL3/SDL.h>
#include "ui.h"
#include "clay.h"

typedef struct
{
	SDL_Window   *window;
	SDL_Renderer *renderer;
	bool          is_running;
} AppCore;

typedef struct
{
	uint32_t    width, height;
	const char *title;
	bool        turn_vsync_on;
} WindowSpecs;

bool core_application_init(AppCore *app, WindowSpecs specs);
void core_application_run(AppCore *app, create_ui layout_func);
void application_cleanup(AppCore *core);
