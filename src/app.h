#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include <stdbool.h>
#include "clay.h"
#include "core.h"
#include "document.h"

typedef SDL_Event Event;

typedef enum
{
	LOAD_FILE = 0,
	FILE_VIEW,
} AppState;

typedef enum
{
	VIEW_MODE_FILL = 0,
	VIEW_MODE_FIT_HEIGHT,
} ViewMode;

typedef struct App
{
	size_t                  sensitivity;
	AppState                program_state;
	ViewMode                view_mode;
	Document               *document;
	Clay_RenderCommandArray ui_commands;
	Clay_Vector2            sidebar_scroll_offset;
	Clay_Vector2            content_scroll_offset;
	bool                    sidebar_scroll_valid;
	bool                    content_scroll_valid;
} App;

void app_init(App *self);
void app_on_update(App *self);
void app_on_render(App *self, void *renderer);
void app_on_event(App *self, Application *core, Event event, float deltaTime);
void app_destroy(App *self);
