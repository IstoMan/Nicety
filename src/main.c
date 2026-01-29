#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "nicety.h"
#include "application_core.h"
#include <stdbool.h>
#include <stdlib.h>

int main(void)
{
	WindowSpecs specs = {
	    .height        = 2000,
	    .width         = 1500,
	    .title         = "Nicety",
	    .turn_vsync_on = false,
	};

	Application core;
	if (!application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	// Menu menu;
	App my_app;
	app_init(&my_app, specs);
	application_add_layer(&core, &my_app.interface);

	application_run(&core);

	// SDL_free(clayMemory.memory);
	return EXIT_SUCCESS;
}
