#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

#define LIST_OF_APP_APIS                                                                           \
    APP_API(app_init, void, engine *)                                                              \
    APP_API(app_pre_reload, void *, void)                                                          \
    APP_API(app_post_reload, void, void *)                                                         \
    APP_API(app_cleanup, void, void)

#define APP_API(name, ret, ...) typedef ret(name##_t)(__VA_ARGS__);
LIST_OF_APP_APIS
#undef APP_API

#endif // APPLICATION_H
