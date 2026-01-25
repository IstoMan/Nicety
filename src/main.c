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
	    .height        = 2000,
	    .width         = 1500,
	    .title         = "Nicety",
	    .turn_vsync_on = false,
	};

	AppCore core;
	if (!core_application_init(&core, specs))
	{
		return EXIT_FAILURE;
	}

	App my_app = {
	    .scroll_state = {
	        .x = 0,
	        .y = 0,
	    }};

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

	core_application_run(&core, &my_app, &doc, nicety_create_layout);

	free(clayMemory.memory);
	return EXIT_SUCCESS;
}
