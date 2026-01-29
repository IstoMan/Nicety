#pragma once
#include "layer.h"

typedef struct
{
	ILayer      interface;
	const char *string;
} Menu;

void menu_on_update(void *self);
void menu_on_render(void *self);
void menu_on_event(void *self, Event event);
void memu_init(Menu *menu);
