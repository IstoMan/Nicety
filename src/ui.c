#include "ui.h"
#include "document.h"

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

Clay_RenderCommandArray nicety_file_view_ui(Document doc)
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
