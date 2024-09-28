#include "messages.h"
#include <endian.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hreversal.h"
#include "sha256.h"

#define QUEUE_SIZE 10

struct server
{
    int socket_fd;
};

static struct server* current_server = NULL;

static void close_server()
{
    if (current_server == NULL || current_server->socket_fd < 0)
        return;
    printf("Closing server...\n");
    close(current_server->socket_fd);
    current_server->socket_fd = -1;
    current_server = NULL;
    signal(SIGINT, SIG_DFL);
}

static void sigint_handler(int sig)
{
    signal(sig, SIG_IGN);

    close_server();

    exit(0);
}

void open_server(uint32_t port, int reuse)
{
    if (current_server != NULL) {
        printf("Server already open.\n");
        return;
    }

    printf("Opening server...\n");

    signal(SIGINT, sigint_handler);
    struct server serv;
    serv.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    current_server = &serv;

    if (reuse) {
        int option = 1;
        setsockopt(serv.socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    }

    if (serv.socket_fd < 0) {
        printf("Error opening socket: %s\n", strerror(errno));
        close_server();
        return;
    }

    struct sockaddr_in server_addr;
    memset((char*)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(serv.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error on binding: %s\n", strerror(errno));
        close_server();
        return;
    }

    if (listen(serv.socket_fd, QUEUE_SIZE) < 0) {
        printf("Error on listen: %s\n", strerror(errno));
        close_server();
        return;
    }

    printf("Listening...\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket_fd = accept(serv.socket_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket_fd < 0) {
            printf("Error on accept: %s\n", strerror(errno));
            break;
        }

        uint8_t buffer[PACKET_REQUEST_SIZE];
        int n = read(client_socket_fd, buffer, PACKET_REQUEST_SIZE);

        if (n == 0) { // EOF
            close(client_socket_fd);
            break;
        }

        if (n < 0) {
            printf("Error on reading client message: %s\n", strerror(errno));
            break;
        }

        uint8_t* target_hash = buffer + PACKET_REQUEST_HASH_OFFSET;
        uint64_t start = be64toh(*((uint64_t*)(buffer + PACKET_REQUEST_START_OFFSET)));
        uint64_t end = be64toh(*((uint64_t*)(buffer + PACKET_REQUEST_END_OFFSET)));
        uint8_t priority = buffer[PACKET_REQUEST_PRIO_OFFSET];

#ifdef SB_VERBOSE
        printf("\n[ NEW REQUEST FROM %d ]\n", client_socket_fd);
        char hash_str[65];
        hash_to_str(hash_str, target_hash);
        printf("HASH  %s\nPRIOR %d\nSTART %ld\nEND   %ld\n", hash_str, priority, start, end);
#endif

        uint64_t reversal_result = reverse_hash(target_hash, start, end);

#ifdef SB_VERBOSE
        printf("\n[ RESPONSE TO %d ]\nRES   %ld\n", client_socket_fd, reversal_result);
#endif

        uint64_t response = htobe64(reversal_result);
        n = write(client_socket_fd, &response, sizeof(response));

        if (n < 0) {
            printf("Error on writing to client: %s\n", strerror(errno));
            break;
        }

        close(client_socket_fd);
    }

    close_server();

    return;
}