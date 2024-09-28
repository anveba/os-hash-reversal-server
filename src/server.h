#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <stdint.h>

void open_server(uint32_t port, int reuse);

#endif