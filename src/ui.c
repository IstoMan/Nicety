#include "ui.h"

static const int FONT_ID_0 = 0;

static Clay_Color  base_color  = {36, 39, 58, 255};
static Clay_Sizing grow_sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};

Clay_RenderCommandArray ui_load_file_layout(void)
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

static void toggle_view_mode(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData)
{
	(void) elementId;

	if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
	{
		App *app       = (App *) userData;
		app->view_mode = app->view_mode == VIEW_MODE_FILL ? VIEW_MODE_FIT_HEIGHT : VIEW_MODE_FILL;
	}
}

Clay_RenderCommandArray ui_document_view(const Document doc, App *app)
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
		                                .padding         = CLAY_PADDING_ALL(8),
		                                .childAlignment  = {.y = CLAY_ALIGN_Y_CENTER},
		                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
		                            },
		                        })
		{
			CLAY(CLAY_ID("ViewModeBtn"), {
			                                 .backgroundColor = Clay_Hovered() ? (Clay_Color) {150, 160, 200, 255} : (Clay_Color) {100, 110, 150, 255},
			                                 .layout          = {.padding = CLAY_PADDING_ALL(4)},
			                                 .cornerRadius    = CLAY_CORNER_RADIUS(4),
			                             })
			{
				Clay_OnHover(toggle_view_mode, (intptr_t) app);
				CLAY_TEXT(app->view_mode == VIEW_MODE_FILL ? CLAY_STRING("Mode: Fill") : CLAY_STRING("Mode: Fit"), CLAY_TEXT_CONFIG({
				                                                                                                       .fontId    = FONT_ID_0,
				                                                                                                       .fontSize  = 16,
				                                                                                                       .textColor = {255, 255, 255, 255},
				                                                                                                   }));
			}
		}
		CLAY(CLAY_ID("Body"), {
		                          .backgroundColor = base_color,
		                          .layout          = {
		                              .layoutDirection = CLAY_LEFT_TO_RIGHT,
		                              .sizing          = grow_sizing,
		                          },
		                      })
		{
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
				for (size_t i = 0; i < doc.number_of_pages; i++)
				{
					Page current_page = doc.pages[i];
					CLAY_AUTO_ID({
					    .layout = {
					        .sizing = {
					            .width  = CLAY_SIZING_GROW(0),
					            .height = CLAY_SIZING_GROW(0),
					        },
					        .layoutDirection = CLAY_TOP_TO_BOTTOM,
					    },
					    .aspectRatio = {(float) current_page.page_bitmap.width / current_page.page_bitmap.height},
					    .image       = {
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

					Clay_Sizing pageSizing;
					if (app->view_mode == VIEW_MODE_FIT_HEIGHT && contentData.found && contentData.scrollContainerDimensions.height > 40)
					{
						float target_height = contentData.scrollContainerDimensions.height - 40;
						float aspect_ratio  = (float) current_page.page_bitmap.width / current_page.page_bitmap.height;
						pageSizing          = (Clay_Sizing) {
						    .width  = CLAY_SIZING_FIXED(target_height * aspect_ratio),
						    .height = CLAY_SIZING_FIXED(target_height),
						};
					}
					else
					{
						pageSizing = (Clay_Sizing) {
						    .width  = CLAY_SIZING_GROW(0),
						    .height = CLAY_SIZING_GROW(0),
						};
					}

					CLAY_AUTO_ID({
					    .layout = {
					        .sizing = pageSizing,
					    },
					    .aspectRatio = {(float) current_page.page_bitmap.width / current_page.page_bitmap.height},
					    .image       = {
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
