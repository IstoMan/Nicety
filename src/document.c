#include "document.h"
#include "core.h"
#include <math.h>
#include <SDL3/SDL.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float (*document_visible_row_height_fn)(const Document *doc, size_t i, void *ctx);

static float document_thumb_max_edge_px(NicetyRenderMode mode)
{
	switch (mode)
	{
		case NICETY_RENDER_LOW:
			return NICETY_SIDEBAR_THUMB_MAX_PX;
		default:
			return NICETY_SIDEBAR_THUMB_MAX_PX;
	}
}

float document_app_pixel_density(const Application *app)
{
	if (app == NULL)
	{
		return 1.0f;
	}
	SDL_Window *win = SDL_GetRenderWindow(app->renderer);
	if (win == NULL)
	{
		return 1.0f;
	}
	return SDL_GetWindowPixelDensity(win);
}

static fz_matrix document_content_page_ctm_density(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width,
                                                   float page_w_pts, float pixel_density)
{
	switch (content_mode)
	{
		case NICETY_RENDER_HIGH:
			/* Same as normal until zoom supplies a scale matrix. */
			break;
		case NICETY_RENDER_NORMAL:
			if (fill_width_mode && content_inner_width > 1.0f && page_w_pts > 0.5f)
			{
				float d         = pixel_density > 0.0f ? pixel_density : 1.0f;
				float target_px = content_inner_width * d;
				float s         = target_px / page_w_pts;
				if (s < 1.0f)
				{
					s = 1.0f;
				}
				if (s > NICETY_RENDER_NORMAL_FILL_MAX_SCALE)
				{
					s = NICETY_RENDER_NORMAL_FILL_MAX_SCALE;
				}
				return fz_scale(s, s);
			}
			break;
		case NICETY_RENDER_LOW:
		default:
			break;
	}
	return fz_identity;
}

static fz_matrix document_content_page_ctm(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width,
                                           float page_w_pts, Application *app)
{
	return document_content_page_ctm_density(content_mode, fill_width_mode, content_inner_width, page_w_pts,
	                                         document_app_pixel_density(app));
}

static bool document_page_index_in_main_band(size_t i, size_t center, size_t r_main)
{
	if (i <= center)
	{
		return (center - i) <= r_main;
	}
	return (i - center) <= r_main;
}

/* Union of main interval [c_main - r_main, c_main + r_main] and sidebar [c_side - r_side, c_side + r_side]. */
static void document_page_window_union_range_dual(size_t center_main, size_t center_sidebar, size_t r_main, size_t r_sidebar,
                                                  size_t total_pages, size_t *out_from, size_t *out_till)
{
	size_t from_m = center_main > r_main ? center_main - r_main : 0;
	size_t till_m = center_main + r_main;
	size_t from_s = center_sidebar > r_sidebar ? center_sidebar - r_sidebar : 0;
	size_t till_s = center_sidebar + r_sidebar;
	size_t from   = from_m < from_s ? from_m : from_s;
	size_t till   = till_m > till_s ? till_m : till_s;

	if (total_pages == 0)
	{
		*out_from = 0;
		*out_till = 0;
		return;
	}
	if (till >= total_pages)
	{
		till = total_pages - 1;
	}
	*out_from = from;
	*out_till = till;
}

static fz_matrix document_thumb_only_page_ctm(float page_w_pts, float pixel_density)
{
	float d  = pixel_density > 0.0f ? pixel_density : 1.0f;
	float tw = document_thumb_max_edge_px(NICETY_RENDER_LOW);
	if (page_w_pts < 0.5f)
	{
		return fz_identity;
	}
	float s = (tw * d) / page_w_pts;
	if (s < 1e-6f)
	{
		s = 1e-6f;
	}
	if (s > NICETY_RENDER_NORMAL_FILL_MAX_SCALE)
	{
		s = NICETY_RENDER_NORMAL_FILL_MAX_SCALE;
	}
	return fz_scale(s, s);
}

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

static const float NICETY_SCROLL_REMAP_EPS = 0.5f;

static float inner_w_from_viewport_w(float viewport_w)
{
	float inner_w = viewport_w - 2.0f * NICETY_DOC_CONTENT_PAD;
	return inner_w < 1.0f ? 1.0f : inner_w;
}

<<<<<<< HEAD
=======
static float document_sidebar_row_height(const Document *doc, size_t i, float sb_inner);

>>>>>>> sidebar
/*
 * Map scroll_y so the viewport center stays at the same fraction along the same page when layout changes.
 * Old vs new may differ in viewport size and/or fit-height vs fill.
 */
static bool document_remap_scroll_y_unified(const Document *doc, float scroll_y_in, float viewport_w_old, float viewport_h_old,
                                          bool from_fit_height, float viewport_w_new, float viewport_h_new, bool to_fit_height,
                                          float *scroll_y_out)
{
	size_t n;
	float  inner_w_old;
	float  inner_w_new;
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

	if (from_fit_height == to_fit_height && fabsf(viewport_w_old - viewport_w_new) < NICETY_SCROLL_REMAP_EPS &&
	    fabsf(viewport_h_old - viewport_h_new) < NICETY_SCROLL_REMAP_EPS)
	{
		*scroll_y_out = scroll_y_in;
		return true;
	}

	n           = doc->session->total_pages;
	inner_w_old = inner_w_from_viewport_w(viewport_w_old);
	inner_w_new = inner_w_from_viewport_w(viewport_w_new);

	page = document_page_at_scroll_y(doc, scroll_y_in, viewport_w_old, viewport_h_old, from_fit_height);

	if (viewport_h_old <= 0.0f)
	{
		y_target = -scroll_y_in;
	}
	else
	{
		y_target = -scroll_y_in + viewport_h_old * 0.5f;
	}

	y_start_old = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < page; i++)
	{
		y_start_old += content_row_height(doc, i, inner_w_old, viewport_h_old, from_fit_height);
		if (i + 1 < n)
		{
			y_start_old += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}

	rh_old = content_row_height(doc, page, inner_w_old, viewport_h_old, from_fit_height);
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
		y_start_new += content_row_height(doc, i, inner_w_new, viewport_h_new, to_fit_height);
		if (i + 1 < n)
		{
			y_start_new += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}

	rh_new       = content_row_height(doc, page, inner_w_new, viewport_h_new, to_fit_height);
	y_target_new = y_start_new + frac * rh_new;

	if (viewport_h_new <= 0.0f)
	{
		scroll_out = -y_target_new;
	}
	else
	{
		scroll_out = -(y_target_new - viewport_h_new * 0.5f);
	}

	total_h = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < n; i++)
	{
		total_h += content_row_height(doc, i, inner_w_new, viewport_h_new, to_fit_height);
		if (i + 1 < n)
		{
			total_h += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}
	total_h += NICETY_DOC_CONTENT_PAD;

	if (viewport_h_new > 1.0f && total_h <= viewport_h_new)
	{
		*scroll_y_out = 0.0f;
		return true;
	}

	max_neg = 0.0f;
	if (viewport_h_new > 1.0f && total_h > viewport_h_new)
	{
		max_neg = -(total_h - viewport_h_new);
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

bool document_remap_scroll_y_for_view_mode(const Document *doc, float scroll_y_in, float viewport_w, float viewport_h,
                                           bool from_fit_height, bool to_fit_height, float *scroll_y_out)
{
	return document_remap_scroll_y_unified(doc, scroll_y_in, viewport_w, viewport_h, from_fit_height, viewport_w, viewport_h, to_fit_height,
	                                     scroll_y_out);
}

bool document_remap_scroll_y_for_viewport_change(const Document *doc, float scroll_y_in, float viewport_w_old, float viewport_h_old,
                                                 float viewport_w_new, float viewport_h_new, bool fit_height_mode, float *scroll_y_out)
{
	return document_remap_scroll_y_unified(doc, scroll_y_in, viewport_w_old, viewport_h_old, fit_height_mode, viewport_w_new, viewport_h_new,
	                                       fit_height_mode, scroll_y_out);
}

<<<<<<< HEAD
=======
size_t document_page_at_sidebar_scroll_y(const Document *doc, float scroll_y, float sb_inner, float viewport_h)
{
	if (doc == NULL || doc->session == NULL || doc->page_layout_w == NULL || doc->session->total_pages == 0)
	{
		return 0;
	}

	size_t total = doc->session->total_pages;
	float  y_target;
	if (viewport_h <= 0.0f)
	{
		y_target = -scroll_y;
	}
	else
	{
		y_target = -scroll_y + viewport_h * 0.5f;
	}

	float y = NICETY_DOC_SIDEBAR_PAD;
	for (size_t i = 0; i < total; i++)
	{
		float rh = document_sidebar_row_height(doc, i, sb_inner);
		if (y_target < y + rh)
		{
			return i;
		}
		y += rh;
		if (i + 1 < total)
		{
			y += NICETY_DOC_SIDEBAR_INTER_GAP;
		}
	}
	return total - 1;
}

/*
 * Same anchor semantics as document_remap_scroll_y_unified for the sidebar lane when viewport height changes.
 */
static bool document_remap_sidebar_scroll_y_unified(const Document *doc, float scroll_y_in, float sb_inner_old, float viewport_h_old,
                                                    float sb_inner_new, float viewport_h_new, float *scroll_y_out)
{
	size_t n;
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

	if (fabsf(sb_inner_old - sb_inner_new) < NICETY_SCROLL_REMAP_EPS && fabsf(viewport_h_old - viewport_h_new) < NICETY_SCROLL_REMAP_EPS)
	{
		*scroll_y_out = scroll_y_in;
		return true;
	}

	n    = doc->session->total_pages;
	page = document_page_at_sidebar_scroll_y(doc, scroll_y_in, sb_inner_old, viewport_h_old);

	if (viewport_h_old <= 0.0f)
	{
		y_target = -scroll_y_in;
	}
	else
	{
		y_target = -scroll_y_in + viewport_h_old * 0.5f;
	}

	y_start_old = NICETY_DOC_SIDEBAR_PAD;
	for (size_t i = 0; i < page; i++)
	{
		y_start_old += document_sidebar_row_height(doc, i, sb_inner_old);
		if (i + 1 < n)
		{
			y_start_old += NICETY_DOC_SIDEBAR_INTER_GAP;
		}
	}

	rh_old = document_sidebar_row_height(doc, page, sb_inner_old);
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

	y_start_new = NICETY_DOC_SIDEBAR_PAD;
	for (size_t i = 0; i < page; i++)
	{
		y_start_new += document_sidebar_row_height(doc, i, sb_inner_new);
		if (i + 1 < n)
		{
			y_start_new += NICETY_DOC_SIDEBAR_INTER_GAP;
		}
	}

	rh_new       = document_sidebar_row_height(doc, page, sb_inner_new);
	y_target_new = y_start_new + frac * rh_new;

	if (viewport_h_new <= 0.0f)
	{
		scroll_out = -y_target_new;
	}
	else
	{
		scroll_out = -(y_target_new - viewport_h_new * 0.5f);
	}

	total_h = NICETY_DOC_SIDEBAR_PAD;
	for (size_t i = 0; i < n; i++)
	{
		total_h += document_sidebar_row_height(doc, i, sb_inner_new);
		if (i + 1 < n)
		{
			total_h += NICETY_DOC_SIDEBAR_INTER_GAP;
		}
	}
	total_h += NICETY_DOC_SIDEBAR_PAD;

	if (viewport_h_new > 1.0f && total_h <= viewport_h_new)
	{
		*scroll_y_out = 0.0f;
		return true;
	}

	max_neg = 0.0f;
	if (viewport_h_new > 1.0f && total_h > viewport_h_new)
	{
		max_neg = -(total_h - viewport_h_new);
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

bool document_remap_sidebar_scroll_y_for_viewport_change(const Document *doc, float scroll_y_in, float sb_inner, float viewport_h_old,
                                                           float viewport_h_new, float *scroll_y_out)
{
	return document_remap_sidebar_scroll_y_unified(doc, scroll_y_in, sb_inner, viewport_h_old, sb_inner, viewport_h_new, scroll_y_out);
}

static float document_total_content_height(const Document *doc, float inner_w, float viewport_h, bool fit_height_mode)
{
	size_t n;
	float  total_h;

	if (doc == NULL || doc->session == NULL || doc->page_layout_w == NULL)
	{
		return 0.0f;
	}
	n = doc->session->total_pages;
	if (n == 0)
	{
		return 0.0f;
	}
	total_h = NICETY_DOC_CONTENT_PAD;
	for (size_t i = 0; i < n; i++)
	{
		total_h += content_row_height(doc, i, inner_w, viewport_h, fit_height_mode);
		if (i + 1 < n)
		{
			total_h += NICETY_DOC_CONTENT_INTER_PAGE_GAP;
		}
	}
	total_h += NICETY_DOC_CONTENT_PAD;
	return total_h;
}

static float document_total_sidebar_height(const Document *doc, float sb_inner)
{
	size_t n;
	float  total_h;

	if (doc == NULL || doc->session == NULL || doc->page_layout_w == NULL)
	{
		return 0.0f;
	}
	n = doc->session->total_pages;
	if (n == 0)
	{
		return 0.0f;
	}
	total_h = NICETY_DOC_SIDEBAR_PAD;
	for (size_t i = 0; i < n; i++)
	{
		total_h += document_sidebar_row_height(doc, i, sb_inner);
		if (i + 1 < n)
		{
			total_h += NICETY_DOC_SIDEBAR_INTER_GAP;
		}
	}
	total_h += NICETY_DOC_SIDEBAR_PAD;
	return total_h;
}

bool document_sidebar_scroll_y_from_content_scroll_y(const Document *doc, float content_scroll_y, float content_viewport_w,
                                                     float content_viewport_h, bool fit_height_mode, float sb_inner, float lane_viewport_h,
                                                     float *sidebar_scroll_y_out)
{
	float inner_w;
	float total_main;
	float total_side;
	float scroll_main;
	float scroll_side;
	float ratio;
	float out_y;
	float max_neg;

	if (sidebar_scroll_y_out == NULL || doc == NULL || doc->session == NULL || doc->page_layout_w == NULL || doc->session->total_pages == 0)
	{
		return false;
	}

	inner_w     = inner_w_from_viewport_w(content_viewport_w);
	total_main  = document_total_content_height(doc, inner_w, content_viewport_h, fit_height_mode);
	total_side  = document_total_sidebar_height(doc, sb_inner);
	scroll_main = 0.0f;
	if (lane_viewport_h > 1.0f && total_main > lane_viewport_h)
	{
		scroll_main = total_main - lane_viewport_h;
	}
	scroll_side = 0.0f;
	if (lane_viewport_h > 1.0f && total_side > lane_viewport_h)
	{
		scroll_side = total_side - lane_viewport_h;
	}

	if (scroll_main <= 1e-6f)
	{
		ratio = 0.0f;
	}
	else
	{
		ratio = (-content_scroll_y) / scroll_main;
		if (ratio < 0.0f)
		{
			ratio = 0.0f;
		}
		else if (ratio > 1.0f)
		{
			ratio = 1.0f;
		}
	}

	out_y = -ratio * scroll_side;

	max_neg = 0.0f;
	if (lane_viewport_h > 1.0f && total_side > lane_viewport_h)
	{
		max_neg = -(total_side - lane_viewport_h);
	}
	if (out_y > 0.0f)
	{
		out_y = 0.0f;
	}
	if (out_y < max_neg)
	{
		out_y = max_neg;
	}

	*sidebar_scroll_y_out = out_y;
	return true;
}

>>>>>>> sidebar
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
		*out_lo         = 0;
		*out_hi         = n - 1;
		*out_spacer_top = *out_spacer_bottom = 0.0f;
		return;
	}

	{
		float  top = -scroll_y;
		float  bot = top + viewport_h;
		float  o   = NICETY_UI_VIRTUAL_OVERSCAN_PX;
		float  y   = edge_pad;
		size_t lo  = n;
		size_t hi  = 0;
		size_t i;

		for (i = 0; i < n; i++)
		{
			float h      = row_h(doc, i, row_ctx);
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

static void pages_destroy_textures(Page *pages, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (pages[i].thumb_texture != NULL)
		{
			SDL_DestroyTexture(pages[i].thumb_texture);
		}
		if (pages[i].page_texture != NULL)
		{
			SDL_DestroyTexture(pages[i].page_texture);
		}
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
		doc->session                 = session;
		doc->file_path               = file_path;
		doc->number_of_pages         = 0;
		doc->pages                   = NULL;
		doc->window_center           = 0;
		doc->window_sidebar_center   = 0;
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
	document_page_window_union_range_dual(center_main, center_sidebar, radius_main, radius_sidebar, session->total_pages, &from, &till);

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
	fz_pixmap *tpix = NULL;

	for (size_t k = 0; k < count; k++)
	{
		size_t i       = from + k;
		bool   in_main = document_page_index_in_main_band(i, center_main, radius_main);

		pages[k].thumb_texture = NULL;
		memset(&pages[k].thumb_bitmap, 0, sizeof pages[k].thumb_bitmap);

		if (in_main)
		{
			page = fz_load_page(session->ctx, session->doc, (int) i);
			pix  = fz_new_pixmap_from_page(session->ctx, page,
			                               document_content_page_ctm(content_mode, fill_width_mode, content_inner_width, doc->page_layout_w[i], app),
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
			page_init(&pages[k], app);
			pages[k].page_bitmap.pixel_data = NULL;

			if (pix->w > 0)
			{
				float tw = document_thumb_max_edge_px(NICETY_RENDER_LOW);
				float th = tw * (float) pix->h / (float) pix->w;
				tpix     = fz_scale_pixmap(session->ctx, pix, 0.0f, 0.0f, tw, th, NULL);

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
						tpix = NULL;
					}
					else
					{
						fprintf(stderr, "Unsupported thumbnail pixel format\n");
						fz_drop_pixmap(session->ctx, tpix);
						tpix = NULL;
					}
				}
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
			pix  = fz_new_pixmap_from_page(session->ctx, page, document_thumb_only_page_ctm(doc->page_layout_w[i], pd),
			                               fz_device_rgb(session->ctx), 0);
			if (pix == NULL)
			{
				fz_drop_page(session->ctx, page);
				release_page_window(doc, pages, k);
				return 1;
			}

			if (pix->w > 0)
			{
				float tw = document_thumb_max_edge_px(NICETY_RENDER_LOW);
				float th = tw * (float) pix->h / (float) pix->w;
				tpix     = fz_scale_pixmap(session->ctx, pix, 0.0f, 0.0f, tw, th, NULL);
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
						tpix = NULL;
					}
					else
					{
						fprintf(stderr, "Unsupported thumbnail pixel format\n");
						fz_drop_pixmap(session->ctx, tpix);
						tpix = NULL;
					}
				}
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
	doc->window_center            = center_main;
	doc->window_sidebar_center    = center_sidebar;
	doc->raster_content_inner_w   = content_inner_width;
	doc->raster_fill_width_mode   = fill_width_mode ? 1 : 0;
	return 0;
}

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

int document_raster_page_window_to_cpu(const char *file_path, const float *page_layout_w, size_t total_pages, size_t center_main,
                                       size_t center_sidebar, size_t radius_main, size_t radius_sidebar, NicetyRenderMode content_mode,
                                       bool fill_width_mode, float content_inner_width, float pixel_density, u64 doc_token,
                                       u64 request_seq, NicetyPageWindowCpuResult **out)
{
	fz_context                *ctx    = NULL;
	fz_document               *doc    = NULL;
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
	document_page_window_union_range_dual(center_main, center_sidebar, radius_main, radius_sidebar, total_pages, &from, &till);
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
		doc = fz_open_document(ctx, file_path);
	}
	fz_catch(ctx)
	{
		doc = NULL;
	}
	if (doc == NULL)
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
		fz_pixmap *tpix = NULL;
		bool       in_main;

		i       = from + k;
		in_main = document_page_index_in_main_band(i, center_main, radius_main);

		nicety_cpu_bitmap_clear(&slots[k].page);
		nicety_cpu_bitmap_clear(&slots[k].thumb);
		slots[k].index = i;

		if (in_main)
		{
			fz_try(ctx)
			{
				page = fz_load_page(ctx, doc, (int) i);
				pix  = fz_new_pixmap_from_page(ctx, page,
				                               document_content_page_ctm_density(content_mode, fill_width_mode, content_inner_width,
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
				fz_drop_document(ctx, doc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (nicety_copy_pixmap_to_cpu(pix, &slots[k].page) != 0)
			{
				fz_drop_pixmap(ctx, pix);
				fz_drop_page(ctx, page);
				fz_drop_document(ctx, doc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (pix->w > 0)
			{
				float tw = document_thumb_max_edge_px(NICETY_RENDER_LOW);
				float th = tw * (float) pix->h / (float) pix->w;
				fz_try(ctx)
				{
					tpix = fz_scale_pixmap(ctx, pix, 0.0f, 0.0f, tw, th, NULL);
				}
				fz_catch(ctx)
				{
					tpix = NULL;
				}
				if (tpix != NULL)
				{
					if (tpix->n == 3 || tpix->n == 4)
					{
						if (nicety_copy_pixmap_to_cpu(tpix, &slots[k].thumb) != 0)
						{
							fz_drop_pixmap(ctx, tpix);
							fz_drop_pixmap(ctx, pix);
							fz_drop_page(ctx, page);
							fz_drop_document(ctx, doc);
							fz_drop_context(ctx);
							nicety_page_window_cpu_result_free(result);
							return 1;
						}
					}
					fz_drop_pixmap(ctx, tpix);
					tpix = NULL;
				}
			}

			fz_drop_pixmap(ctx, pix);
			fz_drop_page(ctx, page);
		}
		else
		{
			fz_try(ctx)
			{
				page = fz_load_page(ctx, doc, (int) i);
				pix  = fz_new_pixmap_from_page(ctx, page, document_thumb_only_page_ctm(page_layout_w[i], pixel_density),
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
				fz_drop_document(ctx, doc);
				fz_drop_context(ctx);
				nicety_page_window_cpu_result_free(result);
				return 1;
			}

			if (pix->w > 0)
			{
				float tw = document_thumb_max_edge_px(NICETY_RENDER_LOW);
				float th = tw * (float) pix->h / (float) pix->w;
				fz_try(ctx)
				{
					tpix = fz_scale_pixmap(ctx, pix, 0.0f, 0.0f, tw, th, NULL);
				}
				fz_catch(ctx)
				{
					tpix = NULL;
				}
				if (tpix != NULL)
				{
					if (tpix->n == 3 || tpix->n == 4)
					{
						if (nicety_copy_pixmap_to_cpu(tpix, &slots[k].thumb) != 0)
						{
							fz_drop_pixmap(ctx, tpix);
							fz_drop_pixmap(ctx, pix);
							fz_drop_page(ctx, page);
							fz_drop_document(ctx, doc);
							fz_drop_context(ctx);
							nicety_page_window_cpu_result_free(result);
							return 1;
						}
					}
					fz_drop_pixmap(ctx, tpix);
					tpix = NULL;
				}
			}

			fz_drop_pixmap(ctx, pix);
			fz_drop_page(ctx, page);
		}
	}

	fz_drop_document(ctx, doc);
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
			pages_destroy_textures(doc->pages, doc->number_of_pages);
			arena_pop_to(session->document_arena, doc->arena_checkpoint_before_pages);
			doc->pages           = NULL;
			doc->number_of_pages = 0;
		}
		doc->session                  = session;
		doc->file_path                = file_path;
		doc->window_center            = cpu->center;
		doc->window_sidebar_center    = cpu->center_sidebar;
		doc->raster_content_inner_w   = cpu->raster_content_inner_w;
		doc->raster_fill_width_mode   = cpu->raster_fill_width_mode;
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
		pages_destroy_textures(doc->pages, doc->number_of_pages);
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

			page_upload_texture(app, &page_bm, &pages[k].page_texture);
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
			page_init_thumb(&pages[k], app);
			free(slot->thumb.samples);
			slot->thumb.samples              = NULL;
			pages[k].thumb_bitmap.pixel_data = NULL;
		}
	}

	doc->session                  = session;
	doc->pages                    = pages;
	doc->number_of_pages          = cpu->count;
	doc->file_path                = file_path;
	doc->window_center            = cpu->center;
	doc->window_sidebar_center    = cpu->center_sidebar;
	doc->raster_content_inner_w   = cpu->raster_content_inner_w;
	doc->raster_fill_width_mode   = cpu->raster_fill_width_mode;

	free(cpu->slots);
	free(cpu);
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
