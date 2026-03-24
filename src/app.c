#include "app.h"
#include "ui.h"
#include "clay_renderer_SDL3.h"
#include "tinyfiledialogs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_init(App *self)
{
	memset(self, 0, sizeof *self);
	self->sensitivity          = 3;
	self->program_state        = LOAD_FILE;
	self->document             = NULL;
	self->sidebar_scroll_valid = false;
	self->content_scroll_valid = false;
}

void app_destroy(App *self)
{
	if (self->document != NULL)
	{
		document_destroy(self->document);
	}
}

void app_on_render(App *self, void *renderer)
{
	Application          *app       = (Application *) renderer;
	Clay_SDL3RendererData clay_data = {
	    .renderer   = app->renderer,
	    .textEngine = app->ttf_renderer,
	    .fonts      = app->fonts,
	};
	SDL_Clay_RenderClayCommands(&clay_data, &self->ui_commands);
}

void app_on_update(App *self)
{
	switch (self->program_state)
	{
		case LOAD_FILE:
		{
			self->ui_commands = ui_load_file_layout();
		}
		break;
		case FILE_VIEW:
		{
			self->ui_commands = ui_document_view(*self->document, self);
		}
		break;
		default:
			break;
	}
}

static void app_sync_clay_layout_to_renderer(Application *core)
{
	int w, h;
	if (SDL_GetRenderOutputSize(core->renderer, &w, &h))
	{
		Clay_SetLayoutDimensions((Clay_Dimensions) {(float) w, (float) h});
	}
}

void app_on_event(App *self, Application *core, Event event, float deltaTime)
{
	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			app_sync_clay_layout_to_renderer(core);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y}, event.button.button == SDL_BUTTON_LEFT);
			if (self->program_state == LOAD_FILE && event.button.button == SDL_BUTTON_LEFT)
			{
				char const *filter[]   = {"*.pdf"};
				char       *input_path = tinyfd_openFileDialog("Select a PDF", "./resources/", 1, filter, "PDF File", false);
				if (input_path)
				{
					if (self->document != NULL)
					{
						document_destroy(self->document);
						self->document = NULL;
					}
					char *input_path_copy = SDL_strdup(input_path);
					int   err             = document_init(&self->document, core, input_path_copy);
					if (err == 1 || self->document == NULL)
					{
						fprintf(stderr, "Couldn't Load file %s\n", SDL_GetError());
						SDL_free(input_path_copy);
						exit(1);
					}
					self->program_state = FILE_VIEW;
				}
			}
			break;
		case SDL_EVENT_MOUSE_MOTION:
			Clay_SetPointerState((Clay_Vector2) {event.motion.x, event.motion.y}, event.motion.state & SDL_BUTTON_LMASK);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
		{
			Clay_UpdateScrollContainers(true, (Clay_Vector2) {(float) event.wheel.x * self->sensitivity, (float) event.wheel.y * self->sensitivity}, deltaTime);
		}
		break;
		case SDL_EVENT_DROP_FILE:
		{
			if (self->document != NULL)
			{
				document_destroy(self->document);
				self->document = NULL;
			}
			char *file_path_copy = SDL_strdup(event.drop.data);
			int   err            = document_init(&self->document, core, file_path_copy);
			if (err == 1 || self->document == NULL)
			{
				fprintf(stderr, "Couldn't Load file %s\n", SDL_GetError());
				SDL_free(file_path_copy);
				exit(1);
			}
			self->program_state = FILE_VIEW;
		}
		break;
		default:
			break;
	}
}
