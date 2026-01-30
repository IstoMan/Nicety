#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_ttf/SDL_textengine.h>
#include "clay.h"

typedef struct App App;

typedef struct Application
{
	SDL_Window     *window;
	SDL_Renderer   *renderer;
	TTF_TextEngine *ttf_renderer;
	TTF_Font      **fonts;
	Clay_Arena      clay_memory;

	bool is_running;
} Application;

typedef struct
{
	uint32_t    width, height;
	const char *title;
	bool        turn_vsync_on;
} WindowSpecs;

bool application_init(Application *core, WindowSpecs specs);
void application_run(Application *core, App *app);
void application_cleanup(Application *core);
