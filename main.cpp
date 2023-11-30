#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <turbojpeg.h>
#include <opencv2/opencv.hpp>
#include "simpleConfigParser.h"

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

    std::cout << config["capture_device"] << std::endl;

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
    format.fmt.pix.width = 1920;
    format.fmt.pix.height = 1080;
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
    fps.parm.capture.timeperframe.numerator = 1; // Set the numerator of the frame rate
    fps.parm.capture.timeperframe.denominator = 30; // Set the denominator of the frame rate (e.g., 30 frames per second)
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

        cv::Mat image(height, width, CV_8UC3, rgbBuffer);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
        cv::imshow("Image", image);
        if(cv::waitKey(1) != -1) {
            break;
        }

        std::cout << "Update" << std::endl;

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