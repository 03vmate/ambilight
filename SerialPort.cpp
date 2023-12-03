#include "SerialPort.hpp"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


int SerialPort::init(const char* port, int baudrate) {
    fp = open(port, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fp < 0) {
        return errno;
    }

    struct termios tty;
    if(tcgetattr(fp, &tty) != 0) {
        return errno;
    }

    tty.c_cflag &= ~PARENB; //Parity off
    tty.c_cflag &= ~CSTOPB; //Stop bits one
    tty.c_cflag &= ~CSIZE; //Clear size
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable read
    tty.c_lflag &= ~ICANON; //Disable canonical mode
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable special handling of received bytes
    tty.c_oflag &= ~OPOST; // Disable special interpretation of output bytes
    tty.c_oflag &= ~ONLCR; // Disable conversion of newline to carriage return/line feed
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    speed_t baudrate_const = get_baudrate_constant(baudrate);
    cfsetospeed(&tty, baudrate_const);
    cfsetispeed(&tty, baudrate_const);

    if (tcsetattr(fp, TCSANOW, &tty) != 0) {
        return errno;
    }

    return 0;
}

void SerialPort::write(const char* data, size_t len) {
    ::write(fp, data, len);
}

void SerialPort::flush() {
    tcflush(fp, TCIOFLUSH);
}

speed_t SerialPort::get_baudrate_constant(int baudrate) {
    switch (baudrate) {
        case 110: return B110;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 2500000: return B2500000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        case 4000000: return B4000000;
        default: return -1;
    }
}
