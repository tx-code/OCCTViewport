#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

// gRPC includes
#include <grpcpp/grpcpp.h>
#include "geometry_service.grpc.pb.h"
#include "common/grpc_performance_monitor.h"

// Forward declarations
struct MeshRenderData;

class GeometryClient {
public:
    explicit GeometryClient(const std::string& server_address = "localhost:50051", 
                           const std::string& client_id = "CPP-ImGui");
    ~GeometryClient();

    // Connection management
    bool Connect();
    void Disconnect();
    bool DisconnectFromServer(); // Notify server of disconnection
    bool IsConnected() const;

    // Geometry operations
    std::string CreateBox(double x, double y, double z, 
                         double width, double height, double depth,
                         double r = 0.8, double g = 0.8, double b = 0.8);
    
    std::string CreateCone(double x, double y, double z,
                          double base_radius, double top_radius, double height,
                          double r = 0.7, double g = 0.9, double b = 0.7);
    
    std::string CreateSphere(double x, double y, double z, double radius,
                            double r = 0.9, double g = 0.7, double b = 0.7);
    
    std::string CreateCylinder(double x, double y, double z, double radius, double height,
                              double r = 0.7, double g = 0.7, double b = 0.9);

    // Shape management
    bool DeleteShape(const std::string& shape_id);
    bool SetShapeColor(const std::string& shape_id, double r, double g, double b);
    
    // Scene operations
    bool CreateDemoScene();
    bool ClearAll();
    
    // Mesh data retrieval
    struct MeshData {
        std::string shape_id;
        std::vector<float> vertices;    // x,y,z triplets
        std::vector<float> normals;     // nx,ny,nz triplets
        std::vector<int> indices;       // Triangle indices
        float color[4];                 // RGBA
        bool visible;
        bool selected;
        bool highlighted;
    };
    
    std::vector<MeshData> GetAllMeshes();
    MeshData GetMeshData(const std::string& shape_id);
    
    // System info
    struct SystemInfo {
        std::string version;
        int active_shapes;
        std::string occt_version;
    };
    
    SystemInfo GetSystemInfo();
    
    // Performance monitoring
    struct PerformanceMetrics {
        size_t total_operations = 0;
        double avg_response_time_ms = 0.0;
        double success_rate_percent = 0.0;
        double uptime_seconds = 0.0;
        double network_throughput_mbps = 0.0;
        std::vector<std::pair<std::string, double>> slowest_operations;
        
        // Network latency breakdown
        double connection_latency_ms = 0.0;
        double data_transfer_latency_ms = 0.0;
        double server_processing_latency_ms = 0.0;
    };
    
    PerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceStats();
    
    // Network diagnostics
    bool performLatencyTest(int samples = 10);
    double measureConnectionLatency();
    
    // Benchmarking utilities
    struct BenchmarkResult {
        std::string operation_name;
        size_t iterations;
        double total_time_ms;
        double avg_time_per_op_ms;
        double operations_per_second;
        std::vector<double> individual_times;
    };
    
    BenchmarkResult benchmarkOperation(const std::string& operation_type, size_t iterations = 100);
    
    // Data size estimation for performance analysis
    size_t estimateRequestSize(const std::string& operation_type) const;
    size_t estimateResponseSize(const std::string& operation_type) const;
    

    // Unified model file operations
    struct ModelImportOptions {
        bool auto_detect_format = true;
        std::string force_format = "";
        bool import_colors = true;
        bool import_names = true;
        bool import_materials = true;
        double precision = 0.01;
        bool merge_shapes = false;
        bool validate_shapes = true;
        bool heal_shapes = false;
        double linear_tolerance = 0.01;
        double angular_tolerance = 0.1;
    };

    struct ModelExportOptions {
        std::string format = "STEP";
        bool export_colors = true;
        bool export_names = true;
        bool export_materials = true;
        std::string schema_version = "AP214";
        std::string units = "mm";
        bool export_as_compound = false;
        bool validate_before_export = true;
        double precision = 0.01;
        bool binary_mode = false;
    };

    struct ModelImportResult {
        bool success;
        std::string message;
        std::vector<std::string> shape_ids;
        std::string detected_format;
        std::string filename;
        int64_t file_size;
        int32_t shape_count;
        std::string format;
        std::string creation_time;
        std::string format_version;
        std::vector<std::string> supported_features;
    };

    struct ModelExportResult {
        bool success;
        std::string message;
        std::string model_data;
        std::string filename;
        int64_t file_size;
        int32_t shape_count;
        std::string format;
        std::string creation_time;
        std::string format_version;
        std::vector<std::string> supported_features;
    };

    ModelImportResult ImportModelFile(const std::string& file_path, const ModelImportOptions& options = {});
    ModelExportResult ExportModelFile(const std::vector<std::string>& shape_ids, const ModelExportOptions& options = {});

    // Event handling (for future real-time updates)
    using ShapeUpdateCallback = std::function<void(const std::string& shape_id, const MeshData& mesh_data)>;
    void SetShapeUpdateCallback(ShapeUpdateCallback callback);

private:
    std::string server_address_;
    std::string client_id_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<geometry::GeometryService::Stub> stub_;
    bool connected_;
    
    ShapeUpdateCallback update_callback_;
    
    // Helper methods
    void AddClientMetadata(grpc::ClientContext& context) const;
    std::string FormatGrpcError(const grpc::Status& status, const std::string& operation) const;
    geometry::Point3D CreatePoint3D(double x, double y, double z);
    geometry::Vector3D CreateVector3D(double x, double y, double z);
    geometry::Color CreateColor(double r, double g, double b, double a = 1.0);
    
    MeshData ConvertProtoMesh(const geometry::MeshData& proto_mesh);
};