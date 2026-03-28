#pragma once
#include "utils.h"
#include <mupdf/fitz.h>
#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Application Application;

typedef enum
{
	COLOR_FORMAT_BGRA,        // 8-bit per channel, premultiplied alpha
	COLOR_FORMAT_RGBA,
	COLOR_FORMAT_RGB,
	COLOR_FORMAT_GRAY8
} PixelFormat;

typedef struct
{
	u32         width, height;
	u32         rows_per_byte;
	PixelFormat format;
	u8         *pixel_data;
} Bitmap;

typedef struct
{
	Bitmap page_bitmap;
	void  *page_texture;
	Bitmap thumb_bitmap; /* pixels for aspect when thumb_texture is set; width/height 0 otherwise */
	void  *thumb_texture;
	size_t index;
} Page;

typedef struct
{
	mem_arena   *document_arena;
	fz_context  *ctx;
	fz_document *doc;
	size_t       total_pages;
} DocumentContext;

/* Must match Clay padding / gaps in ui_document_view (Content column: image-only rows). */
#define NICETY_DOC_CONTENT_PAD 20.0f
#define NICETY_DOC_CONTENT_INTER_PAGE_GAP 20.0f
#define NICETY_DOC_FIT_HEIGHT_TOP_RESERVE 40.0f

/* Sidebar column; must match Clay Sidebar in ui_document_view. */
#define NICETY_DOC_SIDEBAR_OUTER_W 150.0f
#define NICETY_DOC_SIDEBAR_PAD 10.0f
#define NICETY_DOC_SIDEBAR_INTER_GAP 10.0f

/* Max width (CSS-ish) for sidebar thumbnails; scaled from full page pixmap via fz_scale_pixmap. */
#define NICETY_SIDEBAR_THUMB_MAX_PX 128.0f

typedef struct
{
	DocumentContext *session;
	Page            *pages;
	size_t           number_of_pages;
	size_t           window_center;
	float           *page_layout_w;
	float           *page_layout_h;
	const char      *file_path;
} Document;

#define NICETY_PAGE_WINDOW_RADIUS 2

/* Extra pixels above/below viewport when virtualizing scroll lists (Clay elements). */
#define NICETY_UI_VIRTUAL_OVERSCAN_PX 400.0f

DocumentContext *document_context_init(mem_arena *document_arena, const char *file_path);
void             document_context_destroy(DocumentContext *session);

int document_measure_pages(DocumentContext *session, Document *doc);
int document_load_page_window(DocumentContext *session, Application *app, size_t center, size_t radius,
                              const char *file_path, Document *doc);

size_t document_page_at_scroll_y(const Document *doc, float scroll_y, float viewport_w, float viewport_h, bool fit_height_mode);

/*
 * After toggling fit-height vs fill, row heights change; map scroll so the viewport center stays at the same
 * fraction along the same page (matches document_page_at_scroll_y centering semantics).
 */
bool document_remap_scroll_y_for_view_mode(const Document *doc, float scroll_y_in, float viewport_w, float viewport_h,
                                           bool from_fit_height, bool to_fit_height, float *scroll_y_out);

Page *document_page_for_index(const Document *doc, size_t page_index);

/*
 * Visible page index range for virtualized sidebar/content lists. Spacers preserve total scroll
 * extent: top spacer + rows [lo..hi] + bottom spacer matches full non-virtual layout height.
 */
void document_visible_sidebar_range(const Document *doc, float sb_inner, float scroll_y, float viewport_h, size_t *out_lo,
                                    size_t *out_hi, float *out_spacer_top, float *out_spacer_bottom);

void document_visible_content_range(const Document *doc, float content_inner_w, float scroll_y, float viewport_w, float viewport_h,
                                    bool fit_height_mode, size_t *out_lo, size_t *out_hi, float *out_spacer_top,
                                    float *out_spacer_bottom);

void document_destroy(DocumentContext *session, Document *document);
