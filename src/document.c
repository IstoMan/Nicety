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

DocumentContext *document_context_init(mem_arena *document_arena, const char *file_path)
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

static float content_row_height(const Document *doc, size_t i, float inner_w, float viewport_h, bool fit_height_mode)
{
	float aspect = doc->page_layout_w[i] / doc->page_layout_h[i];
	float img_h;
	if (fit_height_mode && viewport_h > NICETY_DOC_FIT_HEIGHT_TOP_RESERVE)
	{
		img_h = viewport_h - NICETY_DOC_FIT_HEIGHT_TOP_RESERVE;
	}
	else
	{
		img_h = inner_w / aspect;
	}
	return img_h;
}

size_t document_page_at_scroll_y(const Document *doc, float scroll_y, float viewport_w, float viewport_h, bool fit_height_mode)
{
	if (doc == NULL || doc->session == NULL || doc->page_layout_w == NULL || doc->session->total_pages == 0)
	{
		return 0;
	}

	size_t total = doc->session->total_pages;
	float  inner_w = viewport_w - 2.0f * NICETY_DOC_CONTENT_PAD;
	if (inner_w < 1.0f)
	{
		inner_w = 1.0f;
	}

	/* Clay scroll offsets are <= 0 when scrolled down; content Y grows downward from the top. */
	float y_target;
	if (viewport_h <= 0.0f)
	{
		y_target = -scroll_y;
	}
	else
	{
		y_target = -scroll_y + viewport_h * 0.5f;
	}

	float y = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < total; i++)
	{
		float rh = content_row_height(doc, i, inner_w, viewport_h, fit_height_mode);
		if (y_target < y + rh)
		{
			return i;
		}
		y += rh;
		if (i + 1 < total)
		{
			y += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}
	return total - 1;
}

Page *document_page_for_index(const Document *doc, size_t page_index)
{
	if (doc == NULL || doc->pages == NULL)
	{
		return NULL;
	}
	for (size_t k = 0; k < doc->number_of_pages; k++)
	{
		if (doc->pages[k].index == page_index)
		{
			return &doc->pages[k];
		}
	}
	return NULL;
}

int document_measure_pages(DocumentContext *session, Document *doc)
{
	if (session == NULL || doc == NULL)
	{
		return 1;
	}

	free(doc->page_layout_w);
	free(doc->page_layout_h);
	doc->page_layout_w = NULL;
	doc->page_layout_h = NULL;

	if (session->total_pages == 0)
	{
		return 0;
	}

	size_t n         = session->total_pages;
	doc->page_layout_w = malloc(n * sizeof(float));
	doc->page_layout_h = malloc(n * sizeof(float));
	if (doc->page_layout_w == NULL || doc->page_layout_h == NULL)
	{
		free(doc->page_layout_w);
		free(doc->page_layout_h);
		doc->page_layout_w = NULL;
		doc->page_layout_h = NULL;
		return 1;
	}

	for (size_t i = 0; i < n; i++)
	{
		fz_page *page = fz_load_page(session->ctx, session->doc, (int) i);
		fz_rect  bounds = fz_bound_page(session->ctx, page);
		fz_irect ibox = fz_round_rect(bounds);
		fz_drop_page(session->ctx, page);

		float w = (float) (ibox.x1 - ibox.x0);
		float h = (float) (ibox.y1 - ibox.y0);
		if (w < 1.0f)
		{
			w = 1.0f;
		}
		if (h < 1.0f)
		{
			h = 1.0f;
		}
		doc->page_layout_w[i] = w;
		doc->page_layout_h[i] = h;
	}

	return 0;
}

static void free_rendered_page_textures(Page *pages, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		SDL_DestroyTexture(pages[i].page_texture);
	}
	free(pages);
}

int document_load_page_window(DocumentContext *session, Application *app, size_t center, size_t radius, const char *file_path,
                              Document *doc)
{
	if (session == NULL || app == NULL || doc == NULL || file_path == NULL)
	{
		return 1;
	}

	if (session->total_pages == 0)
	{
		doc->session         = session;
		doc->file_path       = file_path;
		doc->number_of_pages = 0;
		doc->pages           = NULL;
		doc->window_center   = 0;
		return 0;
	}

	if (center >= session->total_pages)
	{
		center = session->total_pages - 1;
	}

	size_t from = center > radius ? center - radius : 0;
	size_t till = center + radius;
	if (till >= session->total_pages)
	{
		till = session->total_pages - 1;
	}

	if (from > till)
	{
		return 1;
	}

	if (doc->pages != NULL)
	{
		free_rendered_page_textures(doc->pages, doc->number_of_pages);
		doc->pages           = NULL;
		doc->number_of_pages = 0;
	}

	size_t count = till - from + 1;
	Page  *pages = calloc(count, sizeof *pages);
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

		pages[k].index       = i;
		pages[k].page_bitmap = page_bitmap;
		page_init(&pages[k], app);
		pages[k].page_bitmap.pixel_data = NULL;

		fz_drop_pixmap(session->ctx, pix);
		fz_drop_page(session->ctx, page);
		pix  = NULL;
		page = NULL;
	}

	doc->session         = session;
	doc->pages           = pages;
	doc->number_of_pages = count;
	doc->file_path       = file_path;
	doc->window_center   = center;
	return 0;
}

void document_destroy(DocumentContext *session, Document *document)
{
	(void) session;
	if (document == NULL)
	{
		return;
	}
	if (document->pages != NULL)
	{
		for (size_t i = 0; i < document->number_of_pages; i++)
		{
			SDL_DestroyTexture(document->pages[i].page_texture);
		}
		free(document->pages);
	}
	free(document->page_layout_w);
	free(document->page_layout_h);
	SDL_free((void *) document->file_path);
	free(document);
}
