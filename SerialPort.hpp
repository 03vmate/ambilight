#pragma once
#include <termios.h>
#include <memory>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <string_view>

class SerialPort {
public:
    SerialPort(std::string_view port, int baudrate);
    ~SerialPort();

    void write(const char* data, size_t len) const;
    void write(char c) const;
    char read() const;

    void flush() const;
    std::string readLine() const;

    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    friend std::ostream& operator<<(std::ostream& os, const SerialPort& sp);
    friend std::istream& operator>>(std::istream& is, SerialPort& sp);

private:
    static speed_t getBaudrateConstant(int baudrate) ;

    int fp{-1};
    static const std::unordered_map<int, speed_t> baudrateMap;
};