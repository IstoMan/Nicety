#include "nicety.h"
#include "stdio.h"

void handle_clay_errors(Clay_ErrorData errorData);

void app_init(App *app, WindowSpecs specs)
{
	app->scroll_state.x = 0;
	app->scroll_state.y = 0;
	app->sensitivity    = 5;
	app->program_state  = LOAD_FILE;

	uint64_t   total_memory_size = Clay_MinMemorySize();
	Clay_Arena clay_memory       = (Clay_Arena) {
	          .memory   = malloc(total_memory_size),
	          .capacity = total_memory_size};

	Clay_Initialize(clay_memory, (Clay_Dimensions) {specs.width, specs.height}, (Clay_ErrorHandler) {handle_clay_errors});
}

void update_app(App *app)
{
	switch (app->program_state)
	{
		case LOAD_FILE:
		{
		}
		break;
		case FILE_VIEW:
		{
		}
		break;
		default:
			break;
	}
}

void handle_clay_errors(Clay_ErrorData errorData)
{
	fprintf(stderr, "%s\n", errorData.errorText.chars);
}
