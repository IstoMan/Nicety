#include "app.h"
#include "ui.h"
#include "clay_renderer_SDL3.h"
#include "tinyfiledialogs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
	DOCUMENT_ARENA_BYTES = 65536
};

static void app_close_document(App *self)
{
	if (self->document != NULL)
	{
		document_destroy(self->document_ctx, self->document);
		self->document = NULL;
	}
	if (self->document_ctx != NULL)
	{
		document_context_destroy(self->document_ctx);
		self->document_ctx = NULL;
	}
	if (self->document_arena != NULL)
	{
		arena_destroy(self->document_arena);
		self->document_arena = NULL;
	}
}

static int app_open_pdf(App *self, Application *core, char *file_path_owned)
{
	nicety_arena *arena = arena_init(DOCUMENT_ARENA_BYTES);
	if (arena == NULL)
	{
		SDL_free(file_path_owned);
		return 1;
	}

	DocumentContext *ctx = document_context_init(arena, file_path_owned);
	if (ctx == NULL)
	{
		SDL_free(file_path_owned);
		arena_destroy(arena);
		return 1;
	}

	Document *doc = malloc(sizeof(Document));
	if (doc == NULL)
	{
		document_context_destroy(ctx);
		arena_destroy(arena);
		SDL_free(file_path_owned);
		return 1;
	}

	size_t till = ctx->total_pages > 0 ? ctx->total_pages - 1 : 0;
	int    err  = document_load_pages(ctx, core, 0, till, file_path_owned, doc);
	if (err != 0)
	{
		free(doc);
		document_context_destroy(ctx);
		arena_destroy(arena);
		SDL_free(file_path_owned);
		return 1;
	}

	self->document_arena = arena;
	self->document_ctx   = ctx;
	self->document       = doc;
	return 0;
}

void app_init(App *self)
{
	memset(self, 0, sizeof *self);
	self->sensitivity          = 3;
	self->program_state        = LOAD_FILE;
	self->document             = NULL;
	self->document_ctx         = NULL;
	self->document_arena       = NULL;
	self->sidebar_scroll_valid = false;
	self->content_scroll_valid = false;
}

void app_destroy(App *self)
{
	app_close_document(self);
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
					app_close_document(self);
					char *input_path_copy = SDL_strdup(input_path);
					if (app_open_pdf(self, core, input_path_copy) != 0)
					{
						fprintf(stderr, "Couldn't load file\n");
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
			app_close_document(self);
			char *file_path_copy = SDL_strdup(event.drop.data);
			if (app_open_pdf(self, core, file_path_copy) != 0)
			{
				fprintf(stderr, "Couldn't load file\n");
				exit(1);
			}
			self->program_state = FILE_VIEW;
		}
		break;
		default:
			break;
	}
}
