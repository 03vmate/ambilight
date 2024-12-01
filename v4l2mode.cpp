#include <unistd.h>
#include <turbojpeg.h>
#include <csignal>
#include <iostream>
#include <chrono>
#include <complex>
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
    unsigned char *rgbBuffer = nullptr;


    const size_t ledCount = (horizontal_leds + vertical_leds) * 2 * 3;

    // Buffer for holding the LED rgb data to send to MCU
    uint8_t* leddata = new uint8_t[ledCount];

    ArrayAverager<uint8_t> leddataAverager(averaging_samples, ledCount);

    Averager<int64_t> dqtimeAverager(20);
    Averager<int64_t> decomptimeAverager(20);
    Averager<int64_t> extracttimeAverager(20);
    Averager<int64_t> proctimeAverager(20);
    Averager<int64_t> writetimeAverager(20);
    Averager<int64_t> queuedurationAverager(20);
    Averager<int64_t> totaldurationAverager(20);

    int blank_count = 0;
    bool sleep_now = false;

    while (v4l2_run) {
        auto start = std::chrono::high_resolution_clock::now();

        // Dequeue buffer
        const V4L2Buffer& buf = v4l2Capture.dequeueBuffer();
        auto dqtime = std::chrono::high_resolution_clock::now();

        // Decompress header
        int width, height, jpegsubsamp, jpegcolorspace;
        int header_result = tjDecompressHeader3(tjhandle, static_cast<unsigned char*>(buf.get_ptr()), buf.get_length(), &width, &height, &jpegsubsamp, &jpegcolorspace);

        // If decompression failed, requeue the buffer and start over
        if(header_result == -1) {
            std::cout << "Error decompressing header, fetching new buffer" << std::endl;
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
            continue;
        }

        // First time running, allocate buffer for decoded rgb data
        if(rgbBuffer == nullptr) {
            rgbBuffer = new unsigned char[width * height * 3 + 16]; //extra padding for the end, to prevent segfaults when using some SIMD optimizations (see comments in simdblock())
        }

        // Decompress jpeg
        tjDecompress2(tjhandle, static_cast<unsigned char*>(buf.get_ptr()), buf.get_length(), rgbBuffer, width, 0, height, TJPF_RGB, 0);

        auto decomptime = std::chrono::high_resolution_clock::now();

        //"extract" the colors of the LEDs from the image
        ssize_t leddata_index = 0;
        //right column, bottom to top
        for(int i = vertical_leds - 1; i >= 0; i--) {
            int block_top = i * column_block_height;
            int block_left = capture_width - border_size;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, border_size, (int)column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //top row, right to left
        for(int i = horizontal_leds - 1; i >= 0; i--) {
            int block_top = 0;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, (int)row_block_width, border_size);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //left column, top to bottom
        for(int i = 0; i < vertical_leds; i++) {
            int block_top = i * column_block_height;
            int block_left = 0;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, border_size, (int)column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //bottom row, left to right
        for(int i = 0; i < horizontal_leds; i++) {
            int block_top = capture_height - border_size;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, (int)row_block_width, border_size);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        auto extracttime = std::chrono::high_resolution_clock::now();

        // Do gamma correction
        for(int i = 0; i < (horizontal_leds + vertical_leds) * 2 * 3; i++) {
            leddata[i] = gammaCorrection(leddata[i], gamma);
        }

        leddataAverager.add(leddata);

        auto proctime = std::chrono::high_resolution_clock::now();

        // Send data
        uint8_t* leddata_avg = new uint8_t[ledCount];
        leddataAverager.getAverage<uint64_t>(leddata_avg); // Use uint64_t for summing internally to prevent overflow

        // Slow down on blank
        bool blank = true;
        for(int i = 0; i < ledCount; i++) {
            // \n is special, as it is used for the end of the message. Replace data in LED colors with the closest brightness that is not \n.
            if(leddata_avg[i] == '\n') {
                leddata_avg[i] -= 1;
            }
            // Check if the data is not blank for sleep detection
            if(leddata_avg[i] != 0) {
                blank = false;
            }
        }

        if(blank) {
            blank_count++;
            if(blank_count >= sleep_after) {
                blank_count = sleep_after; //prevent overflow
                sleep_now = true;
            }
        }
        else {
            blank_count = 0;
            sleep_now = false;
        }

        mcu.write(reinterpret_cast<const char *>(leddata_avg), ledCount);
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
        if(sleep_now) {
            std::cout << "  SLEEPING     ";
        }
        else {
            std::cout << "               ";
        }
        std::cout.flush();

        // Sleep if needed
        if(sleep_now) {
            usleep(1000000); //sleep for 1 second, slowing down to ~1 FPS
        }
    }

    std::cout << "Stopping" << std::endl;

    delete[] rgbBuffer;
}
