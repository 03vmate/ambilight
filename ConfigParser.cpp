#include <fstream>
#include <algorithm>
#include "ConfigParser.h"

std::map<std::string, std::string> ConfigParser::parse(const std::string& filename)  {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file: " + filename);
    }

    std::map<std::string, std::string> configMap;
    std::string line;

    while (std::getline(file, line)) {
        trim(line);
        if (!line.empty()) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                trim(key);
                trim(value);
                configMap[key] = value;
            }
        }
    }

    return configMap;
}

void ConfigParser::trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}