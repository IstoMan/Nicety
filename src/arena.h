#pragma once
#include <assert.h>
#include <stddef.h>
#include "utils.h"

typedef struct
{
	u64 capacity;
	u64 pos;
} mem_arena;

#define ARENA_BASE_POS (sizeof(mem_arena))
#define ALIGN_UP_POW2(n, p) (((u64) (n) + ((u64) (p) - 1)) & ~((u64) (p) - 1))
#define ARENA_ALIGN (sizeof(void *))

#define PUSH_STRUCT(arena, T) (T *) arena_push((arena), sizeof(T), false)
#define PUSH_STRUCT_NZ(arena, T) (T *) arena_push((arena), sizeof(T), true)
#define PUSH_ARRAY(arena, T, n) (T *) arena_push((arena), sizeof(T) * (n), false)
#define PUSH_ARRAY_NZ(arena, T, n) (T *) arena_push((arena), sizeof(T) * (n), true)

mem_arena *arena_init(u64 size);
void       arena_destroy(mem_arena *arena);
void      *arena_push(mem_arena *arena, u64 size, b8 zero_it);
void       arena_pop(mem_arena *arena, u64 size);
void       arena_pop_to(mem_arena *arena, u64 till);
void       arena_clear(mem_arena *arena);
