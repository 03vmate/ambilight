#include <emmintrin.h>
#include <immintrin.h>
#include "ColorOfBlock.hpp"

template <typename SIMDType>
struct ColorOfBlockImpl {
    static std::tuple<uint8_t, uint8_t, uint8_t> calculate(
            const uint8_t* img, int imgwidth, int imgheight __attribute__((unused)), int x, int y, int width, int height) {

        //If the width is odd, make it even. One of the SIMD optimizations requires this, but do it for all of them to be consistent.
        if (width % 2 == 1) {
            if (width > 1) width -= 1;
            else width += 1;
        }

        uint32_t numOfPixels = width * height;
        uint32_t color[] = {0, 0, 0};  // For storing summed color channels

        // Default implementation for non-SIMD case (fallback)
        for (int ypos = y; ypos < y + height; ypos++) {
            for (int xpos = x; xpos < x + width; xpos++) {
                for (int i = 0; i < 3; i++) {
                    int index = (ypos * imgwidth + xpos) * 3 + i;
                    color[i] += img[index];
                }
            }
        }

        // Compute average and return
        uint8_t b0 = color[0] / numOfPixels;
        uint8_t b1 = color[1] / numOfPixels;
        uint8_t b2 = color[2] / numOfPixels;
        return std::make_tuple(b0, b1, b2);
    }
};

// Specialization for AVX2
template <>
struct ColorOfBlockImpl<AVX2> {
    static std::tuple<uint8_t, uint8_t, uint8_t> calculate(
            const uint8_t* img, int imgwidth, int imgheight __attribute__((unused)), int x, int y, int width, int height) {

        if (width % 2 == 1) {
            if (width > 1) width -= 1;
            else width += 1;
        }

        //use a 256 bit SIMD register containing 8 32 bit integers. The sum of even R,G,B pixels is in the first 3 uint32s, the sum of odd R,G,B pixels is in the following 3 uint32s, and the remaining 2 uint16s are unused.
        uint32_t numOfPixels = width * height;
        __m256i sum = _mm256_setzero_si256();
        for (int ypos = y; ypos < y + height; ypos++) {
            for (int xpos = x; xpos < x + width; xpos += 2) {
                int index = (ypos * imgwidth + xpos) * 3;
                __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 are needed. If we are at the very bottom right of the image, this may try to load values outside the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
                __m256i q = _mm256_cvtepu8_epi32(p); //convert 8bit registers to 32bit registers, throwing away the upper 8 values. This will leave us with 8 32bit registers, the last 2 of which are unused.
                sum = _mm256_add_epi32(sum, q);
            }
        }

        int32_t result[8];
        _mm256_storeu_si256((__m256i*)result, sum);
        uint8_t b0 = (result[0] + result[3]) / numOfPixels;
        uint8_t b1 = (result[1] + result[4]) / numOfPixels;
        uint8_t b2 = (result[2] + result[5]) / numOfPixels;
        return std::make_tuple(b0, b1, b2);
    }
};

// Specialization for SSE2
template <>
struct ColorOfBlockImpl<SSE2> {
    static std::tuple<uint8_t, uint8_t, uint8_t> calculate(
            const uint8_t* img, int imgwidth, int imgheight __attribute__((unused)), int x, int y, int width, int height) {

        if (width % 2 == 1) {
            if (width > 1) width -= 1;
            else width += 1;
        }

        uint32_t numOfPixels = width * height;
        if (width >= 512) {
            //use a 128 bit SIMD register containing 4 32-bit integers. The first 3 are used for R,G,B sums, the fourth is unused.
            __m128i sum = _mm_setzero_si128();
            for (int ypos = y; ypos < y + height; ypos++) {
                for (int xpos = x; xpos < x + width; xpos++) {
                    int index = (ypos * imgwidth + xpos) * 3;
                    __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 are needed. If we are at the very bottom right of the image, this may try to load values outside the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
                    __m128i q = _mm_cvtepu8_epi32(p); //convert 8bit registers to 32bit registers, throwing away the upper 12 values. This will leave us with 4 32bit registers, the last of which is unused.
                    sum = _mm_add_epi32(sum, q);
                }
            }

            int32_t result[4];
            _mm_storeu_si128((__m128i*)result, sum);
            uint8_t b0 = result[0] / numOfPixels;
            uint8_t b1 = result[1] / numOfPixels;
            uint8_t b2 = result[2] / numOfPixels;
            return std::make_tuple(b0, b1, b2);
        }
        else {
            //use a 128 bit SIMD register containing 8 16bit integers. The sum of even R,G,B pixels is in the first 3 uint16s, the sum of odd R,G,B pixels is in the following 3 uint16s, and the remaining 2 uint16s are unused.
            //After each row has been summed, the odd and even sums are added together, and the result is added to the total sum. This only works for <512 pixels wide, because the sum of the odd and even sums may overflow an uint16 otherwise.
            uint32_t sum[] = {0, 0, 0};
            for (int ypos = y; ypos < y + height; ypos++) {
                __m128i rowsum = _mm_setzero_si128();
                for (int xpos = x; xpos < x + width; xpos += 2) {
                    int index = (ypos * imgwidth + xpos) * 3;
                    __m128i p = _mm_loadu_si128((__m128i*)&img[index]); //load next 16 subpixels into 16x 8bit registers. Only the next 6 is needed. If we are at the very bottom right of the image, this may try to load values outside the image. To prevent segfaults, 16 bytes of padding is also allocated after the image data ends.
                    __m128i q = _mm_cvtepu8_epi16(p); //convert 8bit registers to 16bit registers, throwing away the upper 8 values. This will leave us with 8 16bit registers, the last 2 of which are unused.
                    rowsum = _mm_adds_epu16(rowsum, q);
                }
                uint16_t rowSumResult[8];
                _mm_storeu_si128((__m128i*)rowSumResult, rowsum);
                sum[0] += rowSumResult[0];
                sum[1] += rowSumResult[1];
                sum[2] += rowSumResult[2];
                sum[0] += rowSumResult[3];
                sum[1] += rowSumResult[4];
                sum[2] += rowSumResult[5];
            }

            uint8_t b0 = sum[0] / numOfPixels;
            uint8_t b1 = sum[1] / numOfPixels;
            uint8_t b2 = sum[2] / numOfPixels;
            return std::make_tuple(b0, b1, b2);
        }
    }
};

template <typename SIMDType>
std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height) {
    return ColorOfBlockImpl<SIMDType>::calculate(img, imgwidth, imgheight, x, y, width, height);
}

template std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock<void>(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height);
template std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock<SSE2>(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height);
template std::tuple<uint8_t, uint8_t, uint8_t> colorOfBlock<AVX2>(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height);
