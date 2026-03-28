#include "document.h"
#include "core.h"
#include <SDL3/SDL.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float (*document_visible_row_height_fn)(const Document *doc, size_t i, void *ctx);

static int sdl_pixel_format(PixelFormat fmt)
{
	return fmt == COLOR_FORMAT_RGB ? SDL_PIXELFORMAT_RGB24 : SDL_PIXELFORMAT_RGBA32;
}

/* Upload MuPDF bitmap pixels to an SDL texture (resources/Clay.md: imageData for CLAY image elements). */
static void page_upload_texture(Application *app, const Bitmap *bm, void **out_tex)
{
	SDL_Surface *surface;
	SDL_Texture *texture;
	int          format = sdl_pixel_format(bm->format);

	surface = SDL_CreateSurfaceFrom(bm->width, bm->height, format, bm->pixel_data, bm->rows_per_byte);
	texture = SDL_CreateTextureFromSurface(app->renderer, surface);
	SDL_DestroySurface(surface);
	if (texture != NULL)
	{
		SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
	}
	*out_tex = texture;
}

static void page_init(Page *page, Application *core)
{
	page_upload_texture(core, &page->page_bitmap, &page->page_texture);
}

static void page_init_thumb(Page *page, Application *app)
{
	if (page->thumb_bitmap.width == 0 || page->thumb_bitmap.pixel_data == NULL)
	{
		return;
	}
	page_upload_texture(app, &page->thumb_bitmap, &page->thumb_texture);
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

	size_t total   = doc->session->total_pages;
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

bool document_remap_scroll_y_for_view_mode(const Document *doc, float scroll_y_in, float viewport_w, float viewport_h,
                                           bool from_fit_height, bool to_fit_height, float *scroll_y_out)
{
	size_t n;
	float  inner_w;
	float  y_target;
	size_t page;
	float  y_start_old;
	float  rh_old;
	float  frac;
	float  y_start_new;
	float  rh_new;
	float  y_target_new;
	float  scroll_out;
	float  total_h;
	float  max_neg;

	if (scroll_y_out == NULL || doc == NULL || doc->session == NULL || doc->page_layout_w == NULL || doc->session->total_pages == 0)
	{
		return false;
	}

	if (from_fit_height == to_fit_height)
	{
		*scroll_y_out = scroll_y_in;
		return true;
	}

	n = doc->session->total_pages;

	inner_w = viewport_w - 2.0f * NICETY_DOC_CONTENT_PAD;
	if (inner_w < 1.0f)
	{
		inner_w = 1.0f;
	}

	page = document_page_at_scroll_y(doc, scroll_y_in, viewport_w, viewport_h, from_fit_height);

	if (viewport_h <= 0.0f)
	{
		y_target = -scroll_y_in;
	}
	else
	{
		y_target = -scroll_y_in + viewport_h * 0.5f;
	}

	y_start_old = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < page; i++)
	{
		y_start_old += content_row_height(doc, i, inner_w, viewport_h, from_fit_height);
		if (i + 1 < n)
		{
			y_start_old += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}

	rh_old = content_row_height(doc, page, inner_w, viewport_h, from_fit_height);
	frac   = 0.0f;
	if (rh_old > 1e-6f)
	{
		frac = (y_target - y_start_old) / rh_old;
		if (frac < 0.0f)
		{
			frac = 0.0f;
		}
		else if (frac > 1.0f)
		{
			frac = 1.0f;
		}
	}

	y_start_new = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < page; i++)
	{
		y_start_new += content_row_height(doc, i, inner_w, viewport_h, to_fit_height);
		if (i + 1 < n)
		{
			y_start_new += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}

	rh_new         = content_row_height(doc, page, inner_w, viewport_h, to_fit_height);
	y_target_new   = y_start_new + frac * rh_new;

	if (viewport_h <= 0.0f)
	{
		scroll_out = -y_target_new;
	}
	else
	{
		scroll_out = -(y_target_new - viewport_h * 0.5f);
	}

	total_h = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < n; i++)
	{
		total_h += content_row_height(doc, i, inner_w, viewport_h, to_fit_height);
		if (i + 1 < n)
		{
			total_h += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}
	total_h += NICETY_DOC_CONTENT_PAD;

	if (viewport_h > 1.0f && total_h <= viewport_h)
	{
		*scroll_y_out = 0.0f;
		return true;
	}

	max_neg = 0.0f;
	if (viewport_h > 1.0f && total_h > viewport_h)
	{
		max_neg = -(total_h - viewport_h);
	}

	if (scroll_out > 0.0f)
	{
		scroll_out = 0.0f;
	}
	if (scroll_out < max_neg)
	{
		scroll_out = max_neg;
	}

	*scroll_y_out = scroll_out;
	return true;
}

static float document_sidebar_row_height(const Document *doc, size_t i, float sb_inner)
{
	float layout_aspect = doc->page_layout_w[i] / doc->page_layout_h[i];
	return sb_inner / layout_aspect;
}

static float visible_row_h_sidebar(const Document *doc, size_t i, void *ctx)
{
	return document_sidebar_row_height(doc, i, *(float *) ctx);
}

typedef struct
{
	float inner_w;
	float viewport_h;
	bool  fit_height_mode;
} VisibleContentRowCtx;

static float visible_row_h_content(const Document *doc, size_t i, void *ctx)
{
	VisibleContentRowCtx *c = ctx;
	return content_row_height(doc, i, c->inner_w, c->viewport_h, c->fit_height_mode);
}

/*
 * Virtualized scroll list: visible index range + spacers so total height matches full layout
 * (Clay.md scrolling — clip + childOffset; spacers preserve scroll extent).
 */
static void document_visible_range_impl(const Document *doc, float scroll_y, float viewport_h, float edge_pad, float inter_gap,
                                        document_visible_row_height_fn row_h, void *row_ctx, size_t *out_lo, size_t *out_hi,
                                        float *out_spacer_top, float *out_spacer_bottom)
{
	size_t n;

	if (doc == NULL || doc->page_layout_w == NULL || doc->session == NULL)
	{
		*out_lo = *out_hi = 0;
		*out_spacer_top = *out_spacer_bottom = 0.0f;
		return;
	}

	n = doc->session->total_pages;
	if (n == 0)
	{
		*out_lo = *out_hi = 0;
		*out_spacer_top = *out_spacer_bottom = 0.0f;
		return;
	}

	if (viewport_h <= 1.0f)
	{
		*out_lo = 0;
		*out_hi = n - 1;
		*out_spacer_top = *out_spacer_bottom = 0.0f;
		return;
	}

	{
		float   top = -scroll_y;
		float   bot = top + viewport_h;
		float   o   = NICETY_UI_VIRTUAL_OVERSCAN_PX;
		float   y   = edge_pad;
		size_t  lo  = n;
		size_t  hi  = 0;
		size_t  i;

		for (i = 0; i < n; i++)
		{
			float h     = row_h(doc, i, row_ctx);
			float y_next = y + h;

			if (y_next > top - o && y < bot + o)
			{
				if (lo == n)
				{
					lo = i;
				}
				hi = i;
			}
			y = y_next;
			if (i + 1 < n)
			{
				y += inter_gap;
			}
		}

		if (lo == n)
		{
			lo = 0;
			hi = n - 1;
		}

		*out_lo = lo;
		*out_hi = hi;

		{
			float y_top_row_lo = edge_pad;
			for (size_t j = 0; j < lo; j++)
			{
				y_top_row_lo += row_h(doc, j, row_ctx);
				if (j + 1 < n)
				{
					y_top_row_lo += inter_gap;
				}
			}
			*out_spacer_top = y_top_row_lo - edge_pad;
		}

		{
			float total_h = edge_pad;
			for (size_t j = 0; j < n; j++)
			{
				total_h += row_h(doc, j, row_ctx);
				if (j + 1 < n)
				{
					total_h += inter_gap;
				}
			}
			total_h += edge_pad;

			{
				float y_after_hi = edge_pad;
				for (size_t j = 0; j <= hi; j++)
				{
					if (j > 0)
					{
						y_after_hi += inter_gap;
					}
					y_after_hi += row_h(doc, j, row_ctx);
				}
				*out_spacer_bottom = total_h - y_after_hi;
			}
		}
	}
}

void document_visible_sidebar_range(const Document *doc, float sb_inner, float scroll_y, float viewport_h, size_t *out_lo,
                                    size_t *out_hi, float *out_spacer_top, float *out_spacer_bottom)
{
	document_visible_range_impl(doc, scroll_y, viewport_h, NICETY_DOC_SIDEBAR_PAD, NICETY_DOC_SIDEBAR_INTER_GAP, visible_row_h_sidebar,
	                            &sb_inner, out_lo, out_hi, out_spacer_top, out_spacer_bottom);
}

void document_visible_content_range(const Document *doc, float content_inner_w, float scroll_y, float viewport_w, float viewport_h,
                                    bool fit_height_mode, size_t *out_lo, size_t *out_hi, float *out_spacer_top,
                                    float *out_spacer_bottom)
{
	VisibleContentRowCtx c = {.inner_w = content_inner_w, .viewport_h = viewport_h, .fit_height_mode = fit_height_mode};
	(void) viewport_w;
	document_visible_range_impl(doc, scroll_y, viewport_h, NICETY_DOC_CONTENT_PAD, NICETY_DOC_CONTENT_INTER_PAGE_GAP, visible_row_h_content,
	                            &c, out_lo, out_hi, out_spacer_top, out_spacer_bottom);
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

static b8 arena_can_push_two_float_arrays(mem_arena *a, size_t n)
{
	u64 p   = a->pos;
	u64 a1  = ALIGN_UP_POW2(p, ARENA_ALIGN);
	u64 end = a1 + (u64) n * sizeof(float);
	a1      = ALIGN_UP_POW2(end, ARENA_ALIGN);
	end     = a1 + (u64) n * sizeof(float);
	return end <= a->capacity;
}

static void pages_destroy_textures(Page *pages, size_t count);

int document_measure_pages(DocumentContext *session, Document *doc)
{
	if (session == NULL || doc == NULL)
	{
		return 1;
	}

	mem_arena *arena = session->document_arena;

	if (doc->pages != NULL)
	{
		pages_destroy_textures(doc->pages, doc->number_of_pages);
		doc->pages           = NULL;
		doc->number_of_pages = 0;
	}
	arena_pop_to(arena, doc->arena_checkpoint_after_document);
	if (doc->page_layout_heap)
	{
		free(doc->page_layout_w);
		free(doc->page_layout_h);
		doc->page_layout_heap = false;
	}
	doc->page_layout_w = NULL;
	doc->page_layout_h = NULL;

	if (session->total_pages == 0)
	{
		doc->arena_checkpoint_before_pages = doc->arena_checkpoint_after_document;
		return 0;
	}

	size_t n = session->total_pages;

	if (arena_can_push_two_float_arrays(arena, n))
	{
		doc->page_layout_w = PUSH_ARRAY(arena, float, n);
		doc->page_layout_h = PUSH_ARRAY(arena, float, n);
		doc->page_layout_heap = false;
	}
	else
	{
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
		doc->page_layout_heap = true;
	}

	doc->arena_checkpoint_before_pages = arena->pos;

	for (size_t i = 0; i < n; i++)
	{
		fz_page *page   = fz_load_page(session->ctx, session->doc, (int) i);
		fz_rect  bounds = fz_bound_page(session->ctx, page);
		fz_irect ibox   = fz_round_rect(bounds);
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

static void pages_destroy_textures(Page *pages, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (pages[i].thumb_texture != NULL)
		{
			SDL_DestroyTexture(pages[i].thumb_texture);
		}
		SDL_DestroyTexture(pages[i].page_texture);
	}
}

static void release_page_window(Document *doc, Page *pages, size_t texture_count)
{
	if (pages == NULL)
	{
		return;
	}
	pages_destroy_textures(pages, texture_count);
	if (doc->session != NULL && doc->session->document_arena != NULL)
	{
		arena_pop_to(doc->session->document_arena, doc->arena_checkpoint_before_pages);
	}
}

int document_load_page_window(DocumentContext *session, Application *app, size_t center, size_t radius, const char *file_path,
                              Document *doc)
{
	if (session == NULL || app == NULL || doc == NULL || file_path == NULL)
	{
		return 1;
	}

	mem_arena *arena = session->document_arena;

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
		pages_destroy_textures(doc->pages, doc->number_of_pages);
		arena_pop_to(arena, doc->arena_checkpoint_before_pages);
		doc->pages           = NULL;
		doc->number_of_pages = 0;
	}

	size_t count = till - from + 1;
	if (!arena_can_push(arena, (u64) count * sizeof(Page)))
	{
		return 1;
	}
	Page *pages = PUSH_ARRAY(arena, Page, count);

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
			release_page_window(doc, pages, k);
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

		pages[k].thumb_texture = NULL;
		memset(&pages[k].thumb_bitmap, 0, sizeof pages[k].thumb_bitmap);
		if (pix->w > 0)
		{
			float    tw   = NICETY_SIDEBAR_THUMB_MAX_PX;
			float    th   = tw * (float) pix->h / (float) pix->w;
			fz_pixmap *tpix = fz_scale_pixmap(session->ctx, pix, 0.0f, 0.0f, tw, th, NULL);

			if (tpix != NULL)
			{
				if (tpix->n == 3 || tpix->n == 4)
				{
					Bitmap thumb_bm;
					u32    tf = tpix->n == 3 ? COLOR_FORMAT_RGB : COLOR_FORMAT_RGBA;

					thumb_bm.width         = tpix->w;
					thumb_bm.height        = tpix->h;
					thumb_bm.format        = tf;
					thumb_bm.pixel_data    = tpix->samples;
					thumb_bm.rows_per_byte = tpix->stride;
					pages[k].thumb_bitmap  = thumb_bm;
					page_init_thumb(&pages[k], app);
					pages[k].thumb_bitmap.pixel_data = NULL;
					fz_drop_pixmap(session->ctx, tpix);
				}
				else
				{
					fprintf(stderr, "Unsupported thumbnail pixel format\n");
					fz_drop_pixmap(session->ctx, tpix);
				}
			}
		}

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
		pages_destroy_textures(document->pages, document->number_of_pages);
	}
	if (document->page_layout_heap)
	{
		free(document->page_layout_w);
		free(document->page_layout_h);
	}
	SDL_free((void *) document->file_path);
}
