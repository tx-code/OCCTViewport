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

    // Internal data
    std::unordered_map<std::string, ShapeData> shapes_;
    std::atomic<int> shape_counter_{0};
    bool connected_{true};  // Service connection status
    
    // OCCT context
    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_Viewer) viewer_;
};