#pragma once
#include "utils.h"
#include <mupdf/fitz.h>
#include "arena.h"
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
	nicety_arena *document_arena;
	fz_context   *ctx;
	fz_document  *doc;
	size_t        total_pages;
} DocumentContext;

typedef struct
{
	DocumentContext *session;
	Page            *pages;
	size_t           number_of_pages;
	const char      *file_path;
} Document;

DocumentContext *document_context_init(nicety_arena *document_arena, const char *file_path);
void             document_context_destroy(DocumentContext *session);

int document_load_pages(DocumentContext *session, Application *app, size_t from, size_t till, const char *file_path,
                        Document *out);

void document_destroy(DocumentContext *session, Document *document);
