#include "document.h"
#include "document_internal.h"
#include "core.h"
#include "arena.h"
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static b8 arena_can_push_two_float_arrays(mem_arena *a, size_t n)
{
	u64 p   = a->pos;
	u64 a1  = ALIGN_UP_POW2(p, ARENA_ALIGN);
	u64 end = a1 + (u64) n * sizeof(float);
	a1      = ALIGN_UP_POW2(end, ARENA_ALIGN);
	end     = a1 + (u64) n * sizeof(float);
	return end <= a->capacity;
}

static void release_page_window(Document *doc, Page *pages, size_t texture_count)
{
	if (pages == NULL)
	{
		return;
	}
	doc_sdl_pages_destroy_textures(pages, texture_count);
	if (doc->session != NULL && doc->session->document_arena != NULL)
	{
		arena_pop_to(doc->session->document_arena, doc->arena_checkpoint_before_pages);
	}
}

/*
 * Scale pixmap to target thumb size (edge tw) and upload to slot; drops tpix on success path.
 * Returns 0 on success, 1 on unsupported format / error (caller may still need to drop tpix).
 */
static int sync_upload_thumb_scaled(fz_context *ctx, Application *app, fz_pixmap *pix, Page *slot)
{
	fz_pixmap *tpix = NULL;
	float      tw    = doc_raster_thumb_max_edge_px(NICETY_RENDER_LOW);
	float      th    = tw * (float) pix->h / (float) pix->w;

	tpix = fz_scale_pixmap(ctx, pix, 0.0f, 0.0f, tw, th, NULL);

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
			slot->thumb_bitmap     = thumb_bm;
			doc_sdl_page_init_thumb(slot, app);
			slot->thumb_bitmap.pixel_data = NULL;
			fz_drop_pixmap(ctx, tpix);
			return 0;
		}
		fprintf(stderr, "Unsupported thumbnail pixel format\n");
		fz_drop_pixmap(ctx, tpix);
		return 1;
	}
	return 0;
}

int document_measure_pages(DocumentContext *session, Document *doc)
{
	if (session == NULL || doc == NULL)
	{
		return 1;
	}

	mem_arena *arena = session->document_arena;

	if (doc->pages != NULL)
	{
		doc_sdl_pages_destroy_textures(doc->pages, doc->number_of_pages);
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
		doc->page_layout_w    = PUSH_ARRAY(arena, float, n);
		doc->page_layout_h    = PUSH_ARRAY(arena, float, n);
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

int document_load_page_window(DocumentContext *session, Application *app, size_t center_main, size_t center_sidebar, size_t radius_main,
                              size_t radius_sidebar, const char *file_path, Document *doc, NicetyRenderMode content_mode,
                              bool fill_width_mode, float content_inner_width)
{
	if (session == NULL || app == NULL || doc == NULL || file_path == NULL)
	{
		return 1;
	}

	mem_arena *arena = session->document_arena;

	if (session->total_pages == 0)
	{
		doc->session               = session;
		doc->file_path             = file_path;
		doc->number_of_pages       = 0;
		doc->pages                 = NULL;
		doc->window_center         = 0;
		doc->window_sidebar_center = 0;
		return 0;
	}

	if (center_main >= session->total_pages)
	{
		center_main = session->total_pages - 1;
	}
	if (center_sidebar >= session->total_pages)
	{
		center_sidebar = session->total_pages - 1;
	}

	size_t from;
	size_t till;
	doc_raster_page_window_union_range_dual(center_main, center_sidebar, radius_main, radius_sidebar, session->total_pages, &from, &till);

	if (from > till)
	{
		return 1;
	}

	if (doc->pages != NULL)
	{
		doc_sdl_pages_destroy_textures(doc->pages, doc->number_of_pages);
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
		size_t i       = from + k;
		bool   in_main = doc_raster_page_index_in_main_band(i, center_main, radius_main);

		pages[k].thumb_texture = NULL;
		memset(&pages[k].thumb_bitmap, 0, sizeof pages[k].thumb_bitmap);

		if (in_main)
		{
			page = fz_load_page(session->ctx, session->doc, (int) i);
			pix  = fz_new_pixmap_from_page(session->ctx, page,
			                               doc_raster_content_page_ctm(content_mode, fill_width_mode, content_inner_width, doc->page_layout_w[i], app),
			                               fz_device_rgb(session->ctx), 0);

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
			doc_sdl_page_init(&pages[k], app);
			pages[k].page_bitmap.pixel_data = NULL;

			if (pix->w > 0)
			{
				(void) sync_upload_thumb_scaled(session->ctx, app, pix, &pages[k]);
			}

			fz_drop_pixmap(session->ctx, pix);
			fz_drop_page(session->ctx, page);
			pix  = NULL;
			page = NULL;
		}
		else
		{
			float pd = document_app_pixel_density(app);

			pages[k].index = i;
			memset(&pages[k].page_bitmap, 0, sizeof pages[k].page_bitmap);
			pages[k].page_texture = NULL;

			page = fz_load_page(session->ctx, session->doc, (int) i);
			pix  = fz_new_pixmap_from_page(session->ctx, page, doc_raster_thumb_only_page_ctm(doc->page_layout_w[i], pd),
			                               fz_device_rgb(session->ctx), 0);
			if (pix == NULL)
			{
				fz_drop_page(session->ctx, page);
				release_page_window(doc, pages, k);
				return 1;
			}

			if (pix->w > 0)
			{
				(void) sync_upload_thumb_scaled(session->ctx, app, pix, &pages[k]);
			}

			fz_drop_pixmap(session->ctx, pix);
			fz_drop_page(session->ctx, page);
			pix  = NULL;
			page = NULL;
		}
	}

	doc->session                = session;
	doc->pages                  = pages;
	doc->number_of_pages        = count;
	doc->file_path              = file_path;
	doc->window_center          = center_main;
	doc->window_sidebar_center  = center_sidebar;
	doc->raster_content_inner_w = content_inner_width;
	doc->raster_fill_width_mode = fill_width_mode ? 1 : 0;
	return 0;
}
