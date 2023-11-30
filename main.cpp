#include <opencv2/opencv.hpp>
#include "simpleConfigParser.h"
#include "v4l2mode.hpp"
#include "SerialPort.hpp"
#include <iostream>


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> config = parseConfig(argv[1]);
    start_v4l2mode(config);
}