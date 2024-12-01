#pragma once
#include <termios.h>
#include <memory>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <string_view>

class SerialPort {
public:
    // Constructor and Destructor
    SerialPort(std::string_view port, int baudrate);
    ~SerialPort();

    void write(const char* data, size_t len) const;
    void write(char c) const;
    char read() const;

    void flush() const;
    std::string read_line() const;

    // Move semantics
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    // Disable copy semantics
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Stream insertion and extraction operators
    friend std::ostream& operator<<(std::ostream& os, const SerialPort& sp);
    friend std::istream& operator>>(std::istream& is, SerialPort& sp);

private:
    static speed_t get_baudrate_constant(int baudrate) ;

    int fp{-1};  // Raw file descriptor (int), default to invalid (-1)
    static const std::unordered_map<int, speed_t> baudrate_map;
};