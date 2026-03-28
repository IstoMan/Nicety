#include "document.h"
#include <math.h>
#include <stddef.h>

typedef float (*document_visible_row_height_fn)(const Document *doc, size_t i, void *ctx);

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

static float document_sidebar_row_height(const Document *doc, size_t i, float sb_inner);

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
