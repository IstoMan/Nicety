#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "nicety.h"
#include "stdio.h"
#include "ui.h"
#include "application_core.h"
#include <stdbool.h>
#include <stdlib.h>

void HandleClayErrors(Clay_ErrorData errorData)
{
	fprintf(stderr, "%s\n", errorData.errorText.chars);
}

int main(void)
{
	WindowSpecs specs = {
	    .height        = 900,
	    .width         = 720,
	    .title         = "Nicety",
	    .turn_vsync_on = true,
	};

	AppCore core;
	if (!core_application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	Document doc;
	int      err = init_document("resources/book.pdf", &doc, &core);
	if (err == 1)
	{
		return EXIT_FAILURE;
	}

	uint64_t   totalMemorySize = Clay_MinMemorySize();
	Clay_Arena clayMemory      = (Clay_Arena) {
	         .memory   = malloc(totalMemorySize),
	         .capacity = totalMemorySize};

	int width, height;
	SDL_GetWindowSize(core.window, &width, &height);
	Clay_Initialize(clayMemory, (Clay_Dimensions) {(float) width, (float) height}, (Clay_ErrorHandler) {HandleClayErrors});

	core_application_run(&core, &doc, nicety_create_layout);

	free(clayMemory.memory);
	return EXIT_SUCCESS;
}
