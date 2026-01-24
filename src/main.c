#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "base.h"
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

	uint64_t   totalMemorySize = Clay_MinMemorySize();
	Clay_Arena clayMemory      = {
	         .memory   = malloc(totalMemorySize),
	         .capacity = totalMemorySize};

	core_application_run(&core);

	free(clayMemory.memory);
	return EXIT_SUCCESS;
}
