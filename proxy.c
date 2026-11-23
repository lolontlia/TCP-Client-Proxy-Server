/*
CSCE 3530 Programming Assignment 1
Proxy
Chloe McGrigg
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PROXY_LISTEN_PORT 5000
#define MAIN_SERVER_IP    "127.0.0.1"
#define MAIN_SERVER_PORT  6000
#define BUF_SIZE 1024
#define LOCAL_HIT_PROBABILITY 0.4

/*
local handling
data cached at the proxy
*/
double compute_local_estimate(const char *make, const char *model, int year) {
    int base = 400;
    int age_factor = (2026 - year) * 10;
    double estimate = base + age_factor + (int)strlen(make) * 5;

    if (estimate < 100)
        estimate = 100;

    return estimate;
}

int parse_request(const char *req, char *make, char *model, int *year) {
    if (sscanf(req, "REQUEST MAKE=%49s MODEL=%49s YEAR=%d", make, model, year) != 3)
        return -1;

    return 0;
}

/*
forwards a request to the main server + returns reply
*/
int forward_to_main_server(const char *request, char *response_out, int out_size) {
    int sock_fd;
    struct sockaddr_in server_addr;

    /*
    socket makes proxy act as a client of the main server
    */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket() to main server failed");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MAIN_SERVER_PORT);

    if (inet_pton(AF_INET, MAIN_SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() failed");
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() to main server failed");
        close(sock_fd);
        return -1;
    }

    send(sock_fd, request, strlen(request), 0);

    memset(response_out, 0, out_size);
    ssize_t n = recv(sock_fd, response_out, out_size - 1, 0);
    close(sock_fd);

    if (n <= 0)
        return -1;

    response_out[n] = '\0';
    return 0;
}

int main(void) {
    int listen_fd, conn_fd;
    struct sockaddr_in proxy_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];

    srand((unsigned int)time(NULL));

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket() failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(PROXY_LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("bind() has failed");
        close(listen_fd);
        exit(1);
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen() failed");
        close(listen_fd);
        exit(1);
    }

    printf("[PROXY] Listening for clients on port %d ...\n", PROXY_LISTEN_PORT);
    printf("[PROXY] Will forward non-local requests to %s:%d\n",
           MAIN_SERVER_IP, MAIN_SERVER_PORT);

    while (1) {
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept() has failed");
            continue;
        }

        printf("\n[PROXY] Client connected (%s:%d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        memset(buffer, 0, BUF_SIZE);
        ssize_t n = recv(conn_fd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            close(conn_fd);
            continue;
        }

        buffer[n] = '\0';
        printf("[PROXY] Received from client: %s", buffer);

        double r = (double)rand() / (double)RAND_MAX;
        char response[BUF_SIZE];

        if (r < LOCAL_HIT_PROBABILITY) {
            /*
            handle locally
            */
            char make[50], model[50];
            int year = 0;

            if (parse_request(buffer, make, model, &year) == 0) {
                double est = compute_local_estimate(make, model, year);
                snprintf(response, BUF_SIZE,
                         "RESPONSE ESTIMATE=%.2f SOURCE=LOCAL\n", est);
            } else {
                snprintf(response, BUF_SIZE,
                         "RESPONSE ERROR=BAD_REQUEST SOURCE=LOCAL\n");
            }

            printf("[PROXY] r=%.3f < %.1f -> handled locally\n",
                   r, LOCAL_HIT_PROBABILITY);
        } else {
            /*
            forward to main server
            */
            printf("[PROXY] r=%.3f >= %.1f -> forwarding to main server\n",
                   r, LOCAL_HIT_PROBABILITY);

            if (forward_to_main_server(buffer, response, BUF_SIZE) < 0) {
                snprintf(response, BUF_SIZE,
                         "RESPONSE ERROR=MAIN_SERVER_UNREACHABLE SOURCE=PROXY\n");
            }
        }

        send(conn_fd, response, strlen(response), 0);
        printf("[PROXY] Sent to client: %s", response);
        close(conn_fd);
    }

    close(listen_fd);
    return 0;
}
