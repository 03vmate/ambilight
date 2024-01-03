#include "simpleConfigParser.h"
#include "v4l2mode.hpp"
#include "networkmode.hpp"
#include <iostream>

int main(int argc, char** argv) {
#ifdef __AVX2__
    std::cout << "Using AVX2 optimizations" << std::endl;
#elif __SSE2__
    std::cout << "Using SSE2 optimizations" << std::endl;
#endif
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> config = parseConfig(argv[1]);

    if (config["mode"] == "v4l2") {
        start_v4l2mode(config);
    } else if (config["mode"] == "network") {
        start_networkmode(config);
    } else {
        std::cout << "Invalid mode: " << config["mode"] << std::endl;
        return -1;
    }
}