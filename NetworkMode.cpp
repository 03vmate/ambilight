#include <turbojpeg.h>
#include <iostream>
#include <cstdint>
#include <complex>
#include <sys/socket.h>
#include <netinet/in.h>
#include "SerialPort.hpp"
#include "NetworkMode.hpp"

void NetworkMode::start(std::map<std::string, std::string> config) {
    const int baudrate = std::stoi(config["baud"]);
    const int port = std::stoi(config["port"]);
    const int verticalLeds = std::stoi(config["verticalLeds"]);
    const int horizontalLeds = std::stoi(config["horizontalLeds"]);

    // Initialize serial port
    SerialPort mcu = SerialPort(config["serial_port"], baudrate);

    // Initialize socket
    int serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        throw std::runtime_error("Error creating socket");
    }

    // Bind socket to port
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(static_cast<uint16_t>(port));
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        throw std::runtime_error("Error binding socket");
    }

    // Listen for connections
    if (listen(serverSocket, 1) == -1) {
        throw std::runtime_error("Error listening on socket");
    }

    const size_t dataCount = verticalLeds * horizontalLeds * 2 * 3;
    std::unique_ptr<char[]> receiveBuf = std::make_unique<char[]>(dataCount * 2);
    std::unique_ptr<char[]> ledBuf = std::make_unique<char[]>(dataCount * 2);
    ssize_t ledBufPos = 0;

    while(true) {
        // Accept connection
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        std::cout << "Accepted connection" << std::endl;
        if (clientSocket == -1) {
            std::cout << "Error accepting connection" << std::endl;
            exit(EXIT_FAILURE);
        }

        while(true) {
            ssize_t len = ::recv(clientSocket, receiveBuf.get(), dataCount * 2, 0);
            if(len == 0) {
                std::cout << "Client disconnected" << std::endl;
                break;
            }
            if (len == -1) {
                std::cout << "Error reading from socket" << std::endl;
                continue;
            }
            for(int i = 0; i < len; i++) {
                if (receiveBuf[i] == '\n') {
                    // Write data to serial port
                    mcu.write(ledBuf.get(), ledBufPos);
                    mcu.write("\n", 1);
                    ledBufPos = 0;
                }
                else {
                    ledBuf[ledBufPos++] = receiveBuf[i];
                    if(static_cast<size_t>(ledBufPos) >= static_cast<size_t>(dataCount * 2 - 1)) {
                        ledBufPos = 0;
                    }
                }
            }
        }
    }
}