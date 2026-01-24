#include "nicety.h"
#include <mupdf/fitz.h>
#include <stdlib.h>

int init_document_mupdf(const char *file_path, Document *document)
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
	}

	fz_drop_page(ctx, page);
	fz_drop_pixmap(ctx, pix);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}

int init_document(const char *file_path, Document *document)
{
	return init_document_mupdf(file_path, document);
}
