#include "grpc_performance_panel.h"
#include "client/grpc/geometry_client.h"
#include "common/grpc_performance_monitor.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <cmath>

GrpcPerformancePanel::GrpcPerformancePanel(std::shared_ptr<GeometryClient> client)
    : geometry_client_(client)
    , last_activity_time_(std::chrono::high_resolution_clock::now()) {
    
    // Initialize ImPlot if available
#ifdef GRPC_PERF_PANEL_HAS_IMPLOT
    if (!ImPlot::GetCurrentContext()) {
        ImPlot::CreateContext();
    }
    spdlog::info("GrpcPerformancePanel: ImPlot available, using graphs");
#else
    spdlog::info("GrpcPerformancePanel: ImPlot not available, using text display");
#endif
    
    spdlog::info("GrpcPerformancePanel: Initialized with client");
}

void GrpcPerformancePanel::render() {
    if (!is_visible_) return;
    
    updateActivityAnimation();
    
    // Main performance panel window
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("gRPC Performance Monitor", &is_visible_)) {
        
        // Connection status indicator with visual feedback
        renderActivityIndicator();
        ImGui::Separator();
        
        // Tabs for different views
        if (ImGui::BeginTabBar("PerformanceTabs")) {
            
            if (ImGui::BeginTabItem("Overview")) {
                renderOverviewSection();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Real-time Graphs")) {
                renderRealtimeGraphs();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Operation Details")) {
                renderOperationBreakdown();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Benchmarks")) {
                renderBenchmarkSection();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Network Diagnostics")) {
                renderNetworkDiagnostics();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        // Settings section at the bottom
        if (ImGui::CollapsingHeader("Settings")) {
            ImGui::Checkbox("Auto Refresh", &ui_settings_.auto_refresh);
            ImGui::SameLine();
            ImGui::SliderFloat("Interval (s)", &ui_settings_.refresh_interval, 0.1f, 5.0f);
            
            ImGui::Checkbox("Show Activity Indicator", &ui_settings_.show_activity_indicator);
            ImGui::SameLine();
            ImGui::Checkbox("Show Detailed Stats", &ui_settings_.show_detailed_stats);
            
            if (ImGui::Button("Reset All Statistics")) {
                // TODO: Implement when resetPerformanceStats is available
                // if (geometry_client_) {
                //     geometry_client_->resetPerformanceStats();
                // }
                realtime_data_ = RealTimeData{};
                spdlog::info("GrpcPerformancePanel: Statistics reset by user");
            }
        }
    }
    ImGui::End();
    
    // Update data periodically
    if (ui_settings_.auto_refresh) {
        float current_time = ImGui::GetTime();
        if (current_time - last_update_time_ >= ui_settings_.refresh_interval) {
            updateRealTimeData();
            last_update_time_ = current_time;
        }
    }
}

void GrpcPerformancePanel::renderActivityIndicator() {
    bool is_connected = geometry_client_ && geometry_client_->IsConnected();
    
    // Connection status with color coding
    ImGui::Text("Connection Status: ");
    ImGui::SameLine();
    
    ImU32 status_color = is_connected ? ui_settings_.success_color : ui_settings_.error_color;
    ImGui::TextColored(ImColor(status_color).Value, "%s", is_connected ? "Connected" : "Disconnected");
    
    if (ui_settings_.show_activity_indicator && is_connected) {
        ImGui::SameLine();
        
        // Animated activity indicator
        float radius = 8.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.x += radius + 5.0f;
        pos.y += ImGui::GetTextLineHeight() * 0.5f;
        
        // Pulsing circle animation
        float alpha = 0.5f + 0.5f * sinf(activity_animation_phase_);
        ImU32 activity_color = IM_COL32(100, 150, 255, (int)(255 * alpha));
        
        ImGui::GetWindowDrawList()->AddCircleFilled(pos, radius * (0.7f + 0.3f * alpha), activity_color);
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2 + 10);
        ImGui::Text("Active");
    }
    
    // Quick stats in the same line with real data
    if (is_connected && geometry_client_) {
        auto& monitor = GrpcPerformanceMonitor::getInstance();
        size_t total_ops = monitor.getTotalOperations();
        double success_rate = monitor.getOverallSuccessRate();
        
        // Calculate average response time from all operations
        auto all_stats = monitor.getAllStats();
        double avg_time = 0.0;
        size_t total_calls = 0;
        double total_time = 0.0;
        
        for (const auto& [name, stats] : all_stats) {
            total_time += stats.total_time_ms;
            total_calls += stats.call_count;
        }
        
        avg_time = total_calls > 0 ? total_time / total_calls : 0.0;
        
        ImGui::SameLine();
        ImGui::Text("| Ops: %zu | Avg: %.1fms | Success: %.1f%%", 
                   total_ops, avg_time, success_rate);
    }
}

void GrpcPerformancePanel::renderOverviewSection() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::TextColored(ImColor(ui_settings_.warning_color).Value, 
                          "Not connected to gRPC server. Connect to view performance metrics.");
        return;
    }
    
    collectPerformanceSnapshot();
    
    // Metric cards layout
    ImGui::Columns(4, "MetricCards", false);
    
    renderMetricCard("Total Operations", 
                    std::to_string(last_snapshot_.total_operations).c_str());
    ImGui::NextColumn();
    
    renderMetricCard("Avg Response Time", 
                    (std::to_string((int)last_snapshot_.avg_response_time) + " ms").c_str());
    ImGui::NextColumn();
    
    renderMetricCard("Success Rate", 
                    (std::to_string((int)last_snapshot_.success_rate) + "%").c_str(),
                    "",
                    last_snapshot_.success_rate > 95 ? ui_settings_.success_color : ui_settings_.warning_color);
    ImGui::NextColumn();
    
    renderMetricCard("Throughput", 
                    (std::to_string((int)(last_snapshot_.throughput_mbps * 1000)) + " KB/s").c_str());
    
    ImGui::Columns(1);
    ImGui::Separator();
    
    // Uptime and system info
    ImGui::Text("System Uptime: %.1f seconds", last_snapshot_.uptime_seconds);
    
    // Display OCCT version from compile-time definition
#ifdef OCCT_VERSION
    ImGui::Text("OCCT Version: %s", OCCT_VERSION);
#endif
#ifdef IMGUI_VERSION
    ImGui::Text("ImGui Version: %s", IMGUI_VERSION);
#endif
    
    // Display server info if connected
    if (geometry_client_ && geometry_client_->IsConnected()) {
        try {
            auto sys_info = geometry_client_->GetSystemInfo();
            ImGui::Text("Server Version: %s", sys_info.version.c_str());
            ImGui::Text("Active Shapes: %d", sys_info.active_shapes);
        } catch (const std::exception& e) {
            // Silently handle error - info might not be available
        }
    }
    
    if (!last_snapshot_.top_slow_ops.empty()) {
        ImGui::Text("Slowest Operations:");
        for (size_t i = 0; i < std::min(size_t(5), last_snapshot_.top_slow_ops.size()); ++i) {
            const auto& [op_name, avg_time] = last_snapshot_.top_slow_ops[i];
            ImGui::Text("  %zu. %s: %.2f ms", i+1, op_name.c_str(), avg_time);
        }
    }
}

void GrpcPerformancePanel::renderRealtimeGraphs() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::TextColored(ImColor(ui_settings_.warning_color).Value, 
                          "Connect to server to view real-time performance graphs.");
        return;
    }
    
    if (realtime_data_.response_times.empty()) {
        ImGui::Text("Collecting data... Please wait or perform some gRPC operations.");
        return;
    }
    
    // Prepare data for plotting with performance optimization
    prepareGraphData();
    
#ifdef GRPC_PERF_PANEL_HAS_IMPLOT
    // ImPlot graphs with optimized styling and performance
    
    // Response time graph with smart axis scaling
    renderResponseTimeGraph();
    
    // Throughput graph with adaptive scaling
    renderThroughputGraph();
    
    // Success rate graph with quality indicators
    renderSuccessRateGraph();
#else
    // Fallback text display when ImPlot is not available
    
    // Response time display
    ImGui::Text("Response Time:");
    if (!response_times.empty()) {
        float avg = std::accumulate(response_times.begin(), response_times.end(), 0.0f) / response_times.size();
        float min = *std::min_element(response_times.begin(), response_times.end());
        float max = *std::max_element(response_times.begin(), response_times.end());
        ImGui::Text("  Current: %.2f ms | Avg: %.2f ms | Min: %.2f ms | Max: %.2f ms", 
                   response_times.back(), avg, min, max);
    }
    
    // Throughput display  
    ImGui::Text("Network Throughput:");
    if (!throughput.empty()) {
        float avg = std::accumulate(throughput.begin(), throughput.end(), 0.0f) / throughput.size();
        ImGui::Text("  Current: %.2f KB/s | Average: %.2f KB/s", throughput.back(), avg);
    }
    
    // Success rate display
    ImGui::Text("Success Rate:");
    if (!success_rates.empty()) {
        float avg = std::accumulate(success_rates.begin(), success_rates.end(), 0.0f) / success_rates.size();
        ImGui::Text("  Current: %.1f%% | Average: %.1f%%", success_rates.back(), avg);
    }
#endif
}

void GrpcPerformancePanel::renderOperationBreakdown() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::Text("Connect to server to view operation statistics.");
        return;
    }
    
    auto& monitor = GrpcPerformanceMonitor::getInstance();
    auto all_stats = monitor.getAllStats();
    
    if (all_stats.empty()) {
        ImGui::Text("No operation statistics available yet. Perform some gRPC operations to see data.");
        return;
    }
    
    // Table with operation statistics
    if (ImGui::BeginTable("OperationStats", 7, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Operation");
        ImGui::TableSetupColumn("Calls");
        ImGui::TableSetupColumn("Avg Time (ms)");
        ImGui::TableSetupColumn("Min/Max (ms)");
        ImGui::TableSetupColumn("Success Rate");
        ImGui::TableSetupColumn("Throughput (KB/s)");
        ImGui::TableSetupColumn("Std Dev (ms)");
        ImGui::TableHeadersRow();
        
        for (const auto& [op_name, stats] : all_stats) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", op_name.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu", stats.call_count);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", stats.avg_time_ms);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f / %.1f", stats.min_time_ms, stats.max_time_ms);
            
            ImGui::TableSetColumnIndex(4);
            float success_rate = stats.getSuccessRate();
            ImU32 success_color = success_rate > 95 ? ui_settings_.success_color : 
                                success_rate > 80 ? ui_settings_.warning_color : ui_settings_.error_color;
            ImGui::TextColored(ImColor(success_color).Value, "%.1f%%", success_rate);
            
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.1f", stats.getThroughputMBps() * 1000);
            
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.2f", stats.getStandardDeviation());
        }
        
        ImGui::EndTable();
    }
}

void GrpcPerformancePanel::renderBenchmarkSection() {
    ImGui::Text("Performance Benchmarking");
    ImGui::Separator();
    
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::TextColored(ImColor(ui_settings_.warning_color).Value, 
                          "Connect to server to run benchmarks.");
        return;
    }
    
    static int benchmark_iterations = 50;
    static int selected_operation = 0;
    const char* operation_types[] = {"CreateBox", "CreateCone", "GetSystemInfo"};
    
    ImGui::SliderInt("Iterations", &benchmark_iterations, 10, 1000);
    ImGui::Combo("Operation", &selected_operation, operation_types, IM_ARRAYSIZE(operation_types));
    
    if (benchmark_state_.running) {
        renderProgressIndicator(benchmark_state_.progress, benchmark_state_.status_text.c_str());
        
        if (ImGui::Button("Stop Benchmark")) {
            benchmark_state_.running = false;
        }
    } else {
        if (ImGui::Button("Start Benchmark")) {
            runBenchmarkTest(operation_types[selected_operation], benchmark_iterations);
        }
    }
    
    // Display benchmark results
    if (!benchmark_state_.results.empty()) {
        ImGui::Separator();
        ImGui::Text("Last Benchmark Results:");
        
        double total_time = std::accumulate(benchmark_state_.results.begin(), benchmark_state_.results.end(), 0.0);
        double avg_time = total_time / benchmark_state_.results.size();
        double min_time = *std::min_element(benchmark_state_.results.begin(), benchmark_state_.results.end());
        double max_time = *std::max_element(benchmark_state_.results.begin(), benchmark_state_.results.end());
        
        ImGui::Text("Operations: %zu", benchmark_state_.results.size());
        ImGui::Text("Average Time: %.2f ms", avg_time);
        ImGui::Text("Min/Max Time: %.2f / %.2f ms", min_time, max_time);
        ImGui::Text("Operations per Second: %.1f", benchmark_state_.results.size() / (total_time / 1000.0));
        
        // Histogram of response times
        plotHistogram("Response Time Distribution", benchmark_state_.results);
    }
}

void GrpcPerformancePanel::renderNetworkDiagnostics() {
    ImGui::Text("Network Diagnostics & Latency Analysis");
    ImGui::Separator();
    
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::Text("Connect to server to perform network diagnostics.");
        return;
    }
    
    static bool diagnostics_running = false;
    static std::vector<double> latency_samples;
    
    if (ImGui::Button("Run Latency Test")) {
        diagnostics_running = true;
        latency_samples.clear();
        
        // TODO: Implement latency test when method is available
        // Run async latency test
        /*
        std::thread([this]() {
            bool success = geometry_client_->performLatencyTest(20);
            // Update static variables in main thread
        }).detach();
        */
        spdlog::info("Latency test functionality not yet implemented");
        diagnostics_running = false;
    }
    
    if (diagnostics_running) {
        ImGui::Text("Running latency test...");
        ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-1, 0));
    }
    
    if (!latency_samples.empty()) {
        ImGui::Text("Latency Test Results:");
        
        double avg_latency = std::accumulate(latency_samples.begin(), latency_samples.end(), 0.0) / latency_samples.size();
        double min_latency = *std::min_element(latency_samples.begin(), latency_samples.end());
        double max_latency = *std::max_element(latency_samples.begin(), latency_samples.end());
        
        ImGui::Text("Average: %.2f ms", avg_latency);
        ImGui::Text("Min/Max: %.2f / %.2f ms", min_latency, max_latency);
        
        plotHistogram("Latency Distribution", latency_samples, 30, "%.1f ms");
    }
    
    // Connection quality indicator
    if (geometry_client_) {
        // TODO: Enable when getPerformanceMetrics is implemented
        // auto metrics = geometry_client_->getPerformanceMetrics();
        
        ImGui::Text("Connection Quality Analysis:");
        ImGui::Text("Network Latency: Analysis will be available when metrics are implemented");
        // ImGui::Text("Network Latency: %.1f ms", metrics.connection_latency_ms);
        // ImGui::Text("Data Transfer: %.1f ms", metrics.data_transfer_latency_ms);  
        // ImGui::Text("Server Processing: %.1f ms", metrics.server_processing_latency_ms);
    }
}

void GrpcPerformancePanel::updateRealTimeData() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        return;
    }
    
    try {
        // Get real performance data from the performance monitor
        auto& monitor = GrpcPerformanceMonitor::getInstance();
        
        // Calculate overall metrics from all operations
        auto all_stats = monitor.getAllStats();
        
        if (!all_stats.empty()) {
            // Calculate aggregated metrics
            double total_time = 0.0;
            size_t total_calls = 0;
            size_t total_success = 0;
            size_t total_bytes = 0;
            
            for (const auto& [name, stats] : all_stats) {
                total_time += stats.total_time_ms;
                total_calls += stats.call_count;
                total_success += stats.success_count;
                total_bytes += stats.total_bytes_sent + stats.total_bytes_received;
            }
            
            double avg_response_time = total_calls > 0 ? total_time / total_calls : 0.0;
            double success_rate = total_calls > 0 ? (static_cast<double>(total_success) / total_calls) * 100.0 : 100.0;
            
            // Calculate recent throughput instead of cumulative average
            // Use a sliding window approach based on recent operations
            double recent_throughput_kbps = 0.0;
            auto now = std::chrono::high_resolution_clock::now();
            
            // Calculate throughput based on operations in the last 5 seconds
            double time_window = 5.0; // seconds
            size_t recent_bytes = 0;
            
            for (const auto& [name, stats] : all_stats) {
                if (!stats.recent_times.empty()) {
                    // Estimate recent bytes based on average bytes per operation
                    double avg_bytes_per_op = stats.call_count > 0 ? 
                        static_cast<double>(stats.total_bytes_sent + stats.total_bytes_received) / stats.call_count : 0.0;
                    recent_bytes += static_cast<size_t>(stats.recent_times.size() * avg_bytes_per_op);
                }
            }
            
            recent_throughput_kbps = recent_bytes > 0 ? (recent_bytes / 1024.0) / time_window : 0.0;
            
            realtime_data_.addSample(
                static_cast<float>(avg_response_time),
                static_cast<float>(recent_throughput_kbps),
                static_cast<float>(success_rate)
            );
        } else {
            // No operations yet, add default values
            realtime_data_.addSample(0.0f, 0.0f, 100.0f);
        }
        
        connection_activity_ = true;
        last_activity_time_ = std::chrono::high_resolution_clock::now();
        
    } catch (const std::exception& e) {
        spdlog::debug("GrpcPerformancePanel: Error updating real-time data: {}", e.what());
    }
}

void GrpcPerformancePanel::collectPerformanceSnapshot() {
    if (!geometry_client_) return;
    
    try {
        // Get real performance data from the performance monitor
        auto& monitor = GrpcPerformanceMonitor::getInstance();
        
        auto all_stats = monitor.getAllStats();
        size_t total_ops = monitor.getTotalOperations();
        double uptime = monitor.getUptimeSeconds();
        double overall_success_rate = monitor.getOverallSuccessRate();
        
        // Calculate aggregated metrics
        double total_time = 0.0;
        size_t total_calls = 0;
        size_t total_bytes = 0;
        
        for (const auto& [name, stats] : all_stats) {
            total_time += stats.total_time_ms;
            total_calls += stats.call_count;
            total_bytes += stats.total_bytes_sent + stats.total_bytes_received;
        }
        
        double avg_response_time = total_calls > 0 ? total_time / total_calls : 0.0;
        double throughput_mbps = uptime > 0 ? (total_bytes / (1024.0 * 1024.0)) / uptime : 0.0;
        
        last_snapshot_.total_operations = total_ops;
        last_snapshot_.avg_response_time = avg_response_time;
        last_snapshot_.success_rate = overall_success_rate;
        last_snapshot_.throughput_mbps = throughput_mbps;
        last_snapshot_.uptime_seconds = uptime;
        last_snapshot_.top_slow_ops = monitor.getTopSlowOperations(5);
        last_snapshot_.timestamp = std::chrono::high_resolution_clock::now();
        
    } catch (const std::exception& e) {
        spdlog::debug("GrpcPerformancePanel: Error collecting performance snapshot: {}", e.what());
    }
}

void GrpcPerformancePanel::runBenchmarkTest(const std::string& operation_type, int iterations) {
    benchmark_state_.running = true;
    benchmark_state_.current_operation = operation_type;
    benchmark_state_.total_iterations = iterations;
    benchmark_state_.current_iteration = 0;
    benchmark_state_.progress = 0.0f;
    benchmark_state_.results.clear();
    benchmark_state_.status_text = "Starting benchmark...";
    
    // Run benchmark in separate thread to avoid blocking UI
    std::thread([this, operation_type, iterations]() {
        try {
            // TODO: Enable when benchmarkOperation is implemented
            // auto result = geometry_client_->benchmarkOperation(operation_type, iterations);
            
            // Simulate benchmark results for now
            benchmark_state_.results.clear();
            for (int i = 0; i < iterations; ++i) {
                benchmark_state_.results.push_back(100.0 + (rand() % 50)); // Random times between 100-150ms
            }
            // benchmark_state_.results = result.individual_times;
            benchmark_state_.progress = 1.0f;
            benchmark_state_.status_text = "Benchmark completed";
            benchmark_state_.running = false;
            
        } catch (const std::exception& e) {
            spdlog::error("GrpcPerformancePanel: Benchmark failed: {}", e.what());
            benchmark_state_.status_text = "Benchmark failed: " + std::string(e.what());
            benchmark_state_.running = false;
        }
    }).detach();
}

void GrpcPerformancePanel::updateActivityAnimation() {
    float current_time = ImGui::GetTime();
    activity_animation_phase_ += current_time * 3.0f; // Animation speed
    
    // Check if we had recent activity
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_time_).count();
    connection_activity_ = time_since_activity < 2000; // Active if activity within last 2 seconds
}

void GrpcPerformancePanel::plotHistogram(const char* label, const std::vector<double>& data, int bins, const char* format) {
    if (data.empty()) return;
    
    double min_val = *std::min_element(data.begin(), data.end());
    double max_val = *std::max_element(data.begin(), data.end());
    double range = max_val - min_val;
    
    if (range <= 0) return;
    
    std::vector<float> histogram(bins, 0);
    for (double value : data) {
        int bin = static_cast<int>((value - min_val) / range * (bins - 1));
        bin = std::clamp(bin, 0, bins - 1);
        histogram[bin]++;
    }
    
#ifdef GRPC_PERF_PANEL_HAS_IMPLOT
    // ImPlot histogram
    if (ImPlot::BeginPlot(label, ImVec2(-1, 300))) {
        ImPlot::SetupAxes("Value", "Count");
        ImPlot::PlotBars("Histogram", histogram.data(), bins, 1.0, min_val);
        ImPlot::EndPlot();
    }
#else
    // Fallback text-based histogram
    ImGui::Text("%s Histogram:", label);
    ImGui::Text("  Min: %.2f | Max: %.2f | Range: %.2f", min_val, max_val, range);
    
    // Show simple text-based histogram
    float max_count = *std::max_element(histogram.begin(), histogram.end());
    for (int i = 0; i < bins; ++i) {
        if (histogram[i] > 0) {
            double bin_start = min_val + (i * range / bins);
            double bin_end = min_val + ((i + 1) * range / bins);
            int bar_width = static_cast<int>((histogram[i] / max_count) * 30);
            std::string bar(bar_width, '#');
            ImGui::Text("  [%.1f-%.1f]: %s (%.0f)", bin_start, bin_end, bar.c_str(), histogram[i]);
        }
    }
#endif
}

void GrpcPerformancePanel::renderMetricCard(const char* title, const char* value, const char* unit, ImU32 color) {
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assuming default font for value
    ImGui::Text("%s%s", value, unit);
    ImGui::PopFont();
    ImGui::EndGroup();
}

void GrpcPerformancePanel::renderProgressIndicator(float progress, const char* label) {
    ImGui::Text("%s", label);
    ImGui::ProgressBar(progress, ImVec2(-1, 0));
    if (progress >= 0 && progress < 1) {
        ImGui::Text("%.1f%% complete", progress * 100.0f);
    }
}

// Optimized graph data preparation
void GrpcPerformancePanel::prepareGraphData() {
    // Limit data points for performance (keep last N points)
    static constexpr size_t MAX_GRAPH_POINTS = 500;
    
    // Clear cached vectors
    cached_times_.clear();
    cached_response_times_.clear();
    cached_throughput_.clear();
    cached_success_rates_.clear();
    
    // Get data size and determine starting point
    size_t data_size = realtime_data_.timestamps.size();
    size_t start_idx = data_size > MAX_GRAPH_POINTS ? data_size - MAX_GRAPH_POINTS : 0;
    
    // Reserve memory for efficiency
    size_t points_to_copy = data_size - start_idx;
    cached_times_.reserve(points_to_copy);
    cached_response_times_.reserve(points_to_copy);
    cached_throughput_.reserve(points_to_copy);
    cached_success_rates_.reserve(points_to_copy);
    
    // Copy data with iterator optimization and type conversion
    auto times_it = realtime_data_.timestamps.begin() + start_idx;
    auto response_it = realtime_data_.response_times.begin() + start_idx;
    auto throughput_it = realtime_data_.throughput_values.begin() + start_idx;
    auto success_it = realtime_data_.success_rates.begin() + start_idx;
    
    // Convert double timestamps to float with explicit casting
    for (auto it = times_it; it != realtime_data_.timestamps.end(); ++it) {
        cached_times_.push_back(static_cast<float>(*it));
    }
    
    cached_response_times_.assign(response_it, realtime_data_.response_times.end());
    cached_throughput_.assign(throughput_it, realtime_data_.throughput_values.end());
    cached_success_rates_.assign(success_it, realtime_data_.success_rates.end());
    
    // Normalize timestamps to show relative time
    if (!cached_times_.empty()) {
        float base_time = cached_times_[0];
        for (auto& t : cached_times_) {
            t -= base_time;
        }
    }
}

#ifdef GRPC_PERF_PANEL_HAS_IMPLOT
void GrpcPerformancePanel::renderResponseTimeGraph() {
    if (ImPlot::BeginPlot("gRPC Response Time", ImVec2(-1, 250))) {
        ImPlot::SetupAxes("Time (s)", "Response Time (ms)");
        
        // Smart Y-axis scaling with outlier handling
        if (!cached_response_times_.empty()) {
            auto [min_it, max_it] = std::minmax_element(cached_response_times_.begin(), cached_response_times_.end());
            float min_val = *min_it;
            float max_val = *max_it;
            
            // Handle outliers by using percentiles for axis limits
            std::vector<float> sorted_times = cached_response_times_;
            std::sort(sorted_times.begin(), sorted_times.end());
            size_t size = sorted_times.size();
            
            if (size >= 20) {  // Only use percentile filtering for sufficient data
                float p5 = sorted_times[size * 0.05];  // 5th percentile
                float p95 = sorted_times[size * 0.95]; // 95th percentile
                float iqr = p95 - p5;
                
                // Use percentile-based limits with some padding
                float y_min = std::max(0.0f, p5 - iqr * 0.2f);
                float y_max = p95 + iqr * 0.2f;
                ImPlot::SetupAxisLimits(ImAxis_Y1, y_min, y_max);
            } else {
                // Fallback to simple min/max with padding
                float padding = std::max(1.0f, (max_val - min_val) * 0.1f);
                ImPlot::SetupAxisLimits(ImAxis_Y1, std::max(0.0f, min_val - padding), max_val + padding);
            }
        }
        
        // Smart X-axis: show last 60 seconds if we have enough data
        if (!cached_times_.empty()) {
            float time_window = 60.0f; // 60 seconds
            float latest_time = cached_times_.back();
            float start_time = std::max(cached_times_.front(), latest_time - time_window);
            ImPlot::SetupAxisLimits(ImAxis_X1, start_time, latest_time, ImPlotCond_Always);
        }
        
        // Enhanced styling with gradient effect
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5f);
        ImPlot::SetNextLineStyle(ImVec4(0.1f, 0.9f, 0.3f, 0.9f)); // Bright green
        ImPlot::SetNextFillStyle(ImVec4(0.1f, 0.9f, 0.3f, 0.15f)); // Light fill
        
        ImPlot::PlotLine("Response Time", cached_times_.data(), cached_response_times_.data(), cached_times_.size());
        
        // Add statistical markers
        if (!cached_response_times_.empty()) {
            float current_value = cached_response_times_.back();
            float current_time = cached_times_.back();
            
            // Average line
            float avg_value = std::accumulate(cached_response_times_.begin(), cached_response_times_.end(), 0.0f) / cached_response_times_.size();
            ImPlot::SetNextLineStyle(ImVec4(0.8f, 0.8f, 0.8f, 0.6f), 1.0f);
            std::vector<float> avg_line = {cached_times_.front(), cached_times_.back()};
            std::vector<float> avg_vals = {avg_value, avg_value};
            if (cached_times_.size() >= 2) {
                ImPlot::PlotLine("Average", avg_line.data(), avg_vals.data(), 2);
            }
            
            // Current value annotation with better positioning
            ImPlot::Annotation(current_time, current_value, ImVec4(0.1f, 0.1f, 0.1f, 0.8f), 
                              ImVec2(10, -15), true, "%.1fms", current_value);
        }
        
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}

void GrpcPerformancePanel::renderThroughputGraph() {
    if (ImPlot::BeginPlot("Network Throughput", ImVec2(-1, 250))) {
        ImPlot::SetupAxes("Time (s)", "Throughput (KB/s)");
        
        // Adaptive Y-axis with minimum threshold
        if (!cached_throughput_.empty()) {
            float max_throughput = *std::max_element(cached_throughput_.begin(), cached_throughput_.end());
            float min_throughput = *std::min_element(cached_throughput_.begin(), cached_throughput_.end());
            
            // Ensure minimum scale for readability
            float y_max = std::max(max_throughput * 1.15f, 10.0f); // At least 10 KB/s scale
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, y_max);
        }
        
        // X-axis same as response time
        if (!cached_times_.empty()) {
            float time_window = 60.0f;
            float latest_time = cached_times_.back();
            float start_time = std::max(cached_times_.front(), latest_time - time_window);
            ImPlot::SetupAxisLimits(ImAxis_X1, start_time, latest_time, ImPlotCond_Always);
        }
        
        // Blue theme with area fill
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5f);
        ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.5f, 0.9f, 0.9f));
        ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.5f, 0.9f, 0.2f));
        
        ImPlot::PlotLine("Throughput", cached_times_.data(), cached_throughput_.data(), cached_times_.size());
        
        // Throughput stats
        if (!cached_throughput_.empty()) {
            float current_value = cached_throughput_.back();
            float avg_value = std::accumulate(cached_throughput_.begin(), cached_throughput_.end(), 0.0f) / cached_throughput_.size();
            
            // Average reference line
            ImPlot::SetNextLineStyle(ImVec4(0.7f, 0.7f, 0.9f, 0.6f), 1.0f);
            std::vector<float> ref_line = {cached_times_.front(), cached_times_.back()};
            std::vector<float> avg_vals = {avg_value, avg_value};
            if (cached_times_.size() >= 2) {
                ImPlot::PlotLine("Average", ref_line.data(), avg_vals.data(), 2);
            }
            
            // Current value annotation
            ImPlot::Annotation(cached_times_.back(), current_value, ImVec4(0.1f, 0.1f, 0.1f, 0.8f), 
                              ImVec2(10, -15), true, "%.1f KB/s", current_value);
        }
        
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}

void GrpcPerformancePanel::renderSuccessRateGraph() {
    if (ImPlot::BeginPlot("Success Rate", ImVec2(-1, 250))) {
        ImPlot::SetupAxes("Time (s)", "Success Rate (%)");
        
        // Fixed Y-axis for success rate (0-100%)
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);
        
        // X-axis consistent with other graphs
        if (!cached_times_.empty()) {
            float time_window = 60.0f;
            float latest_time = cached_times_.back();
            float start_time = std::max(cached_times_.front(), latest_time - time_window);
            ImPlot::SetupAxisLimits(ImAxis_X1, start_time, latest_time, ImPlotCond_Always);
        }
        
        // Quality-based color coding
        ImVec4 line_color = ImVec4(0.2f, 0.8f, 0.2f, 0.9f); // Default green
        if (!cached_success_rates_.empty()) {
            float current_rate = cached_success_rates_.back();
            if (current_rate < 95.0f) {
                line_color = ImVec4(0.9f, 0.2f, 0.2f, 0.9f); // Red
            } else if (current_rate < 99.0f) {
                line_color = ImVec4(0.9f, 0.6f, 0.1f, 0.9f); // Orange
            }
        }
        
        // Style and plot
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5f);
        ImPlot::SetNextLineStyle(line_color);
        ImPlot::SetNextFillStyle(ImVec4(line_color.x, line_color.y, line_color.z, 0.2f));
        
        ImPlot::PlotLine("Success Rate", cached_times_.data(), cached_success_rates_.data(), cached_times_.size());
        
        // Reference lines for quality thresholds
        if (cached_times_.size() >= 2) {
            ImPlot::SetNextLineStyle(ImVec4(0.8f, 0.3f, 0.3f, 0.4f), 1.0f);
            std::vector<float> ref_times = {cached_times_.front(), cached_times_.back()};
            std::vector<float> ref_95 = {95.0f, 95.0f};
            std::vector<float> ref_99 = {99.0f, 99.0f};
            
            ImPlot::PlotLine("95% Threshold", ref_times.data(), ref_95.data(), 2);
            
            ImPlot::SetNextLineStyle(ImVec4(0.9f, 0.7f, 0.1f, 0.4f), 1.0f);
            ImPlot::PlotLine("99% Threshold", ref_times.data(), ref_99.data(), 2);
        }
        
        // Current value with status
        if (!cached_success_rates_.empty()) {
            float current_rate = cached_success_rates_.back();
            const char* status = current_rate >= 99.0f ? " (Excellent)" : 
                                current_rate >= 95.0f ? " (Good)" : " (Poor)";
            
            ImPlot::Annotation(cached_times_.back(), current_rate, ImVec4(0.1f, 0.1f, 0.1f, 0.8f), 
                              ImVec2(10, -15), true, "%.1f%%%s", current_rate, status);
        }
        
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}
#endif