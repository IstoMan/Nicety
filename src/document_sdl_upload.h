#pragma once

#include "document.h"

struct Application;

/* Bitmap / Page → SDL texture; shared by sync load and CPU commit. */

void doc_sdl_pages_destroy_textures(Page *pages, size_t count);

void doc_sdl_page_upload_texture(Application *app, const Bitmap *bm, void **out_tex);

void doc_sdl_page_init(Page *page, Application *core);

void doc_sdl_page_init_thumb(Page *page, Application *app);
