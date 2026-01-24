#include "ui.h"
#include "application_core.h"

Clay_RenderCommandArray nicety_create_layout(void)
{
	Clay_BeginLayout();
	CLAY(CLAY_ID("Outer"), {
	                           .backgroundColor = {36, 39, 58, 255},
	                           .layout          = {
	                                        .sizing = {
	                                            .height = CLAY_SIZING_GROW(0),
	                                            .width  = CLAY_SIZING_GROW(0),
                                   },
	                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                   // .padding = CLAY_PADDING_ALL(10),
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
		                          .backgroundColor = {73, 77, 100, 255},
		                          .layout          = {
		                                       .layoutDirection = CLAY_LEFT_TO_RIGHT,
		                                       .sizing          = {
		                                                    .width  = CLAY_SIZING_GROW(0),
		                                                    .height = CLAY_SIZING_GROW(0),
                                      },
                                  },
		                      })
		{
			CLAY(CLAY_ID("Sidebar"), {
			                             .backgroundColor = {30, 32, 48, 255},
			                             .layout          = {
			                                          .sizing = {
			                                              .height = CLAY_SIZING_GROW(0),
			                                              .width  = CLAY_SIZING_FIXED(150),
                                             },
                                         },
			                         })
			{}
			CLAY(CLAY_ID("Content"), {.image = {}

			                         })
			{}
		}
	}

	return Clay_EndLayout();
}
