#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <turbojpeg.h>
#include <opencv2/opencv.hpp>
#include "simpleConfigParser.h"
#include "image_utils.h"
#include "SerialPort.hpp"
#include <csignal>

bool run = true;

struct buffer {
    void* start;
    size_t length;
};

void printV4L2Capability(const struct v4l2_capability& cap) {
    printf("Driver: %s\n", cap.driver);
    printf("Card: %s\n", cap.card);
    printf("Bus Info: %s\n", cap.bus_info);
    printf("Version: %u.%u.%u\n\n", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
}

void sighandler(int signum) {
    run = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, sighandler);
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> config = parseConfig(argv[1]);

    SerialPort mcu = SerialPort();
    int baudrate = std::stoi(config["baud"]);
    if (mcu.init(config["serial_port"].c_str(), baudrate) != 0) {
        std::cout << "Error initializing serial port" << std::endl;
        return -1;
    }

    int vertical_leds = std::stoi(config["vertical_leds"]);
    int horizontal_leds = std::stoi(config["horizontal_leds"]);
    int border_size = std::stoi(config["border_size"]);
    int capture_width = std::stoi(config["capture_width"]);
    int capture_height = std::stoi(config["capture_height"]);
    int capture_fps = std::stoi(config["capture_fps"]);
    float column_block_width = border_size;
    float column_block_height = (float)capture_height / vertical_leds;
    float row_block_height = border_size;
    float row_block_width = (float)capture_width / horizontal_leds;
    double gamma = std::stod(config["gamma_correction"]);
    int buffer_count = std::stoi(config["v4l2_buffer_count"]);

    int fd = open(config["capture_device"].c_str(), O_RDWR);
    if (fd == -1) {
        std::cout << "Error opening device" << std::endl;
        return -1;
    }

    // Query device capabilities
    struct v4l2_capability cap;
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cout << "Error querying device" << std::endl;
        return -1;
    }
    printV4L2Capability(cap);
    

    // Set capture format
    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = capture_width;
    format.fmt.pix.height = capture_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if(ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        std::cout << "Error setting format" << std::endl;
        return -1;
    }

    // Set frame rate
    struct v4l2_streamparm fps;
    memset(&fps, 0, sizeof(fps));
    fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fps.parm.capture.timeperframe.numerator = 1;
    fps.parm.capture.timeperframe.denominator = capture_fps; //FPS = denominator / numerator
    if (ioctl(fd, VIDIOC_S_PARM, &fps) == -1) {
        std::cout << "Error setting frame rate" << std::endl;
        return -1;
    }

    // Request buffer
    struct v4l2_requestbuffers req;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = 4;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        std::cout << "Error requesting buffer" << std::endl;
        return -1;
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
            return -1;
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
            return -1;
        }
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cout << "Error starting stream" << std::endl;
        return -1;
    }

    // Create JPEG decompressor
    tjhandle tjhandle = tjInitDecompress();

    // Buffer for holding the decoded rgb data
    unsigned char *rgbBuffer = nullptr;

    // Buffer for holding the LED rgb data to send to MCU
    uint8_t* leddata = new uint8_t[(horizontal_leds + vertical_leds) * 2 * 3];

    while (run) {
        auto start = std::chrono::high_resolution_clock::now();

        // Dequeue buffer
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            std::cout << "Error dequeueing buffer" << std::endl;
            return -1;
        }

        auto dqtime = std::chrono::high_resolution_clock::now();


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

        // First time, allocate buffer
        if(rgbBuffer == nullptr) {
            rgbBuffer = new unsigned char[width * height * 3];
        }

        // Decompress jpeg
        tjDecompress2(tjhandle, static_cast<unsigned char*>(buffers[buf.index].start), buffers[buf.index].length, rgbBuffer, width, 0, height, TJPF_RGB, 0);

        auto decomptime = std::chrono::high_resolution_clock::now();


        ssize_t leddata_index = 0;
        
        //right column, bottom to top
        for(int i = vertical_leds - 1; i >= 0; i--) {
            int block_top = i * column_block_height;
            int block_left = capture_width - column_block_width;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, column_block_width, column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //top row, right to left
        for(int i = horizontal_leds - 1; i >= 0; i--) {
            int block_top = 0;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, row_block_width, row_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //left column, top to bottom
        for(int i = 0; i < vertical_leds; i++) {
            int block_top = i * column_block_height;
            int block_left = 0;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, column_block_width, column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //bottom row, left to right
        for(int i = 0; i < horizontal_leds; i++) {
            int block_top = capture_height - row_block_height;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbBuffer, width, height, block_left, block_top, row_block_width, row_block_height);
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
        std::cout << "\rdq: " << dqduration.count() << "ms | decomp: " << decompduration.count() << "ms | proc: " << procduration.count() << "ms | write: " << writeduration.count() << "ms | queue: " << queueduration.count() << "ms | total: " << totalduration.count() << "ms";
        std::cout.flush();
    }

    std::cout << "Stopping" << std::endl;

    delete[] rgbBuffer;

    // Stop streaming
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        std::cout << "Error stopping stream" << std::endl;
        return -1;
    }

    // Unmap buffer
    for (int i = 0; i < buffer_count; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }

    // Close device
    close(fd);


    return 0;
}