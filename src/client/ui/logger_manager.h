#pragma once

#include <spdlog/spdlog.h>
#include <memory>

// Forward declarations
class ConsolePanel;
template<typename Mutex> class console_sink;

/**
 * @brief Manages spdlog configuration for Console panel integration
 * 
 * This class provides a centralized way to configure spdlog to redirect
 * logs to the ImGui Console panel while maintaining existing logging
 * functionality.
 * 
 * Features:
 * - Seamless integration with existing spdlog calls
 * - Multiple sink support (console + file if needed)
 * - Thread-safe logging
 * - Automatic cleanup on destruction
 */
class LoggerManager {
public:
    /**
     * @brief Initialize spdlog with Console panel integration
     * @param console_panel Shared pointer to the Console panel
     */
    static void initializeWithConsole(std::shared_ptr<ConsolePanel> console_panel);
    
    /**
     * @brief Reset spdlog to default configuration
     * 
     * This method should be called during application shutdown
     * to ensure proper cleanup of custom sinks.
     */
    static void shutdown();
    
    /**
     * @brief Check if logger is currently configured with Console sink
     * @return True if Console sink is active
     */
    static bool isConsoleSinkActive();

private:
    // Static members to track initialization state
    static bool is_initialized_;
    static std::weak_ptr<ConsolePanel> console_panel_;
};