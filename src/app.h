#pragma once
#include <SDL3/SDL.h>
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
	ViewMode                view_mode_prev;
	mem_arena              *document_arena;
	DocumentContext        *document_ctx;
	Document               *document;
	Clay_RenderCommandArray ui_commands;
	Clay_Vector2            sidebar_scroll_offset;
	Clay_Vector2            content_scroll_offset;
	bool                    sidebar_scroll_valid;
	bool                    content_scroll_valid;
	float                   content_viewport_width;
	float                   content_viewport_height;
	bool                    content_viewport_valid;
	float                   sidebar_viewport_height;
	bool                    sidebar_viewport_valid;
	bool                    sidebar_visible;

	/* Async page window raster (background thread); doc_load_token invalidates in-flight work on close/reopen. */
	u64                        doc_load_token;
	SDL_Thread                *page_loader_thread;
	SDL_Mutex                 *page_loader_mutex;
	SDL_Condition             *page_loader_cond;
	bool                       page_loader_shutdown;
	bool                       page_loader_have_request;
	u64                        page_loader_request_seq;
	u64                        page_loader_pending_doc_token;
	char                      *page_loader_path;
	float                     *page_loader_layout_w;
	float                     *page_loader_layout_h;
	size_t                     page_loader_total_pages;
	size_t                     page_loader_center;
	size_t                     page_loader_center_sidebar;
	size_t                     page_loader_radius_main;
	size_t                     page_loader_radius_sidebar;
	bool                       page_loader_fill;
	float                      page_loader_inner_w;
	float                      page_loader_pixel_density;
	NicetyPageWindowCpuResult *page_loader_completed;
	bool                       page_loader_have_completed;
} App;

void app_init(App *self);
void app_on_update(App *self, Application *core);
void app_on_render(App *self, void *renderer);
void app_on_event(App *self, Application *core, Event event, float deltaTime);
void app_destroy(App *self);
