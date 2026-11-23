/*
CSCE 3530 Programming Assignment 1
Server
Chloe McGrigg
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAIN_SERVER_PORT 6000
#define BUF_SIZE 1024

/*
simplified estimate calculation so the main server produces a different
result than the proxy's local handling
*/
double compute_estimate(const char *make, const char *model, int year) {
    int base = 500;
    int age_factor = (2026 - year) * 15;
    int len_factor = (int)(strlen(make) + strlen(model)) * 3;
    double estimate = base + age_factor + len_factor;

    if (estimate < 100)
        estimate = 100;

    return estimate;
}

/*
Parse "request make=x model=y year=z" into fields
*/
int parse_request(const char *req, char *make, char *model, int *year) {
    if (sscanf(req, "REQUEST MAKE=%49s MODEL=%49s YEAR=%d", make, model, year) != 3)
        return -1;

    return 0;
}

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];

    /*
    Create a TCP socket
    socket(domain, type, protocol)
    */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket() has failed");
        exit(1);
    }

    /*
    Allow immediate re-binding to the port after restart
    avoids "Address already in use" while you're testing repeatedly
    */
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /*
    Fill in the address we want to bind to
    any local IP, fixed port, in network byte order
    */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(MAIN_SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(listen_fd);
        exit(1);
    }

    /*
    Mark the socket as passive/listening w a backlog queue
    */
    if (listen(listen_fd, 10) < 0) {
        perror("listen() failed");
        close(listen_fd);
        exit(1);
    }

    printf("[MAIN SERVER] Listening on port %d ...\n", MAIN_SERVER_PORT);

    /*
    Main accept loop
    serve only one proxy connection at a time
    */
    while (1) {
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept() has failed");
            continue;
        }

        printf("[MAIN SERVER] Connection accepted from proxy (%s:%d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        memset(buffer, 0, BUF_SIZE);
        ssize_t n = recv(conn_fd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            close(conn_fd);
            continue;
        }

        buffer[n] = '\0';
        printf("[MAIN SERVER] Received: %s", buffer);

        char make[50], model[50];
        int year = 0;
        char response[BUF_SIZE];

        if (parse_request(buffer, make, model, &year) == 0) {
            double est = compute_estimate(make, model, year);
            snprintf(response, BUF_SIZE,
                     "RESPONSE ESTIMATE=%.2f SOURCE=MAIN\n", est);
        } else {
            snprintf(response, BUF_SIZE,
                     "RESPONSE ERROR=BAD_REQUEST SOURCE=MAIN\n");
        }

        send(conn_fd, response, strlen(response), 0);
        printf("[MAIN SERVER] Sent: %s", response);
        close(conn_fd);
    }

    close(listen_fd);
    return 0;
}
