#include "arena.h"
#include <string.h>
#include <stdlib.h>

mem_arena *arena_init(u64 size)
{
	mem_arena *temp = malloc(size);
	temp->capacity  = size;
	temp->pos       = ARENA_BASE_POS;

	return temp;
}

void arena_destroy(mem_arena *arena)
{
	free(arena);
}

void *arena_push(mem_arena *arena, u64 size, b8 zero_it)
{
	u64 pos_aligned = ALIGN_UP_POW2(arena->pos, ARENA_ALIGN);
	u64 new_pos     = pos_aligned + size;

	if (new_pos > arena->capacity)
		abort();

	arena->pos = new_pos;

	u8 *out = (u8 *) arena + pos_aligned;

	if (!zero_it)
	{
		memset(out, 0, size);
	}

	return out;
}

void arena_pop(mem_arena *arena, u64 size)
{
	size = MIN(size, arena->pos - ARENA_BASE_POS);
	arena->pos -= size;
}

void arena_pop_to(mem_arena *arena, u64 till)
{
	u64 size = till < arena->pos ? arena->pos - till : 0;
	arena->pos -= size;
}

void arena_clear(mem_arena *arena)
{
	arena_pop(arena, ARENA_BASE_POS);
}
