#include <SDL3/SDL_events.h>
#include <stddef.h>

typedef SDL_Event Event;

typedef struct
{
	void (*on_update)(void *self);
	void (*on_event)(void *self, Event event);
	void (*on_render)(void *self);
} ILayer;

typedef struct
{
	ILayer *layers;
	size_t  size;
} ILayer_Array;
