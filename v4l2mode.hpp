#pragma once
#include <string>
#include <cstdint>
#include <map>

void v4l2_sighandler(int signum);
uint8_t gammaCorrection(uint8_t inputBrightness, double gamma);
void start_v4l2mode(std::map<std::string, std::string> config);