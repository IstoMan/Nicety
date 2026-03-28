#pragma once
#include "app.h"

Clay_RenderCommandArray ui_load_file_layout(void);
Clay_RenderCommandArray ui_document_view(const Document doc, App *app, Application *core, float layout_w, float layout_h);
