#include "document.h"
#include "core.h"
#include <SDL3/SDL.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

DocumentContext *document_context_init(nicety_arena *document_arena, const char *file_path)
{
	DocumentContext *session = PUSH_STRUCT(document_arena, DocumentContext);
	session->document_arena  = document_arena;

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create MuPDF context\n");
		arena_clear(document_arena);
		return NULL;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document\n");
		fz_drop_context(ctx);
		arena_clear(document_arena);
		return NULL;
	}

	int np = fz_count_pages(ctx, doc);
	if (np < 0)
	{
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		arena_clear(document_arena);
		return NULL;
	}

	session->ctx         = ctx;
	session->doc         = doc;
	session->total_pages = (size_t) np;

	return session;
}

void document_context_destroy(DocumentContext *session)
{
	if (session == NULL)
	{
		return;
	}
	fz_drop_document(session->ctx, session->doc);
	fz_drop_context(session->ctx);
	arena_clear(session->document_arena);
}

static void free_rendered_page_textures(Page *pages, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		SDL_DestroyTexture(pages[i].page_texture);
	}
	free(pages);
}

int document_load_pages(DocumentContext *session, Application *app, size_t from, size_t till, const char *file_path,
                        Document *out)
{
	if (session == NULL || app == NULL || out == NULL || file_path == NULL)
	{
		return 1;
	}

	memset(out, 0, sizeof *out);

	if (session->total_pages == 0)
	{
		out->session         = session;
		out->file_path       = file_path;
		out->number_of_pages = 0;
		out->pages           = NULL;
		return 0;
	}

	if (from > till || from >= session->total_pages || till >= session->total_pages)
	{
		return 1;
	}

	size_t       count = till - from + 1;
	Page        *pages = calloc(count, sizeof *pages);
	if (pages == NULL)
	{
		return 1;
	}

	fz_page   *page = NULL;
	fz_pixmap *pix  = NULL;

	for (size_t k = 0; k < count; k++)
	{
		size_t i = from + k;
		page     = fz_load_page(session->ctx, session->doc, (int) i);
		pix      = fz_new_pixmap_from_page(session->ctx, page, fz_identity, fz_device_rgb(session->ctx), 0);

		u32    format;
		Bitmap page_bitmap;

		if (pix->n == 3)
		{
			format = COLOR_FORMAT_RGB;
		}
		else if (pix->n == 4)
		{
			format = COLOR_FORMAT_RGBA;
		}
		else
		{
			fprintf(stderr, "Unsupported pixel format\n");
			fz_drop_pixmap(session->ctx, pix);
			fz_drop_page(session->ctx, page);
			free_rendered_page_textures(pages, k);
			return 1;
		}

		page_bitmap.width         = pix->w;
		page_bitmap.height        = pix->h;
		page_bitmap.format        = format;
		page_bitmap.pixel_data    = pix->samples;
		page_bitmap.rows_per_byte = pix->stride;

		pages[k].index         = i;
		pages[k].page_bitmap   = page_bitmap;
		page_init(&pages[k], app);
		pages[k].page_bitmap.pixel_data = NULL;

		fz_drop_pixmap(session->ctx, pix);
		fz_drop_page(session->ctx, page);
		pix  = NULL;
		page = NULL;
	}

	out->session         = session;
	out->pages           = pages;
	out->number_of_pages = count;
	out->file_path       = file_path;
	return 0;
}

void document_destroy(DocumentContext *session, Document *document)
{
	(void) session;
	if (document == NULL)
	{
		return;
	}
	for (size_t i = 0; i < document->number_of_pages; i++)
	{
		SDL_DestroyTexture(document->pages[i].page_texture);
	}
	free(document->pages);
	SDL_free((void *) document->file_path);
	free(document);
}
