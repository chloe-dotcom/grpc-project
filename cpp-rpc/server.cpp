#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <ctime>
#include <array>
#include <atomic>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

extern "C" {
#include <prom.h>
#include <promhttp.h>
}

namespace {

class Students {
    public:
    struct Student { std::string name; int age; };

    void add(std::string name, int age) {
        students_.push_back({std::move(name), age});
    }
    size_t count() const { return students_.size(); }
    const std::vector<Student>& all() const {
        return students_;
    }

    private:
    std::vector<Student> students_;
};

constexpr int kPort = 8787;
constexpr int kBacklog = 8; // kernel accept queue
constexpr size_t kBufSize = 256;

std::atomic<bool> running{true};

// note: prometheus-client-c library uses atomic ops/internal locking (so counter is safe read/write)
prom_counter_t* request_counter = nullptr;
prom_gauge_t* student_gauge = nullptr;

Students students;

extern "C" void handle_sigint(int) {running = false;}

void install_handler(int signo) {
    struct sigaction sa{};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(signo, &sa, nullptr);
}

// RAII wrapper so a fd is always closed when it goes out of scope.
class Fd {
public:
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
    int get() const { return fd_; }
private:
    int fd_;
};

ssize_t read_line(int fd, char* buf, size_t buflen) {
    size_t total = 0;
    while (total + 1 < buflen) {
        ssize_t n = ::read(fd, buf + total, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return total == 0 ? 0 : static_cast<ssize_t>(total);
        if (buf[total] == '\n') {
            buf[total] = '\0';
            return static_cast<ssize_t>(total);
        }
        ++total;
    }
    errno = ENOMEM;
    return -1;
}

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

int write_line(int fd, std::string_view line) {
    if (write_all(fd, line) != 0) return -1;
    return write_all(fd, "\n");
}

int handle_get_time(int client_fd) {
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
    std::string response;
    if (localtime_r(&now, &tm_buf) == nullptr) {
        response = "ERROR time_failed\n";
    } else {
        char tmp[kBufSize];
        if (std::strftime(tmp, sizeof(tmp), "OK %Y-%m-%d %H:%M:%S %Z\n", &tm_buf) == 0)
            response = "ERROR format_failed\n";
        else
            response = tmp;
    }
    return write_all(client_fd, response);
}

int handle_get_hostname(int client_fd) {
    std::array<char, 256> hostname{};
    if (::gethostname(hostname.data(), hostname.size()) != 0)
        return write_all(client_fd, std::string("ERROR ") + std::strerror(errno) + "\n");
    hostname.back() = '\0';
    return write_all(client_fd, "OK " + std::string(hostname.data()) + "\n");
}

int handle_add_student(int client_fd, std::string_view args) {
    // add_student Alice 20
    size_t sep = args.rfind(' ');
    if (sep == std::string_view::npos) return write_line(client_fd, "ERROR bad args");

    std::string name(args.substr(0, sep));
    std::string age_str(args.substr(sep + 1));
    int age;
    try {
        age = std::stoi(age_str);
    } 
    catch(...) {
        return write_line(client_fd, "ERROR bad args: arge");
    }

    students.add(std::move(name), age);
    prom_gauge_set(student_gauge, students.count(), nullptr);
    return write_line(client_fd, "OK:added student");
}

int handle_get_students(int client_fd) {
    std::string body = "OK " + std::to_string(students.count()) + " ";
    bool first = true;
    for (const auto& stud : students.all()) {
        if (!first) body += ", ";
        body += stud.name + " : " + std::to_string(stud.age);
        first = false;
    }

    return write_line(client_fd, body);
}

int handle_client(int client_fd) {
    char request[kBufSize];
    ssize_t n = read_line(client_fd, request, sizeof(request));
    if (n < 0) { std::perror("read"); return -1; }
    if (n == 0) return 0;

    std::string_view req(request);
    constexpr std::string_view kAddPrefix = "add_student "; // split func and args

    if (req == "get_time") {
        prom_counter_inc(request_counter, (const char*[]){"get_time"});
        return handle_get_time(client_fd);
    }
    if (req == "get_hostname") {
        prom_counter_inc(request_counter, (const char*[]){"get_hostname"});
        return handle_get_hostname(client_fd);
    }
    if (req == "get_students") {
        prom_counter_inc(request_counter, (const char*[]){"get_students"});
        return handle_get_students(client_fd);
    }
    if (req.substr(0, kAddPrefix.size()) == kAddPrefix) {
        prom_counter_inc(request_counter, (const char*[]){"add_student"});
        return handle_add_student(client_fd, req.substr(kAddPrefix.size()));
    }
    prom_counter_inc(request_counter, (const char*[]){"unknown"});
    return write_line(client_fd, "ERROR unknown_request");
}

}

int main(void) {
    
    install_handler(SIGINT);
    install_handler(SIGTERM);

    Fd server(::socket(AF_INET, SOCK_STREAM, 0));
    if (server.get() < 0) {
        std::perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (::setsockopt(server.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::perror("setsockopt");
        return EXIT_FAILURE;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort);

    if (::bind(server.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        return EXIT_FAILURE;
    }

    if (::listen(server.get(), kBacklog) < 0) {
        std::perror("listen");
        return EXIT_FAILURE;
    }

	// initialize default Prometheus collector registry
	if (prom_collector_registry_default_init() != 0) {
		std::cerr << "Failed to initialize Prometheus registry\n";
		return EXIT_FAILURE;
	}

    // bind registry to HTTP component (without it, PromHTTP server cannot find metric registry)
    promhttp_set_active_collector_registry(PROM_COLLECTOR_REGISTRY_DEFAULT);

	// create and register custom counter metric
    const char *keys[] = { "command" };
	request_counter = prom_collector_registry_must_register_metric(
		prom_counter_new("server_requests_total", "Total number of processed requests", 1, keys)
	);
    student_gauge = prom_collector_registry_must_register_metric(
        prom_gauge_new("total_students", "Current number of enrolled students", 0, nullptr)
    );
	
	// start Prometheus HTTP server on port 8000 (background thread)
	MHD_Daemon* daemon = promhttp_start_daemon(MHD_USE_SELECT_INTERNALLY, 8000, nullptr, nullptr);
	if(!daemon) {
		std::cerr << "Failed to start Prometheus HTTP daemon\n";
		return EXIT_FAILURE;
	}

    std::cout << "RPC server listening on port " << kPort << " (commands: get_time, get_hostname)\n";

    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int raw_fd = ::accept(server.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_len);
	
        if (raw_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("accept");
            break;
        }

        Fd client(raw_fd); //closed automatically at end of iteration

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        std::cout << "client connected from " << client_ip << ":"
                  << ntohs(client_addr.sin_port) << "\n";
        
        handle_client(client.get());
    }

    std::cout << "server stopped\n";
	MHD_stop_daemon(daemon);

    return EXIT_SUCCESS;
}