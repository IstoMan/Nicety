#include "application_core.h"
#include <stdio.h>

void application_cleanup(AppCore *core)
{
	SDL_DestroyRenderer(core->renderer);
	SDL_DestroyWindow(core->window);
	SDL_Quit();
}

bool core_application_init(AppCore *app, WindowSpecs specs)
{
	memset(app, 0, sizeof *app);
	bool is_initialized = true;

	app->is_running = false;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "Couldn't Init SDL: %s\n", SDL_GetError());
		application_cleanup(app);
		is_initialized = false;
		app            = NULL;
	}

	if (!SDL_CreateWindowAndRenderer(specs.title, specs.width, specs.height, SDL_WINDOW_RESIZABLE, &app->window, &app->renderer))
	{
		fprintf(stderr, "Couldn't Init Window and Renderer: %s\n", SDL_GetError());
		application_cleanup(app);
		is_initialized = false;
		app            = NULL;
	}
	else
	{
		SDL_SetRenderVSync(app->renderer, specs.turn_vsync_on);
	}

	return is_initialized;
}

void core_application_run(AppCore *app)
{
	SDL_Event event;
	app->is_running = true;

	while (app->is_running)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_EVENT_QUIT:
					app->is_running = false;
					break;
			}
		}

		SDL_RenderClear(app->renderer);
		SDL_SetRenderDrawColor(app->renderer, 0, 255, 255, 255);

		SDL_RenderPresent(app->renderer);
	}
}
