#include <unistd.h>
#include <turbojpeg.h>
#include <csignal>
#include <iostream>
#include <chrono>
#include <complex>
#include <memory> // for smart pointers
#include <vector> // for std::vector
#include <algorithm>
#include "SerialPort.hpp"
#include "Averager.h"
#include "colorOfBlock.hpp"
#include "V4L2Capture.h"
#include "v4l2mode.hpp"
#include "ArrayAverager.h"

bool v4l2_run = true;

void v4l2_sighandler(int signum) {
    std::cout << "Caught signal " << signum << ", exiting" << std::endl;
    v4l2_run = false;
}

uint8_t gammaCorrection(uint8_t inputBrightness, double gamma) {
    double adjustedBrightness = 255 * std::pow((inputBrightness / 255.0), gamma);
    adjustedBrightness = std::max(0.0, std::min(adjustedBrightness, 255.0));
    return static_cast<uint8_t>(adjustedBrightness);
}

void start_v4l2mode(std::map<std::string, std::string> config) {
    signal(SIGINT, v4l2_sighandler);

    // Parse config
    const int vertical_leds = std::stoi(config["vertical_leds"]);
    const int horizontal_leds = std::stoi(config["horizontal_leds"]);
    const int border_size = std::stoi(config["border_size"]);
    const int capture_width = std::stoi(config["capture_width"]);
    const int capture_height = std::stoi(config["capture_height"]);
    const int capture_fps = std::stoi(config["capture_fps"]);
    const float column_block_height = static_cast<float>(capture_height) / vertical_leds;
    const float row_block_width = static_cast<float>(capture_width) / horizontal_leds;
    const double gamma = std::stod(config["gamma_correction"]);
    const int buffer_count = std::stoi(config["v4l2_buffer_count"]);
    const int baudrate = std::stoi(config["baud"]);
    const int sleep_after = std::stoi(config["sleep_after"]);
    const int averaging_samples = std::stoi(config["averaging_samples"]);

    // Initialize serial port
    SerialPort mcu(config["serial_port"], baudrate);

    // Open v4l2 device
    V4L2Capture v4l2Capture(config["capture_device"], capture_width, capture_height, capture_fps, buffer_count);

    // Create JPEG decompressor
    tjhandle tjhandle = tjInitDecompress();

    // Buffer for holding the decoded rgb data
    std::unique_ptr<unsigned char[]> rgbBuffer = nullptr;

    const size_t ledCount = (horizontal_leds + vertical_leds) * 2 * 3;

    // Buffer to hold LED data for the current frame
    std::vector<uint8_t> leddata(ledCount);

    // Averager for LED data
    ArrayAverager<uint8_t> leddataAverager(averaging_samples, ledCount);

    // Averagers for timing debug info
    Averager<int64_t> dqtimeAverager(20);
    Averager<int64_t> decomptimeAverager(20);
    Averager<int64_t> extracttimeAverager(20);
    Averager<int64_t> proctimeAverager(20);
    Averager<int64_t> writetimeAverager(20);
    Averager<int64_t> queuedurationAverager(20);
    Averager<int64_t> totaldurationAverager(20);

    // Sleep mode related variables
    int blank_count = 0;
    bool sleep_now = false;

    while (v4l2_run) {
        auto start = std::chrono::high_resolution_clock::now();

        // Dequeue buffer
        const V4L2Buffer &buf = v4l2Capture.dequeueBuffer();
        auto dqtime = std::chrono::high_resolution_clock::now();

        // Decompress header
        int width, height, jpegsubsamp, jpegcolorspace;
        int header_result = tjDecompressHeader3(tjhandle, static_cast<unsigned char *>(buf.get_ptr()), buf.get_length(),
                                                &width, &height, &jpegsubsamp, &jpegcolorspace);

        // If decompression failed, requeue the buffer and start over
        if (header_result == -1) {
            std::cout << "Error decompressing header, fetching new buffer" << std::endl;
            int retry_count = 0;
            while (true) {
                try {
                    v4l2Capture.queueBuffer(buf);
                    break;
                }
                catch (const std::runtime_error &e) {
                    std::cout << "Error queuing buffer: " << e.what() << ", retrying..." << std::endl;
                }
                retry_count++;
                if (retry_count > 10) {
                    throw std::runtime_error("Failed to queue buffer after 10 retries");
                }
            }
            continue;
        }

        // First time running, allocate buffer for decoded rgb data
        if (!rgbBuffer) {
            rgbBuffer = std::make_unique<unsigned char[]>(
                    width * height * 3 + 16); // extra padding for SIMD optimizations
        }

        // Decompress jpeg
        tjDecompress2(tjhandle, static_cast<unsigned char *>(buf.get_ptr()), buf.get_length(), rgbBuffer.get(), width,0, height, TJPF_RGB, 0);

        auto decomptime = std::chrono::high_resolution_clock::now();

        // "Extract" the colors of the LEDs from the image
        ssize_t leddata_index = 0;
        // Right column, bottom to top
        for (int i = vertical_leds - 1; i >= 0; i--) {
            int block_top = i * column_block_height;
            int block_left = capture_width - border_size;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, block_left, block_top, border_size,static_cast<int>(column_block_height));
            leddata[leddata_index++] = color[0];  // Red
            leddata[leddata_index++] = color[1];  // Green
            leddata[leddata_index++] = color[2];  // Blue
            delete[] color;
        }
        // Top row, right to left
        for (int i = horizontal_leds - 1; i >= 0; i--) {
            int block_top = 0;
            int block_left = i * row_block_width;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, block_left, block_top,static_cast<int>(row_block_width), border_size);
            leddata[leddata_index++] = color[0];  // Red
            leddata[leddata_index++] = color[1];  // Green
            leddata[leddata_index++] = color[2];  // Blue
            delete[] color;
        }
        // Left column, top to bottom
        for (int i = 0; i < vertical_leds; i++) {
            int block_top = i * column_block_height;
            int block_left = 0;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, block_left, block_top, border_size,static_cast<int>(column_block_height));
            leddata[leddata_index++] = color[0];  // Red
            leddata[leddata_index++] = color[1];  // Green
            leddata[leddata_index++] = color[2];  // Blue
            delete[] color;
        }
        // Bottom row, left to right
        for (int i = 0; i < horizontal_leds; i++) {
            int block_top = capture_height - border_size;
            int block_left = i * row_block_width;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, block_left, block_top,static_cast<int>(row_block_width), border_size);
            leddata[leddata_index++] = color[0];  // Red
            leddata[leddata_index++] = color[1];  // Green
            leddata[leddata_index++] = color[2];  // Blue
            delete[] color;
        }

        auto extracttime = std::chrono::high_resolution_clock::now();

        // Do gamma correction
        std::transform(leddata.begin(), leddata.end(), leddata.begin(), [&gamma](uint8_t &val) {
            return gammaCorrection(val, gamma);
        });

        leddataAverager.add(leddata.data());

        // Get averaged data
        std::vector<uint8_t> leddata_avg(ledCount);
        leddataAverager.getAverage<uint64_t>(leddata_avg.data());

        // Detect if blank and replace newlines (special delimiter)
        bool blank = true;
        for (int i = 0; i < ledCount; i++) {
            if (leddata_avg[i] == '\n') {
                leddata_avg[i] -= 1;
            }
            if (leddata_avg[i] != 0) {
                blank = false;
            }
        }

        if (blank) {
            blank_count++;
            if (blank_count >= sleep_after) {
                blank_count = sleep_after;
                sleep_now = true;
            }
        } else {
            blank_count = 0;
            sleep_now = false;
        }

        auto proctime = std::chrono::high_resolution_clock::now();

        // Send data to MCU
        mcu.write(reinterpret_cast<const char *>(leddata_avg.data()), ledCount);

        auto writetime = std::chrono::high_resolution_clock::now();

        // Sleep logic
        if (sleep_now) {
            usleep(10000);
        }

        auto totalduration = std::chrono::high_resolution_clock::now();

        // Print timing statistics
        std::cout << "Dequeue: " << std::chrono::duration_cast<std::chrono::milliseconds>(dqtime - start).count()
                  << " ms, "
                  << "Decompress: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(decomptime - dqtime).count() << " ms, "
                  << "Extract: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(extracttime - decomptime).count() << " ms, "
                  << "Process: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(proctime - extracttime).count() << " ms, "
                  << "Write: " << std::chrono::duration_cast<std::chrono::milliseconds>(writetime - proctime).count()
                  << " ms, "
                  << "Total: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalduration - start).count()
                  << " ms" << std::endl;

        // Queue the buffer again
        v4l2Capture.queueBuffer(buf);
    }

    tjDestroy(tjhandle);
}
