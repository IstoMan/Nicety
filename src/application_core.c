#include "application_core.h"
#include "clay_renderer_SDL3.h"
#include <stdbool.h>
#include <stdint.h>
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

void core_application_run(AppCore *core, App *app, Document *doc, create_ui layout_func)
{
	SDL_Event event;
	core->is_running = true;

	Clay_SDL3RendererData data = {
	    .renderer = core->renderer,
	};
	uint32_t last_tick_time = SDL_GetTicks();
	float    deltaTime      = 0.0f;

	while (core->is_running)
	{
		Uint32 tick_time = SDL_GetTicks();
		// Calculate delta time in milliseconds
		Uint32 delta = tick_time - last_tick_time;
		// Convert to seconds (float)
		deltaTime      = delta / 1000.0f;
		last_tick_time = tick_time;
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
					app->scroll_state.x = event.wheel.x * 5;
					app->scroll_state.y = event.wheel.y * 5;
					break;
				default:
					break;
			}
		}

		Clay_RenderCommandArray commands = layout_func(*app, *doc);

		SDL_SetRenderDrawColor(core->renderer, 255, 255, 255, 255);
		SDL_RenderClear(core->renderer);

		SDL_Clay_RenderClayCommands(&data, &commands);

		SDL_RenderPresent(core->renderer);
	}
}
