#include "config_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace IOAlgorithm {

ConfigParser::ConfigParser(const std::string& filename) {
    ParseFile(filename);
}

void ConfigParser::ParseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filename << ". Using default parameters.\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos) continue;

        std::string key = line.substr(0, equal_pos);
        std::string value = line.substr(equal_pos + 1);

        auto trim = [](std::string& s) {
            if (s.empty()) return;
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            if (!s.empty()) {
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
            }
        };

        trim(key);
        trim(value);

        if (!key.empty()) {
            data[key] = value;
        }
    }
}

int ConfigParser::GetInt(const std::string& key, int default_value) const {
    auto it = data.find(key);
    if (it != data.end()) {
        try { return std::stoi(it->second); } catch(...) {}
    }
    return default_value;
}

double ConfigParser::GetDouble(const std::string& key, double default_value) const {
    auto it = data.find(key);
    if (it != data.end()) {
        try { return std::stod(it->second); } catch(...) {}
    }
    return default_value;
}

std::string ConfigParser::GetString(const std::string& key, const std::string& default_value) const {
    auto it = data.find(key);
    if (it != data.end()) return it->second;
    return default_value;
}

bool ConfigParser::HasKey(const std::string& key) const {
    return data.find(key) != data.end();
}

}
