#pragma once

#include "console_panel.h"
#include <spdlog/details/log_msg.h>
#include <spdlog/common.h>
#include <string>

template<typename Mutex>
console_sink<Mutex>::console_sink(std::weak_ptr<ConsolePanel> console_panel)
    : console_panel_(std::move(console_panel))
{
}

template<typename Mutex>
void console_sink<Mutex>::sink_it_(const spdlog::details::log_msg& msg)
{
    // Try to lock the weak pointer to ensure Console panel is still alive
    if (auto console = console_panel_.lock()) {
        // Format the message using spdlog's memory buffer
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        // Convert to string (formatted buffer is null-terminated)
        std::string log_text = fmt::to_string(formatted);
        
        // Remove trailing newline if present for cleaner display
        if (!log_text.empty() && log_text.back() == '\n') {
            log_text.pop_back();
        }
        
        // Route to appropriate Console method based on log level
        switch (msg.level) {
            case spdlog::level::trace:
            case spdlog::level::debug:
                console->addLogDebugThreadSafe(log_text);
                break;
                
            case spdlog::level::info:
                console->addLogInfoThreadSafe(log_text);
                break;
                
            case spdlog::level::warn:
                console->addLogWarningThreadSafe(log_text);
                break;
                
            case spdlog::level::err:
            case spdlog::level::critical:
                console->addLogErrorThreadSafe(log_text);
                break;
                
            default:
                // Fallback to info for unknown levels
                console->addLogInfoThreadSafe(log_text);
                break;
        }
    }
    // If console panel is destroyed, silently ignore the log message
    // This prevents crashes when logger outlives the UI components
}

template<typename Mutex>
void console_sink<Mutex>::flush_()
{
    // Console panel doesn't need explicit flushing as it displays immediately
    // This method is called by spdlog during shutdown or manual flush operations
}