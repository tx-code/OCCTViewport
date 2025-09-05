#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <atomic>

// gRPC and Protocol Buffer includes
#include "geometry_service.grpc.pb.h"

// OCCT includes
#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_Viewer.hxx>
// STEP import/export includes
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>

// Standard includes
#include <grpcpp/grpcpp.h>

class GeometryServiceImpl final : public geometry::GeometryService::Service {
public:
    GeometryServiceImpl();
    ~GeometryServiceImpl();

    // Primitive creation methods
    grpc::Status CreateBox(grpc::ServerContext* context,
                          const geometry::BoxRequest* request,
                          geometry::ShapeResponse* response) override;

    grpc::Status CreateCone(grpc::ServerContext* context,
                           const geometry::ConeRequest* request,
                           geometry::ShapeResponse* response) override;

    grpc::Status CreateSphere(grpc::ServerContext* context,
                             const geometry::SphereRequest* request,
                             geometry::ShapeResponse* response) override;

    grpc::Status CreateCylinder(grpc::ServerContext* context,
                               const geometry::CylinderRequest* request,
                               geometry::ShapeResponse* response) override;

    // Shape operations
    grpc::Status DeleteShape(grpc::ServerContext* context,
                            const geometry::ShapeRequest* request,
                            geometry::StatusResponse* response) override;

    grpc::Status TransformShape(grpc::ServerContext* context,
                               const geometry::TransformRequest* request,
                               geometry::ShapeResponse* response) override;

    grpc::Status SetShapeColor(grpc::ServerContext* context,
                              const geometry::ColorRequest* request,
                              geometry::StatusResponse* response) override;

    // Mesh data retrieval
    grpc::Status GetMeshData(grpc::ServerContext* context,
                            const geometry::ShapeRequest* request,
                            geometry::MeshData* response) override;

    grpc::Status GetAllMeshes(grpc::ServerContext* context,
                             const geometry::EmptyRequest* request,
                             grpc::ServerWriter<geometry::MeshData>* writer) override;


    // System operations
    grpc::Status ClearAll(grpc::ServerContext* context,
                         const geometry::EmptyRequest* request,
                         geometry::StatusResponse* response) override;

    grpc::Status GetSystemInfo(grpc::ServerContext* context,
                              const geometry::EmptyRequest* request,
                              geometry::SystemInfoResponse* response) override;

    grpc::Status CreateDemoScene(grpc::ServerContext* context,
                               const geometry::EmptyRequest* request,
                               geometry::StatusResponse* response) override;

    // Legacy STEP/BREP operations removed - use unified ImportModelFile/ExportModelFile

    // Unified model file operations
    grpc::Status ImportModelFile(grpc::ServerContext* context,
                                const geometry::ModelFileRequest* request,
                                geometry::ModelImportResponse* response) override;

    grpc::Status ExportModelFile(grpc::ServerContext* context,
                                const geometry::ModelExportRequest* request,
                                geometry::ModelFileResponse* response) override;

    // LoadModelFromData removed - not needed for current implementation

private:
    struct ShapeData {
        Handle(AIS_Shape) ais_shape;
        TopoDS_Shape topo_shape;
        geometry::Color color;
        std::string shape_id;
        bool visible{true};
        bool selected{false};
        bool highlighted{false};
    };

    // Internal helper methods
    std::string generateShapeId();
    Handle(AIS_Shape) createBoxShape(const geometry::BoxRequest& request);
    Handle(AIS_Shape) createConeShape(const geometry::ConeRequest& request);
    Handle(AIS_Shape) createSphereShape(const geometry::SphereRequest& request);
    Handle(AIS_Shape) createCylinderShape(const geometry::CylinderRequest& request);
    
    geometry::MeshData extractMeshData(const std::string& shape_id);
    void setShapeColorInternal(const std::string& shape_id, const geometry::Color& color);
    
    // Convert between OCCT and Proto types
    geometry::Point3D toProtoPoint(const gp_Pnt& point);
    geometry::Vector3D toProtoVector(const gp_Vec& vector);
    geometry::Color toProtoColor(const Quantity_Color& color);
    gp_Pnt fromProtoPoint(const geometry::Point3D& point);
    gp_Vec fromProtoVector(const geometry::Vector3D& vector);
    Quantity_Color fromProtoColor(const geometry::Color& color);

    // Unified model file helper methods
    std::vector<std::string> importModelFileInternal(const std::string& file_path,
                                                     const geometry::ModelImportOptions& options);
    std::vector<std::string> importModelDataInternal(const std::string& model_data,
                                                     const std::string& filename,
                                                     const geometry::ModelImportOptions& options);
    bool exportModelFileInternal(const std::vector<std::string>& shape_ids,
                                const geometry::ModelExportOptions& options,
                                std::string& model_data,
                                geometry::ModelFileInfo& file_info);
    geometry::ModelFileInfo getModelFileInfo(const std::string& filename,
                                            const std::string& model_data,
                                            int shape_count,
                                            const std::string& format);
    std::string detectFileFormat(const std::string& file_path, const std::string& force_format);
    std::string detectFormatFromExtension(const std::string& filename);
    std::string detectFormatFromContent(const std::string& content);
    
    // Format-specific import methods
    std::vector<std::string> importModelFileInternal_FormatSpecific(const std::string& file_path,
                                                                   const std::string& format,
                                                                   const geometry::ModelImportOptions& options);
    std::vector<std::string> importStlFileInternal(const std::string& file_path,
                                                   const geometry::ModelImportOptions& options);
    std::vector<std::string> importIgesFileInternal(const std::string& file_path,
                                                    const geometry::ModelImportOptions& options);
    std::vector<std::string> importObjFileInternal(const std::string& file_path,
                                                   const geometry::ModelImportOptions& options);
    std::vector<std::string> importPlyFileInternal(const std::string& file_path,
                                                   const geometry::ModelImportOptions& options);
    std::vector<std::string> importGltfFileInternal(const std::string& file_path,
                                                    const geometry::ModelImportOptions& options);
    

    // Internal data
    std::unordered_map<std::string, ShapeData> shapes_;
    std::atomic<int> shape_counter_{0};
    bool connected_{true};  // Service connection status
    
    // OCCT context
    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_Viewer) viewer_;
};