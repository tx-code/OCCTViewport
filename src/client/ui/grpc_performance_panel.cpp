#include "grpc_performance_panel.h"
#include "client/grpc/geometry_client.h"
#include "common/grpc_performance_monitor.h"
#include <spdlog/spdlog.h>

GrpcPerformancePanel::GrpcPerformancePanel(std::shared_ptr<GeometryClient> client)
    : geometry_client_(client)
    , last_activity_time_(std::chrono::high_resolution_clock::now()) {
    spdlog::info("GrpcPerformancePanel: Initialized with client");
}

void GrpcPerformancePanel::render() {
    if (!is_visible_) return;
    
    updateActivityAnimation();
    
    // Main performance panel window
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("gRPC Performance Monitor", &is_visible_)) {
        
        // Connection status indicator
        renderActivityIndicator();
        ImGui::Separator();
        
        // Core metrics section
        renderStatusSection();
        
        // Metrics display
        renderMetricsSection();
        
        // Server info
        renderServerInfoSection();
        
        ImGui::Separator();
        
        // Simple settings
        if (ImGui::CollapsingHeader("Settings")) {
            ImGui::Checkbox("Auto Refresh", &ui_settings_.auto_refresh);
            ImGui::SameLine();
            ImGui::SliderFloat("Interval (s)", &ui_settings_.refresh_interval, 0.5f, 5.0f);
            
            ImGui::Checkbox("Show Activity Indicator", &ui_settings_.show_activity_indicator);
        }
    }
    ImGui::End();
    
    // Update data periodically
    if (ui_settings_.auto_refresh) {
        float current_time = ImGui::GetTime();
        if (current_time - last_update_time_ >= ui_settings_.refresh_interval) {
            updateMetrics();
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
        
        // Simple activity indicator
        float radius = 6.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.x += radius + 5.0f;
        pos.y += ImGui::GetTextLineHeight() * 0.5f;
        
        // Pulsing circle animation
        float alpha = 0.6f + 0.4f * sinf(activity_animation_phase_);
        ImU32 activity_color = IM_COL32(100, 200, 100, (int)(255 * alpha));
        
        ImGui::GetWindowDrawList()->AddCircleFilled(pos, radius * (0.8f + 0.2f * alpha), activity_color);
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2 + 10);
        ImGui::Text("Active");
    }
}

void GrpcPerformancePanel::renderStatusSection() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        ImGui::TextColored(ImColor(ui_settings_.warning_color).Value, 
                          "Not connected to gRPC server");
        return;
    }
    
    updateMetrics();
    
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(ImColor(ui_settings_.success_color).Value, "Connected & Active");
}

void GrpcPerformancePanel::renderMetricsSection() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        return;
    }
    
    ImGui::Separator();
    ImGui::Text("Performance Metrics");
    
    // Simple metric cards layout
    ImGui::Columns(2, "MetricCards", false);
    
    renderMetricCard("Total Operations", 
                    std::to_string(current_metrics_.total_operations).c_str());
    ImGui::NextColumn();
    
    renderMetricCard("Avg Response Time", 
                    (std::to_string((int)current_metrics_.avg_response_time) + " ms").c_str());
    
    ImGui::NextColumn();
    
    renderMetricCard("Success Rate", 
                    (std::to_string((int)current_metrics_.success_rate) + "%").c_str(),
                    "",
                    current_metrics_.success_rate > 95 ? ui_settings_.success_color : ui_settings_.warning_color);
    ImGui::NextColumn();
    
    renderMetricCard("Active Shapes", 
                    std::to_string(current_metrics_.active_shapes).c_str());
    
    ImGui::Columns(1);
}

void GrpcPerformancePanel::renderServerInfoSection() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        return;
    }
    
    ImGui::Separator();
    ImGui::Text("Server Information");
    
    if (!current_metrics_.server_version.empty()) {
        ImGui::Text("Server Version: %s", current_metrics_.server_version.c_str());
    }
    
    if (!current_metrics_.occt_version.empty()) {
        ImGui::Text("OCCT Version: %s", current_metrics_.occt_version.c_str());
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::seconds>(now - current_metrics_.last_update).count();
    ImGui::Text("Last updated: %lds ago", time_since_update);
}

void GrpcPerformancePanel::updateMetrics() {
    if (!geometry_client_ || !geometry_client_->IsConnected()) {
        return;
    }
    
    try {
        auto& monitor = GrpcPerformanceMonitor::getInstance();
        auto all_stats = monitor.getAllStats();
        
        // Calculate aggregated metrics
        double total_time = 0.0;
        size_t total_calls = 0;
        size_t total_success = 0;
        
        for (const auto& [name, stats] : all_stats) {
            total_time += stats.total_time_ms;
            total_calls += stats.call_count;
            total_success += stats.success_count;
        }
        
        current_metrics_.total_operations = monitor.getTotalOperations();
        current_metrics_.avg_response_time = total_calls > 0 ? total_time / total_calls : 0.0;
        current_metrics_.success_rate = total_calls > 0 ? (static_cast<double>(total_success) / total_calls) * 100.0 : 100.0;
        
        // Get server info
        try {
            auto sys_info = geometry_client_->GetSystemInfo();
            current_metrics_.active_shapes = sys_info.active_shapes;
            current_metrics_.server_version = sys_info.version;
        } catch (...) {
            // Silently handle error
        }
        
        current_metrics_.last_update = std::chrono::high_resolution_clock::now();
        
    } catch (const std::exception& e) {
        spdlog::debug("GrpcPerformancePanel: Error updating metrics: {}", e.what());
    }
}

void GrpcPerformancePanel::renderMetricCard(const char* title, const char* value, const char* unit, ImU32 color) {
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    
    ImGui::Text("%s%s", value, unit);
    ImGui::EndGroup();
}

void GrpcPerformancePanel::updateActivityAnimation() {
    activity_animation_phase_ += ImGui::GetIO().DeltaTime * 3.0f;
    
    // Check for recent activity
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_time_).count();
    
    // Update activity time when there are recent operations
    if (geometry_client_ && geometry_client_->IsConnected()) {
        auto& monitor = GrpcPerformanceMonitor::getInstance();
        if (monitor.getTotalOperations() > 0) {
            last_activity_time_ = now;
        }
    }
}