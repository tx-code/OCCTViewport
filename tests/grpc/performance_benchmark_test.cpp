#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <iomanip>

#include "server/geometry_service_impl.h"
#include "geometry_service.pb.h"

// Performance benchmark tests
class PerformanceBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<GeometryServiceImpl>();
        spdlog::set_level(spdlog::level::warn); // Reduce logging for benchmarks
    }
    
    // Helper to measure operation time
    template<typename Func>
    double MeasureTime(Func func, int iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0 / iterations; // Return average time in ms
    }
    
    // Helper to calculate statistics
    struct Stats {
        double min, max, mean, median, stddev;
    };
    
    Stats CalculateStats(const std::vector<double>& times) {
        Stats stats;
        stats.min = *std::min_element(times.begin(), times.end());
        stats.max = *std::max_element(times.begin(), times.end());
        stats.mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        
        // Calculate median
        std::vector<double> sorted_times = times;
        std::sort(sorted_times.begin(), sorted_times.end());
        size_t mid = sorted_times.size() / 2;
        stats.median = sorted_times.size() % 2 == 0 
            ? (sorted_times[mid - 1] + sorted_times[mid]) / 2 
            : sorted_times[mid];
        
        // Calculate standard deviation
        double variance = 0.0;
        for (double t : times) {
            variance += (t - stats.mean) * (t - stats.mean);
        }
        stats.stddev = std::sqrt(variance / times.size());
        
        return stats;
    }
    
    void PrintStats(const std::string& operation, const Stats& stats) {
        std::cout << "\n" << operation << " Performance:" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Min:    " << stats.min << " ms" << std::endl;
        std::cout << "  Max:    " << stats.max << " ms" << std::endl;
        std::cout << "  Mean:   " << stats.mean << " ms" << std::endl;
        std::cout << "  Median: " << stats.median << " ms" << std::endl;
        std::cout << "  StdDev: " << stats.stddev << " ms" << std::endl;
    }
    
    std::unique_ptr<GeometryServiceImpl> service_;
};

// Benchmark primitive creation
TEST_F(PerformanceBenchmarkTest, PrimitiveCreationBenchmark) {
    const int samples = 100;
    std::vector<double> box_times, sphere_times, cone_times, cylinder_times;
    
    for (int i = 0; i < samples; ++i) {
        // Benchmark Box creation
        box_times.push_back(MeasureTime([this, i]() {
            grpc::ServerContext ctx;
            geometry::BoxRequest request;
            request.mutable_position()->set_x(i * 10);
            request.mutable_position()->set_y(0);
            request.mutable_position()->set_z(0);
            request.set_width(5);
            request.set_height(5);
            request.set_depth(5);
            
            geometry::ShapeResponse response;
            service_->CreateBox(&ctx, &request, &response);
        }));
        
        // Benchmark Sphere creation
        sphere_times.push_back(MeasureTime([this, i]() {
            grpc::ServerContext ctx;
            geometry::SphereRequest request;
            request.mutable_center()->set_x(i * 10);
            request.mutable_center()->set_y(10);
            request.mutable_center()->set_z(0);
            request.set_radius(3);
            
            geometry::ShapeResponse response;
            service_->CreateSphere(&ctx, &request, &response);
        }));
        
        // Benchmark Cone creation
        cone_times.push_back(MeasureTime([this, i]() {
            grpc::ServerContext ctx;
            geometry::ConeRequest request;
            request.mutable_position()->set_x(i * 10);
            request.mutable_position()->set_y(20);
            request.mutable_position()->set_z(0);
            request.set_base_radius(3);
            request.set_top_radius(1);
            request.set_height(5);
            
            geometry::ShapeResponse response;
            service_->CreateCone(&ctx, &request, &response);
        }));
        
        // Benchmark Cylinder creation
        cylinder_times.push_back(MeasureTime([this, i]() {
            grpc::ServerContext ctx;
            geometry::CylinderRequest request;
            request.mutable_position()->set_x(i * 10);
            request.mutable_position()->set_y(30);
            request.mutable_position()->set_z(0);
            request.set_radius(2);
            request.set_height(6);
            
            geometry::ShapeResponse response;
            service_->CreateCylinder(&ctx, &request, &response);
        }));
    }
    
    PrintStats("Box Creation", CalculateStats(box_times));
    PrintStats("Sphere Creation", CalculateStats(sphere_times));
    PrintStats("Cone Creation", CalculateStats(cone_times));
    PrintStats("Cylinder Creation", CalculateStats(cylinder_times));
    
    // All operations should complete reasonably fast
    EXPECT_LT(CalculateStats(box_times).mean, 10.0); // Less than 10ms average
    EXPECT_LT(CalculateStats(sphere_times).mean, 15.0);
    EXPECT_LT(CalculateStats(cone_times).mean, 15.0);
    EXPECT_LT(CalculateStats(cylinder_times).mean, 15.0);
}

// Benchmark mesh generation
TEST_F(PerformanceBenchmarkTest, MeshGenerationBenchmark) {
    const int num_shapes = 50;
    std::vector<std::string> shape_ids;
    
    // Create various shapes
    for (int i = 0; i < num_shapes; ++i) {
        grpc::ServerContext ctx;
        geometry::BoxRequest request;
        request.mutable_position()->set_x(i * 10);
        request.mutable_position()->set_y(0);
        request.mutable_position()->set_z(0);
        request.set_width(5 + (i % 3));
        request.set_height(5 + (i % 4));
        request.set_depth(5 + (i % 5));
        
        geometry::ShapeResponse response;
        service_->CreateBox(&ctx, &request, &response);
        shape_ids.push_back(response.shape_id());
    }
    
    std::vector<double> mesh_times;
    
    // Benchmark individual mesh generation
    for (const auto& shape_id : shape_ids) {
        mesh_times.push_back(MeasureTime([this, &shape_id]() {
            grpc::ServerContext ctx;
            geometry::ShapeRequest request;
            request.set_shape_id(shape_id);
            
            geometry::MeshData response;
            service_->GetMeshData(&ctx, &request, &response);
        }));
    }
    
    PrintStats("Mesh Generation", CalculateStats(mesh_times));
    
    // Benchmark GetAllMeshes streaming  
    // Note: Cannot test streaming directly as ServerWriter is final
    // We'll test individual mesh retrieval instead
    double total_stream_time = 0;
    for (const auto& id : shape_ids) {
        double mesh_time = MeasureTime([this, &id]() {
            grpc::ServerContext ctx;
            geometry::ShapeRequest request;
            request.set_shape_id(id);
            geometry::MeshData response;
            service_->GetMeshData(&ctx, &request, &response);
        });
        total_stream_time += mesh_time;
    }
    
    std::cout << "\nTotal mesh retrieval time: " << total_stream_time << " ms for " 
              << num_shapes << " shapes" << std::endl;
    
    EXPECT_LT(CalculateStats(mesh_times).mean, 20.0); // Individual mesh < 20ms
    EXPECT_LT(total_stream_time, num_shapes * 10.0); // Should be efficient
}

// Benchmark batch operations
TEST_F(PerformanceBenchmarkTest, BatchOperationsBenchmark) {
    const int batch_size = 100;
    
    // Benchmark batch creation
    double creation_time = MeasureTime([this, batch_size]() {
        for (int i = 0; i < batch_size; ++i) {
            grpc::ServerContext ctx;
            geometry::BoxRequest request;
            request.mutable_position()->set_x(i * 5);
            request.mutable_position()->set_y(0);
            request.mutable_position()->set_z(0);
            request.set_width(2);
            request.set_height(2);
            request.set_depth(2);
            
            geometry::ShapeResponse response;
            service_->CreateBox(&ctx, &request, &response);
        }
    });
    
    std::cout << "\nBatch Creation (" << batch_size << " shapes): " 
              << creation_time << " ms" << std::endl;
    std::cout << "  Average per shape: " << creation_time / batch_size << " ms" << std::endl;
    
    // Benchmark ClearAll
    double clear_time = MeasureTime([this]() {
        grpc::ServerContext ctx;
        geometry::EmptyRequest request;
        geometry::StatusResponse response;
        service_->ClearAll(&ctx, &request, &response);
    });
    
    std::cout << "\nClearAll (" << batch_size << " shapes): " 
              << clear_time << " ms" << std::endl;
    
    EXPECT_LT(creation_time / batch_size, 5.0); // Less than 5ms per shape
    EXPECT_LT(clear_time, 100.0); // Clear should be fast
}

// Benchmark system info calls (should be very fast)
TEST_F(PerformanceBenchmarkTest, SystemInfoBenchmark) {
    const int iterations = 1000;
    std::vector<double> times;
    
    for (int i = 0; i < iterations; ++i) {
        times.push_back(MeasureTime([this]() {
            grpc::ServerContext ctx;
            geometry::EmptyRequest request;
            geometry::SystemInfoResponse response;
            service_->GetSystemInfo(&ctx, &request, &response);
        }));
    }
    
    auto stats = CalculateStats(times);
    PrintStats("GetSystemInfo", stats);
    
    // System info should be extremely fast
    EXPECT_LT(stats.mean, 0.5); // Less than 0.5ms average
    EXPECT_LT(stats.max, 2.0);   // Max less than 2ms
}

// Benchmark memory usage pattern
TEST_F(PerformanceBenchmarkTest, MemoryUsagePattern) {
    const int cycles = 10;
    const int shapes_per_cycle = 50;
    
    std::vector<double> creation_times;
    std::vector<double> clear_times;
    
    for (int cycle = 0; cycle < cycles; ++cycle) {
        // Create shapes
        double create_time = MeasureTime([this, shapes_per_cycle]() {
            for (int i = 0; i < shapes_per_cycle; ++i) {
                grpc::ServerContext ctx;
                geometry::BoxRequest request;
                request.mutable_position()->set_x(i * 5);
                request.mutable_position()->set_y(0);
                request.mutable_position()->set_z(0);
                request.set_width(3);
                request.set_height(3);
                request.set_depth(3);
                
                geometry::ShapeResponse response;
                service_->CreateBox(&ctx, &request, &response);
            }
        });
        creation_times.push_back(create_time);
        
        // Clear shapes
        double clear_time = MeasureTime([this]() {
            grpc::ServerContext ctx;
            geometry::EmptyRequest request;
            geometry::StatusResponse response;
            service_->ClearAll(&ctx, &request, &response);
        });
        clear_times.push_back(clear_time);
    }
    
    // Check for memory leaks - times should remain consistent
    auto create_stats = CalculateStats(creation_times);
    auto clear_stats = CalculateStats(clear_times);
    
    std::cout << "\nMemory Pattern Test (" << cycles << " cycles):" << std::endl;
    PrintStats("Creation per cycle", create_stats);
    PrintStats("Clear per cycle", clear_stats);
    
    // Performance should not degrade over cycles (indicates no memory leak)
    double degradation = (creation_times.back() - creation_times.front()) / creation_times.front();
    std::cout << "\nPerformance degradation: " << degradation * 100 << "%" << std::endl;
    
    EXPECT_LT(std::abs(degradation), 0.2); // Less than 20% degradation
}