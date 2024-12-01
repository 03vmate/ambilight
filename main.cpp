#include <iostream>
#include <variant>
#include <optional>
#include "ConfigParser.h"
#include "V4L2Mode.hpp"
#include "NetworkMode.hpp"

int main(int argc, char** argv) {
    // Check arguments
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    // Parse config
    std::map<std::string, std::string> config;
    try {
        config = ConfigParser::parse(argv[1]);
    }
    catch (std::runtime_error& e) {
        std::cout << "Error parsing config file: " << e.what() << std::endl;
        return -1;
    }

    // Start in the correct mode
    if (config["mode"] == "v4l2") {
        V4L2Mode::start(config);
    } else if (config["mode"] == "network") {
        NetworkMode::start(config);
    } else {
        std::cout << "Invalid mode: " << config["mode"] << std::endl;
        return -1;
    }
}