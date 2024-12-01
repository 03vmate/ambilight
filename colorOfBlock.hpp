#pragma once
#include <cstdint>
#include <tuple>

std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height);