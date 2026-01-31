#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include "clay.h"
#include "core.h"
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
	uint32_t    width, height;
	uint32_t    rows_per_byte;
	PixelFormat format;
	uint8_t    *pixel_data;
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

typedef struct App
{
	Clay_Vector2            scroll_state;
	size_t                  sensitivity;
	AppState                program_state;
	Document               *document;
	Clay_RenderCommandArray ui_commands;
} App;

void app_init(App *self);
void app_on_update(App *self);
void app_on_render(App *self, void *renderer);
void app_on_event(App *self, Application *core, Event event, float deltaTime);
void app_destroy(App *self);

int  document_init(Document **document, Application *core, const char *file_path);
void document_destroy(Document *document);
