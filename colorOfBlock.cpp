#include "colorOfBlock.hpp"
#include <emmintrin.h>
#include <immintrin.h>

//Calculate average color of a rectangle
uint8_t* colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height) {
    //If the width is odd, make it even. One of the SIMD optimizations requires this, but do it for all of them to be consistent.
    if(width % 2 == 1) {
        if(width > 1) {
            width -= 1;
        }
        else {
            width += 1;
        }
    }

    uint32_t numOfPixels = width * height;
#ifdef __AVX2__
    //use a 256 bit SIMD register containing 8 32 bit integers. The sum of even R,G,B pixels is in the first 3 uint32s, the sum of odd R,G,B pixels is in the following 3 uint32s, and the remaining 2 uint16s are unused.
    __m256i sum = _mm256_setzero_si256();
    for(int ypos = y; ypos < y + height; ypos++) {
        for(int xpos = x; xpos < x + width; xpos += 2) {
            int index = (ypos * imgwidth + xpos) * 3;
            __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 are needed. If we are at the very bottom right of the image, this may try to load values outside of the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
            __m256i q = _mm256_cvtepu8_epi32(p); //convert 8bit registers to 32bit registers, throwing away the upper 8 values. This will leave us with 8 32bit registers, the last 2 of which are unused.
            sum = _mm256_add_epi32(sum, q);
        }
    }
    int32_t result[8];
    _mm256_storeu_si256((__m256i*)result, sum);
    uint8_t *result_byte = new uint8_t[3];
    result_byte[0] = (result[0] + result[3]) / numOfPixels;
    result_byte[1] = (result[1] + result[4]) / numOfPixels;
    result_byte[2] = (result[2] + result[5]) / numOfPixels;
    return result_byte;
#elif __SSE2__
    if(width >= 512) {
        //use a 128 bit SIMD register containing 4 32 bit integers. The first 3 are used for R,G,B sums, the fourth is unused.
        __m128i sum = _mm_setzero_si128();
        for (int ypos = y; ypos < y + height; ypos++) {
            for(int xpos = x; xpos < x + width; xpos++) {
                int index = (ypos * imgwidth + xpos) * 3;
                __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 are needed. If we are at the very bottom right of the image, this may try to load values outside of the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
                __m128i q = _mm_cvtepu8_epi32(p); //convert 8bit registers to 32bit registers, throwing away the upper 12 values. This will leave us with 4 32bit registers, the last of which is unused.
                sum = _mm_add_epi32(sum, q);
            }
        }

        int32_t result[4];
        _mm_storeu_si128((__m128i*)result, sum);
        uint8_t *result_byte = new uint8_t[3];
        result_byte[0] = result[0] / numOfPixels;
        result_byte[1] = result[1] / numOfPixels;
        result_byte[2] = result[2] / numOfPixels;
        return result_byte;
    }
    else {
        //use a 128 bit SIMD register containing 8 16bit integers. The sum of even R,G,B pixels is in the first 3 uint16s, the sum of odd R,G,B pixels is in the following 3 uint16s, and the remaining 2 uint16s are unused.
        //After each row has been summed, the odd and even sums are added together, and the result is added to the total sum. This only works for <512 pixels wide, because the sum of the odd and even sums may overflow a uint16 otherwise.
        uint32_t sum[] = {0, 0, 0};
        for (int ypos = y; ypos < y + height; ypos++) {
            __m128i rowsum = _mm_setzero_si128();
            for(int xpos = x; xpos < x + width; xpos += 2) {
                int index = (ypos * imgwidth + xpos) * 3;
                __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 is needed. If we are at the very bottom right of the image, this may try to load values outside of the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
                __m128i q = _mm_cvtepu8_epi16(p); //convert 8bit registers to 16bit registers, throwing away the upper 8 values. This will leave us with 8 16bit registers, the last 2 of which are unused.
                rowsum = _mm_adds_epu16(rowsum, q);
            }
            uint16_t rowsum_result[8];
            _mm_storeu_si128((__m128i*)rowsum_result, rowsum);
            sum[0] += rowsum_result[0];
            sum[1] += rowsum_result[1];
            sum[2] += rowsum_result[2];
            sum[0] += rowsum_result[3];
            sum[1] += rowsum_result[4];
            sum[2] += rowsum_result[5];
        }
        uint8_t *result_byte = new uint8_t[3];
        result_byte[0] = sum[0] / numOfPixels;
        result_byte[1] = sum[1] / numOfPixels;
        result_byte[2] = sum[2] / numOfPixels;
        return result_byte;
    }
#else
    uint32_t color[] = {0,0,0};

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
        color_byte[i] = color[i] / numOfPixels;
    }

    return color_byte;
    }
#endif
    return nullptr;
}