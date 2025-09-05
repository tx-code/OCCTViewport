#include "logger_manager.h"
#include "console_sink.h"
#include "console_panel.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>
#include <memory>

// Static member definitions
bool LoggerManager::is_initialized_ = false;
std::weak_ptr<ConsolePanel> LoggerManager::console_panel_;

void LoggerManager::initializeWithConsole(std::shared_ptr<ConsolePanel> console_panel) {
    if (is_initialized_) {
        spdlog::warn("LoggerManager already initialized, skipping");
        return;
    }
    
    if (!console_panel) {
        spdlog::error("Cannot initialize LoggerManager with null console panel");
        return;
    }
    
    try {
        // Store reference to console panel
        console_panel_ = console_panel;
        
        // Create sink vector for multiple outputs
        std::vector<spdlog::sink_ptr> sinks;
        
        // Add console sink (redirects to ImGui Console panel)
        auto console_sink = std::make_shared<console_sink_mt>(console_panel);
        sinks.push_back(console_sink);
        
        // Optionally add stdout sink for debugging (can be disabled in release)
#ifdef _DEBUG
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(stdout_sink);
#endif
        
        // Create multi-sink logger and set as default
        auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::warn);
        
        // Set as default logger
        spdlog::set_default_logger(logger);
        
        // Configure global pattern for console sink
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        
        is_initialized_ = true;
        
        spdlog::info("Logger initialized with Console panel integration");
        
    } catch (const spdlog::spdlog_ex& ex) {
        // Fallback to default logger if initialization fails
        spdlog::error("Failed to initialize logger with console: {}", ex.what());
    }
}

void LoggerManager::shutdown() {
    if (!is_initialized_) {
        return;
    }
    
    try {
        // Flush all logs before shutdown
        spdlog::get("multi_sink")->flush();
        
        // Reset to default logger to ensure proper cleanup
        spdlog::set_default_logger(spdlog::stdout_color_mt("console"));
        
        // Clear our reference
        console_panel_.reset();
        
        is_initialized_ = false;
        
        spdlog::debug("Logger manager shut down successfully");
        
    } catch (const spdlog::spdlog_ex& ex) {
        // Even if shutdown fails, mark as uninitialized
        is_initialized_ = false;
        spdlog::error("Error during logger shutdown: {}", ex.what());
    }
}

bool LoggerManager::isConsoleSinkActive() {
    return is_initialized_ && !console_panel_.expired();
}