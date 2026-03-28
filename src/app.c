#include "app.h"
#include "ui.h"
#include "clay_renderer_SDL3.h"
#include "tinyfiledialogs.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
	DOCUMENT_ARENA_BYTES = 65536
};

static int SDLCALL page_loader_thread_fn(void *data);

static void app_sync_sidebar_visibility(App *self, Application *core);

static void page_loader_init(App *self)
{
	self->doc_load_token                = 0;
	self->page_loader_request_seq       = 0;
	self->page_loader_pending_doc_token = 0;
	self->page_loader_shutdown          = false;
	self->page_loader_have_request      = false;
	self->page_loader_path              = NULL;
	self->page_loader_layout_w          = NULL;
	self->page_loader_layout_h          = NULL;
	self->page_loader_completed         = NULL;
	self->page_loader_have_completed    = false;
	self->page_loader_thread            = NULL;
	self->page_loader_mutex             = SDL_CreateMutex();
	self->page_loader_cond              = SDL_CreateCondition();
	if (self->page_loader_mutex == NULL || self->page_loader_cond == NULL)
	{
		fprintf(stderr, "page_loader: SDL_CreateMutex/Condition failed\n");
		return;
	}
	self->page_loader_thread = SDL_CreateThread(page_loader_thread_fn, "nicety_page_loader", self);
	if (self->page_loader_thread == NULL)
	{
		fprintf(stderr, "page_loader: SDL_CreateThread failed\n");
	}
}

static void page_loader_cancel(App *self)
{
	if (self->page_loader_mutex == NULL)
	{
		return;
	}
	SDL_LockMutex(self->page_loader_mutex);
	SDL_free(self->page_loader_path);
	self->page_loader_path = NULL;
	free(self->page_loader_layout_w);
	free(self->page_loader_layout_h);
	self->page_loader_layout_w     = NULL;
	self->page_loader_layout_h     = NULL;
	self->page_loader_have_request = false;
	if (self->page_loader_completed != NULL)
	{
		nicety_page_window_cpu_result_free(self->page_loader_completed);
		self->page_loader_completed = NULL;
	}
	self->page_loader_have_completed = false;
	SDL_UnlockMutex(self->page_loader_mutex);
}

static void page_loader_shutdown(App *self)
{
	if (self->page_loader_mutex == NULL)
	{
		return;
	}
	SDL_LockMutex(self->page_loader_mutex);
	self->page_loader_shutdown = true;
	SDL_SignalCondition(self->page_loader_cond);
	SDL_UnlockMutex(self->page_loader_mutex);
	if (self->page_loader_thread != NULL)
	{
		SDL_WaitThread(self->page_loader_thread, NULL);
		self->page_loader_thread = NULL;
	}
	page_loader_cancel(self);
	SDL_DestroyMutex(self->page_loader_mutex);
	self->page_loader_mutex = NULL;
	SDL_DestroyCondition(self->page_loader_cond);
	self->page_loader_cond = NULL;
}

static void page_loader_enqueue(App *self, Application *core, Document *doc, size_t center_main, size_t center_sidebar, bool fill,
                                float inner_w)
{
	size_t n;

	if (self->page_loader_mutex == NULL || self->page_loader_thread == NULL || doc == NULL || doc->session == NULL || doc->file_path == NULL || doc->page_layout_w == NULL || doc->page_layout_h == NULL || core == NULL)
	{
		return;
	}

	n = doc->session->total_pages;

	SDL_LockMutex(self->page_loader_mutex);
	SDL_free(self->page_loader_path);
	self->page_loader_path = NULL;
	free(self->page_loader_layout_w);
	free(self->page_loader_layout_h);
	self->page_loader_layout_w = NULL;
	self->page_loader_layout_h = NULL;

	self->page_loader_path = SDL_strdup(doc->file_path);
	if (self->page_loader_path == NULL)
	{
		SDL_UnlockMutex(self->page_loader_mutex);
		return;
	}
	if (n > 0)
	{
		self->page_loader_layout_w = (float *) malloc(n * sizeof(float));
		self->page_loader_layout_h = (float *) malloc(n * sizeof(float));
		if (self->page_loader_layout_w == NULL || self->page_loader_layout_h == NULL)
		{
			free(self->page_loader_layout_w);
			free(self->page_loader_layout_h);
			self->page_loader_layout_w = NULL;
			self->page_loader_layout_h = NULL;
			SDL_free(self->page_loader_path);
			self->page_loader_path = NULL;
			SDL_UnlockMutex(self->page_loader_mutex);
			return;
		}
		memcpy(self->page_loader_layout_w, doc->page_layout_w, n * sizeof(float));
		memcpy(self->page_loader_layout_h, doc->page_layout_h, n * sizeof(float));
	}

	self->page_loader_total_pages        = n;
	self->page_loader_center             = center_main;
	self->page_loader_center_sidebar     = center_sidebar;
	self->page_loader_radius_main        = NICETY_PAGE_WINDOW_RADIUS_MAIN;
	self->page_loader_radius_sidebar     = NICETY_PAGE_WINDOW_RADIUS_SIDEBAR;
	self->page_loader_fill          = fill;
	self->page_loader_inner_w       = inner_w;
	self->page_loader_pixel_density = document_app_pixel_density(core);
	self->page_loader_request_seq++;
	self->page_loader_pending_doc_token = self->doc_load_token;
	self->page_loader_have_request      = true;
	SDL_SignalCondition(self->page_loader_cond);
	SDL_UnlockMutex(self->page_loader_mutex);

	/* So next frame we do not enqueue again until scroll/view context changes (centers track last requested window). */
	doc->window_center            = center_main;
	doc->window_sidebar_center    = center_sidebar;
	doc->raster_content_inner_w   = inner_w;
	doc->raster_fill_width_mode   = fill ? 1 : 0;
}

static void page_loader_try_commit(App *self, Application *core)
{
	NicetyPageWindowCpuResult *r;
	u64                        token;

	if (self->page_loader_mutex == NULL || self->document == NULL || self->document_ctx == NULL || core == NULL)
	{
		return;
	}

	SDL_LockMutex(self->page_loader_mutex);
	if (!self->page_loader_have_completed || self->page_loader_completed == NULL)
	{
		SDL_UnlockMutex(self->page_loader_mutex);
		return;
	}
	r                                = self->page_loader_completed;
	self->page_loader_completed      = NULL;
	self->page_loader_have_completed = false;
	token                            = self->doc_load_token;
	SDL_UnlockMutex(self->page_loader_mutex);

	if (document_commit_page_window_from_cpu(core, self->document_ctx, self->document, self->document->file_path, r, token) != 0)
	{
		/* Stale or error; cpu freed inside commit */
	}
}

static int SDLCALL page_loader_thread_fn(void *data)
{
	App *self = (App *) data;

	for (;;)
	{
		char                      *path;
		float                     *lw;
		float                     *lh;
		u64                        my_seq;
		u64                        my_doc_gen;
		size_t                     center_main;
		size_t                     center_sidebar;
		size_t                     radius_main;
		size_t                     radius_sidebar;
		size_t                     total_pages;
		bool                       fill;
		float                      inner_w;
		float                      density;
		int                        err;
		NicetyPageWindowCpuResult *result = NULL;

		SDL_LockMutex(self->page_loader_mutex);
		while (!self->page_loader_shutdown && !self->page_loader_have_request)
		{
			SDL_WaitCondition(self->page_loader_cond, self->page_loader_mutex);
		}
		if (self->page_loader_shutdown)
		{
			SDL_UnlockMutex(self->page_loader_mutex);
			break;
		}

		path        = self->page_loader_path != NULL ? SDL_strdup(self->page_loader_path) : NULL;
		my_seq      = self->page_loader_request_seq;
		my_doc_gen  = self->page_loader_pending_doc_token;
		center_main      = self->page_loader_center;
		center_sidebar   = self->page_loader_center_sidebar;
		radius_main    = self->page_loader_radius_main;
		radius_sidebar = self->page_loader_radius_sidebar;
		total_pages = self->page_loader_total_pages;
		fill        = self->page_loader_fill;
		inner_w     = self->page_loader_inner_w;
		density     = self->page_loader_pixel_density;
		lw          = NULL;
		lh          = NULL;
		if (total_pages > 0)
		{
			lw = (float *) malloc(total_pages * sizeof(float));
			lh = (float *) malloc(total_pages * sizeof(float));
			if (lw != NULL && lh != NULL)
			{
				memcpy(lw, self->page_loader_layout_w, total_pages * sizeof(float));
				memcpy(lh, self->page_loader_layout_h, total_pages * sizeof(float));
			}
		}
		self->page_loader_have_request = false;
		SDL_UnlockMutex(self->page_loader_mutex);

		if (path == NULL || (total_pages > 0 && (lw == NULL || lh == NULL)))
		{
			free(lw);
			free(lh);
			SDL_free(path);
			continue;
		}

		err = document_raster_page_window_to_cpu(path, lw, total_pages, center_main, center_sidebar, radius_main, radius_sidebar,
		                                         NICETY_RENDER_NORMAL, fill, inner_w, density, my_doc_gen, my_seq, &result);
		free(lw);
		free(lh);
		SDL_free(path);

		if (err != 0 || result == NULL)
		{
			continue;
		}

		SDL_LockMutex(self->page_loader_mutex);
		if (self->page_loader_shutdown)
		{
			nicety_page_window_cpu_result_free(result);
			SDL_UnlockMutex(self->page_loader_mutex);
			continue;
		}
		if (my_doc_gen != self->doc_load_token)
		{
			nicety_page_window_cpu_result_free(result);
			SDL_UnlockMutex(self->page_loader_mutex);
			continue;
		}
		if (my_seq != self->page_loader_request_seq)
		{
			nicety_page_window_cpu_result_free(result);
			SDL_UnlockMutex(self->page_loader_mutex);
			continue;
		}
		if (self->page_loader_completed != NULL)
		{
			nicety_page_window_cpu_result_free(self->page_loader_completed);
			self->page_loader_completed = NULL;
		}
		self->page_loader_completed      = result;
		self->page_loader_have_completed = true;
		SDL_UnlockMutex(self->page_loader_mutex);
	}

	return 0;
}

static bool path_has_pdf_extension(const char *path)
{
	size_t n;
	if (path == NULL)
	{
		return false;
	}
	n = strlen(path);
	if (n < 4)
	{
		return false;
	}
	path += n - 4;
	return path[0] == '.' && (path[1] == 'p' || path[1] == 'P') && (path[2] == 'd' || path[2] == 'D') && (path[3] == 'f' || path[3] == 'F');
}

static void app_close_document(App *self)
{
	self->sidebar_viewport_valid                 = false;
	self->content_sidebar_link_seeded            = false;
	self->content_scroll_y_sidebar_link_baseline = 0.0f;
	if (self->document != NULL)
	{
		self->doc_load_token++;
	}
	page_loader_cancel(self);
	if (self->document != NULL)
	{
		document_destroy(self->document_ctx, self->document);
		self->document = NULL;
	}
	if (self->document_ctx != NULL)
	{
		document_context_destroy(self->document_ctx);
		self->document_ctx = NULL;
	}
	if (self->document_arena != NULL)
	{
		arena_destroy(self->document_arena);
		self->document_arena = NULL;
	}
}

static int app_open_pdf(App *self, Application *core, char *file_path_owned)
{
	mem_arena       *arena = arena_init(DOCUMENT_ARENA_BYTES);
	DocumentContext *ctx;
	Document        *doc;

	if (arena == NULL)
	{
		SDL_free(file_path_owned);
		return 1;
	}

	ctx = document_context_init(arena, file_path_owned);
	if (ctx == NULL)
	{
		goto fail_arena;
	}

	doc = PUSH_STRUCT(arena, Document);
	memset(doc, 0, sizeof *doc);
	doc->session                         = ctx;
	doc->arena_checkpoint_after_document = arena->pos;

	if (document_measure_pages(ctx, doc) != 0)
	{
		goto fail_ctx;
	}

	{
		float inner_w = 1.0f;
		int   rw, rh;
		if (SDL_GetRenderOutputSize(core->renderer, &rw, &rh))
		{
			float layout_w = (float) rw - 4.0f;
			self->sidebar_visible = (layout_w >= NICETY_DOC_MIN_LAYOUT_W_FOR_SIDEBAR);
			if (self->sidebar_visible)
			{
				inner_w = layout_w - NICETY_DOC_SIDEBAR_OUTER_W - 2.0f * NICETY_DOC_CONTENT_PAD;
			}
			else
			{
				inner_w = layout_w - 2.0f * NICETY_DOC_CONTENT_PAD;
			}
			if (inner_w < 1.0f)
			{
				inner_w = 1.0f;
			}
		}
		else
		{
			self->sidebar_visible = true;
		}
		bool fill = (self->view_mode == VIEW_MODE_FILL);
		if (document_load_page_window(ctx, core, 0, 0, NICETY_PAGE_WINDOW_RADIUS_MAIN, NICETY_PAGE_WINDOW_RADIUS_SIDEBAR, file_path_owned, doc,
		                            NICETY_RENDER_NORMAL, fill, inner_w) != 0)
		{
			document_destroy(ctx, doc);
			goto fail_ctx;
		}
	}

	self->document_arena = arena;
	self->document_ctx   = ctx;
	self->document       = doc;
	self->doc_load_token++;
	self->view_mode_prev = self->view_mode;
	return 0;

fail_ctx:
	document_context_destroy(ctx);
fail_arena:
	arena_destroy(arena);
	SDL_free(file_path_owned);
	return 1;
}

/* Consumes path_owned (SDL_strdup or equivalent). */
static bool app_try_open_pdf_path(App *self, Application *core, char *path_owned, const char *msg_bad_ext)
{
	if (!path_has_pdf_extension(path_owned))
	{
		fprintf(stderr, "%s\n", msg_bad_ext);
		SDL_free(path_owned);
		return false;
	}
	if (app_open_pdf(self, core, path_owned) != 0)
	{
		fprintf(stderr, "Couldn't load PDF (MuPDF/SDL)\n");
		self->program_state = LOAD_FILE;
		return false;
	}
	self->program_state = FILE_VIEW;
	return true;
}

static void app_toggle_view_mode(App *self)
{
	bool old_fit = (self->view_mode == VIEW_MODE_FIT_HEIGHT);

	if (self->document != NULL && self->document->session != NULL && self->document->session->total_pages > 0 && self->content_scroll_valid && self->content_viewport_valid && self->content_viewport_width > 1.0f)
	{
		float new_y;
		if (document_remap_scroll_y_for_view_mode(self->document, self->content_scroll_offset.y, self->content_viewport_width,
		                                          self->content_viewport_height, old_fit, !old_fit, &new_y))
		{
			self->content_scroll_offset.y = new_y;
			Clay_ScrollContainerData cd   = Clay_GetScrollContainerData(CLAY_ID("Content"));
			if (cd.found && cd.scrollPosition)
			{
				cd.scrollPosition->y = new_y;
			}
		}
	}

	self->view_mode = old_fit ? VIEW_MODE_FILL : VIEW_MODE_FIT_HEIGHT;
}

void app_init(App *self)
{
	memset(self, 0, sizeof *self);
	self->sensitivity            = 3;
	self->program_state          = LOAD_FILE;
	self->view_mode_prev         = VIEW_MODE_FILL;
	self->document               = NULL;
	self->document_ctx           = NULL;
	self->document_arena         = NULL;
	self->sidebar_scroll_valid   = false;
	self->content_scroll_valid   = false;
	self->content_viewport_valid = false;
	self->sidebar_viewport_valid = false;
	self->sidebar_visible        = true;
	page_loader_init(self);
}

void app_destroy(App *self)
{
	page_loader_shutdown(self);
	app_close_document(self);
}

void app_on_render(App *self, void *renderer)
{
	Application          *app       = (Application *) renderer;
	Clay_SDL3RendererData clay_data = {
	    .renderer   = app->renderer,
	    .textEngine = app->ttf_renderer,
	    .fonts      = app->fonts,
	};
	SDL_Clay_RenderClayCommands(&clay_data, &self->ui_commands);
}

void app_on_update(App *self, Application *core)
{
	switch (self->program_state)
	{
		case LOAD_FILE:
		{
			self->ui_commands = ui_load_file_layout();
		}
		break;
		case FILE_VIEW:
		{
			/* Match current window size every frame, not only on WINDOW_RESIZED, so ui_predict_content_viewport
			 * and Clay layout stay aligned when the sidebar auto-hides or reappears. */
			app_sync_sidebar_visibility(self, core);
			bool view_mode_changed = (self->view_mode != self->view_mode_prev);
			page_loader_try_commit(self, core);
			if (self->document != NULL && self->document->session != NULL && self->document->session->total_pages > 0 && self->content_scroll_valid && self->content_viewport_valid && self->content_viewport_width > 1.0f && core != NULL)
			{
				bool   fit     = (self->view_mode == VIEW_MODE_FIT_HEIGHT);
				size_t c       = document_page_at_scroll_y(self->document, self->content_scroll_offset.y, self->content_viewport_width,
				                                           self->content_viewport_height, fit);
				float  sb_inner = NICETY_DOC_SIDEBAR_OUTER_W - 2.0f * NICETY_DOC_SIDEBAR_PAD;
				size_t c_side   = c;
				if (self->sidebar_visible && self->sidebar_scroll_valid && self->sidebar_viewport_valid && self->document->page_layout_w != NULL)
				{
					c_side = document_page_at_sidebar_scroll_y(self->document, self->sidebar_scroll_offset.y, sb_inner,
					                                           self->sidebar_viewport_height);
				}
				bool   fill    = (self->view_mode == VIEW_MODE_FILL);
				float  inner_w = self->content_viewport_width - 2.0f * NICETY_DOC_CONTENT_PAD;
				if (inner_w < 1.0f)
				{
					inner_w = 1.0f;
				}

				bool lane_needs_load = (c != self->document->window_center) || (c_side != self->document->window_sidebar_center);
				bool ctx_changed     = (fill != (bool) self->document->raster_fill_width_mode) || (fabsf(inner_w - self->document->raster_content_inner_w) > 3.0f);

				if (lane_needs_load || view_mode_changed || ctx_changed)
				{
					if (self->page_loader_thread != NULL)
					{
						page_loader_enqueue(self, core, self->document, c, c_side, fill, inner_w);
					}
					else if (document_load_page_window(self->document_ctx, core, c, c_side, NICETY_PAGE_WINDOW_RADIUS_MAIN,
					                                   NICETY_PAGE_WINDOW_RADIUS_SIDEBAR, self->document->file_path, self->document,
					                                   NICETY_RENDER_NORMAL, fill, inner_w) != 0)
					{
						fprintf(stderr, "document_load_page_window failed\n");
					}
				}
			}
			self->view_mode_prev = self->view_mode;
			{
				float lw = 800.0f, lh = 600.0f;
				if (core != NULL && core->renderer != NULL)
				{
					int rw, rh;
					if (SDL_GetRenderOutputSize(core->renderer, &rw, &rh))
					{
						lw = (float) rw;
						lh = (float) rh;
					}
				}
				self->ui_commands = ui_document_view(*self->document, self, lw, lh);
			}
		}
		break;
		default:
			break;
	}
}

static void app_sync_clay_layout_to_renderer(Application *core)
{
	int w, h;
	if (SDL_GetRenderOutputSize(core->renderer, &w, &h))
	{
		Clay_SetLayoutDimensions((Clay_Dimensions) {(float) w, (float) h});
	}
}

static void app_sync_sidebar_visibility(App *self, Application *core)
{
	int rw, rh;

	if (core == NULL || core->renderer == NULL)
	{
		return;
	}
	if (!SDL_GetRenderOutputSize(core->renderer, &rw, &rh))
	{
		return;
	}
	self->sidebar_visible = ((float) rw - 4.0f) >= NICETY_DOC_MIN_LAYOUT_W_FOR_SIDEBAR;
}

void app_on_event(App *self, Application *core, Event event, float deltaTime)
{
	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			app_sync_clay_layout_to_renderer(core);
			app_sync_sidebar_visibility(self, core);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y}, event.button.button == SDL_BUTTON_LEFT);
			if (self->program_state == FILE_VIEW && event.button.button == SDL_BUTTON_LEFT && Clay_PointerOver(Clay_GetElementId(CLAY_STRING("ViewModeBtn"))))
			{
				app_toggle_view_mode(self);
			}
			if (self->program_state == LOAD_FILE && event.button.button == SDL_BUTTON_LEFT)
			{
				char const *filter[]   = {"*.pdf"};
				char       *input_path = tinyfd_openFileDialog("Select a PDF", "./resources/", 1, filter, "PDF File", false);
				if (input_path)
				{
					app_close_document(self);
					char *input_path_copy = SDL_strdup(input_path);
					if (input_path_copy == NULL)
					{
						fprintf(stderr, "Out of memory copying path\n");
						break;
					}
					if (!app_try_open_pdf_path(self, core, input_path_copy, "Please select a .pdf file"))
					{
						break;
					}
				}
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y},
			                     (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			Clay_SetPointerState((Clay_Vector2) {event.motion.x, event.motion.y}, event.motion.state & SDL_BUTTON_LMASK);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
		{
			Clay_UpdateScrollContainers(true, (Clay_Vector2) {(float) event.wheel.x * self->sensitivity, (float) event.wheel.y * self->sensitivity}, deltaTime);
		}
		break;
		case SDL_EVENT_DROP_FILE:
		{
			app_close_document(self);
			char *file_path_copy = SDL_strdup(event.drop.data);
			if (file_path_copy == NULL)
			{
				fprintf(stderr, "Out of memory copying path\n");
				break;
			}
			if (!app_try_open_pdf_path(self, core, file_path_copy, "Please drop a .pdf file"))
			{
				break;
			}
		}
		break;
		default:
			break;
	}
}
