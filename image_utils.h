#include <stdint.h>
#include <stdio.h>

uint8_t* colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height) {
    uint32_t* color = new uint32_t[3];
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;

    for (int xpos = x; xpos < x + width; xpos++) {
        for (int ypos = y; ypos < y + height; ypos++) {
            for (int i = 0; i < 3; i++) {
                int index = (ypos * imgwidth + xpos) * 3 + i;
                color[i] += img[index];
            }
        }
    }

    uint8_t* color_byte = new uint8_t[3];

    for (int i = 0; i < 3; i++) {
        color_byte[i] = color[i] / (width * height);
    }

    delete[] color;
    return color_byte;
}

uint8_t gammaCorrection(uint8_t inputBrightness, double gamma) {
    double adjustedBrightness = 255 * std::pow((inputBrightness / 255.0), gamma);
    adjustedBrightness = std::max(0.0, std::min(adjustedBrightness, 255.0));
    return static_cast<uint8_t>(adjustedBrightness);
}