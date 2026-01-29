#include "nicety.h"
#include <SDL3/SDL_render.h>
#include <mupdf/fitz.h>
#include <stdlib.h>
#include "core.h"
#include "stdio.h"

void handle_clay_errors(Clay_ErrorData errorData);
void page_init(Page *page, Application *core);

Clay_RenderCommandArray nicety_load_file_ui(void);
Clay_RenderCommandArray nicety_file_view_ui(const Document doc);

void handle_clay_errors(Clay_ErrorData errorData)
{
	fprintf(stderr, "%s\n", errorData.errorText.chars);
}

void app_init(App *self, WindowSpecs specs)
{
	self->scroll_state.x = 0;
	self->scroll_state.y = 0;
	self->sensitivity    = 5;
	self->program_state  = LOAD_FILE;
	self->document       = NULL;

	uint64_t total_memory_size = Clay_MinMemorySize();
	self->clay_memory          = (Clay_Arena) {
	             .memory   = malloc(total_memory_size),
	             .capacity = total_memory_size};

	Clay_Initialize(self->clay_memory, (Clay_Dimensions) {specs.width, specs.height}, (Clay_ErrorHandler) {handle_clay_errors});
}

void app_destroy(App *self)
{
	if (self->document != NULL)
	{
		document_destroy(self->document);
	}

	free(self->clay_memory.memory);
}

void app_on_render(App *app)
{
	switch (app->program_state)
	{
		case LOAD_FILE:
		{
			nicety_load_file_ui();
		}
		break;
		case FILE_VIEW:
		{
			nicety_file_view_ui(*app->document);
		}
		break;
		default:
			break;
	}
}

void app_on_event(App *app, Event event, float deltaTime)
{
	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
			Clay_SetLayoutDimensions((Clay_Dimensions) {(float) event.window.data1, (float) event.window.data2});
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			Clay_SetPointerState((Clay_Vector2) {event.button.x, event.button.y},
			                     event.button.button == SDL_BUTTON_LEFT);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			Clay_SetPointerState((Clay_Vector2) {event.motion.x, event.motion.y}, event.motion.state & SDL_BUTTON_LMASK);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			Clay_UpdateScrollContainers(true, (Clay_Vector2) {app->scroll_state.x, app->scroll_state.y}, deltaTime);
			app->scroll_state.x = event.wheel.x * app->sensitivity;
			app->scroll_state.y = event.wheel.y * app->sensitivity;
			break;
		default:
			break;
	}
}

int document_init_mupdf(Document *document, Application *core, const char *file_path)
{
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create context\n");
		return 1;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document");
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
	}

	fz_drop_page(ctx, page);
	fz_drop_pixmap(ctx, pix);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
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

int document_init(Document *document, Application *core, const char *file_path)
{
	return document_init_mupdf(document, core, file_path);
}

void document_destroy(Document *document)
{
	for (size_t i = 0; i < document->number_of_pages; i++)
	{
		SDL_DestroyTexture(document->pages->page_texture);
	}
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
	                                        .sizing = grow_sizing,
                               },
	                       })
	{}
	return Clay_EndLayout();
}

Clay_RenderCommandArray nicety_file_view_ui(const Document doc)
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
			CLAY(CLAY_ID("Sidebar"), {
			                             .backgroundColor = {54, 58, 79, 255},
			                             .layout          = {
			                                          .sizing = {
			                                              .height = CLAY_SIZING_GROW(0),
			                                              .width  = CLAY_SIZING_FIXED(150),
                                             },
                                         },
			                         })
			{}

			CLAY(CLAY_ID("Content"), {
			                             .backgroundColor = {24, 25, 38, 255},
			                             .clip            = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
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

	return Clay_EndLayout();
}
