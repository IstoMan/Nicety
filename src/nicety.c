#include "nicety.h"
#include "application_core.h"
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <mupdf/fitz.h>
#include <stddef.h>
#include <stdlib.h>

int init_document_mupdf(const char *file_path, Document *document, AppCore core)
{
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create context\n");
		return 1;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document");
		return 1;
	}

	size_t number_of_pages    = fz_count_pages(ctx, doc);
	document->file_path       = file_path;
	document->number_of_pages = number_of_pages;
	document->pages           = calloc(number_of_pages, sizeof *document->pages);

	fz_page   *page = NULL;
	fz_pixmap *pix  = NULL;
	uint32_t   format;
	Bitmap     page_bitmap;

	for (size_t i = 0; i < number_of_pages; i++)
	{
		page = fz_load_page(ctx, doc, i);
		pix  = fz_new_pixmap_from_page(ctx, page, fz_identity, fz_device_rgb(ctx), 0);

		if (pix->n == 3)
		{
			format = COLOR_FORMAT_RGB;        // RGB
		}
		else if (pix->n == 4)
		{
			format = COLOR_FORMAT_RGBA;        // RGBA
		}
		else
		{
			fprintf(stderr, "Unsupported pixel format\n");
			return 1;
		}

		page_bitmap.width         = pix->w,
		page_bitmap.height        = pix->h,
		page_bitmap.format        = format,
		page_bitmap.pixel_data    = pix->samples,
		page_bitmap.rows_per_byte = pix->stride,

		document->pages[i].index       = i;
		document->pages[i].page_bitmap = page_bitmap;
		init_page_texture(&document->pages[i], core);
	}

	fz_drop_page(ctx, page);
	fz_drop_pixmap(ctx, pix);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}

void init_page_texture_sdl(Page *page, AppCore app)
{
	SDL_Surface *surface = NULL;
	SDL_Texture *texture = NULL;
	int          format;

	if (page->page_bitmap.format == COLOR_FORMAT_RGB)
	{
		format = SDL_PIXELFORMAT_RGB24;
	}
	else
	{
		format = SDL_PIXELFORMAT_RGBA32;
	}
	surface = SDL_CreateSurfaceFrom(page->page_bitmap.width, page->page_bitmap.height, format, page->page_bitmap.pixel_data, page->page_bitmap.rows_per_byte);
	texture = SDL_CreateTextureFromSurface(app.renderer, surface);

	SDL_DestroySurface(surface);
	page->page_texture = texture;
}

int init_document(const char *file_path, Document *document, AppCore core)
{
	return init_document_mupdf(file_path, document, core);
}

void init_page_texture(Page *page, AppCore core)
{
	init_page_texture_sdl(page, core);
}
