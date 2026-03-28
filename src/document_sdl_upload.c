#include "document_sdl_upload.h"
#include "core.h"
#include <SDL3/SDL.h>

static int sdl_pixel_format(PixelFormat fmt)
{
	return fmt == COLOR_FORMAT_RGB ? SDL_PIXELFORMAT_RGB24 : SDL_PIXELFORMAT_RGBA32;
}

void doc_sdl_page_upload_texture(Application *app, const Bitmap *bm, void **out_tex)
{
	SDL_Surface *surface;
	SDL_Texture *texture;
	int          format = sdl_pixel_format(bm->format);

	surface = SDL_CreateSurfaceFrom(bm->width, bm->height, format, bm->pixel_data, bm->rows_per_byte);
	texture = SDL_CreateTextureFromSurface(app->renderer, surface);
	SDL_DestroySurface(surface);
	if (texture != NULL)
	{
		SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
	}
	*out_tex = texture;
}

void doc_sdl_page_init(Page *page, Application *core)
{
	doc_sdl_page_upload_texture(core, &page->page_bitmap, &page->page_texture);
}

void doc_sdl_page_init_thumb(Page *page, Application *app)
{
	if (page->thumb_bitmap.width == 0 || page->thumb_bitmap.pixel_data == NULL)
	{
		return;
	}
	doc_sdl_page_upload_texture(app, &page->thumb_bitmap, &page->thumb_texture);
}

void doc_sdl_pages_destroy_textures(Page *pages, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (pages[i].thumb_texture != NULL)
		{
			SDL_DestroyTexture(pages[i].thumb_texture);
		}
		if (pages[i].page_texture != NULL)
		{
			SDL_DestroyTexture(pages[i].page_texture);
		}
	}
}
