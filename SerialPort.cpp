#include "SerialPort.hpp"
#include <fcntl.h>
#include <termios.h>
#include <stdexcept>
#include <system_error>


// Baudrate map (using const instead of constexpr)
const std::unordered_map<int, speed_t> SerialPort::baudrate_map = {
        { 110, B110 }, { 300, B300 }, { 600, B600 }, { 1200, B1200 },
        { 2400, B2400 }, { 4800, B4800 }, { 9600, B9600 }, { 19200, B19200 },
        { 38400, B38400 }, { 57600, B57600 }, { 115200, B115200 }, { 230400, B230400 },
        { 460800, B460800 }, { 500000, B500000 }, { 576000, B576000 }, { 921600, B921600 },
        { 1000000, B1000000 }, { 1152000, B1152000 }, { 1500000, B1500000 }, { 2000000, B2000000 },
        { 2500000, B2500000 }, { 3000000, B3000000 }, { 3500000, B3500000 }, { 4000000, B4000000 }
};

SerialPort::SerialPort(std::string_view port, int baudrate) {
    fp = open(std::string(port).c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fp < 0) {
        throw std::system_error(errno, std::system_category(), "Failed to open serial port");
    }

    struct termios tty;
    if (tcgetattr(fp, &tty) != 0) {
        throw std::system_error(errno, std::system_category(), "Failed to get terminal attributes");
    }

    // Set up serial port configuration
    tty.c_cflag &= ~PARENB;  // Parity off
    tty.c_cflag &= ~CSTOPB;  // Stop bits one
    tty.c_cflag &= ~CSIZE;   // Clear size
    tty.c_cflag |= CS8;      // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable read

    tty.c_lflag &= ~ICANON;  // Disable canonical mode
    tty.c_lflag &= ~ECHO;    // Disable echo
    tty.c_lflag &= ~ECHOE;   // Disable erasure
    tty.c_lflag &= ~ECHONL;  // Disable new-line echo
    tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT, and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable special byte handling

    tty.c_oflag &= ~OPOST;   // Disable output processing
    tty.c_oflag &= ~ONLCR;   // Disable newline translation

    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    try {
        auto baudrate_const = get_baudrate_constant(baudrate);
        if (baudrate_const) {
            cfsetospeed(&tty, baudrate_const);
            cfsetispeed(&tty, baudrate_const);
        } else {
            throw std::invalid_argument("Unsupported baud rate");
        }
    } catch (const std::exception& e) {
        throw std::system_error(errno, std::system_category(), e.what());
    }

    if (tcsetattr(fp, TCSANOW, &tty) != 0) {
        throw std::system_error(errno, std::system_category(), "Failed to set terminal attributes");
    }
}

SerialPort::~SerialPort() {
    if (fp >= 0) {
        ::close(fp);
    }
}

void SerialPort::write(const char* data, size_t len) const {
    if (fp >= 0) {
        if(::write(fp, data, len) == -1) {
            throw std::runtime_error("Failed to write to serial port");
        }
    } else {
        throw std::runtime_error("Serial port not initialized");
    }
}

void SerialPort::write(const char c) const {
    write(&c, 1);
}

char SerialPort::read() const {
    if (fp < 0) {
        throw std::runtime_error("Serial port not initialized");
    }

    char buf;
    if (::read(fp, &buf, 1) > 0) {
        return buf;
    }
    else {
        throw std::runtime_error("Failed to read from serial port");
    }
}

void SerialPort::flush() const {
    if (fp >= 0) {
        tcflush(fp, TCIOFLUSH);
    } else {
        throw std::runtime_error("Serial port not initialized");
    }
}

std::string SerialPort::read_line() const {
    if (fp < 0) {
        throw std::runtime_error("Serial port not initialized");
    }

    char buf;
    std::string result;
    while (::read(fp, &buf, 1) > 0) {
        if (buf == '\n') {
            break;
        }
        result += buf;
    }
    return result;
}

speed_t SerialPort::get_baudrate_constant(int baudrate) {
    auto it = baudrate_map.find(baudrate);
    if (it == baudrate_map.end()) {
        throw std::invalid_argument("Unsupported baud rate");
    }
    return it->second;

}

SerialPort::SerialPort(SerialPort&& other) noexcept {
    fp = other.fp;
    other.fp = -1;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        fp = other.fp;
        other.fp = -1;
    }
    return *this;
}

std::ostream& operator<<(std::ostream& os, const SerialPort& sp) {
    if (sp.fp < 0) {
        throw std::runtime_error("Serial port not initialized");
    }

    std::string data = sp.read_line();
    os << data;
    return os;
}

std::istream& operator>>(std::istream& is, SerialPort& sp) {
    if (sp.fp < 0) {
        throw std::runtime_error("Serial port not initialized");
    }

    std::string data;
    is >> data;
    sp.write(data.c_str(), data.size());
    return is;
}