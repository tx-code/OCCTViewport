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
 * @brief ImGui panel for visualizing gRPC performance metrics
 * 
 * Features:
 * - Real-time performance graphs
 * - Operation timing histograms  
 * - Network throughput visualization
 * - Success rate monitoring
 * - Benchmark controls and results
 * - Visual indicators for gRPC activity
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
    // UI rendering methods
    void renderOverviewSection();
    void renderRealtimeGraphs();
    void renderOperationBreakdown();
    void renderBenchmarkSection();
    void renderNetworkDiagnostics();
    void renderActivityIndicator();
    
    // Data collection and processing
    void updateRealTimeData();
    void collectPerformanceSnapshot();
    void runBenchmarkTest(const std::string& operation_type, int iterations);
    
    // Utility methods for visualization
    void plotTimeSeries(const char* label, const std::deque<float>& data, 
                       const std::deque<double>& timestamps, const char* format = "%.1f ms");
    void plotHistogram(const char* label, const std::vector<double>& data, 
                      int bins = 50, const char* format = "%.1f ms");
    void renderMetricCard(const char* title, const char* value, const char* unit = "",
                         ImU32 color = IM_COL32(100, 200, 100, 255));
    
    // Animation and visual effects
    void updateActivityAnimation();
    void renderLatencyHeatmap();
    void renderProgressIndicator(float progress, const char* label);
    
    // Optimized graph rendering methods
    void prepareGraphData();
    
#ifdef GRPC_PERF_PANEL_HAS_IMPLOT
    void renderResponseTimeGraph();
    void renderThroughputGraph();
    void renderSuccessRateGraph();
#endif
    
    struct RealTimeData {
        std::deque<float> response_times;
        std::deque<float> throughput_values;
        std::deque<float> success_rates;
        std::deque<double> timestamps;
        
        static constexpr size_t MAX_SAMPLES = 100;
        
        void addSample(float response_time, float throughput, float success_rate) {
            auto now = std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            
            response_times.push_back(response_time);
            throughput_values.push_back(throughput);
            success_rates.push_back(success_rate);
            timestamps.push_back(now);
            
            // Keep only recent samples
            if (response_times.size() > MAX_SAMPLES) {
                response_times.pop_front();
                throughput_values.pop_front();
                success_rates.pop_front();
                timestamps.pop_front();
            }
        }
    };
    
    struct BenchmarkState {
        bool running = false;
        std::string current_operation;
        int current_iteration = 0;
        int total_iterations = 0;
        float progress = 0.0f;
        std::vector<double> results;
        std::string status_text;
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    };
    
    struct UISettings {
        bool auto_refresh = true;
        float refresh_interval = 1.0f; // seconds
        bool show_detailed_stats = true;
        bool show_activity_indicator = true;
        bool show_network_diagnostics = false;
        int graph_history_size = 60; // samples
        
        // Color scheme
        ImU32 success_color = IM_COL32(100, 200, 100, 255);
        ImU32 warning_color = IM_COL32(255, 200, 100, 255);
        ImU32 error_color = IM_COL32(200, 100, 100, 255);
        ImU32 activity_color = IM_COL32(100, 150, 255, 255);
    };
    
    // Member variables
    std::shared_ptr<GeometryClient> geometry_client_;
    bool is_visible_ = true;
    
    RealTimeData realtime_data_;
    BenchmarkState benchmark_state_;
    UISettings ui_settings_;
    
    // Update timing
    float last_update_time_ = 0.0f;
    float last_snapshot_time_ = 0.0f;
    
    // Animation state
    float activity_animation_phase_ = 0.0f;
    bool connection_activity_ = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_activity_time_;
    
    // Cached performance data
    struct PerformanceSnapshot {
        size_t total_operations = 0;
        double avg_response_time = 0.0;
        double success_rate = 0.0;
        double throughput_mbps = 0.0;
        double uptime_seconds = 0.0;
        std::vector<std::pair<std::string, double>> top_slow_ops;
        std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
    } last_snapshot_;
    
    // Cached vectors for graph performance optimization
    std::vector<float> cached_times_;
    std::vector<float> cached_response_times_;
    std::vector<float> cached_throughput_;
    std::vector<float> cached_success_rates_;
};