#pragma once
#include "utils.h"
#include <stddef.h>

typedef struct Application Application;

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

int  document_init(Document **document, Application *core, const char *file_path);
void document_destroy(Document *document);
