#include "base.h"
#include "application_core.h"
#include "application_core.c"

int main(void)
{
	WindowSpecs specs = {
	    .height        = 800,
	    .width         = 600,
	    .title         = "Nicety",
	    .turn_vsync_on = true,
	};

	AppCore core;
	if (!core_application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	core_application_run(&core, 60);
	return EXIT_SUCCESS;
}
