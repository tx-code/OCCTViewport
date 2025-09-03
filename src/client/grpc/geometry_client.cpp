#include "geometry_client.h"
#include <spdlog/spdlog.h>
#include <chrono>

GeometryClient::GeometryClient(const std::string& server_address) 
    : server_address_(server_address), connected_(false) {
    spdlog::info("GeometryClient: Initializing client for server: {}", server_address_);
}

GeometryClient::~GeometryClient() {
    try {
        Disconnect();
    } catch (...) {
        // Suppress all exceptions in destructor to prevent OpenGL context issues
        spdlog::warn("GeometryClient::~GeometryClient(): Exception during cleanup, ignoring");
    }
}

bool GeometryClient::Connect() {
    try {
        spdlog::info("GeometryClient: Connecting to server: {}", server_address_);
        
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = geometry::GeometryService::NewStub(channel_);
        
        // Test connection with GetSystemInfo (with timeout)
        geometry::EmptyRequest request;
        geometry::SystemInfoResponse response;
        grpc::ClientContext context;
        
        // Set a 2-second timeout for connection test
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::seconds(2);
        context.set_deadline(deadline);
        
        grpc::Status status = stub_->GetSystemInfo(&context, request, &response);
        
        if (status.ok()) {
            connected_ = true;
            spdlog::info("GeometryClient: Connected successfully. Server version: {}, OCCT version: {}", 
                        response.version(), response.occt_version());
            return true;
        } else {
            spdlog::error("GeometryClient: Failed to connect: {} - {}", 
                         static_cast<int>(status.error_code()), status.error_message());
            connected_ = false;
            return false;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient: Connection exception: {}", e.what());
        connected_ = false;
        return false;
    }
}

void GeometryClient::Disconnect() {
    if (connected_) {
        spdlog::info("GeometryClient: Disconnecting from server");
        try {
            // Set connected flag first to prevent new operations
            connected_ = false;
            
            // Reset gRPC objects safely
            if (stub_) {
                stub_.reset();
            }
            if (channel_) {
                channel_.reset();
            }
            
            spdlog::info("GeometryClient: Disconnection completed successfully");
        } catch (const std::exception& e) {
            spdlog::warn("GeometryClient: Exception during disconnect: {}", e.what());
        } catch (...) {
            spdlog::warn("GeometryClient: Unknown exception during disconnect");
        }
    }
}

bool GeometryClient::IsConnected() const {
    return connected_;
}

std::string GeometryClient::CreateBox(double x, double y, double z, 
                                     double width, double height, double depth,
                                     double r, double g, double b) {
    if (!connected_) {
        spdlog::error("GeometryClient::CreateBox: Not connected to server");
        return "";
    }
    
    try {
        geometry::BoxRequest request;
        *request.mutable_position() = CreatePoint3D(x, y, z);
        request.set_width(width);
        request.set_height(height);
        request.set_depth(depth);
        *request.mutable_color() = CreateColor(r, g, b);
        
        geometry::ShapeResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->CreateBox(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::CreateBox: Created box with ID: {}", response.shape_id());
            return response.shape_id();
        } else {
            spdlog::error("GeometryClient::CreateBox: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return "";
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::CreateBox: Exception: {}", e.what());
        return "";
    }
}

std::string GeometryClient::CreateCone(double x, double y, double z,
                                      double base_radius, double top_radius, double height,
                                      double r, double g, double b) {
    if (!connected_) {
        spdlog::error("GeometryClient::CreateCone: Not connected to server");
        return "";
    }
    
    try {
        geometry::ConeRequest request;
        *request.mutable_position() = CreatePoint3D(x, y, z);
        *request.mutable_axis() = CreateVector3D(0, 0, 1); // Default Z-axis
        request.set_base_radius(base_radius);
        request.set_top_radius(top_radius);
        request.set_height(height);
        *request.mutable_color() = CreateColor(r, g, b);
        
        geometry::ShapeResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->CreateCone(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::CreateCone: Created cone with ID: {}", response.shape_id());
            return response.shape_id();
        } else {
            spdlog::error("GeometryClient::CreateCone: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return "";
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::CreateCone: Exception: {}", e.what());
        return "";
    }
}

bool GeometryClient::CreateDemoScene() {
    if (!connected_) {
        spdlog::error("GeometryClient::CreateDemoScene: Not connected to server");
        return false;
    }
    
    try {
        geometry::EmptyRequest request;
        geometry::StatusResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->CreateDemoScene(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::CreateDemoScene: Success - {}", response.message());
            return true;
        } else {
            spdlog::error("GeometryClient::CreateDemoScene: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return false;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::CreateDemoScene: Exception: {}", e.what());
        return false;
    }
}

bool GeometryClient::ClearAll() {
    if (!connected_) {
        spdlog::error("GeometryClient::ClearAll: Not connected to server");
        return false;
    }
    
    try {
        geometry::EmptyRequest request;
        geometry::StatusResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->ClearAll(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::ClearAll: Success - {}", response.message());
            return true;
        } else {
            spdlog::error("GeometryClient::ClearAll: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return false;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::ClearAll: Exception: {}", e.what());
        return false;
    }
}

std::vector<std::string> GeometryClient::ListShapes() {
    std::vector<std::string> shapes;
    
    if (!connected_) {
        spdlog::error("GeometryClient::ListShapes: Not connected to server");
        return shapes;
    }
    
    try {
        geometry::EmptyRequest request;
        geometry::ShapeListResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->ListShapes(&context, request, &response);
        
        if (status.ok()) {
            for (int i = 0; i < response.shape_ids_size(); ++i) {
                shapes.push_back(response.shape_ids(i));
            }
            spdlog::info("GeometryClient::ListShapes: Found {} shapes", shapes.size());
        } else {
            spdlog::error("GeometryClient::ListShapes: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::ListShapes: Exception: {}", e.what());
    }
    
    return shapes;
}

GeometryClient::SystemInfo GeometryClient::GetSystemInfo() {
    SystemInfo info;
    info.version = "unknown";
    info.active_shapes = 0;
    info.occt_version = "unknown";
    
    if (!connected_) {
        spdlog::error("GeometryClient::GetSystemInfo: Not connected to server");
        return info;
    }
    
    try {
        geometry::EmptyRequest request;
        geometry::SystemInfoResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->GetSystemInfo(&context, request, &response);
        
        if (status.ok()) {
            info.version = response.version();
            info.active_shapes = response.active_shapes();
            info.occt_version = response.occt_version();
            spdlog::info("GeometryClient::GetSystemInfo: Server version: {}, Active shapes: {}, OCCT: {}", 
                        info.version, info.active_shapes, info.occt_version);
        } else {
            spdlog::error("GeometryClient::GetSystemInfo: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::GetSystemInfo: Exception: {}", e.what());
    }
    
    return info;
}

// BREP file operations implementation

GeometryClient::BrepImportResult GeometryClient::ImportBrepFile(
    const std::string& file_path, bool merge_shapes, bool validate_shapes) {
    
    BrepImportResult result;
    result.success = false;
    
    if (!connected_) {
        result.message = "Not connected to server";
        spdlog::error("GeometryClient::ImportBrepFile: Not connected to server");
        return result;
    }
    
    try {
        geometry::BrepFileRequest request;
        request.set_file_path(file_path);
        
        // Set import options
        geometry::BrepImportOptions* options = request.mutable_options();
        options->set_merge_shapes(merge_shapes);
        options->set_validate_shapes(validate_shapes);
        
        geometry::BrepImportResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->ImportBrepFile(&context, request, &response);
        
        if (status.ok()) {
            result.success = response.success();
            result.message = response.message();
            
            for (int i = 0; i < response.shape_ids_size(); ++i) {
                result.shape_ids.push_back(response.shape_ids(i));
            }
            
            if (response.has_file_info()) {
                const auto& file_info = response.file_info();
                result.file_size = file_info.file_size();
                result.creation_time = file_info.creation_time();
                result.format_version = file_info.format_version();
            }
            
            spdlog::info("GeometryClient::ImportBrepFile: {} - Imported {} shapes from {}", 
                        result.message, result.shape_ids.size(), file_path);
        } else {
            result.message = "gRPC call failed: " + status.error_message();
            spdlog::error("GeometryClient::ImportBrepFile: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        result.message = "Exception: " + std::string(e.what());
        spdlog::error("GeometryClient::ImportBrepFile: Exception: {}", e.what());
    }
    
    return result;
}

GeometryClient::BrepImportResult GeometryClient::LoadBrepFromData(
    const std::string& brep_data, const std::string& filename, 
    bool merge_shapes, bool validate_shapes) {
    
    BrepImportResult result;
    result.success = false;
    
    if (!connected_) {
        result.message = "Not connected to server";
        spdlog::error("GeometryClient::LoadBrepFromData: Not connected to server");
        return result;
    }
    
    try {
        geometry::BrepDataRequest request;
        request.set_brep_data(brep_data);
        request.set_filename(filename);
        
        // Set import options
        geometry::BrepImportOptions* options = request.mutable_options();
        options->set_merge_shapes(merge_shapes);
        options->set_validate_shapes(validate_shapes);
        
        geometry::BrepImportResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->LoadBrepFromData(&context, request, &response);
        
        if (status.ok()) {
            result.success = response.success();
            result.message = response.message();
            
            for (int i = 0; i < response.shape_ids_size(); ++i) {
                result.shape_ids.push_back(response.shape_ids(i));
            }
            
            if (response.has_file_info()) {
                const auto& file_info = response.file_info();
                result.file_size = file_info.file_size();
                result.creation_time = file_info.creation_time();
                result.format_version = file_info.format_version();
            }
            
            spdlog::info("GeometryClient::LoadBrepFromData: {} - Loaded {} shapes from data ({})", 
                        result.message, result.shape_ids.size(), filename);
        } else {
            result.message = "gRPC call failed: " + status.error_message();
            spdlog::error("GeometryClient::LoadBrepFromData: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        result.message = "Exception: " + std::string(e.what());
        spdlog::error("GeometryClient::LoadBrepFromData: Exception: {}", e.what());
    }
    
    return result;
}

GeometryClient::BrepExportResult GeometryClient::ExportBrepFile(
    const std::vector<std::string>& shape_ids, 
    bool export_as_compound, bool validate_before_export) {
    
    BrepExportResult result;
    result.success = false;
    
    if (!connected_) {
        result.message = "Not connected to server";
        spdlog::error("GeometryClient::ExportBrepFile: Not connected to server");
        return result;
    }
    
    if (shape_ids.empty()) {
        result.message = "No shapes to export";
        spdlog::error("GeometryClient::ExportBrepFile: No shapes to export");
        return result;
    }
    
    try {
        geometry::BrepExportRequest request;
        
        for (const auto& shape_id : shape_ids) {
            request.add_shape_ids(shape_id);
        }
        
        // Set export options
        geometry::BrepExportOptions* options = request.mutable_options();
        options->set_export_as_compound(export_as_compound);
        options->set_validate_before_export(validate_before_export);
        
        geometry::BrepFileResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->ExportBrepFile(&context, request, &response);
        
        if (status.ok()) {
            result.success = response.success();
            result.message = response.message();
            result.brep_data = response.brep_data();
            result.filename = response.filename();
            
            if (response.has_file_info()) {
                const auto& file_info = response.file_info();
                result.file_size = file_info.file_size();
                result.creation_time = file_info.creation_time();
                result.format_version = file_info.format_version();
            }
            
            spdlog::info("GeometryClient::ExportBrepFile: {} - Exported {} shapes, data size: {} bytes", 
                        result.message, shape_ids.size(), result.brep_data.size());
        } else {
            result.message = "gRPC call failed: " + status.error_message();
            spdlog::error("GeometryClient::ExportBrepFile: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        result.message = "Exception: " + std::string(e.what());
        spdlog::error("GeometryClient::ExportBrepFile: Exception: {}", e.what());
    }
    
    return result;
}

// Helper methods
geometry::Point3D GeometryClient::CreatePoint3D(double x, double y, double z) {
    geometry::Point3D point;
    point.set_x(x);
    point.set_y(y);
    point.set_z(z);
    return point;
}

geometry::Vector3D GeometryClient::CreateVector3D(double x, double y, double z) {
    geometry::Vector3D vector;
    vector.set_x(x);
    vector.set_y(y);
    vector.set_z(z);
    return vector;
}

geometry::Color GeometryClient::CreateColor(double r, double g, double b, double a) {
    geometry::Color color;
    color.set_r(r);
    color.set_g(g);
    color.set_b(b);
    color.set_a(a);
    return color;
}

// Placeholder implementations for methods not yet fully implemented
std::string GeometryClient::CreateSphere(double x, double y, double z, double radius,
                                        double r, double g, double b) {
    spdlog::warn("GeometryClient::CreateSphere: Not implemented yet");
    return "";
}

std::string GeometryClient::CreateCylinder(double x, double y, double z, double radius, double height,
                                          double r, double g, double b) {
    spdlog::warn("GeometryClient::CreateCylinder: Not implemented yet");
    return "";
}

bool GeometryClient::DeleteShape(const std::string& shape_id) {
    spdlog::warn("GeometryClient::DeleteShape: Not implemented yet");
    return false;
}

bool GeometryClient::SetShapeColor(const std::string& shape_id, double r, double g, double b) {
    spdlog::warn("GeometryClient::SetShapeColor: Not implemented yet");
    return false;
}

std::vector<GeometryClient::MeshData> GeometryClient::GetAllMeshes() {
    std::vector<MeshData> meshes;
    
    if (!connected_) {
        spdlog::error("GeometryClient::GetAllMeshes: Not connected to server");
        return meshes;
    }
    
    try {
        geometry::EmptyRequest request;
        grpc::ClientContext context;
        
        std::unique_ptr<grpc::ClientReader<geometry::MeshData>> reader = 
            stub_->GetAllMeshes(&context, request);
        
        geometry::MeshData proto_mesh;
        while (reader->Read(&proto_mesh)) {
            MeshData mesh_data = ConvertProtoMesh(proto_mesh);
            spdlog::info("GeometryClient::GetAllMeshes: Received mesh for shape: {} ({} vertices)", 
                        mesh_data.shape_id, mesh_data.vertices.size() / 3);
            meshes.push_back(std::move(mesh_data));
        }
        
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            spdlog::error("GeometryClient::GetAllMeshes: Stream failed - {}", status.error_message());
        } else {
            spdlog::info("GeometryClient::GetAllMeshes: Successfully received {} meshes", meshes.size());
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::GetAllMeshes: Exception: {}", e.what());
    }
    
    return meshes;
}

GeometryClient::MeshData GeometryClient::GetMeshData(const std::string& shape_id) {
    MeshData mesh_data;
    mesh_data.shape_id = shape_id;
    
    if (!connected_) {
        spdlog::error("GeometryClient::GetMeshData: Not connected to server");
        return mesh_data;
    }
    
    try {
        geometry::ShapeRequest request;
        request.set_shape_id(shape_id);
        geometry::MeshData response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->GetMeshData(&context, request, &response);
        
        if (status.ok()) {
            mesh_data = ConvertProtoMesh(response);
            spdlog::info("GeometryClient::GetMeshData: Retrieved mesh for {}: {} vertices, {} triangles", 
                        shape_id, mesh_data.vertices.size() / 3, mesh_data.indices.size() / 3);
        } else {
            spdlog::error("GeometryClient::GetMeshData: Failed - {}", status.error_message());
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::GetMeshData: Exception: {}", e.what());
    }
    
    return mesh_data;
}

GeometryClient::MeshData GeometryClient::ConvertProtoMesh(const geometry::MeshData& proto_mesh) {
    MeshData mesh_data;
    mesh_data.shape_id = proto_mesh.shape_id();
    
    // Convert vertices
    mesh_data.vertices.reserve(proto_mesh.vertices_size() * 3);
    for (const auto& vertex : proto_mesh.vertices()) {
        mesh_data.vertices.push_back(static_cast<float>(vertex.x()));
        mesh_data.vertices.push_back(static_cast<float>(vertex.y()));
        mesh_data.vertices.push_back(static_cast<float>(vertex.z()));
    }
    
    // Convert normals
    mesh_data.normals.reserve(proto_mesh.normals_size() * 3);
    for (const auto& normal : proto_mesh.normals()) {
        mesh_data.normals.push_back(static_cast<float>(normal.x()));
        mesh_data.normals.push_back(static_cast<float>(normal.y()));
        mesh_data.normals.push_back(static_cast<float>(normal.z()));
    }
    
    // Convert indices
    mesh_data.indices.reserve(proto_mesh.indices_size());
    for (int index : proto_mesh.indices()) {
        mesh_data.indices.push_back(index);
    }
    
    // Convert color
    if (proto_mesh.has_color()) {
        const auto& color = proto_mesh.color();
        mesh_data.color[0] = static_cast<float>(color.r());
        mesh_data.color[1] = static_cast<float>(color.g());
        mesh_data.color[2] = static_cast<float>(color.b());
        mesh_data.color[3] = static_cast<float>(color.a());
    } else {
        // Default color
        mesh_data.color[0] = 0.8f;
        mesh_data.color[1] = 0.8f;
        mesh_data.color[2] = 0.8f;
        mesh_data.color[3] = 1.0f;
    }
    
    // Set other properties
    mesh_data.visible = true;
    mesh_data.selected = false;
    mesh_data.highlighted = false;
    
    return mesh_data;
}

void GeometryClient::SetShapeUpdateCallback(ShapeUpdateCallback callback) {
    update_callback_ = std::move(callback);
}