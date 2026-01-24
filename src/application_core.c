#include "application_core.h"
#include "clay_renderer_SDL3.h"
#include <SDL3/SDL_video.h>
#include <stdbool.h>
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

	if (!SDL_CreateWindowAndRenderer(specs.title, specs.width, specs.height, 0, &app->window, &app->renderer))
	{
		fprintf(stderr, "Couldn't Init Window and Renderer: %s\n", SDL_GetError());
		application_cleanup(app);
		is_initialized = false;
		app            = NULL;
	}
	else
	{
		SDL_SetRenderVSync(app->renderer, specs.turn_vsync_on);
		SDL_SetWindowResizable(app->window, true);
	}

	return is_initialized;
}

void core_application_run(AppCore *app, create_ui layout_func)
{
	SDL_Event event;
	app->is_running = true;

	Clay_SDL3RendererData data = {
	    .renderer = app->renderer,
	};

	while (app->is_running)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_EVENT_QUIT:
					app->is_running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
					Clay_SetLayoutDimensions((Clay_Dimensions) {(float) event.window.data1, (float) event.window.data2});
			}
		}

		Clay_RenderCommandArray commands = layout_func();

		SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
		SDL_RenderClear(app->renderer);

		SDL_Clay_RenderClayCommands(&data, &commands);

		SDL_RenderPresent(app->renderer);
	}
}
