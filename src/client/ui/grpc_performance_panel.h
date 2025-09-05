#pragma once

#include <imgui.h>
#ifdef IMPLOT_VERSION
#include <implot.h>
#define GRPC_PERF_PANEL_HAS_IMPLOT
#endif
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <chrono>

// Forward declarations
class GeometryClient;
struct GrpcPerformanceMonitor;

/**
 * @brief Simplified ImGui panel for gRPC connection monitoring
 * 
 * Features:
 * - Connection status with activity indicator
 * - Basic performance metrics (operations, response time, success rate)
 * - Server information display
 * - Simple settings control
 */
class GrpcPerformancePanel {
public:
    explicit GrpcPerformancePanel(std::shared_ptr<GeometryClient> client);
    ~GrpcPerformancePanel() = default;
    
    // Disable copy/move for simplicity
    GrpcPerformancePanel(const GrpcPerformancePanel&) = delete;
    GrpcPerformancePanel& operator=(const GrpcPerformancePanel&) = delete;
    
    void render();
    void setVisible(bool visible) { is_visible_ = visible; }
    bool isVisible() const { return is_visible_; }
    
private:
    // Core UI rendering methods
    void renderStatusSection();
    void renderMetricsSection();
    void renderServerInfoSection();
    void renderActivityIndicator();
    
    // Utility methods
    void renderMetricCard(const char* title, const char* value, const char* unit = "",
                         ImU32 color = IM_COL32(100, 200, 100, 255));
    void updateActivityAnimation();
    void updateMetrics();
    
    struct SimpleMetrics {
        size_t total_operations = 0;
        double avg_response_time = 0.0;
        double success_rate = 0.0;
        size_t active_shapes = 0;
        std::string server_version;
        std::string occt_version;
        std::chrono::time_point<std::chrono::high_resolution_clock> last_update;
    };
    
    struct UISettings {
        bool auto_refresh = true;
        float refresh_interval = 2.0f; // seconds
        bool show_activity_indicator = true;
        
        // Color scheme
        ImU32 success_color = IM_COL32(100, 200, 100, 255);
        ImU32 warning_color = IM_COL32(255, 200, 100, 255);
        ImU32 error_color = IM_COL32(200, 100, 100, 255);
    };
    
    // Member variables
    std::shared_ptr<GeometryClient> geometry_client_;
    bool is_visible_ = true;
    
    SimpleMetrics current_metrics_;
    UISettings ui_settings_;
    
    // Update timing
    float last_update_time_ = 0.0f;
    
    // Animation state
    float activity_animation_phase_ = 0.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_activity_time_;
};