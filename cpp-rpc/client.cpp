#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kPort = 8787;
constexpr const char* kHost = "127.0.0.1";
constexpr size_t kBufSize = 256;

class Fd {
public:
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};

int write_all(int fd, std::string_view data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::write(fd, data.data() + sent, data.size() - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("write");
            return -1;
        }
        sent += static_cast<size_t>(n);
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args(argv, argv + argc);
    std::string command;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) command += " ";
        command += argv[i];
    }
    if (command.empty()) command = "get_time";

    std::string host    = kHost;
    int port            = kPort;

    Fd sock(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() < 0) { std::perror("socket"); return EXIT_FAILURE; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "invalid address: " << host << "\n";
        return EXIT_FAILURE;
    }
    if (::connect(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        return EXIT_FAILURE;
    }

    std::string request = command + "\n";
    if (request.size() >= kBufSize) {
        std::cerr << "command too long\n";
        return EXIT_FAILURE;
    }
    if (write_all(sock.get(), request) != 0) return EXIT_FAILURE;

    char buf[kBufSize];
    ssize_t n = ::read(sock.get(), buf, sizeof(buf) - 1);
    if (n < 0) { std::perror("read"); return EXIT_FAILURE; }
    buf[n] = '\0';

    std::cout << buf;
    if (n > 0 && buf[n - 1] != '\n') std::cout << "\n";
    return EXIT_SUCCESS;
}