#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("One argument was not given.\n");
        return 1;
    } else {
        char* endptr;
        uint32_t port = strtol(argv[1], &endptr, 10);
        if (argv[1] == endptr) {
            printf("Argument not a number.\n");
            return 1;
        }
#ifdef SB_USE_SIMD
        printf("SIMD enabled.\n");
#endif
        open_server(port, 0);
    }
}