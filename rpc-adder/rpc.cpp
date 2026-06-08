#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>

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

int main() {
    // test Dispatch locally
    // std::cout << "Add 2 and 3: " << dispatch("add", {"2", "3"}) << "\n";
    // std::cout << "Sub 2 and 3: " << dispatch("sub", {"2", "3"}) << "\n";
    // std::cout << "Mul 2 and 3: " << dispatch("mul", {"2", "3"}) << "\n";

    auto r1 = parse("add 2 3");
    std::cout << "Parse Result:\n";
    std::cout << "name: " << r1.first << ", args: ";
    for(const auto& a: r1.second) std::cout << a << " ";
    std::cout << "\n";

    auto r2 = parse("mul");
    std::cout << "name " << r2.first << ", argc: " << r2.second.size() << "\n";

    return 0;
}