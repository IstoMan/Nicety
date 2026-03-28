#include "document.h"
#include "document_internal.h"
#include "arena.h"
#include "core.h"
#include <mupdf/fitz.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nicety_page_window_cpu_result_free(NicetyPageWindowCpuResult *r)
{
	size_t i;

	if (r == NULL)
	{
		return;
	}
	if (r->slots != NULL)
	{
		for (i = 0; i < r->count; i++)
		{
			if (r->slots[i].page.samples != NULL)
			{
				free(r->slots[i].page.samples);
			}
			if (r->slots[i].thumb.samples != NULL)
			{
				free(r->slots[i].thumb.samples);
			}
		}
		free(r->slots);
	}
	free(r);
}

static int nicety_copy_pixmap_to_cpu(fz_pixmap *pix, NicetyCpuBitmap *out)
{
	size_t nbytes;

	if (pix->n != 3 && pix->n != 4)
	{
		return 1;
	}
	out->format  = pix->n == 3 ? COLOR_FORMAT_RGB : COLOR_FORMAT_RGBA;
	out->width   = (u32) pix->w;
	out->height  = (u32) pix->h;
	out->stride  = (u32) pix->stride;
	nbytes       = (size_t) pix->stride * (size_t) pix->h;
	out->samples = (u8 *) malloc(nbytes);
	if (out->samples == NULL)
	{
		return 1;
	}
	memcpy(out->samples, pix->samples, nbytes);
	return 0;
}

static void nicety_cpu_bitmap_clear(NicetyCpuBitmap *b)
{
	b->samples = NULL;
	b->width = b->height = b->stride = 0;
	b->format                        = COLOR_FORMAT_RGB;
}

/*
 * If pix->w > 0, scale to sidebar thumb size and copy to out_thumb; frees intermediate pixmap.
 * Returns 0 on success or nothing to do; non-zero on allocation failure (caller cleans up ctx/doc).
 */
static int cpu_scale_thumb_and_copy(fz_context *ctx, fz_pixmap *pix, NicetyCpuBitmap *out_thumb)
{
	fz_pixmap *tpix = NULL;
	float      tw;
	float      th;

	if (pix->w <= 0)
	{
		return 0;
	}

	tw   = doc_raster_thumb_max_edge_px(NICETY_RENDER_LOW);
	th   = tw * (float) pix->h / (float) pix->w;
	tpix = NULL;
	fz_try(ctx)
	{
		tpix = fz_scale_pixmap(ctx, pix, 0.0f, 0.0f, tw, th, NULL);
	}
	fz_catch(ctx)
	{
		tpix = NULL;
	}
	if (tpix == NULL)
	{
		return 0;
	}
	if (tpix->n == 3 || tpix->n == 4)
	{
		if (nicety_copy_pixmap_to_cpu(tpix, out_thumb) != 0)
		{
			fz_drop_pixmap(ctx, tpix);
			return 1;
		}
	}
	fz_drop_pixmap(ctx, tpix);
	return 0;
}

int document_raster_page_window_to_cpu(const char *file_path, const float *page_layout_w, size_t total_pages, size_t center_main,
                                       size_t center_sidebar, size_t radius_main, size_t radius_sidebar, NicetyRenderMode content_mode,
                                       bool fill_width_mode, float content_inner_width, float pixel_density, u64 doc_token,
                                       u64 request_seq, NicetyPageWindowCpuResult **out)
{
	fz_context                *ctx    = NULL;
	fz_document               *fzdoc  = NULL;
	NicetyPageWindowCpuResult *result = NULL;
	NicetyCpuPageSlot         *slots  = NULL;
	size_t                     from;
	size_t                     till;
	size_t                     count;
	size_t                     k;

	if (out == NULL || file_path == NULL || page_layout_w == NULL)
	{
		return 1;
	}
	*out = NULL;

	if (total_pages == 0)
	{
		result = (NicetyPageWindowCpuResult *) calloc(1, sizeof *result);
		if (result == NULL)
		{
			return 1;
		}
		result->doc_token              = doc_token;
		result->request_seq            = request_seq;
		result->center                 = 0;
		result->center_sidebar         = 0;
		result->from_index             = 0;
		result->count                  = 0;
		result->raster_content_inner_w = content_inner_width;
		result->raster_fill_width_mode = fill_width_mode ? 1 : 0;
		result->slots                  = NULL;
		*out                           = result;
		return 0;
	}

	if (center_main >= total_pages)
	{
		center_main = total_pages - 1;
	}
	if (center_sidebar >= total_pages)
	{
		center_sidebar = total_pages - 1;
	}
	doc_raster_page_window_union_range_dual(center_main, center_sidebar, radius_main, radius_sidebar, total_pages, &from, &till);
	if (from > till)
	{
		return 1;
	}
	count = till - from + 1;

	result = (NicetyPageWindowCpuResult *) calloc(1, sizeof *result);
	slots  = (NicetyCpuPageSlot *) calloc(count, sizeof *slots);
	if (result == NULL || slots == NULL)
	{
		free(result);
		free(slots);
		return 1;
	}
	result->slots                  = slots;
	result->doc_token              = doc_token;
	result->request_seq            = request_seq;
	result->center                 = center_main;
	result->center_sidebar         = center_sidebar;
	result->from_index             = from;
	result->count                  = count;
	result->raster_content_inner_w = content_inner_width;
	result->raster_fill_width_mode = fill_width_mode ? 1 : 0;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (ctx == NULL)
	{
		nicety_page_window_cpu_result_free(result);
		return 1;
	}
	fz_register_document_handlers(ctx);
	fz_try(ctx)
	{
		fzdoc = fz_open_document(ctx, file_path);
	}
	fz_catch(ctx)
	{
		fzdoc = NULL;
	}
	if (fzdoc == NULL)
	{
		fz_drop_context(ctx);
		nicety_page_window_cpu_result_free(result);
		return 1;
	}

	for (k = 0; k < count; k++)
	{
		size_t     i;
		fz_page   *page = NULL;
		fz_pixmap *pix  = NULL;
		bool       in_main;

		i       = from + k;
		in_main = doc_raster_page_index_in_main_band(i, center_main, radius_main);

		nicety_cpu_bitmap_clear(&slots[k].page);
		nicety_cpu_bitmap_clear(&slots[k].thumb);
		slots[k].index = i;

		if (in_main)
		{
			fz_try(ctx)
			{
				page = fz_load_page(ctx, fzdoc, (int) i);
				pix  = fz_new_pixmap_from_page(ctx, page,
				                               doc_raster_content_page_ctm_density(content_mode, fill_width_mode, content_inner_width,
				                                                                   page_layout_w[i], pixel_density),
				                               fz_device_rgb(ctx), 0);
			}
			fz_catch(ctx)
			{
				page = NULL;
				pix  = NULL;
			}
			if (page == NULL || pix == NULL)
			{
				if (page != NULL)
				{
					fz_drop_page(ctx, page);
				}
				if (pix != NULL)
				{
					fz_drop_pixmap(ctx, pix);
				}
				fz_drop_document(ctx, fzdoc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (nicety_copy_pixmap_to_cpu(pix, &slots[k].page) != 0)
			{
				fz_drop_pixmap(ctx, pix);
				fz_drop_page(ctx, page);
				fz_drop_document(ctx, fzdoc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (cpu_scale_thumb_and_copy(ctx, pix, &slots[k].thumb) != 0)
			{
				fz_drop_pixmap(ctx, pix);
				fz_drop_page(ctx, page);
				fz_drop_document(ctx, fzdoc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			fz_drop_pixmap(ctx, pix);
			fz_drop_page(ctx, page);
		}
		else
		{
			fz_try(ctx)
			{
				page = fz_load_page(ctx, fzdoc, (int) i);
				pix  = fz_new_pixmap_from_page(ctx, page, doc_raster_thumb_only_page_ctm(page_layout_w[i], pixel_density),
				                               fz_device_rgb(ctx), 0);
			}
			fz_catch(ctx)
			{
				page = NULL;
				pix  = NULL;
			}
			if (page == NULL || pix == NULL)
			{
				if (page != NULL)
				{
					fz_drop_page(ctx, page);
				}
				if (pix != NULL)
				{
					fz_drop_pixmap(ctx, pix);
				}
				fz_drop_document(ctx, fzdoc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (cpu_scale_thumb_and_copy(ctx, pix, &slots[k].thumb) != 0)
			{
				fz_drop_pixmap(ctx, pix);
				fz_drop_page(ctx, page);
				fz_drop_document(ctx, fzdoc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			fz_drop_pixmap(ctx, pix);
			fz_drop_page(ctx, page);
		}
	}

	fz_drop_document(ctx, fzdoc);
	fz_drop_context(ctx);
	*out = result;
	return 0;
}

int document_commit_page_window_from_cpu(Application *app, DocumentContext *session, Document *doc, const char *file_path,
                                          NicetyPageWindowCpuResult *cpu, u64 expected_doc_token)
{
	mem_arena *arena;
	Page      *pages;
	size_t     k;

	if (app == NULL || session == NULL || doc == NULL || cpu == NULL)
	{
		if (cpu != NULL)
		{
			nicety_page_window_cpu_result_free(cpu);
		}
		return 2;
	}

	if (cpu->doc_token != expected_doc_token)
	{
		nicety_page_window_cpu_result_free(cpu);
		return 1;
	}
	if (doc->file_path == NULL || file_path == NULL || strcmp(doc->file_path, file_path) != 0)
	{
		nicety_page_window_cpu_result_free(cpu);
		return 1;
	}

	if (session->total_pages == 0)
	{
		if (doc->pages != NULL)
		{
			doc_sdl_pages_destroy_textures(doc->pages, doc->number_of_pages);
			arena_pop_to(session->document_arena, doc->arena_checkpoint_before_pages);
			doc->pages           = NULL;
			doc->number_of_pages = 0;
		}
		doc->session                = session;
		doc->file_path              = file_path;
		doc->window_center          = cpu->center;
		doc->window_sidebar_center  = cpu->center_sidebar;
		doc->raster_content_inner_w = cpu->raster_content_inner_w;
		doc->raster_fill_width_mode = cpu->raster_fill_width_mode;
		nicety_page_window_cpu_result_free(cpu);
		return 0;
	}

	if (cpu->count == 0)
	{
		nicety_page_window_cpu_result_free(cpu);
		return 2;
	}

	arena = session->document_arena;
	if (doc->pages != NULL)
	{
		doc_sdl_pages_destroy_textures(doc->pages, doc->number_of_pages);
		arena_pop_to(arena, doc->arena_checkpoint_before_pages);
		doc->pages           = NULL;
		doc->number_of_pages = 0;
	}

	if (!arena_can_push(arena, (u64) cpu->count * sizeof(Page)))
	{
		nicety_page_window_cpu_result_free(cpu);
		return 2;
	}
	pages = PUSH_ARRAY(arena, Page, cpu->count);

	for (k = 0; k < cpu->count; k++)
	{
		NicetyCpuPageSlot *slot = &cpu->slots[k];
		Bitmap             page_bm;

		pages[k].index         = slot->index;
		pages[k].thumb_texture = NULL;
		memset(&pages[k].thumb_bitmap, 0, sizeof pages[k].thumb_bitmap);
		memset(&pages[k].page_bitmap, 0, sizeof pages[k].page_bitmap);
		pages[k].page_texture = NULL;

		if (slot->page.width > 0 && slot->page.samples != NULL)
		{
			page_bm.width         = slot->page.width;
			page_bm.height        = slot->page.height;
			page_bm.format        = slot->page.format;
			page_bm.rows_per_byte = slot->page.stride;
			page_bm.pixel_data    = slot->page.samples;

			pages[k].page_bitmap = page_bm;

			doc_sdl_page_upload_texture(app, &page_bm, &pages[k].page_texture);
			free(slot->page.samples);
			slot->page.samples              = NULL;
			pages[k].page_bitmap.pixel_data = NULL;
		}

		if (slot->thumb.width > 0 && slot->thumb.samples != NULL)
		{
			Bitmap thumb_bm;
			thumb_bm.width         = slot->thumb.width;
			thumb_bm.height        = slot->thumb.height;
			thumb_bm.format        = slot->thumb.format;
			thumb_bm.rows_per_byte = slot->thumb.stride;
			thumb_bm.pixel_data    = slot->thumb.samples;
			pages[k].thumb_bitmap  = thumb_bm;
			doc_sdl_page_init_thumb(&pages[k], app);
			free(slot->thumb.samples);
			slot->thumb.samples              = NULL;
			pages[k].thumb_bitmap.pixel_data = NULL;
		}
	}

	doc->session                = session;
	doc->pages                  = pages;
	doc->number_of_pages        = cpu->count;
	doc->file_path              = file_path;
	doc->window_center          = cpu->center;
	doc->window_sidebar_center  = cpu->center_sidebar;
	doc->raster_content_inner_w = cpu->raster_content_inner_w;
	doc->raster_fill_width_mode = cpu->raster_fill_width_mode;

	free(cpu->slots);
	free(cpu);
	return 0;
}
