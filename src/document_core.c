#include "document.h"
#include "document_sdl_upload.h"
#include "arena.h"
#include <mupdf/fitz.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

DocumentContext *document_context_init(mem_arena *document_arena, const char *file_path)
{
	DocumentContext *session = PUSH_STRUCT(document_arena, DocumentContext);
	session->document_arena  = document_arena;

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "Failed to create MuPDF context\n");
		arena_clear(document_arena);
		return NULL;
	}

	fz_register_document_handlers(ctx);

	fz_document *doc = fz_open_document(ctx, file_path);
	if (doc == NULL)
	{
		fprintf(stderr, "Failed to load document\n");
		fz_drop_context(ctx);
		arena_clear(document_arena);
		return NULL;
	}

	int np = fz_count_pages(ctx, doc);
	if (np < 0)
	{
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		arena_clear(document_arena);
		return NULL;
	}

	session->ctx         = ctx;
	session->doc         = doc;
	session->total_pages = (size_t) np;

	return session;
}

void document_context_destroy(DocumentContext *session)
{
	if (session == NULL)
	{
		return;
	}
	fz_drop_document(session->ctx, session->doc);
	fz_drop_context(session->ctx);
	arena_clear(session->document_arena);
}

void document_destroy(DocumentContext *session, Document *document)
{
	(void) session;
	if (document == NULL)
	{
		return;
	}
	if (document->pages != NULL)
	{
		doc_sdl_pages_destroy_textures(document->pages, document->number_of_pages);
	}
	if (document->page_layout_heap)
	{
		free(document->page_layout_w);
		free(document->page_layout_h);
	}
	SDL_free((void *) document->file_path);
}
