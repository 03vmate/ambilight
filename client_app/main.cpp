#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include <complex>
#include "simpleConfigParser.h"
#include <arpa/inet.h>

bool run = true;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    run = false;
}

uint8_t gammaCorrection(uint8_t inputBrightness, double gamma) {
    double adjustedBrightness = 255 * std::pow((inputBrightness / 255.0), gamma);
    adjustedBrightness = std::max(0.0, std::min(adjustedBrightness, 255.0));
    return static_cast<uint8_t>(adjustedBrightness);
}

uint8_t* colorOfBlock(const uint8_t* img, int imgwidth, int imgheight, int x, int y, int width, int height) {
    uint32_t* color = new uint32_t[3];
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;

    for (int xpos = x; xpos < x + width; xpos++) {
        for (int ypos = y; ypos < y + height; ypos++) {
            for (int i = 0; i < 3; i++) {
                int index = (ypos * imgwidth + xpos) * 3 + i;
                color[i] += img[index];
            }
        }
    }

    uint8_t* color_byte = new uint8_t[3];

    for (int i = 0; i < 3; i++) {
        color_byte[i] = color[i] / (width * height);
    }

    delete[] color;
    return color_byte;
}

void xImageToRGBArray(XImage *xImage, unsigned char *rgbArray) {
    for (int y = 0; y < xImage->height; y++) {
        for (int x = 0; x < xImage->width; x++) {
            unsigned long pixel = XGetPixel(xImage, x, y);
            unsigned char red = (pixel & xImage->red_mask) >> 16;
            unsigned char green = (pixel & xImage->green_mask) >> 8;
            unsigned char blue = pixel & xImage->blue_mask;

            int index = (y * xImage->width + x) * 3;
            rgbArray[index] = red;
            rgbArray[index + 1] = green;
            rgbArray[index + 2] = blue;
        }
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, signalHandler);

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config file>" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> config = parseConfig(argv[1]);
    int border_size = std::stoi(config["border_size"]);
    int vertical_leds = std::stoi(config["vertical_leds"]);
    int horizontal_leds = std::stoi(config["horizontal_leds"]);
    double gamma_correction = std::stod(config["gamma_correction"]);
    std::string server_ip = config["server_ip"];
    int server_port = std::stoi(config["server_port"]);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    Display *display = XOpenDisplay(NULL);

    if (!display) {
        fprintf(stderr, "Unable to open X display\n");
        exit(1);
    }

    Window root = DefaultRootWindow(display);
    XWindowAttributes windowAttributes;
    XGetWindowAttributes(display, root, &windowAttributes);
    int width = windowAttributes.width;
    int height = windowAttributes.height;
    std::cout << "width: " << width << " height: " << height << std::endl;
    float column_block_height = (float)height / (float)vertical_leds;
    float row_block_width = (float)width / (float)horizontal_leds;

    uint8_t* leddata = new uint8_t[(horizontal_leds + vertical_leds) * 2 * 3 + 1];

    while(true) {
        XImage *image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);


        if (!image) {
            fprintf(stderr, "Failed to get image from X server\n");
            exit(1);
        }

        unsigned char* rgbArray = new unsigned char[width * height * 3];
        xImageToRGBArray(image, rgbArray);

        //"extract" the colors of the LEDs from the image
        ssize_t leddata_index = 0;
        //right column, bottom to top
        for(int i = vertical_leds - 1; i >= 0; i--) {
            int block_top = i * column_block_height;
            int block_left = width - border_size;
            uint8_t* color = colorOfBlock(rgbArray, width, height, block_left, block_top, border_size, (int)column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //top row, right to left
        for(int i = horizontal_leds - 1; i >= 0; i--) {
            int block_top = 0;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbArray, width, height, block_left, block_top, (int)row_block_width, border_size);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //left column, top to bottom
        for(int i = 0; i < vertical_leds; i++) {
            int block_top = i * column_block_height;
            int block_left = 0;
            uint8_t* color = colorOfBlock(rgbArray, width, height, block_left, block_top, border_size, (int)column_block_height);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }
        //bottom row, left to right
        for(int i = 0; i < horizontal_leds; i++) {
            int block_top = height - border_size;
            int block_left = i * row_block_width;
            uint8_t* color = colorOfBlock(rgbArray, width, height, block_left, block_top, (int)row_block_width, border_size);
            leddata[leddata_index++] = color[0];
            leddata[leddata_index++] = color[1];
            leddata[leddata_index++] = color[2];
            delete[] color;
        }

        // \n is special, as it is used for the end of the message. Replace data in LED colors with the closest brightness that is not \n. Also perform gamma correction.
        for(int i = 0; i < (horizontal_leds + vertical_leds) * 2 * 3; i++) {
            leddata[i] = gammaCorrection(leddata[i], gamma_correction);
            if(leddata[i] == '\n') {
                leddata[i] -= 1;
            }
        }

        leddata[(horizontal_leds + vertical_leds) * 2 * 3] = '\n';

        //send data to server
        if (send(client_socket, leddata, (horizontal_leds + vertical_leds) * 2 * 3 + 1, 0) == -1) {
            std::cout << "Failed to send data to server" << std::endl;
        }

        XDestroyImage(image);
    }


    XCloseDisplay(display);

    return 0;
}
