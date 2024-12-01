#include <iostream>
#include <variant>
#include <optional>
#include "ConfigParser.h"
#include "v4l2mode.hpp"
#include "networkmode.hpp"

#include "ArrayAverager.h"

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
        start_v4l2mode(config);
    } else if (config["mode"] == "network") {
        start_networkmode(config);
    } else {
        std::cout << "Invalid mode: " << config["mode"] << std::endl;
        return -1;
    }
}