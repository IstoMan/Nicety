#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "nicety.h"
#include "application_core.h"
#include <stdbool.h>
#include <stdlib.h>

int main(void)
{
	WindowSpecs specs = {
	    .height        = 500,
	    .width         = 500,
	    .title         = "Nicety",
	    .turn_vsync_on = true,
	};

	AppCore core;
	if (!core_application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	Document doc;
	int      err = init_document("resources/book.pdf", &doc);
	if (err == 1)
	{
		return EXIT_FAILURE;
	}

	core_application_run(&core);

	return EXIT_SUCCESS;
}
