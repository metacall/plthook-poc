#include "libnode.h"

#include <stdlib.h>

char *string_function(void) {
#ifdef NODE_STATIC
    return "node-static";
#else
    return "node-dynamic";
#endif
}

free_ptr random_function_for_compiling_with_libc_as_dependency(void) {
    return &free;
}
