#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <turbojpeg.h>
#include <csignal>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <complex>
#include "SerialPort.hpp"

bool v4l2_run = true;

struct buffer {
    void* start;
    size_t length;
};

void v4l2_sighandler(int signum) {
    std::cout << "Caught signal " << signum << ", exiting" << std::endl;
    v4l2_run = false;
}

uint8_t* colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height) {
    signal(SIGINT, v4l2_sighandler);
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

void start_v4l2mode(std::map<std::string, std::string> config) {
    signal(SIGINT, v4l2_sighandler);

    // Parse config
    int vertical_leds = std::stoi(config["vertical_leds"]);
    int horizontal_leds = std::stoi(config["horizontal_leds"]);
    int border_size = std::stoi(config["border_size"]);
    int capture_width = std::stoi(config["capture_width"]);
    int capture_height = std::stoi(config["capture_height"]);
    int capture_fps = std::stoi(config["capture_fps"]);
    float column_block_height = (float)capture_height / (float)vertical_leds;
    float row_block_width = (float)capture_width / (float)horizontal_leds;
    double gamma = std::stod(config["gamma_correction"]);
    int buffer_count = std::stoi(config["v4l2_buffer_count"]);
    int baudrate = std::stoi(config["baud"]);

    // Initialize serial port
    SerialPort mcu = SerialPort();
    if (mcu.init(config["serial_port"].c_str(), baudrate) != 0) {
        std::cout << "Error initializing serial port" << std::endl;
        exit(1);
    }

    // Open capture device
    int fd = open(config["capture_device"].c_str(), O_RDWR);
    if (fd == -1) {
        std::cout << "Error opening device" << std::endl;
        exit(1);
    }

    // Query device capabilities
    struct v4l2_capability cap;
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cout << "Error querying device" << std::endl;
        exit(1);
    }
    std::cout << "Driver: " << cap.driver << std::endl;
    std::cout << "Card: " << cap.card << std::endl;
    std::cout << "Bus Info: " << cap.bus_info << std::endl;
    std::cout << "Version: " << ((cap.version >> 16) & 0xFF) << "." << ((cap.version >> 8) & 0xFF) << "." << (cap.version & 0xFF) << std::endl;

    // Set capture format
    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = capture_width;
    format.fmt.pix.height = capture_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if(ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        std::cout << "Error setting format" << std::endl;
        exit(1);
    }

    // Set frame rate
    struct v4l2_streamparm fps;
    memset(&fps, 0, sizeof(fps));
    fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fps.parm.capture.timeperframe.numerator = 1;
    fps.parm.capture.timeperframe.denominator = capture_fps; //FPS = denominator / numerator
    if (ioctl(fd, VIDIOC_S_PARM, &fps) == -1) {
        std::cout << "Error setting frame rate" << std::endl;
        exit(1);
    }

    // Request buffer
    struct v4l2_requestbuffers req;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = 4;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        std::cout << "Error requesting buffer" << std::endl;
        exit(1);
    }

    // Map buffer
    buffer buffers[buffer_count];
    for(int i = 0; i < buffer_count; i++) {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            std::cout << "Error querying buffer" << std::endl;
            exit(1);
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    }

    // Queue buffer
    for(int i = 0; i < buffer_count; i++) {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cout << "Error queueing buffer" << std::endl;
            exit(1);
        }
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cout << "Error starting stream" << std::endl;
        exit(1);
    }

    // Create JPEG decompressor
    tjhandle tjhandle = tjInitDecompress();

    // Buffer for holding the decoded rgb data
    unsigned char *rgbBuffer = nullptr;

    // Buffer for holding the LED rgb data to send to MCU
    uint8_t* leddata = new uint8_t[(horizontal_leds + vertical_leds) * 2 * 3];

    while (v4l2_run) {
        auto start = std::chrono::high_resolution_clock::now();

        // Dequeue buffer
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            std::cout << "Error dequeueing buffer" << std::endl;
            exit(1);
        }

        auto dqtime = std::chrono::high_resolution_clock::now();


        // Decompress header
        int width, height, jpegsubsamp, jpegcolorspace;
        int header_result = tjDecompressHeader3(tjhandle, static_cast<unsigned char*>(buffers[buf.index].start), buffers[buf.index].length, &width, &height, &jpegsubsamp, &jpegcolorspace);
        if(header_result == -1) {
            std::cout << "Error decompressing header, fetching new buffer" << std::endl;
            // Queue buffer
            while (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                std::cout << "Error queuing buffer" << std::endl;
            }
            continue;
        }

        // First time running, allocate buffer for decoded rgb data
        if(rgbBuffer == nullptr) {
            rgbBuffer = new unsigned char[width * height * 3];
        }

        // Decompress jpeg
        tjDecompress2(tjhandle, static_cast<unsigned char*>(buffers[buf.index].start), buffers[buf.index].length, rgbBuffer, width, 0, height, TJPF_RGB, 0);

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

        // \n is special, as it is used for the end of the message. Replace data in LED colors with the closest brightness that is not \n. Also perform gamma correction.
        for(int i = 0; i < (horizontal_leds + vertical_leds) * 2 * 3; i++) {
            leddata[i] = gammaCorrection(leddata[i], gamma);
            if(leddata[i] == '\n') {
                leddata[i] -= 1;
            }
        }

        auto proctime = std::chrono::high_resolution_clock::now();

        // Send data
        mcu.write(reinterpret_cast<const char*>(leddata), (horizontal_leds + vertical_leds) * 2 * 3);
        mcu.write("\n", 1);

        auto writetime = std::chrono::high_resolution_clock::now();

        // Queue buffer
        while (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cout << "Error queuing buffer" << std::endl;
        }

        auto stop = std::chrono::high_resolution_clock::now();
        auto dqduration = std::chrono::duration_cast<std::chrono::milliseconds>(dqtime - start);
        auto decompduration = std::chrono::duration_cast<std::chrono::milliseconds>(decomptime - dqtime);
        auto procduration = std::chrono::duration_cast<std::chrono::milliseconds>(proctime - decomptime);
        auto writeduration = std::chrono::duration_cast<std::chrono::milliseconds>(writetime - proctime);
        auto queueduration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - writetime);
        auto totalduration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        std::cout << "\rdq: " << dqduration.count() << "ms | decomp: " << decompduration.count() << "ms | proc: " << procduration.count() << "ms | write: " << writeduration.count() << "ms | queue: " << queueduration.count() << "ms | total: " << totalduration.count() << "ms / " << 1000 / totalduration.count() << "fps";
        std::cout.flush();
    }

    std::cout << "Stopping" << std::endl;

    delete[] rgbBuffer;

    // Stop streaming
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        std::cout << "Error stopping stream" << std::endl;
        exit(1);
    }

    // Unmap buffers
    for (int i = 0; i < buffer_count; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }

    // Close v4l2 device
    close(fd);
}