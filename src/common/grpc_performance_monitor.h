#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <numeric>

/**
 * @brief Modern C++ performance monitoring for gRPC operations
 * 
 * Features:
 * - RAII-based timing with scope guards
 * - Thread-safe statistics collection
 * - Real-time performance metrics calculation
 * - Network latency and throughput measurement
 * - Memory usage tracking for large mesh data
 */
class GrpcPerformanceMonitor {
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::duration<double, std::milli>;
    
    struct OperationStats {
        std::string operation_name;
        size_t call_count = 0;
        double total_time_ms = 0.0;
        double min_time_ms = std::numeric_limits<double>::max();
        double max_time_ms = 0.0;
        double avg_time_ms = 0.0;
        size_t total_bytes_sent = 0;
        size_t total_bytes_received = 0;
        size_t success_count = 0;
        size_t error_count = 0;
        std::vector<double> recent_times; // Keep last 100 measurements for detailed analysis
        
        void updateStats(double time_ms, size_t bytes_sent = 0, size_t bytes_received = 0, bool success = true) {
            ++call_count;
            total_time_ms += time_ms;
            min_time_ms = std::min(min_time_ms, time_ms);
            max_time_ms = std::max(max_time_ms, time_ms);
            avg_time_ms = total_time_ms / call_count;
            total_bytes_sent += bytes_sent;
            total_bytes_received += bytes_received;
            
            if (success) {
                ++success_count;
            } else {
                ++error_count;
            }
            
            // Keep only recent measurements (sliding window)
            recent_times.push_back(time_ms);
            if (recent_times.size() > 100) {
                recent_times.erase(recent_times.begin());
            }
        }
        
        double getSuccessRate() const {
            return call_count > 0 ? (static_cast<double>(success_count) / call_count) * 100.0 : 0.0;
        }
        
        double getThroughputMBps() const {
            return total_time_ms > 0 ? 
                ((total_bytes_sent + total_bytes_received) / (1024.0 * 1024.0)) / (total_time_ms / 1000.0) : 0.0;
        }
        
        double getStandardDeviation() const {
            if (recent_times.size() < 2) return 0.0;
            
            double mean = std::accumulate(recent_times.begin(), recent_times.end(), 0.0) / recent_times.size();
            double sq_sum = std::inner_product(recent_times.begin(), recent_times.end(), 
                                             recent_times.begin(), 0.0,
                                             [](double const& x, double const& y) { return x + y; },
                                             [mean](double const& x, double const& y) { return (x - mean) * (y - mean); });
            return std::sqrt(sq_sum / recent_times.size());
        }
    };
    
    /**
     * @brief RAII scope guard for automatic timing
     */
    class ScopedTimer {
    public:
        ScopedTimer(GrpcPerformanceMonitor& monitor, const std::string& operation_name, 
                   size_t bytes_sent = 0, size_t bytes_received = 0)
            : monitor_(monitor)
            , operation_name_(operation_name)
            , bytes_sent_(bytes_sent)
            , bytes_received_(bytes_received)
            , start_time_(std::chrono::high_resolution_clock::now())
            , success_(true) {}
        
        ~ScopedTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<Duration>(end_time - start_time_);
            monitor_.recordOperation(operation_name_, duration.count(), bytes_sent_, bytes_received_, success_);
        }
        
        void setSuccess(bool success) { success_ = success; }
        void setBytesReceived(size_t bytes) { bytes_received_ = bytes; }
        void setBytesSent(size_t bytes) { bytes_sent_ = bytes; }
        
        // Disable copy and move to prevent double recording
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;
        
    private:
        GrpcPerformanceMonitor& monitor_;
        std::string operation_name_;
        size_t bytes_sent_;
        size_t bytes_received_;
        TimePoint start_time_;
        bool success_;
    };
    
    static GrpcPerformanceMonitor& getInstance() {
        static GrpcPerformanceMonitor instance;
        return instance;
    }
    
    // Create scoped timer for automatic measurement
    std::unique_ptr<ScopedTimer> createTimer(const std::string& operation_name, 
                                           size_t bytes_sent = 0, size_t bytes_received = 0) {
        return std::make_unique<ScopedTimer>(*this, operation_name, bytes_sent, bytes_received);
    }
    
    // Manual recording for cases where RAII is not suitable
    void recordOperation(const std::string& operation_name, double duration_ms, 
                        size_t bytes_sent = 0, size_t bytes_received = 0, bool success = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_[operation_name].updateStats(duration_ms, bytes_sent, bytes_received, success);
        total_operations_.fetch_add(1);
    }
    
    // Get statistics for specific operation
    OperationStats getOperationStats(const std::string& operation_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stats_.find(operation_name);
        return (it != stats_.end()) ? it->second : OperationStats{};
    }
    
    // Get all operations statistics
    std::unordered_map<std::string, OperationStats> getAllStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    // Reset all statistics
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.clear();
        total_operations_.store(0);
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    // Get overall metrics
    size_t getTotalOperations() const { return total_operations_.load(); }
    
    double getUptimeSeconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time_);
        return duration.count();
    }
    
    double getOverallSuccessRate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total_success = 0, total_calls = 0;
        for (const auto& [name, stats] : stats_) {
            total_success += stats.success_count;
            total_calls += stats.call_count;
        }
        return total_calls > 0 ? (static_cast<double>(total_success) / total_calls) * 100.0 : 0.0;
    }
    
    // Get the most expensive operations
    std::vector<std::pair<std::string, double>> getTopSlowOperations(size_t top_n = 5) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::string, double>> operations;
        
        for (const auto& [name, stats] : stats_) {
            operations.emplace_back(name, stats.avg_time_ms);
        }
        
        std::partial_sort(operations.begin(), 
                         operations.begin() + std::min(top_n, operations.size()), 
                         operations.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        
        operations.resize(std::min(top_n, operations.size()));
        return operations;
    }
    
private:
    GrpcPerformanceMonitor() : start_time_(std::chrono::high_resolution_clock::now()) {}
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, OperationStats> stats_;
    std::atomic<size_t> total_operations_{0};
    TimePoint start_time_;
};

// Convenient macros for performance monitoring
#define GRPC_PERF_TIMER(operation_name) \
    auto _perf_timer = GrpcPerformanceMonitor::getInstance().createTimer(operation_name)

#define GRPC_PERF_TIMER_WITH_DATA(operation_name, bytes_sent, bytes_received) \
    auto _perf_timer = GrpcPerformanceMonitor::getInstance().createTimer(operation_name, bytes_sent, bytes_received)

#define GRPC_PERF_SET_SUCCESS(success) \
    if (_perf_timer) _perf_timer->setSuccess(success)

#define GRPC_PERF_SET_BYTES_RECEIVED(bytes) \
    if (_perf_timer) _perf_timer->setBytesReceived(bytes)