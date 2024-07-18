#include <application.h>
#include <defines.h>
#include <engine.h>
#include <renderer/context.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define GAME_PATH "libgame.so"

typedef struct game {
    void *handle;
    struct timespec last_modified;
#define APP_API(name, ...) name##_t *name;
    LIST_OF_APP_APIS
#undef APP_API
} game;

static inline b8 is_before(struct timespec a, struct timespec b) {
    return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec);
}

b8 reload_game(game *game) {
    struct stat sb;
    stat(GAME_PATH, &sb);
    if (is_before(sb.st_mtim, game->last_modified)) {
        return true;
    }
    game->last_modified = sb.st_mtim;

    if (game->handle) {
        dlclose(game->handle);
    }

    char *module_path = realpath(GAME_PATH, NULL);

    game->handle = dlopen(module_path, RTLD_NOW);
    if (!game->handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return false;
    }

    free(module_path);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#define APP_API(name, ...)                                                                         \
    game->name = (name##_t *)dlsym(game->handle, #name);                                           \
    if (!game->name) {                                                                             \
        fprintf(stderr, "dlsym: %s\n", dlerror());                                                 \
        return false;                                                                              \
    }
    LIST_OF_APP_APIS
#undef APP_API
#pragma GCC diagnostic pop

    return true;
}

int main(void) {
    engine *engine = engine_create();

    game game = {0};

    reload_game(&game);

    game.app_init(engine);

    context_begin_main_loop(&engine->render_context);

    while (engine_should_keep_running(engine)) {
        reload_game(&game);
        engine_update(engine);
    }

    context_end_main_loop(&engine->render_context);

    game.app_cleanup();

    engine_cleanup(engine);

    dlclose(game.handle);

    return 0;
}
