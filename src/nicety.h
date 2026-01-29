#pragma once
#include <SDL3/SDL_stdinc.h>
#include "clay.h"
#include <stddef.h>
#include "document.h"
#include "application_core.h"

typedef enum
{
	LOAD_FILE,
	FILE_VIEW,
} AppState;

typedef struct
{
	ILayer       interface;
	Clay_Vector2 scroll_state;
	size_t       sensitivity;
	AppState     program_state;
	Document    *doc;
} App;

void app_init(App *self, WindowSpecs specs);
void app_on_update(void *self);
void app_on_event(void *app);
