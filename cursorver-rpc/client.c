#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8787
#define HOST "127.0.0.1"
#define BUF_SIZE 256

static int write_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("write");
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *command = (argc > 1) ? argv[1]: "get_time";
    const char *host = (argc > 2) ? argv[2] : HOST;
    int port = (argc > 3) ? atoi(argv[3]) : PORT;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid address: %s\n", host);
        close(sock);
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return EXIT_FAILURE;
    }

    // build and send <command>
    char request[BUF_SIZE];
    int len = snprintf(request, sizeof(request), "%s\n", command);
    if (len < 0 || (size_t)len >= sizeof(request)) {
        fprintf(stderr, "command too long\n");
        close(sock);
        return EXIT_FAILURE;
    }

    if (write_all(sock, request, (size_t)len) != 0) {
        close(sock);
        return EXIT_FAILURE;
    }

    char buf[BUF_SIZE];
    ssize_t n = read(sock, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(sock);
        return EXIT_FAILURE;
    }
    buf[n] = '\0';

    fputs(buf, stdout);
    if (n > 0 && buf[n - 1] != '\n') {
        putchar('\n');
    }

    close(sock);
    return EXIT_SUCCESS;
}