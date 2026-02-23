#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include "clay.h"
#include "core.h"
#include "utils.h"
#include <stddef.h>

typedef SDL_Event Event;

typedef enum
{
	COLOR_FORMAT_BGRA,        // 8-bit per channel, premultiplied alpha
	COLOR_FORMAT_RGBA,
	COLOR_FORMAT_RGB,
	COLOR_FORMAT_GRAY8
} PixelFormat;

typedef struct
{
	u32         width, height;
	u32         rows_per_byte;
	PixelFormat format;
	u8         *pixel_data;
} Bitmap;

typedef struct
{
	Bitmap page_bitmap;
	void  *page_texture;        // texture data for any renderer (eg. SDL3, Raylib)
	size_t index;
} Page;

typedef struct
{
	Page       *pages;
	size_t      number_of_pages;
	const char *file_path;
} Document;

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

int  document_init(Document **document, Application *core, const char *file_path);
void document_destroy(Document *document);
