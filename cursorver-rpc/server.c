#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <unistd.h>
#include <prom.h>
#include <promhttp.h>

#define PORT 8787
#define BACKLOG 8
#define BUF_SIZE 256
#define HOST_NAME_MAX 255

static volatile sig_atomic_t running = 1;

static void handle_sigint(int signo) {
    (void)signo;
    running = 0;
}

#include <signal.h>

static void install_handler(int signo) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signo, &sa, NULL);
}

static ssize_t read_line(int fd, char *buf, size_t buflen) {
    size_t total = 0;

    while (total + 1 < buflen) {
        ssize_t n = read(fd, buf + total, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return total == 0 ? 0 : (ssize_t)total;
        }
        if (buf[total] == '\n') {
            buf[total] = '\0';
            return (ssize_t)total;
        }
        total++;
    }

    errno = ENOMEM;
    return -1;
}

static int write_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("write");
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static int write_line(int fd, const char *line) {
    if (write_all(fd, line, strlen(line)) != 0) {
        return -1;
    }
    return write_all(fd, "\n", 1);
}

static int handle_get_time(int client_fd) {
    char response[BUF_SIZE];
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm_info = localtime_r(&now, &tm_buf);

    if (tm_info == NULL) {
        snprintf(response, sizeof(response), "ERROR time_failed\n");
    } else if (strftime(response, sizeof(response), "OK %Y-%m-%d %H:%M:%S %Z\n", tm_info) == 0) {
        snprintf(response, sizeof(response), "ERROR format_failed\n");
    }

    return write_all(client_fd, response, strlen(response));
}

static int handle_get_hostname(int client_fd) {
    char response[BUF_SIZE];
    char hostname[HOST_NAME_MAX + 1];

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(response, sizeof(response), "ERROR %s\n", strerror(errno));
    } else {
        hostname[sizeof(hostname) - 1] = '\0';  // ensure termination
        snprintf(response, sizeof(response), "OK %s\n", hostname);
    }

    return write_all(client_fd, response, strlen(response));
}

static int handle_client(int client_fd) {
    char request[BUF_SIZE];
    ssize_t n = read_line(client_fd, request, sizeof(request));

    if (n < 0) {
        perror("read");
        return -1;
    }
    if (n == 0) {
        return 0;
    }

    if (strcmp(request, "get_time") == 0) {
        return handle_get_time(client_fd);
    }

    if (strcmp(request, "get_hostname") == 0) {
        return handle_get_hostname(client_fd);
    }

    return write_line(client_fd, "ERROR unknown_request");
}

int main(void) {
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    // signal(SIGINT, handle_sigint);
    // signal(SIGTERM, handle_sigint);
    install_handler(SIGINT);
    install_handler(SIGTERM);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("RPC server listening on port %d (commands: get_time, get_hostname)\n", PORT);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    printf("server stopped\n");
    return EXIT_SUCCESS;
}