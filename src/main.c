#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "nicety.h"
#include "core.h"
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

	App app;
	app_init(&app, specs);

	application_run(&core, &app);

	app_destroy(&app);
	application_cleanup(&core);

	return EXIT_SUCCESS;
}
