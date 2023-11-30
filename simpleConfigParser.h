#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include <string>
#include <iostream>
#include <algorithm>

// https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

void parseLine(std::string& line, std::map<std::string, std::string>& configMap) {
    trim(line);
    if(line.length() > 0) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            rtrim(key);
            ltrim(value);
            configMap[key] = value;
        }
    }
}

std::map<std::string, std::string> parseConfig(const char* filename) {
    std::map<std::string, std::string> configMap;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    const size_t bufferSize = 4096;
    char buffer[bufferSize];

    ssize_t size = read(fd, buffer, bufferSize);
    if (size == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    std::string line;
    for(ssize_t i = 0; i < size; i++) {
        if (buffer[i] == '\n') {
            parseLine(line, configMap);
            line = "";
        }
        else {
            line += buffer[i];
        }
    }
    parseLine(line, configMap);

    close(fd);
    return configMap;
}