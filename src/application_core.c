#include "application_core.h"
#include "clay_renderer_SDL3.h"
#include <SDL3/SDL_stdinc.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void application_cleanup(Application *core)
{
	SDL_DestroyRenderer(core->renderer);
	SDL_DestroyWindow(core->window);
	TTF_DestroyRendererTextEngine(core->ttf_renderer);
	SDL_free(core->fonts);
	TTF_Quit();
	SDL_Quit();
}

bool application_init(Application *app, WindowSpecs specs)
{
	memset(app, 0, sizeof *app);
	bool is_initialized = true;

	app->is_running       = false;
	app->layer_stack.size = 0;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "Couldn't Init SDL: %s\n", SDL_GetError());
		application_cleanup(app);
		app                   = NULL;
		return is_initialized = false;
	}

	if (!TTF_Init())
	{
		fprintf(stderr, "Couldn't Init TTF: %s\n", SDL_GetError());
		application_cleanup(app);
		app                   = NULL;
		return is_initialized = false;
	}

	if (!SDL_CreateWindowAndRenderer(specs.title, specs.width, specs.height, 0, &app->window, &app->renderer))
	{
		fprintf(stderr, "Couldn't Init Window and Renderer: %s\n", SDL_GetError());
		application_cleanup(app);
		app                   = NULL;
		return is_initialized = false;
	}
	SDL_SetRenderVSync(app->renderer, specs.turn_vsync_on);
	SDL_SetWindowResizable(app->window, true);

	app->ttf_renderer = TTF_CreateRendererTextEngine(app->renderer);
	if (app->ttf_renderer == NULL)
	{
		fprintf(stderr, "Couldn't Init TTF Renderer: %s\n", SDL_GetError());
		application_cleanup(app);
		app                   = NULL;
		return is_initialized = false;
	}

	app->fonts = SDL_calloc(1, sizeof(TTF_Font *));
	if (!app->fonts)
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to allocate memory for the font array: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	return is_initialized;
}

void application_run(Application *core)
{
	SDL_Event event;
	core->is_running = true;

	uint32_t last_tick_time = SDL_GetTicks();
	float    deltaTime      = 0.0f;

	while (core->is_running)
	{
		uint32_t tick_time = SDL_GetTicks();
		uint32_t delta     = tick_time - last_tick_time;
		deltaTime          = delta / 1000.0f;
		last_tick_time     = tick_time;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_EVENT_QUIT:
					core->is_running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
					Clay_SetLayoutDimensions((Clay_Dimensions) {(float) event.window.data1, (float) event.window.data2});
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y},
					                     event.button.button == SDL_BUTTON_LEFT);
					break;
				case SDL_EVENT_MOUSE_MOTION:
					Clay_SetPointerState((Clay_Vector2) {event.motion.x, event.motion.y}, event.motion.state & SDL_BUTTON_LMASK);
					break;
				case SDL_EVENT_MOUSE_WHEEL:
					Clay_UpdateScrollContainers(true, (Clay_Vector2) {app->scroll_state.x, app->scroll_state.y}, deltaTime);
					app->scroll_state.x = event.wheel.x * app->sensitivity;
					app->scroll_state.y = event.wheel.y * app->sensitivity;
					break;
				default:
					break;
			}
		}

		SDL_SetRenderDrawColor(core->renderer, 255, 255, 255, 255);
		SDL_RenderClear(core->renderer);

		SDL_RenderPresent(core->renderer);
	}
}

void application_add_layer(Application *core, ILayer *layer)
{
	if (core->layer_stack.size == 0)
	{
		core->layer_stack.layers = malloc(sizeof *layer);
	}
	else
	{
		core->layer_stack.layers = realloc(core->layer_stack.layers, sizeof *layer);
	}

	core->layer_stack.layers[core->layer_stack.size] = *layer;
	core->layer_stack.size++;
}
