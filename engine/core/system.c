#include "system.h"
#include "containers/darray.h"

system_handle engine_add_system(engine *e, system_create_info create_info) {
    user_system s = {
        .update = create_info.update,
    };

    create_info.init(e, &s.data);

    system_handle handle = darray_length(e->user_systems);
    darray_push(e->user_systems, s);

    return handle;
}
