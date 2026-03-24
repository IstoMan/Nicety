#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "app.h"
#include "core.h"

int main(void)
{
	WindowSpecs specs = {
	    .height        = 800,
	    .width         = 1200,
	    .title         = "Nicety",
	    .turn_vsync_on = false,
	};

	Application core;
	if (!application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	App app;
	app_init(&app);

	application_run(&core, &app);

	app_destroy(&app);
	application_cleanup(&core);

	return EXIT_SUCCESS;
}
