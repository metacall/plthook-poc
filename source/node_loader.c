#include <stdio.h>
#include <assert.h>

#include "libnode2.h"

char *node_loader() {
    printf("INSIDE NODE LOADER (before string_function call): %p\n", string_function);
    fflush(stdout);
    char *str = string_function();
    printf("INSIDE NODE LOADER (after): %s\n", str);
    fflush(stdout);
    return str;
}
