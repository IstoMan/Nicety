#pragma once
#include <stdint.h>
#include "clay.h"
#include <stddef.h>

typedef struct AppCore AppCore;

typedef enum
{
	COLOR_FORMAT_BGkRA,        // 8-bit per channel, premultiplied alpha
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

typedef struct
{
	Clay_Vector2 scroll_state;
} App;

void init_page_texture(Page *page, AppCore *core);
int  init_document(const char *file_path, Document *document, AppCore *core);
