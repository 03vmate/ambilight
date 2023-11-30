#include <stdint.h>
#include <stdio.h>

uint8_t*** flatbufToImg(void* buf, int width, int height) {
    if(buf == nullptr) {
        return nullptr;
    }

    uint8_t*** img = new uint8_t**[width];
    for(int i = 0; i < width; i++) {
        img[i] = new uint8_t*[height];
        for(int j = 0; j < height; j++) {
            img[i][j] = new uint8_t[3];
            for(int k = 0; k < 3; k++) {
                int index = j*width*3 + i*3 + k;
                img[i][j][k] = ((uint8_t*)buf)[index];
            }
        }
    }

    return img;
}

void freeImg(uint8_t*** img, int width, int height) {
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            delete[] img[i][j];
        }
        delete[] img[i];
    }
    delete[] img;
}

uint8_t* colorOfBlock(uint8_t*** img, int x, int y, int width, int height) {
    uint32_t* color = new uint32_t[3];
    for(int xpos = x; xpos < x + width; xpos++) {
        for(int ypos = y; ypos < y + height; ypos++) {
            for(int i = 0; i < 3; i++) {
                color[i] += img[xpos][ypos][i];
            }
        }
    }
    uint8_t* color_byte = new uint8_t[3];
    for(int i = 0; i < 3; i++) {
        color_byte[i] = color[i] / (width * height);
    }
    delete[] color;
    return color_byte;
}