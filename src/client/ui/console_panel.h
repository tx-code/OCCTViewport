#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

/**
 * @brief ImGui Console panel for command input and log display
 * 
 * Features:
 * - Command history with up/down navigation
 * - Auto-completion support
 * - Colored output (info, warning, error)
 * - Scrollable log with auto-scroll to bottom
 * - Clear and copy functionality
 */
class ConsolePanel {
public:
    ConsolePanel();
    ~ConsolePanel() = default;
    
    // Disable copy/move for simplicity
    ConsolePanel(const ConsolePanel&) = delete;
    ConsolePanel& operator=(const ConsolePanel&) = delete;
    
    void render();
    void setVisible(bool visible) { is_visible_ = visible; }
    bool isVisible() const { return is_visible_; }
    
    // Log functions
    void addLog(const char* fmt, ...) IM_FMTARGS(2);
    void addLogInfo(const char* fmt, ...) IM_FMTARGS(2);
    void addLogWarning(const char* fmt, ...) IM_FMTARGS(2);
    void addLogError(const char* fmt, ...) IM_FMTARGS(2);
    void clearLog();
    
    // Thread-safe log functions for spdlog integration
    void addLogDebugThreadSafe(const std::string& message);
    void addLogInfoThreadSafe(const std::string& message);
    void addLogWarningThreadSafe(const std::string& message);
    void addLogErrorThreadSafe(const std::string& message);
    
private:
    // Command execution
    void executeCommand(const char* command);
    int textEditCallback(ImGuiInputTextCallbackData* data);
    
    // Static callback wrapper
    static int textEditCallbackStub(ImGuiInputTextCallbackData* data);
    
    enum class LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };
    
    struct LogItem {
        std::string text;
        ImU32 color;
        LogLevel level;
        
        LogItem(const std::string& t, ImU32 c, LogLevel l) : text(t), color(c), level(l) {}
    };
    
    // UI state
    bool is_visible_ = true;
    bool scroll_to_bottom_ = false;
    bool auto_scroll_ = true;
    
    // Log level filtering
    LogLevel min_log_level_ = LogLevel::Info;  // Default: hide debug messages
    bool show_debug_ = false;  // Hide debug by default
    bool show_info_ = true;
    bool show_warning_ = true;
    bool show_error_ = true;
    
    // Console data
    char input_buffer_[256] = "";
    std::vector<LogItem> log_items_;
    std::vector<std::string> command_history_;
    int history_pos_ = -1;
    
    // Colors
    ImU32 color_info_ = IM_COL32(200, 200, 200, 255);      // Light gray
    ImU32 color_warning_ = IM_COL32(255, 200, 100, 255);   // Orange
    ImU32 color_error_ = IM_COL32(255, 100, 100, 255);     // Red
    ImU32 color_command_ = IM_COL32(100, 200, 255, 255);   // Blue
    
    // Thread safety for spdlog integration
    mutable std::mutex log_mutex_;
};