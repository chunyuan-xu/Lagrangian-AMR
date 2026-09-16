#pragma once
#include <string>
#include <map>

namespace IOAlgorithm {

class ConfigParser {
public:
    ConfigParser(const std::string& filename);

    int GetInt(const std::string& key, int default_value) const;
    double GetDouble(const std::string& key, double default_value) const;
    std::string GetString(const std::string& key, const std::string& default_value) const;
    bool HasKey(const std::string& key) const;

private:
    std::map<std::string, std::string> data;
    void ParseFile(const std::string& filename);
};

}
