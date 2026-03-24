#include "document.h"
#include "core.h"
#include <SDL3/SDL.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>

static void page_init_sdl(Page *page, Application *app)
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
	texture = SDL_CreateTextureFromSurface(app->renderer, surface);

	SDL_DestroySurface(surface);
	page->page_texture = texture;
}

static void page_init(Page *page, Application *core)
{
	page_init_sdl(page, core);
}

static int document_init_mupdf(Document **document_out, Application *core, const char *file_path)
{
	Document *document = malloc(sizeof(Document));
	if (!document)
	{
		return 1;
	}

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create context\n");
		free(document);
		return 1;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document");
		free(document);
		return 1;
	}

	size_t number_of_pages    = fz_count_pages(ctx, doc);
	document->file_path       = file_path;
	document->number_of_pages = number_of_pages;
	document->pages           = calloc(number_of_pages, sizeof *document->pages);

	fz_page   *page = NULL;
	fz_pixmap *pix  = NULL;
	u32        format;
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
			free(document->pages);
			free(document);
			return 1;
		}

		page_bitmap.width         = pix->w,
		page_bitmap.height        = pix->h,
		page_bitmap.format        = format,
		page_bitmap.pixel_data    = pix->samples,
		page_bitmap.rows_per_byte = pix->stride,

		document->pages[i].index       = i;
		document->pages[i].page_bitmap = page_bitmap;
		page_init(&document->pages[i], core);
		document->pages[i].page_bitmap.pixel_data = NULL;
		fz_drop_pixmap(ctx, pix);
		fz_drop_page(ctx, page);
	}

	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);

	*document_out = document;
	return 0;
}

int document_init(Document **document, Application *core, const char *file_path)
{
	return document_init_mupdf(document, core, file_path);
}

void document_destroy(Document *document)
{
	for (size_t i = 0; i < document->number_of_pages; i++)
	{
		SDL_DestroyTexture(document->pages[i].page_texture);
	}
	free(document->pages);
	SDL_free((void *) document->file_path);
	free(document);
}
