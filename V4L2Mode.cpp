#include <unistd.h>
#include <turbojpeg.h>
#include <csignal>
#include <iostream>
#include <chrono>
#include <complex>
#include <functional>
#include "SerialPort.hpp"
#include "Averager.h"
#include "ColorOfBlock.hpp"
#include "V4L2Capture.h"
#include "V4L2Mode.hpp"
#include "ArrayAverager.h"

bool V4L2Mode::V4L2Run = true;

void V4L2Mode::V4L2Sighandler(int signum) {
    std::cout << "Caught signal " << signum << ", exiting" << std::endl;
    V4L2Mode::V4L2Run = false;
}

uint8_t V4L2Mode::gammaCorrection(uint8_t inputBrightness, double gamma) {
    double adjustedBrightness = 255 * std::pow((inputBrightness / 255.0), gamma);
    adjustedBrightness = std::max(0.0, std::min(adjustedBrightness, 255.0));
    return static_cast<uint8_t>(adjustedBrightness);
}

void V4L2Mode::start(std::map<std::string, std::string> config) {
    signal(SIGINT, V4L2Mode::V4L2Sighandler);

    // Parse config
    const int vertical_leds = std::stoi(config["vertical_leds"]);
    const int horizontal_leds = std::stoi(config["horizontal_leds"]);
    const int border_size = std::stoi(config["border_size"]);
    const int capture_width = std::stoi(config["capture_width"]);
    const int capture_height = std::stoi(config["capture_height"]);
    const int capture_fps = std::stoi(config["capture_fps"]);
    const float column_block_height = (float)capture_height / (float)vertical_leds;
    const float row_block_width = (float)capture_width / (float)horizontal_leds;
    const double gamma = std::stod(config["gamma_correction"]);
    const int buffer_count = std::stoi(config["v4l2_buffer_count"]);
    const int baudrate = std::stoi(config["baud"]);
    const int sleep_after = std::stoi(config["sleep_after"]);
    const int averaging_samples = std::stoi(config["averaging_samples"]);

    // Initialize serial port
    SerialPort mcu = SerialPort(config["serial_port"], baudrate);

    // Open v4l2 device
    V4L2Capture v4l2Capture(config["capture_device"], capture_width, capture_height, capture_fps, buffer_count);

    // Create JPEG decompressor
    tjhandle tjhandle = tjInitDecompress();

    // Buffer for holding the decoded rgb data
    std::unique_ptr<uint8_t[]> rgbBuffer = nullptr;

    const size_t ledCount = (horizontal_leds + vertical_leds) * 2 * 3;

    // Buffer to hold LED data for the current frame
    std::vector<uint8_t> ledData(ledCount);

    // Averager for LED data
    ArrayAverager<uint8_t> ledDataAverager(averaging_samples, ledCount);

    // Averagers for timing debug info
    Averager<int64_t> dqtimeAverager(20);
    Averager<int64_t> decomptimeAverager(20);
    Averager<int64_t> extracttimeAverager(20);
    Averager<int64_t> proctimeAverager(20);
    Averager<int64_t> writetimeAverager(20);
    Averager<int64_t> queuedurationAverager(20);
    Averager<int64_t> totaldurationAverager(20);

    // Sleep mode related variables
    int blankCount = 0; // How many sequential frames have been blank (or more precisely, just the LEDs)
    bool sleepNow = false; // If true, slow down framerate to 1 FPS

    // Pick the best available SIMD implementation
    #ifdef __AVX2__
    std::function<std::tuple<uint8_t, uint8_t, uint8_t>(const uint8_t*, int, int, int, int, int, int)> colorOfBlock = ::colorOfBlock<AVX2>;
    #elif __SSE2__
    std::function<std::tuple<uint8_t, uint8_t, uint8_t>(const uint8_t*, int, int, int, int, int, int)> colorOfBlock = ::colorOfBlock<SSE2>;
    #else
    std::function<std::tuple<uint8_t, uint8_t, uint8_t>(const uint8_t*, int, int, int, int, int, int)> colorOfBlock = ::colorOfBlock<void>;
    #endif

    while (V4L2Run) {
        auto start = std::chrono::high_resolution_clock::now();

        // Dequeue buffer
        const V4L2Buffer& buf = v4l2Capture.dequeueBuffer();
        auto dqtime = std::chrono::high_resolution_clock::now();

        // Decompress header
        int width, height, jpegsubsamp, jpegcolorspace;
        int headerResult = tjDecompressHeader3(tjhandle, static_cast<unsigned char*>(buf.get_ptr()), buf.get_length(), &width, &height, &jpegsubsamp, &jpegcolorspace);

        // If decompression failed, requeue the buffer and start over
        if(headerResult == -1) {
            std::cout << "Error decompressing header, fetching new buffer" << std::endl;
            // Try to requeue a few times
            int retryCount = 0;
            while(true) {
                try {
                    v4l2Capture.queueBuffer(buf);
                    break;
                }
                catch(const std::runtime_error& e) {
                    std::cout << "Error queuing buffer: " << e.what() << ", retrying..." << std::endl;
                }
                retryCount++;
                if(retryCount > 10) {
                    throw std::runtime_error("Failed to queue buffer after 10 retries");
                }
            }
            continue;
        }

        // First time running, allocate buffer for decoded rgb data. Needs to be done after the first frame is captured, as we don't know the size of the image before the JPEG header is parsed.
        if (!rgbBuffer) {
            rgbBuffer = std::make_unique<unsigned char[]>(width * height * 3 + 16); // extra padding needed for SIMD optimizations in colorOfBlock
        }

        // Decompress jpeg
        tjDecompress2(tjhandle, static_cast<unsigned char*>(buf.get_ptr()), buf.get_length(), rgbBuffer.get(), width, 0, height, TJPF_RGB, 0);

        auto decomptime = std::chrono::high_resolution_clock::now();

        // Calculate the colors of the LEDs based on the image
        ssize_t ledDataIndex = 0;
        //right column, bottom to top
        for(int i = vertical_leds - 1; i >= 0; i--) {
            int blockTop = static_cast<int>(static_cast<float>(i) * column_block_height);
            int blockLeft = capture_width - border_size;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, blockLeft, blockTop, border_size, (int)column_block_height);
            ledData[ledDataIndex++] = std::get<0>(color);
            ledData[ledDataIndex++] = std::get<1>(color);
            ledData[ledDataIndex++] = std::get<2>(color);
        }
        //top row, right to left
        for(int i = horizontal_leds - 1; i >= 0; i--) {
            int blockTop = 0;
            int blockLeft = static_cast<int>(static_cast<float>(i) * row_block_width);
            auto color = colorOfBlock(rgbBuffer.get(), width, height, blockLeft, blockTop, (int)row_block_width, border_size);
            ledData[ledDataIndex++] = std::get<0>(color);
            ledData[ledDataIndex++] = std::get<1>(color);
            ledData[ledDataIndex++] = std::get<2>(color);
        }
        //left column, top to bottom
        for(int i = 0; i < vertical_leds; i++) {
            int blockTop = static_cast<int>(static_cast<float>(i) * column_block_height);
            int blockLeft = 0;
            auto color = colorOfBlock(rgbBuffer.get(), width, height, blockLeft, blockTop, border_size, (int)column_block_height);
            ledData[ledDataIndex++] = std::get<0>(color);
            ledData[ledDataIndex++] = std::get<1>(color);
            ledData[ledDataIndex++] = std::get<2>(color);
        }
        //bottom row, left to right
        for(int i = 0; i < horizontal_leds; i++) {
            int blockTop = capture_height - border_size;
            int blockLeft = static_cast<int>(static_cast<float>(i) * row_block_width);
            auto color = colorOfBlock(rgbBuffer.get(), width, height, blockLeft, blockTop, (int)row_block_width, border_size);
            ledData[ledDataIndex++] = std::get<0>(color);
            ledData[ledDataIndex++] = std::get<1>(color);
            ledData[ledDataIndex++] = std::get<2>(color);
        }

        auto extracttime = std::chrono::high_resolution_clock::now();

        // Do gamma correction
        for(int i = 0; i < (horizontal_leds + vertical_leds) * 2 * 3; i++) {
            ledData[i] = gammaCorrection(ledData[i], gamma);
        }

        ledDataAverager.add(ledData.data());

        // Get averaged data
        std::vector<uint8_t> ledDataAvg(ledCount);
        ledDataAverager.getAverage<uint64_t>(ledDataAvg.data()); // Use uint64_t for summing internally to prevent overflow

        // Detect if blank and replace newlines(special delimiter)
        bool blank = true;
        for(int i = 0; i < ledCount; i++) {
            // \n is special, as it is used for the end of the message. Replace data in LED colors with the closest brightness that is not \n.
            if(ledDataAvg[i] == '\n') {
                ledDataAvg[i] -= 1;
            }
            // Check if the LEDs are off, for sleep detection
            if(ledDataAvg[i] != 0) {
                blank = false;
            }
        }

        // Start counting up if LEDs are off, and enter sleep mode if the count is high enough
        if(blank) {
            blankCount++;
            if(blankCount >= sleep_after) {
                blankCount = sleep_after; //prevent overflow
                sleepNow = true;
            }
        }
        else {
            blankCount = 0;
            sleepNow = false;
        }

        auto proctime = std::chrono::high_resolution_clock::now();

        // Send data to MCU
        mcu.write(reinterpret_cast<const char*>(ledDataAvg.data()), ledCount);
        mcu.write('\n');
        mcu.flush();

        auto writetime = std::chrono::high_resolution_clock::now();

        // Queue buffer
        int retry_count = 0;
        while(true) {
            try {
                v4l2Capture.queueBuffer(buf);
                break;
            }
            catch(const std::runtime_error& e) {
                std::cout << "Error queuing buffer: " << e.what() << ", retrying..." << std::endl;
            }
            retry_count++;
            if(retry_count > 10) {
                throw std::runtime_error("Failed to queue buffer after 10 retries");
            }
        }

        // Timing info output
        auto stop = std::chrono::high_resolution_clock::now();
        auto dqduration = std::chrono::duration_cast<std::chrono::microseconds>(dqtime - start);
        auto decompduration = std::chrono::duration_cast<std::chrono::microseconds>(decomptime - dqtime);
        auto extractduration = std::chrono::duration_cast<std::chrono::microseconds>(extracttime - decomptime);
        auto procduration = std::chrono::duration_cast<std::chrono::microseconds>(proctime - extracttime);
        auto writeduration = std::chrono::duration_cast<std::chrono::microseconds>(writetime - proctime);
        auto queueduration = std::chrono::duration_cast<std::chrono::microseconds>(stop - writetime);
        auto totalduration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
        dqtimeAverager.add(dqduration.count());
        decomptimeAverager.add(decompduration.count());
        extracttimeAverager.add(extractduration.count());
        proctimeAverager.add(procduration.count());
        writetimeAverager.add(writeduration.count());
        queuedurationAverager.add(queueduration.count());
        totaldurationAverager.add(totalduration.count());
        std::cout << "\r\033[Kdq: " << dqtimeAverager.getAverage() << "us \t| decomp: " << decomptimeAverager.getAverage() << "us\t | extract: " << extracttimeAverager.getAverage() << "us\t | proc: " << proctimeAverager.getAverage() << "us\t | write: " << writetimeAverager.getAverage() << "us\t | queue: " << queuedurationAverager.getAverage() << "us\t | total: " << totaldurationAverager.getAverage() << "us / " << 1000000 / totaldurationAverager.getAverage() << "fps";
        if(sleepNow) {
            std::cout << "  SLEEPING     ";
        }
        else {
            std::cout << "               ";
        }
        std::cout.flush();

        // Sleep if needed
        if(sleepNow) {
            usleep(1000000); //sleep for 1 second, slowing down to ~1 FPS
        }
    }

    std::cout << "Stopping" << std::endl;
}
