#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

// gRPC includes
#include <grpcpp/grpcpp.h>
#include "geometry_service.grpc.pb.h"

// Forward declarations
struct MeshRenderData;

class GeometryClient {
public:
    explicit GeometryClient(const std::string& server_address = "localhost:50051");
    ~GeometryClient();

    // Connection management
    bool Connect();
    void Disconnect();
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
    std::vector<std::string> ListShapes();
    
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
    
    // BREP file operations
    struct BrepImportResult {
        bool success;
        std::string message;
        std::vector<std::string> shape_ids;
        int64_t file_size;
        std::string creation_time;
        std::string format_version;
    };

    struct BrepExportResult {
        bool success;
        std::string message;
        std::string brep_data;
        std::string filename;
        int64_t file_size;
        std::string creation_time;
        std::string format_version;
    };

    BrepImportResult ImportBrepFile(const std::string& file_path, bool merge_shapes = false, bool validate_shapes = true);
    BrepImportResult LoadBrepFromData(const std::string& brep_data, const std::string& filename = "data.brep", 
                                     bool merge_shapes = false, bool validate_shapes = true);
    BrepExportResult ExportBrepFile(const std::vector<std::string>& shape_ids, bool export_as_compound = false, 
                                   bool validate_before_export = true);

    // Event handling (for future real-time updates)
    using ShapeUpdateCallback = std::function<void(const std::string& shape_id, const MeshData& mesh_data)>;
    void SetShapeUpdateCallback(ShapeUpdateCallback callback);

private:
    std::string server_address_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<geometry::GeometryService::Stub> stub_;
    bool connected_;
    
    ShapeUpdateCallback update_callback_;
    
    // Helper methods
    geometry::Point3D CreatePoint3D(double x, double y, double z);
    geometry::Vector3D CreateVector3D(double x, double y, double z);
    geometry::Color CreateColor(double r, double g, double b, double a = 1.0);
    
    MeshData ConvertProtoMesh(const geometry::MeshData& proto_mesh);
};