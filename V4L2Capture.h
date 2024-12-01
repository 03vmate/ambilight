#pragma once
#include <string>
#include <linux/videodev2.h>
#include <memory>
#include <sys/mman.h>
#include <vector>

// RAII wrapper for V4L2 buffer(memory mapped ptr)
class V4L2Buffer {
    void* ptr = nullptr;
    size_t length = 0;
    size_t index = 0; // Index of the buffer within the V4L2 device, used for simpler queueing
public:
    V4L2Buffer(void* ptr, size_t length, size_t index) : ptr(ptr), length(length), index(index) {}
    V4L2Buffer(V4L2Buffer&& other) noexcept : ptr(other.ptr), length(other.length), index(other.index) {
        other.ptr = nullptr;
        other.length = 0;
    }
    V4L2Buffer& operator=(V4L2Buffer&& other) noexcept {
        if (this != &other) {
            ptr = other.ptr;
            length = other.length;
            index = other.index;
            other.ptr = nullptr;
            other.length = 0;
        }
        return *this;
    }
    void* get_ptr() const { return ptr; }
    size_t get_length() const { return length; }
    ~V4L2Buffer() {
        if (ptr != nullptr) {
            munmap(ptr, length);
        }
    }

    V4L2Buffer(const V4L2Buffer&) = delete;
    V4L2Buffer& operator=(const V4L2Buffer&) = delete;

    size_t get_index() const { return index; }
};

class V4L2Capture {
    int buffer_count = 0;
    std::vector<V4L2Buffer> buffers;
    int fd = -1;
public:
    V4L2Capture(std::string_view device, int width, int height, int fps = 30, int buffer_count = 4);
    ~V4L2Capture();

    V4L2Capture(const V4L2Capture&) = delete;
    V4L2Capture& operator=(const V4L2Capture&) = delete;

    V4L2Capture(V4L2Capture&& other) noexcept;
    V4L2Capture& operator=(V4L2Capture&& other) noexcept;

    void setFPS(int fps) const;

    const V4L2Buffer& dequeueBuffer() const;
    void queueBuffer(const V4L2Buffer& buffer) const;
};