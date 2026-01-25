#pragma once
#include <SDL3/SDL.h>
#include "clay.h"
#include "nicety.h"

typedef struct AppCore
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

typedef Clay_RenderCommandArray (*create_ui)(App app, Document doc);

bool core_application_init(AppCore *app, WindowSpecs specs);
void core_application_run(AppCore *core, App *app, Document *doc, create_ui layout_func);
void application_cleanup(AppCore *core);
