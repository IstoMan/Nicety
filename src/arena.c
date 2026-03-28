#include "arena.h"
#include <string.h>
#include <stdlib.h>

nicety_arena *arena_init(u64 size)
{
	nicety_arena *temp = malloc(size);
	temp->capacity  = size;
	temp->pos       = ARENA_BASE_POS;

	return temp;
}

void arena_destroy(nicety_arena *arena)
{
	free(arena);
}

void *arena_push(nicety_arena *arena, u64 size, b8 zero_it)
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

void arena_pop(nicety_arena *arena, u64 size)
{
	size = MIN(size, arena->pos - ARENA_BASE_POS);
	arena->pos -= size;
}

void arena_pop_to(nicety_arena *arena, u64 till)
{
	u64 size = till < arena->pos ? arena->pos - till : 0;
	arena->pos -= size;
}

void arena_clear(nicety_arena *arena)
{
	arena_pop(arena, ARENA_BASE_POS);
}
