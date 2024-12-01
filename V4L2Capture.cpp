#include "V4L2Capture.h"
#include <fcntl.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <csignal>

V4L2Capture::V4L2Capture(std::string_view device, int width, int height, int fps, int buffer_count) : buffer_count(buffer_count) {
    if (buffer_count < 1) {
        throw std::invalid_argument("Buffer count must be at least 1");
    }

    // Open the V4L2 device
    fd = open(device.data(), O_RDWR);
    if (fd == -1) {
        throw std::runtime_error("Failed to open V4L2 device");
    }

    // Set capture format
    struct v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        throw std::runtime_error("Failed to set capture format");
    }

    // Set capture FPS
    setFPS(fps);

    // Request buffers
    struct v4l2_requestbuffers request_buffers{};
    request_buffers.count = buffer_count;
    request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &request_buffers) == -1) {
        throw std::runtime_error("Failed to request buffers");
    }

    // Create buffers
    buffers.reserve(buffer_count);
    for (int i = 0; i < buffer_count; i++) {
        struct v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
            throw std::runtime_error("Failed to query buffer");
        }
        void* ptr = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to map buffer");
        }
        buffers.emplace_back(ptr, buffer.length, i);
    }

    // Queue buffers
    for (int i = 0; i < buffer_count; i++) {
        struct v4l2_buffer buffer{}; // This only serves as a container to pass in an index, the index refers to the memory-mapped buffers in the "buffers" vector
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
            throw std::runtime_error("Failed to queue buffer");
        }
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        throw std::runtime_error("Failed to start streaming");
    }
}

void V4L2Capture::setFPS(int fps) const {
    struct v4l2_streamparm stream_params{};
    stream_params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_params.parm.capture.timeperframe.numerator = 1;
    stream_params.parm.capture.timeperframe.denominator = fps;
    if (ioctl(fd, VIDIOC_S_PARM, &stream_params) == -1) {
        throw std::runtime_error("Failed to set capture FPS");
    }
}

V4L2Capture::V4L2Capture(V4L2Capture&& other) noexcept : buffer_count(other.buffer_count), buffers(std::move(other.buffers)), fd(other.fd) {
    other.fd = -1;
}

V4L2Capture& V4L2Capture::operator=(V4L2Capture&& other) noexcept {
    if (this != &other) {
        buffer_count = other.buffer_count;
        buffers = std::move(other.buffers);
        fd = other.fd;

        other.fd = -1;
    }
    return *this;
}

V4L2Capture::~V4L2Capture() {
    if (fd != -1) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
    }
}

const V4L2Buffer& V4L2Capture::dequeueBuffer() const {
    if (fd == -1) {
        throw std::runtime_error("V4L2 device not initialized");
    }
    struct v4l2_buffer buffer_metadata{};
    buffer_metadata.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_metadata.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buffer_metadata) == -1) {
        throw std::runtime_error("Failed to dequeue buffer");
    }
    return buffers[buffer_metadata.index];
}

void V4L2Capture::queueBuffer(const V4L2Buffer& buffer) const {
    if (fd == -1) {
        throw std::runtime_error("V4L2 device not initialized");
    }
    struct v4l2_buffer buffer_metadata{};
    buffer_metadata.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_metadata.memory = V4L2_MEMORY_MMAP;
    buffer_metadata.index = buffer.get_index();
    if (ioctl(fd, VIDIOC_QBUF, &buffer_metadata) == -1) {
        throw std::runtime_error("Failed to queue buffer");
    }
}