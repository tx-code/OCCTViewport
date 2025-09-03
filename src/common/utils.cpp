#include "common/utils.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace occt_imgui {
namespace common {

std::string Utils::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool Utils::FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::vector<std::string> Utils::SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

} // namespace common
} // namespace occt_imgui