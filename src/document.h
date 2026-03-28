#pragma once
#include "utils.h"
#include <mupdf/fitz.h>
#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Application Application;

/* Raster quality for PDF pixmaps: sidebar thumbnails use LOW; main view uses NORMAL; HIGH when zoomed (future). */
typedef enum
{
	NICETY_RENDER_LOW,
	NICETY_RENDER_NORMAL,
	NICETY_RENDER_HIGH,
} NicetyRenderMode;

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

/* Auto-hide sidebar when Clay layout width (render width minus Outer padding) is below this. */
#define NICETY_DOC_MIN_CONTENT_W_BESIDE_SIDEBAR 280.0f
#define NICETY_DOC_MIN_LAYOUT_W_FOR_SIDEBAR \
	(NICETY_DOC_SIDEBAR_OUTER_W + 2.0f * NICETY_DOC_CONTENT_PAD + NICETY_DOC_MIN_CONTENT_W_BESIDE_SIDEBAR)

/* Max width (CSS-ish) for NICETY_RENDER_LOW sidebar thumbnails; scaled via fz_scale_pixmap. */
#define NICETY_SIDEBAR_THUMB_MAX_PX 128.0f

/* Upper bound for NORMAL fill-width upscale vs default ~72 dpi raster (avoids huge pixmaps). */
#define NICETY_RENDER_NORMAL_FILL_MAX_SCALE 4.0f

typedef struct
{
	DocumentContext *session;
	Page            *pages;
	size_t           number_of_pages;
	size_t           window_center;
	size_t           window_sidebar_center;
	float           *page_layout_w;
	float           *page_layout_h;
	const char      *file_path;
	/* Arena bump checkpoints: after DocumentContext+Document, before layout; after layout, before Page window array. */
	u64 arena_checkpoint_after_document;
	u64 arena_checkpoint_before_pages;
	b8  page_layout_heap;
	/* Last raster hints (reload when fill mode or content width changes). */
	float raster_content_inner_w;
	b8    raster_fill_width_mode;
} Document;

/* Main pane: full NORMAL raster within this radius of center. Sidebar: union extends to SIDEBAR radius; outer indices are thumb-only. */
#define NICETY_PAGE_WINDOW_RADIUS_MAIN    2
#define NICETY_PAGE_WINDOW_RADIUS_SIDEBAR 5

/* Extra pixels above/below viewport when virtualizing scroll lists (Clay elements). */
#define NICETY_UI_VIRTUAL_OVERSCAN_PX 400.0f

DocumentContext *document_context_init(mem_arena *document_arena, const char *file_path);
void             document_context_destroy(DocumentContext *session);

int document_measure_pages(DocumentContext *session, Document *doc);
int document_load_page_window(DocumentContext *session, Application *app, size_t center_main, size_t center_sidebar, size_t radius_main,
                              size_t radius_sidebar, const char *file_path, Document *doc, NicetyRenderMode content_mode,
                              bool fill_width_mode, float content_inner_width);

/* Pixel density for fill-width raster (matches SDL_GetWindowPixelDensity); use 1 if unknown. */
float document_app_pixel_density(const Application *app);

/*
 * CPU-side page window (worker thread): owned pixel buffers for main-thread SDL texture upload.
 */
typedef struct NicetyCpuBitmap
{
	u8         *samples;
	u32         width;
	u32         height;
	u32         stride;
	PixelFormat format;
} NicetyCpuBitmap;

typedef struct NicetyCpuPageSlot
{
	size_t          index;
	NicetyCpuBitmap page;
	NicetyCpuBitmap thumb;
} NicetyCpuPageSlot;

typedef struct NicetyPageWindowCpuResult
{
	u64                doc_token;
	u64                request_seq;
	size_t             center;
	size_t             center_sidebar;
	size_t             from_index;
	size_t             count;
	float              raster_content_inner_w;
	b8                 raster_fill_width_mode;
	NicetyCpuPageSlot *slots;
} NicetyPageWindowCpuResult;

void nicety_page_window_cpu_result_free(NicetyPageWindowCpuResult *r);

/* Raster off the UI thread (own fz_context). Returns 0 and sets *out on success. */
int document_raster_page_window_to_cpu(const char *file_path, const float *page_layout_w, size_t total_pages, size_t center_main,
                                       size_t center_sidebar, size_t radius_main, size_t radius_sidebar, NicetyRenderMode content_mode,
                                       bool fill_width_mode, float content_inner_width, float pixel_density, u64 doc_token,
                                       u64 request_seq, NicetyPageWindowCpuResult **out);

/*
 * Upload CPU buffers to textures and install into doc (main thread only).
 * On success (0), consumes and frees cpu. On stale (1), frees cpu. On error (2), frees cpu.
 */
int document_commit_page_window_from_cpu(Application *app, DocumentContext *session, Document *doc, const char *file_path,
                                         NicetyPageWindowCpuResult *cpu, u64 expected_doc_token);

size_t document_page_at_scroll_y(const Document *doc, float scroll_y, float viewport_w, float viewport_h, bool fit_height_mode);

/*
 * After toggling fit-height vs fill, row heights change; map scroll so the viewport center stays at the same
 * fraction along the same page (matches document_page_at_scroll_y centering semantics).
 */
bool document_remap_scroll_y_for_view_mode(const Document *doc, float scroll_y_in, float viewport_w, float viewport_h,
                                           bool from_fit_height, bool to_fit_height, float *scroll_y_out);

/* Same anchor semantics when only the content viewport size changes (window resize). */
bool document_remap_scroll_y_for_viewport_change(const Document *doc, float scroll_y_in, float viewport_w_old, float viewport_h_old,
                                                 float viewport_w_new, float viewport_h_new, bool fit_height_mode, float *scroll_y_out);

<<<<<<< HEAD
=======
size_t document_page_at_sidebar_scroll_y(const Document *doc, float scroll_y, float sb_inner, float viewport_h);

bool document_remap_sidebar_scroll_y_for_viewport_change(const Document *doc, float scroll_y_in, float sb_inner, float viewport_h_old,
                                                           float viewport_h_new, float *scroll_y_out);

bool document_sidebar_scroll_y_from_content_scroll_y(const Document *doc, float content_scroll_y, float content_viewport_w,
                                                       float content_viewport_h, bool fit_height_mode, float sb_inner, float lane_viewport_h,
                                                       float *sidebar_scroll_y_out);

>>>>>>> sidebar
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
