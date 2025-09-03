#pragma once

#include <string>
#include <vector>

namespace occt_imgui {
namespace common {

// Common utility functions
class Utils {
public:
    static std::string GetTimestamp();
    static bool FileExists(const std::string& path);
    static std::vector<std::string> SplitString(const std::string& str, char delimiter);
};

} // namespace common
} // namespace occt_imgui