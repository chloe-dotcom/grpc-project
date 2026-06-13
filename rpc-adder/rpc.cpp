#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <sys/socket.h>   // socket, bind, listen, accept
#include <netinet/in.h>   // sockaddr_in, htons
#include <unistd.h>       // read, write, close
#include <cstring>        // memset
#include <cstdlib>

void die(const char* msg) {
    perror(msg);      // prints msg + the system's reason for the failure
    exit(1);
}

/**
 * Parse
 * 
 * Parse a single request line into method name and arguments
 * 
 * @param line a string (for now) split by spaces where first token is name and rest are args
 * @return a pair where .first is the method name and .second are the arguments (or empty)
 */
std::pair<std::string, std::vector<std::string>> parse(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string word;
    while (iss >> word) {
        tokens.push_back(word);
    }
    if (tokens.empty()) {
        return {"", {}};
    }
    std::string name = tokens[0];
    std::vector<std::string> args;
    for (size_t i = 1; i < tokens.size(); i++){
        args.push_back(tokens[i]);
    }

    return {name, args};
}

/**
 * Dispatch
 * 
 * Input: a method name and list of arguments
 * Output: result of calling function mapped to method name on arguments
 */
std::string dispatch(const std::string& name, const std::vector<std::string>& args) {
    if (name == "add") {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        return std::to_string(a + b);
    }
    else if (name == "sub") {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        return std::to_string(a - b);
    }
    else {
        return "ERROR unknown method: " + name;
    }
}

int make_server(int port) {
    // AF_INET = IPv4, SOCK_STREAM = TCP
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); // 1. listening socket
    if (listen_fd < 0) die("socket");

    // avoid "Address already in use" - allows server to restart immediately
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. address to bind to, this machine + given port
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); // zero it out
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // accept on any local interface
    addr.sin_port = htons(port); // htons = host-to-network byte order

    // 3. bind socket to address from part 2
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) die("bind");

    // 4. start listening (maximum of 5 pending)
    if (listen(listen_fd, 5) < 0) die("listen");

    return listen_fd;
}


void serve(int listen_fd) {
    while (true) {
        // block here until a client connects; get a socket for that client.
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) die("accept");

        // read request into buffer
        char buf[1024];
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::string line(buf);

            auto parsed = parse(line);
            std::string result = dispatch(parsed.first, parsed.second);
            result += "\n";

            // send the response back to this client
            write(client_fd, result.c_str(), result.size());
        }

        close(client_fd);
    }
}


int main() {
    // test Dispatch locally
    // std::cout << "Add 2 and 3: " << dispatch("add", {"2", "3"}) << "\n";
    // std::cout << "Sub 2 and 3: " << dispatch("sub", {"2", "3"}) << "\n";
    // std::cout << "Mul 2 and 3: " << dispatch("mul", {"2", "3"}) << "\n";

    // auto r1 = parse("add 2 3");
    // std::cout << "Parse Result:\n";
    // std::cout << "name: " << r1.first << ", args: ";
    // for(const auto& a: r1.second) std::cout << a << " ";
    // std::cout << "\n";
    
    // std::cout << "Dispatch Result: " << dispatch(r1.first, r1.second) << "\n";

    // auto r2 = parse("mul");
    // std::cout << "name " << r2.first << ", argc: " << r2.second.size() << "\n";

    int listen_fd = make_server(8000);
    std::cout << "listening on port 8000...\n";
    serve(listen_fd);
    
    return 0;
    
}