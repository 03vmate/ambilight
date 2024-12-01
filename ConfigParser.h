#pragma once
#include <string>
#include <map>

class ConfigParser {
    static void trim(std::string& s);
public:
    static std::map<std::string, std::string> parse(const std::string& filename);
};
