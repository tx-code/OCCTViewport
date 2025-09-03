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

    // Shape query
    grpc::Status ListShapes(grpc::ServerContext* context,
                           const geometry::EmptyRequest* request,
                           geometry::ShapeListResponse* response) override;

    grpc::Status GetShapeProperties(grpc::ServerContext* context,
                                   const geometry::ShapeRequest* request,
                                   geometry::ShapeProperties* response) override;

    // Real-time updates
    grpc::Status Subscribe(grpc::ServerContext* context,
                          grpc::ServerReaderWriter<geometry::ServerEvent, geometry::ClientEvent>* stream) override;

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

    // STEP file operations
    grpc::Status ImportStepFile(grpc::ServerContext* context,
                               const geometry::StepFileRequest* request,
                               geometry::StepImportResponse* response) override;

    grpc::Status ExportStepFile(grpc::ServerContext* context,
                               const geometry::StepExportRequest* request,
                               geometry::StepFileResponse* response) override;

    grpc::Status LoadStepFromData(grpc::ServerContext* context,
                                 const geometry::StepDataRequest* request,
                                 geometry::StepImportResponse* response) override;

    // BREP file operations
    grpc::Status ImportBrepFile(grpc::ServerContext* context,
                               const geometry::BrepFileRequest* request,
                               geometry::BrepImportResponse* response) override;

    grpc::Status ExportBrepFile(grpc::ServerContext* context,
                               const geometry::BrepExportRequest* request,
                               geometry::BrepFileResponse* response) override;

    grpc::Status LoadBrepFromData(grpc::ServerContext* context,
                                 const geometry::BrepDataRequest* request,
                                 geometry::BrepImportResponse* response) override;

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

    // STEP file helper methods
    std::vector<std::string> importStepFileInternal(const std::string& file_path,
                                                   const geometry::StepImportOptions& options);
    std::vector<std::string> importStepDataInternal(const std::string& step_data,
                                                   const std::string& filename,
                                                   const geometry::StepImportOptions& options);
    bool exportStepFileInternal(const std::vector<std::string>& shape_ids,
                               const geometry::StepExportOptions& options,
                               std::string& step_data,
                               geometry::StepFileInfo& file_info);
    geometry::StepFileInfo getStepFileInfo(const std::string& filename,
                                          const std::string& step_data,
                                          int shape_count);

    // BREP file helper methods
    std::vector<std::string> importBrepFileInternal(const std::string& file_path,
                                                   const geometry::BrepImportOptions& options);
    std::vector<std::string> importBrepDataInternal(const std::string& brep_data,
                                                   const std::string& filename,
                                                   const geometry::BrepImportOptions& options);
    bool exportBrepFileInternal(const std::vector<std::string>& shape_ids,
                               const geometry::BrepExportOptions& options,
                               std::string& brep_data,
                               geometry::BrepFileInfo& file_info);
    geometry::BrepFileInfo getBrepFileInfo(const std::string& filename,
                                          const std::string& brep_data,
                                          int shape_count);

    // Internal data
    std::unordered_map<std::string, ShapeData> shapes_;
    std::atomic<int> shape_counter_{0};
    bool connected_{true};  // Service connection status
    
    // OCCT context
    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_Viewer) viewer_;
};