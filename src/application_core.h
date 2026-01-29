#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_ttf/SDL_textengine.h>
#include "layer.h"
#include "clay.h"

typedef struct Application
{
	SDL_Window     *window;
	SDL_Renderer   *renderer;
	TTF_TextEngine *ttf_renderer;
	TTF_Font      **fonts;
	ILayer_Array    layer_stack;

	bool is_running;
} Application;

typedef struct
{
	uint32_t    width, height;
	const char *title;
	bool        turn_vsync_on;
} WindowSpecs;

bool application_init(Application *core, WindowSpecs specs);
void application_add_layer(Application *core, ILayer *pp);
void application_run(Application *core);
void application_cleanup(Application *core);
