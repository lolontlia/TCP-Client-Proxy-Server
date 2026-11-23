/*

CSCE 3530 - Programming Assignment 1

Client

Chloe McGrigg


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PROXY_IP   "127.0.0.1"
#define PROXY_PORT 5000
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in proxy_addr;
    char request[BUF_SIZE];
    char response[BUF_SIZE];

    const char *make = (argc >= 4) ? argv[1] : "Honda";
    const char *model = (argc >= 4) ? argv[2] : "Civic";
    const char *year = (argc >= 4) ? argv[3] : "2018";

    /*
    Create a TCP socket
    */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket() has failed");
        exit(1);
    }

    /*
    Build the destination address
    Proxy's IP address + port #
    */
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(PROXY_PORT);

    if (inet_pton(AF_INET, PROXY_IP, &proxy_addr.sin_addr) <= 0) {
        perror("inet_pton() failed");
        close(sock_fd);
        exit(1);
    }

    /*
    Connect
    TCP three-way handshake
    */
    if (connect(sock_fd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("connect() to proxy has failed");
        close(sock_fd);
        exit(1);
    }

    printf("[CLIENT] Connected to proxy at %s:%d\n", PROXY_IP, PROXY_PORT);

    /*
    Build + send the request
    using my protocol format
    */
    snprintf(request, BUF_SIZE, "REQUEST MAKE=%s MODEL=%s YEAR=%s\n", make, model, year);
    send(sock_fd, request, strlen(request), 0);
    printf("[CLIENT] Sent request:     %s", request);

    /*
    Wait
    Read response
    */
    memset(response, 0, BUF_SIZE);
    ssize_t n = recv(sock_fd, response, BUF_SIZE - 1, 0);

    if (n <= 0) {
        printf("[CLIENT] No response received \n");
        close(sock_fd);
        exit(1);
    }

    response[n] = '\0';

    /*
    Display result to user
    */
    printf("[CLIENT] Response: %s", response);

    double estimate;
    char source[20];

    if (sscanf(response, "RESPONSE ESTIMATE=%lf SOURCE=%19s", &estimate, source) == 2) {
        printf("\n-----------------------------------\n");
        printf(" Vehicle: %s %s (%s)\n", make, model, year);
        printf(" Estimated insurance cost: $%.2f\n", estimate);
        printf(" Served from: %s\n", source);
        printf("-----------------------------------\n");
    } else {
        printf("[CLIENT] proxy/server error/unexpected format.\n");
    }

    close(sock_fd);
    return 0;
}
