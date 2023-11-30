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

void printV4L2Capability(const struct v4l2_capability& cap) {
    printf("Driver: %s\n", cap.driver);
    printf("Card: %s\n", cap.card);
    printf("Bus Info: %s\n", cap.bus_info);
    printf("Version: %u.%u.%u\n\n", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> config = parseConfig(argv[1]);

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
    req.count = 1;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        std::cout << "Error requesting buffer" << std::endl;
        return -1;
    }

    // Map buffer
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
        std::cout << "Error querying buffer" << std::endl;
        return -1;
    }
    void *buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    // Queue buffer
    if(ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        std::cout << "Error queuing buffer" << std::endl;
        return -1;
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cout << "Error starting stream" << std::endl;
        return -1;
    }

    // Create JPEG decompressor
    tjhandle tjhandle = tjInitDecompress();

    unsigned char *rgbBuffer = nullptr;

    while (true) {
        // Dequeue buffer
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            std::cout << "Error dequeuing buffer" << std::endl;
            return -1;
        }

        // Decompress JPEG
        int width, height, jpegsubsamp, jpegcolorspace;
        int header_result = tjDecompressHeader3(tjhandle, static_cast<unsigned char*>(buffer), buf.bytesused, &width, &height, &jpegsubsamp, &jpegcolorspace);
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
        tjDecompress2(tjhandle, static_cast<unsigned char*>(buffer), buf.bytesused, rgbBuffer, width, 0, height, TJPF_RGB, 0);

        uint8_t*** img = flatbufToImg(rgbBuffer, width, height);
        uint8_t* leddata = new uint8_t[(horizontal_leds + vertical_leds) * 2 * 3];
        ssize_t leddata_index = 0;

        
        //right column, bottom to top
        for(int i = vertical_leds - 1; i >= 0; i--) {
            int block_top = i * column_block_height;
            int block_left = 0;
            uint8_t* color = colorOfBlock(img, block_left, block_top, column_block_width, column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //top row, right to left
        for(int i = horizontal_leds - 1; i >= 0; i--) {
            int block_top = 0;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(img, block_left, block_top, row_block_width, row_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //left column, top to bottom
        for(int i = 0; i < vertical_leds; i++) {
            int block_top = i * column_block_height;
            int block_left = 0;
            uint8_t* color = colorOfBlock(img, block_left, block_top, column_block_width, column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        //bottom row, left to right
        for(int i = 0; i < horizontal_leds; i++) {
            int block_top = capture_height - row_block_height;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(img, block_left, block_top, row_block_width, row_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        std::cout << "Update" << std::endl;

        freeImg(img, width, height);

        // Queue buffer
        while (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cout << "Error queuing buffer" << std::endl;
        }
    }

    delete[] rgbBuffer;

    // Stop streaming
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        std::cout << "Error stopping stream" << std::endl;
        return -1;
    }

    // Unmap buffer
    munmap(buffer, buf.length);

    // Close device
    close(fd);


    return 0;
}