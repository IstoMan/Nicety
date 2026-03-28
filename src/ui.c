#include "ui.h"
#include "document.h"

/*
 * Scroll areas follow Clay’s pattern: clip + childOffset on the scroll parent, children laid out
 * with TOP_TO_BOTTOM and childGap. See resources/Clay.md ("Scrolling Elements") and
 * Clay_GetScrollContainerData (persist last frame’s offset so layout matches internal scroll state).
 *
 * Helpers named ui_doc_* that emit CLAY(...) must only be called between Clay_BeginLayout and
 * Clay_EndLayout. ui_doc_capture_scroll_state runs after Clay_EndLayout.
 */

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

static float sidebar_inner_width(void)
{
	return NICETY_DOC_SIDEBAR_OUTER_W - 2.0f * NICETY_DOC_SIDEBAR_PAD;
}

static Clay_Vector2 ui_scroll_persist(Clay_ScrollContainerData d, bool have_last, Clay_Vector2 last)
{
	if (d.found && d.scrollPosition)
	{
		return *d.scrollPosition;
	}
	return have_last ? last : (Clay_Vector2) {0, 0};
}

static void ui_sidebar_virt_top(float h)
{
	if (h <= 0.5f)
	{
		return;
	}
	CLAY(CLAY_ID("SidebarVirtTop"), {
	                                    .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(h)}},
	                                })
	{
	}
}

static void ui_sidebar_virt_bottom(float h)
{
	if (h <= 0.5f)
	{
		return;
	}
	CLAY(CLAY_ID("SidebarVirtBottom"), {
	                                       .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(h)}},
	                                   })
	{
	}
}

static void ui_sidebar_virt_gap(size_t i)
{
	CLAY(CLAY_IDI("SidebarVirtGap", i), {
	                                        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(NICETY_DOC_SIDEBAR_INTER_GAP)}},
	                                    })
	{
	}
}

static void ui_content_virt_top(float h)
{
	if (h <= 0.5f)
	{
		return;
	}
	CLAY(CLAY_ID("ContentVirtTop"), {
	                                    .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(h)}},
	                                })
	{
	}
}

static void ui_content_virt_bottom(float h)
{
	if (h <= 0.5f)
	{
		return;
	}
	CLAY(CLAY_ID("ContentVirtBottom"), {
	                                       .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(h)}},
	                                   })
	{
	}
}

static void ui_content_virt_gap(size_t i)
{
	CLAY(CLAY_IDI("ContentVirtGap", i), {
	                                        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(NICETY_DOC_CONTENT_INTER_PAGE_GAP)}},
	                                    })
	{
	}
}

static void ui_doc_header(App *app)
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
			CLAY_TEXT(app->view_mode == VIEW_MODE_FILL ? CLAY_STRING("Mode: Fill") : CLAY_STRING("Mode: Fit"), CLAY_TEXT_CONFIG({
			                                                                                                       .fontId    = FONT_ID_0,
			                                                                                                       .fontSize  = 16,
			                                                                                                       .textColor = {255, 255, 255, 255},
			                                                                                                   }));
		}
	}
}

static void ui_doc_sidebar(const Document *doc, Clay_Vector2 sidebarOffset, Clay_ScrollContainerData sidebarData, size_t total_pages)
{
	float  sb_inner = sidebar_inner_width();
	float  vh       = sidebarData.found ? sidebarData.scrollContainerDimensions.height : 0.0f;
	size_t lo, hi;
	float  spacer_top, spacer_bottom;
	size_t i;

	if (doc->page_layout_w == NULL)
	{
		CLAY(CLAY_ID("Sidebar"), {
		                             .backgroundColor = {54, 58, 79, 255},
		                             .clip            = {.vertical = true, .childOffset = sidebarOffset},
		                             .layout          = {
		                                          .sizing = {
		                                              .height = CLAY_SIZING_GROW(0),
		                                              .width  = CLAY_SIZING_FIXED(NICETY_DOC_SIDEBAR_OUTER_W),
                                         },
		                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
		                                          .childGap        = 0,
		                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
		                                          .padding         = CLAY_PADDING_ALL(NICETY_DOC_SIDEBAR_PAD),
                                     },
		                         })
		{
		}
		return;
	}

	document_visible_sidebar_range(doc, sb_inner, sidebarOffset.y, vh, &lo, &hi, &spacer_top, &spacer_bottom);

	CLAY(CLAY_ID("Sidebar"), {
	                             .backgroundColor = {54, 58, 79, 255},
	                             .clip            = {.vertical = true, .childOffset = sidebarOffset},
	                             .layout          = {
	                                          .sizing = {
	                                              .height = CLAY_SIZING_GROW(0),
	                                              .width  = CLAY_SIZING_FIXED(NICETY_DOC_SIDEBAR_OUTER_W),
                                     },
	                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
	                                          .childGap        = 0,
	                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
	                                          .padding         = CLAY_PADDING_ALL(NICETY_DOC_SIDEBAR_PAD),
                                 },
	                         })
	{
		if (total_pages == 0)
		{
		}
		else
		{
			ui_sidebar_virt_top(spacer_top);

			for (i = lo; i <= hi; i++)
			{
				if (i > lo)
				{
					ui_sidebar_virt_gap(i);
				}
				float layout_aspect = doc->page_layout_w[i] / doc->page_layout_h[i];
				float img_w         = sb_inner;
				float img_h         = img_w / layout_aspect;
				Page *p             = document_page_for_index(doc, i);

				if (p != NULL)
				{
					void *img_tex = p->thumb_texture != NULL ? p->thumb_texture : p->page_texture;
					float aw      = p->thumb_texture != NULL ? (float) p->thumb_bitmap.width : (float) p->page_bitmap.width;
					float ah      = p->thumb_texture != NULL ? (float) p->thumb_bitmap.height : (float) p->page_bitmap.height;
					CLAY(CLAY_IDI("DocSidebarPage", i), {
					                                        .layout = {
					                                            .sizing = {
					                                                .width  = CLAY_SIZING_FIXED(img_w),
					                                                .height = CLAY_SIZING_FIXED(img_h),
					                                            },
					                                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
					                                        },
					                                        .aspectRatio = {aw / ah},
					                                        .image       = {
					                                                  .imageData = img_tex,
                                                            },
					                                        .border = {
					                                            .width = CLAY_BORDER_ALL(1),
					                                            .color = {138, 173, 244, 255},
					                                        },
					                                    })
					{
					}
				}
				else
				{
					CLAY(CLAY_IDI("DocSidebarPage", i), {
					                                        .layout = {
					                                            .sizing = {
					                                                .width  = CLAY_SIZING_FIXED(img_w),
					                                                .height = CLAY_SIZING_FIXED(img_h),
					                                            },
					                                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
					                                        },
					                                        .backgroundColor = {40, 42, 58, 255},
					                                        .border          = {
					                                                     .width = CLAY_BORDER_ALL(1),
					                                                     .color = {80, 85, 110, 255},
                                                            },
					                                    })
					{
					}
				}
			}

			ui_sidebar_virt_bottom(spacer_bottom);
		}
	}
}

static void ui_doc_content(const Document *doc, App *app, Clay_ScrollContainerData contentData, Clay_Vector2 contentOffset,
                           float content_inner_w, float viewport_w, size_t total_pages)
{
	bool   fit = app->view_mode == VIEW_MODE_FIT_HEIGHT;
	float  vh  = contentData.found ? contentData.scrollContainerDimensions.height : 0.0f;
	size_t lo, hi;
	float  spacer_top, spacer_bottom;
	size_t i;

	if (doc->page_layout_w == NULL)
	{
		CLAY(CLAY_ID("Content"), {
		                             .backgroundColor = {24, 25, 38, 255},
		                             .clip            = {.vertical = true, .childOffset = contentOffset},
		                             .layout          = {
		                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
		                                          .sizing          = grow_sizing,
		                                          .padding         = CLAY_PADDING_ALL(NICETY_DOC_CONTENT_PAD),
		                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
		                                          .childGap        = 0,
                                     },
		                         })
		{
		}
		return;
	}

	document_visible_content_range(doc, content_inner_w, contentOffset.y, viewport_w, vh, fit, &lo, &hi, &spacer_top, &spacer_bottom);

	CLAY(CLAY_ID("Content"), {
	                             .backgroundColor = {24, 25, 38, 255},
	                             .clip            = {.vertical = true, .childOffset = contentOffset},
	                             .layout          = {
	                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
	                                          .sizing          = grow_sizing,
	                                          .padding         = CLAY_PADDING_ALL(NICETY_DOC_CONTENT_PAD),
	                                          .childAlignment  = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
	                                          .childGap        = 0,
                                 },
	                         })
	{
		if (total_pages == 0)
		{
		}
		else
		{
			ui_content_virt_top(spacer_top);

			for (i = lo; i <= hi; i++)
			{
				if (i > lo)
				{
					ui_content_virt_gap(i);
				}

				float layout_aspect = doc->page_layout_w[i] / doc->page_layout_h[i];
				Page *p             = document_page_for_index(doc, i);

				Clay_Sizing pageSizing;
				if (app->view_mode == VIEW_MODE_FIT_HEIGHT && contentData.found && contentData.scrollContainerDimensions.height > 40)
				{
					float target_height = contentData.scrollContainerDimensions.height - NICETY_DOC_FIT_HEIGHT_TOP_RESERVE;
					float aspect_use    = p ? ((float) p->page_bitmap.width / (float) p->page_bitmap.height) : layout_aspect;
					pageSizing          = (Clay_Sizing) {
					             .width  = CLAY_SIZING_FIXED(target_height * aspect_use),
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

				if (p != NULL)
				{
					CLAY(CLAY_IDI("DocContentPage", i), {
					                                        .layout = {
					                                            .sizing = pageSizing,
					                                        },
					                                        .aspectRatio = {(float) p->page_bitmap.width / (float) p->page_bitmap.height},
					                                        .image       = {
					                                                  .imageData = p->page_texture,
                                                            },
					                                        .border = {
					                                            .width = CLAY_BORDER_ALL(1),
					                                            .color = {138, 173, 244, 255},
					                                        },
					                                    })
					{
					}
				}
				else
				{
					float ph = content_inner_w / layout_aspect;
					if (app->view_mode == VIEW_MODE_FIT_HEIGHT && contentData.found && contentData.scrollContainerDimensions.height > 40)
					{
						ph = contentData.scrollContainerDimensions.height - NICETY_DOC_FIT_HEIGHT_TOP_RESERVE;
					}
					float pw = ph * layout_aspect;
					CLAY(CLAY_IDI("DocContentPage", i), {
					                                        .layout = {
					                                            .sizing = {
					                                                .width  = CLAY_SIZING_FIXED(pw),
					                                                .height = CLAY_SIZING_FIXED(ph),
					                                            },
					                                        },
					                                        .backgroundColor = {40, 42, 58, 255},
					                                        .border          = {
					                                                     .width = CLAY_BORDER_ALL(1),
					                                                     .color = {80, 85, 110, 255},
                                                            },
					                                    })
					{
					}
				}
			}

			ui_content_virt_bottom(spacer_bottom);
		}
	}
}

static void ui_doc_capture_scroll_state(App *app)
{
	if (app->program_state != FILE_VIEW)
	{
		return;
	}

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
	if (contentData.found)
	{
		app->content_viewport_width  = contentData.scrollContainerDimensions.width;
		app->content_viewport_height = contentData.scrollContainerDimensions.height;
		app->content_viewport_valid  = true;
	}
}

Clay_RenderCommandArray ui_document_view(const Document doc, App *app)
{
	size_t total_pages;

	if (doc.session == NULL)
	{
		total_pages = 0;
	}
	else
	{
		total_pages = doc.session->total_pages;
	}

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
		ui_doc_header(app);
		CLAY(CLAY_ID("Body"), {
		                          .backgroundColor = base_color,
		                          .layout          = {
		                                       .layoutDirection = CLAY_LEFT_TO_RIGHT,
		                                       .sizing          = grow_sizing,
                                  },
		                      })
		{
			Clay_ScrollContainerData sidebarData   = Clay_GetScrollContainerData(CLAY_ID("Sidebar"));
			Clay_Vector2             sidebarOffset = ui_scroll_persist(sidebarData, app->sidebar_scroll_valid, app->sidebar_scroll_offset);

			ui_doc_sidebar(&doc, sidebarOffset, sidebarData, total_pages);

			Clay_ScrollContainerData contentData   = Clay_GetScrollContainerData(CLAY_ID("Content"));
			Clay_Vector2             contentOffset = ui_scroll_persist(contentData, app->content_scroll_valid, app->content_scroll_offset);
			float                    content_inner_w =
                (contentData.found && contentData.scrollContainerDimensions.width > 2.0f * NICETY_DOC_CONTENT_PAD) ? (contentData.scrollContainerDimensions.width - 2.0f * NICETY_DOC_CONTENT_PAD) : 1.0f;
			float content_viewport_w = contentData.found ? contentData.scrollContainerDimensions.width : 1.0f;

			ui_doc_content(&doc, app, contentData, contentOffset, content_inner_w, content_viewport_w, total_pages);
		}
	}

	Clay_RenderCommandArray commands = Clay_EndLayout();

	ui_doc_capture_scroll_state(app);

	return commands;
}
