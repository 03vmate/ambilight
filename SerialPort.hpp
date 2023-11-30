#include <cstdio>
#include <termios.h>
#pragma once

class SerialPort {
    public:
        int init(const char* port, int baudrate);
        void write(const char* data, size_t len);
        void flush();
        void close();
    
    private:
        int fp;
        static speed_t get_baudrate_constant(int baudrate);
};