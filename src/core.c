#include "core.h"
#include "clay_renderer_SDL3.h"
#include <SDL3/SDL_stdinc.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "nicety.h"

bool application_init(Application *core, WindowSpecs specs)
{
	memset(core, 0, sizeof *core);
	bool is_initialized = true;

	core->is_running = false;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "Couldn't Init SDL: %s\n", SDL_GetError());
		application_cleanup(core);
		core                  = NULL;
		return is_initialized = false;
	}

	if (!TTF_Init())
	{
		fprintf(stderr, "Couldn't Init TTF: %s\n", SDL_GetError());
		application_cleanup(core);
		core                  = NULL;
		return is_initialized = false;
	}

	if (!SDL_CreateWindowAndRenderer(specs.title, specs.width, specs.height, 0, &core->window, &core->renderer))
	{
		fprintf(stderr, "Couldn't Init Window and Renderer: %s\n", SDL_GetError());
		application_cleanup(core);
		core                  = NULL;
		return is_initialized = false;
	}
	SDL_SetRenderVSync(core->renderer, specs.turn_vsync_on);
	SDL_SetWindowResizable(core->window, true);

	core->ttf_renderer = TTF_CreateRendererTextEngine(core->renderer);
	if (core->ttf_renderer == NULL)
	{
		fprintf(stderr, "Couldn't Init TTF Renderer: %s\n", SDL_GetError());
		application_cleanup(core);
		core                  = NULL;
		return is_initialized = false;
	}

	core->fonts = SDL_calloc(1, sizeof(TTF_Font *));
	if (!core->fonts)
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to allocate memory for the font array: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	return is_initialized;
}

void application_run(Application *core, App *app)
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
				default:
					app_on_event(app, event, deltaTime);
			}
		}

		SDL_SetRenderDrawColor(core->renderer, 255, 255, 255, 255);
		SDL_RenderClear(core->renderer);

		app_on_update(app);

		SDL_RenderPresent(core->renderer);
	}
}

void application_cleanup(Application *core)
{
	SDL_DestroyRenderer(core->renderer);
	SDL_DestroyWindow(core->window);
	TTF_DestroyRendererTextEngine(core->ttf_renderer);
	SDL_free(core->fonts);
	TTF_Quit();
	SDL_Quit();
}
