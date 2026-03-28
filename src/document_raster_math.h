#pragma once

#include "document.h"
#include <mupdf/fitz.h>

/* MuPDF matrices and window math shared by sync load and CPU raster paths. */

float doc_raster_thumb_max_edge_px(NicetyRenderMode mode);

fz_matrix doc_raster_content_page_ctm_density(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width,
                                                float page_w_pts, float pixel_density);

fz_matrix doc_raster_content_page_ctm(NicetyRenderMode content_mode, bool fill_width_mode, float content_inner_width, float page_w_pts,
                                      Application *app);

fz_matrix doc_raster_thumb_only_page_ctm(float page_w_pts, float pixel_density);

void doc_raster_page_window_union_range_dual(size_t center_main, size_t center_sidebar, size_t r_main, size_t r_sidebar,
                                             size_t total_pages, size_t *out_from, size_t *out_till);

bool doc_raster_page_index_in_main_band(size_t i, size_t center, size_t r_main);
