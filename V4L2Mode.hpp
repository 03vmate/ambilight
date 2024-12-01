#pragma once
#include <string>
#include <cstdint>
#include <map>

class V4L2Mode {
    static bool V4L2Run;
    static uint8_t gammaCorrection(uint8_t inputBrightness, double gamma);
public:
    static void V4L2Sighandler(int signum);
    static void start(std::map<std::string, std::string> config);
};


