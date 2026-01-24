typedef struct
{
	SDL_Window   *window;
	SDL_Renderer *renderer;
	bool          is_running;
} AppCore;

typedef struct
{
	uint32_t    width, height;
	const char *title;
	bool        turn_vsync_on;
} WindowSpecs;

bool core_application_init(AppCore *app, WindowSpecs specs);
void core_application_run(AppCore *app, uint32_t fps);
void application_cleanup(AppCore *core);
