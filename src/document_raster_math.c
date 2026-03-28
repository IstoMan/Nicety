#include "document_raster_math.h"
#include "core.h"
#include <SDL3/SDL.h>

float doc_raster_thumb_max_edge_px(NicetyRenderMode mode)
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

fz_matrix doc_raster_content_page_ctm_density(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width,
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

fz_matrix doc_raster_content_page_ctm(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width, float page_w_pts,
                                      Application *app)
{
	return doc_raster_content_page_ctm_density(content_mode, fill_width_mode, content_inner_width, page_w_pts,
	                                           document_app_pixel_density(app));
}

bool doc_raster_page_index_in_main_band(size_t i, size_t center, size_t r_main)
{
	if (i <= center)
	{
		return (center - i) <= r_main;
	}
	return (i - center) <= r_main;
}

/* Union of main interval [c_main - r_main, c_main + r_main] and sidebar [c_side - r_side, c_side + r_side]. */
void doc_raster_page_window_union_range_dual(size_t center_main, size_t center_sidebar, size_t r_main, size_t r_sidebar,
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

fz_matrix doc_raster_thumb_only_page_ctm(float page_w_pts, float pixel_density)
{
	float d  = pixel_density > 0.0f ? pixel_density : 1.0f;
	float tw = doc_raster_thumb_max_edge_px(NICETY_RENDER_LOW);
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
