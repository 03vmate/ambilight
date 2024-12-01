#pragma once
#include <tuple>
#include <cstdint>

struct AVX2;
struct SSE2;

template <typename SIMDType = void>
std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height);