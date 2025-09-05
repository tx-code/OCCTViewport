#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <memory>
#include <mutex>

// Forward declaration
class ConsolePanel;

/**
 * @brief Custom spdlog sink that redirects logs to ConsolePanel
 * 
 * This sink provides thread-safe logging integration with the ImGui Console panel.
 * It maps spdlog levels to appropriate Console panel methods while maintaining
 * performance and thread safety.
 * 
 * Features:
 * - Thread-safe logging through mutex protection
 * - Level-based routing (info/warning/error)
 * - Weak pointer pattern to handle Console lifetime safely
 * - Modern C++20 compatible design
 */
template<typename Mutex>
class console_sink : public spdlog::sinks::base_sink<Mutex>
{
public:
    /**
     * @brief Constructor with weak reference to ConsolePanel
     * @param console_panel Weak pointer to the Console panel instance
     */
    explicit console_sink(std::weak_ptr<ConsolePanel> console_panel);
    
    /**
     * @brief Destructor
     */
    ~console_sink() override = default;
    
    // Disable copy/move for safety
    console_sink(const console_sink&) = delete;
    console_sink& operator=(const console_sink&) = delete;
    console_sink(console_sink&&) = delete;
    console_sink& operator=(console_sink&&) = delete;

protected:
    /**
     * @brief Main sink method - processes and forwards log messages
     * @param msg The formatted log message
     */
    void sink_it_(const spdlog::details::log_msg& msg) override;
    
    /**
     * @brief Flush method (no-op for our use case)
     */
    void flush_() override;

private:
    std::weak_ptr<ConsolePanel> console_panel_; ///< Weak reference to Console panel
};

// Type aliases for different mutex strategies
using console_sink_mt = console_sink<std::mutex>;        // Multi-threaded version
using console_sink_st = console_sink<spdlog::details::null_mutex>; // Single-threaded version

// Include implementation since it's a template
#include "console_sink_impl.h"