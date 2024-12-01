#include "networkmode.hpp"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <turbojpeg.h>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <complex>
#include <sys/socket.h>
#include <netinet/in.h>
#include "SerialPort.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>


void start_networkmode(std::map<std::string, std::string> config) {
    int baudrate = std::stoi(config["baud"]);
    int port = std::stoi(config["port"]);
    int vertical_leds = std::stoi(config["vertical_leds"]);
    int horizontal_leds = std::stoi(config["horizontal_leds"]);

    // Initialize serial port
    SerialPort mcu = SerialPort(config["serial_port"], baudrate);

    // Initialize socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cout << "Error creating socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cout << "Error binding socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(serverSocket, 1) == -1) {
        std::cout << "Error listening on socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    int data_count = vertical_leds * horizontal_leds * 2 * 3;
    char receive_buf[data_count * 2];
    char led_buf[data_count * 2];
    ssize_t led_buf_pos = 0;

    while(true) {
        // Accept connection
        int clientSocket = accept(serverSocket, NULL, NULL);
        std::cout << "Accepted connection" << std::endl;
        if (clientSocket == -1) {
            std::cout << "Error accepting connection" << std::endl;
            exit(EXIT_FAILURE);
        }

        while(true) {
            int len = recv(clientSocket, receive_buf, data_count * 2, 0);
            if(len == 0) {
                std::cout << "Client disconnected" << std::endl;
                break;
            }
            if (len == -1) {
                std::cout << "Error reading from socket" << std::endl;
                continue;
            }
            for(int i = 0; i < len; i++) {
                if (receive_buf[i] == '\n') {
                    // Write data to serial port
                    mcu.write(led_buf, led_buf_pos);
                    mcu.write("\n", 1);
                    led_buf_pos = 0;
                }
                else {
                    led_buf[led_buf_pos++] = receive_buf[i];
                    if(led_buf_pos >= data_count * 2 - 1) {
                        led_buf_pos = 0;
                    }
                }
            }
        }
    }
}