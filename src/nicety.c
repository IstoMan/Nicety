#include "nicety.h"
#include <mupdf/fitz.h>
#include "tinyfiledialogs.h"
#include <stdlib.h>
#include "clay_renderer_SDL3.h"
#include "core.h"
#include "stdio.h"

void handle_clay_errors(Clay_ErrorData errorData);
void page_init(Page *page, Application *core);

Clay_RenderCommandArray nicety_load_file_ui(void);
Clay_RenderCommandArray nicety_file_view_ui(const Document doc, App *app);

int document_init_mupdf(Document **document, Application *core, const char *file_path);

static const int FONT_ID_0 = 0;

void app_init(App *self)
{
	memset(self, 0, sizeof *self);
	self->sensitivity          = 3;
	self->program_state        = LOAD_FILE;
	self->document             = NULL;
	self->sidebar_scroll_valid = false;
	self->content_scroll_valid = false;
}

void app_destroy(App *self)
{
	if (self->document != NULL)
	{
		document_destroy(self->document);
	}
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

void app_on_update(App *self)
{
	switch (self->program_state)
	{
		case LOAD_FILE:
		{
			self->ui_commands = nicety_load_file_ui();
		}
		break;
		case FILE_VIEW:
		{
			self->ui_commands = nicety_file_view_ui(*self->document, self);
		}
		break;
		default:
			break;
	}
}

void app_on_event(App *self, Application *core, Event event, float deltaTime)
{
	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
			Clay_SetLayoutDimensions((Clay_Dimensions) {(float) event.window.data1, (float) event.window.data2});
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y}, event.button.button == SDL_BUTTON_LEFT);
			if (self->program_state == LOAD_FILE && event.button.button == SDL_BUTTON_LEFT)
			{
				char const *filter[]   = {"*.pdf"};
				char       *input_path = tinyfd_openFileDialog("Select a PDF", "./resources/", 1, filter, "PDF File", false);
				if (input_path)
				{
					if (self->document != NULL)
					{
						document_destroy(self->document);
						self->document = NULL;
					}
					char *input_path_copy = SDL_strdup(input_path);
					int   err             = document_init(&self->document, core, input_path_copy);
					if (err == 1 || self->document == NULL)
					{
						fprintf(stderr, "Couldn't Load file %s\n", SDL_GetError());
						SDL_free(input_path_copy);
						exit(1);
					}
					self->program_state = FILE_VIEW;
				}
			}
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
			if (self->document != NULL)
			{
				document_destroy(self->document);
				self->document = NULL;
			}
			char *file_path_copy = SDL_strdup(event.drop.data);
			int   err            = document_init(&self->document, core, file_path_copy);
			if (err == 1 || self->document == NULL)
			{
				fprintf(stderr, "Couldn't Load file %s\n", SDL_GetError());
				SDL_free(file_path_copy);
				exit(1);
			}
			self->program_state = FILE_VIEW;
		}
		break;
		default:
			break;
	}
}

int document_init_mupdf(Document **document_out, Application *core, const char *file_path)
{
	Document *document = malloc(sizeof(Document));
	if (!document)
	{
		return 1;
	}

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create context\n");
		free(document);
		return 1;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document");
		free(document);
		return 1;
	}

	size_t number_of_pages    = fz_count_pages(ctx, doc);
	document->file_path       = file_path;
	document->number_of_pages = number_of_pages;
	document->pages           = calloc(number_of_pages, sizeof *document->pages);

	fz_page   *page = NULL;
	fz_pixmap *pix  = NULL;
	uint32_t   format;
	Bitmap     page_bitmap;

	for (size_t i = 0; i < number_of_pages; i++)
	{
		page = fz_load_page(ctx, doc, i);
		pix  = fz_new_pixmap_from_page(ctx, page, fz_identity, fz_device_rgb(ctx), 0);

		if (pix->n == 3)
		{
			format = COLOR_FORMAT_RGB;        // RGB
		}
		else if (pix->n == 4)
		{
			format = COLOR_FORMAT_RGBA;        // RGBA
		}
		else
		{
			fprintf(stderr, "Unsupported pixel format\n");
			free(document->pages);
			free(document);
			return 1;
		}

		page_bitmap.width         = pix->w,
		page_bitmap.height        = pix->h,
		page_bitmap.format        = format,
		page_bitmap.pixel_data    = pix->samples,
		page_bitmap.rows_per_byte = pix->stride,

		document->pages[i].index       = i;
		document->pages[i].page_bitmap = page_bitmap;
		page_init(&document->pages[i], core);
		/* Drop pixmap after texture upload; pixel_data is no longer needed */
		document->pages[i].page_bitmap.pixel_data = NULL;
		fz_drop_pixmap(ctx, pix);
		fz_drop_page(ctx, page);
	}

	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);

	*document_out = document;
	return 0;
}

void page_init_sdl(Page *page, Application *app)
{
	SDL_Surface *surface = NULL;
	SDL_Texture *texture = NULL;
	int          format;

	if (page->page_bitmap.format == COLOR_FORMAT_RGB)
	{
		format = SDL_PIXELFORMAT_RGB24;
	}
	else
	{
		format = SDL_PIXELFORMAT_RGBA32;
	}
	surface = SDL_CreateSurfaceFrom(page->page_bitmap.width, page->page_bitmap.height, format, page->page_bitmap.pixel_data, page->page_bitmap.rows_per_byte);
	texture = SDL_CreateTextureFromSurface(app->renderer, surface);

	SDL_DestroySurface(surface);
	page->page_texture = texture;
}

void page_init(Page *page, Application *core)
{
	page_init_sdl(page, core);
}

// Document

int document_init(Document **document, Application *core, const char *file_path)
{
	return document_init_mupdf(document, core, file_path);
}

void document_destroy(Document *document)
{
	for (size_t i = 0; i < document->number_of_pages; i++)
	{
		SDL_DestroyTexture(document->pages[i].page_texture);
	}
	free(document->pages);
	SDL_free((void *) document->file_path);
	free(document);
}

Clay_Color  base_color  = {36, 39, 58, 255};
Clay_Sizing grow_sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};

Clay_RenderCommandArray nicety_load_file_ui(void)
{
	Clay_BeginLayout();
	CLAY(CLAY_ID("Outer"), {
	                           .backgroundColor = base_color,
	                           .layout          = {
	                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
	                                        .sizing          = grow_sizing,
	                                        .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
	                       })
	{
		CLAY_TEXT(CLAY_STRING("Click to Select"), CLAY_TEXT_CONFIG({
		                                              .fontId    = FONT_ID_0,
		                                              .fontSize  = 50,
		                                              .textColor = {202, 211, 245, 255},
		                                          }));
		CLAY_TEXT(CLAY_STRING("or Drop a File"), CLAY_TEXT_CONFIG({
		                                             .fontId    = FONT_ID_0,
		                                             .fontSize  = 30,
		                                             .textColor = {202, 211, 245, 255},
		                                         }));
	}
	return Clay_EndLayout();
}

Clay_RenderCommandArray nicety_file_view_ui(const Document doc, App *app)
{
	Clay_BeginLayout();

	CLAY(CLAY_ID("Outer"), {
	                           .backgroundColor = base_color,
	                           .layout          = {
	                                        .sizing          = grow_sizing,
	                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
	                                        .padding         = CLAY_PADDING_ALL(2),
                               },
	                       })
	{
		CLAY(CLAY_ID("Header"), {
		                            .backgroundColor = {128, 135, 162, 255},
		                            .layout          = {
		                                         .sizing = {
		                                             .height = CLAY_SIZING_FIXED(40),
		                                             .width  = CLAY_SIZING_GROW(0),
                                        },
                                    },
		                        })
		{}
		CLAY(CLAY_ID("Body"), {
		                          .backgroundColor = base_color,
		                          .layout          = {
		                                       .layoutDirection = CLAY_LEFT_TO_RIGHT,
		                                       .sizing          = grow_sizing,
                                  },
		                      })
		{
			// Try to get current scroll offset, fall back to preserved value if not found
			Clay_ScrollContainerData sidebarData   = Clay_GetScrollContainerData(CLAY_ID("Sidebar"));
			Clay_Vector2             sidebarOffset = (sidebarData.found && sidebarData.scrollPosition) ? *sidebarData.scrollPosition : (app->sidebar_scroll_valid ? app->sidebar_scroll_offset : (Clay_Vector2) {0, 0});
			CLAY(CLAY_ID("Sidebar"), {
			                             .backgroundColor = {54, 58, 79, 255},
			                             .clip            = {.vertical = true, .childOffset = sidebarOffset},
			                             .layout          = {
			                                          .sizing = {
			                                              .height = CLAY_SIZING_GROW(0),
			                                              .width  = CLAY_SIZING_FIXED(150),
                                             },
			                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
			                                          .childGap        = 10,
			                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
			                                          .padding         = CLAY_PADDING_ALL(10),
                                         },
			                         })
			{
				float page_scale = 6;
				for (size_t i = 0; i < doc.number_of_pages; i++)
				{
					Page current_page = doc.pages[i];
					CLAY_AUTO_ID({
					    .layout = {
					        .sizing = {
					            .height = CLAY_SIZING_FIT(.min = current_page.page_bitmap.height / page_scale),
					            .width  = CLAY_SIZING_FIT(.min = current_page.page_bitmap.width / page_scale),
					        },
					        .layoutDirection = CLAY_TOP_TO_BOTTOM,
					    },
					    .image = {
					        .imageData = current_page.page_texture,
					    },
					    .border = {
					        .width = CLAY_BORDER_ALL(1),
					        .color = {138, 173, 244, 255},
					    },
					})
					{}
				}
			}

			// Try to get current scroll offset, fall back to preserved value if not found
			Clay_ScrollContainerData contentData   = Clay_GetScrollContainerData(CLAY_ID("Content"));
			Clay_Vector2             contentOffset = (contentData.found && contentData.scrollPosition) ? *contentData.scrollPosition : (app->content_scroll_valid ? app->content_scroll_offset : (Clay_Vector2) {0, 0});
			CLAY(CLAY_ID("Content"), {
			                             .backgroundColor = {24, 25, 38, 255},
			                             .clip            = {.vertical = true, .childOffset = contentOffset},
			                             .layout          = {
			                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
			                                          .sizing          = grow_sizing,
			                                          .padding         = CLAY_PADDING_ALL(20),
			                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
			                                          .childGap        = 20,
                                         },

			                         })
			{
				for (size_t i = 0; i < doc.number_of_pages; i++)
				{
					Page current_page = doc.pages[i];
					CLAY_AUTO_ID({
					    .layout = {
					        .sizing = {
					            .width  = CLAY_SIZING_FIT(.min = current_page.page_bitmap.width),
					            .height = CLAY_SIZING_FIT(.min = current_page.page_bitmap.height),
					        },
					    },
					    .image = {
					        .imageData = current_page.page_texture,
					    },
					    .border = {
					        .width = CLAY_BORDER_ALL(1),
					        .color = {138, 173, 244, 255},
					    },
					})
					{}
				}
			}
		}
	}

	Clay_RenderCommandArray commands = Clay_EndLayout();

	// Preserve scroll state after layout (for next frame, in case scroll containers get removed)
	if (app->program_state == FILE_VIEW)
	{
		Clay_ScrollContainerData sidebarData = Clay_GetScrollContainerData(CLAY_ID("Sidebar"));
		if (sidebarData.found && sidebarData.scrollPosition)
		{
			app->sidebar_scroll_offset = *sidebarData.scrollPosition;
			app->sidebar_scroll_valid  = true;
		}
		Clay_ScrollContainerData contentData = Clay_GetScrollContainerData(CLAY_ID("Content"));
		if (contentData.found && contentData.scrollPosition)
		{
			app->content_scroll_offset = *contentData.scrollPosition;
			app->content_scroll_valid  = true;
		}
	}

	return commands;
}
