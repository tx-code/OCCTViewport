#include "geometry_client.h"
#include "../../common/grpc_performance_monitor.h"
#include <spdlog/spdlog.h>
#include <chrono>

GeometryClient::GeometryClient(const std::string& server_address, const std::string& client_id) 
    : server_address_(server_address), client_id_(client_id), connected_(false) {
    spdlog::info("GeometryClient: Initializing client '{}' for server: {}", client_id_, server_address_);
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
    GRPC_PERF_TIMER("Connect");
    
    try {
        spdlog::info("GeometryClient: Connecting to server: {}", server_address_);
        
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = geometry::GeometryService::NewStub(channel_);
        
        // Test connection with GetSystemInfo (with timeout)
        geometry::EmptyRequest request;
        geometry::SystemInfoResponse response;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
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
            connected_ = false;
            // Enhanced error reporting based on gRPC status code
            switch (status.error_code()) {
                case grpc::StatusCode::UNAVAILABLE:
                    spdlog::error("GeometryClient: Server unavailable - check if GeometryServer is running on {}", server_address_);
                    break;
                case grpc::StatusCode::DEADLINE_EXCEEDED:
                    spdlog::error("GeometryClient: Connection timeout - server may be overloaded or network issues");
                    break;
                case grpc::StatusCode::PERMISSION_DENIED:
                    spdlog::error("GeometryClient: Permission denied - check client credentials");
                    break;
                case grpc::StatusCode::RESOURCE_EXHAUSTED:
                    spdlog::error("GeometryClient: Server resources exhausted - try again later");
                    break;
                default:
                    spdlog::error("GeometryClient: Connection failed: {} - {}", 
                                 static_cast<int>(status.error_code()), status.error_message());
                    break;
            }
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
            // Notify server before disconnecting
            DisconnectFromServer();
            
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

bool GeometryClient::DisconnectFromServer() {
    if (!connected_ || !stub_) {
        spdlog::warn("GeometryClient::DisconnectFromServer: Not connected to server");
        return false;
    }

    try {
        grpc::ClientContext context;
        AddClientMetadata(context);

        geometry::EmptyRequest request;
        geometry::StatusResponse response;

        auto status = stub_->DisconnectClient(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient: Successfully notified server of disconnection: {}", response.message());
            return true;
        } else {
            spdlog::warn("GeometryClient: Failed to notify server of disconnection: {} ({})", 
                        response.message(), status.error_message());
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::DisconnectFromServer: Exception: {}", e.what());
        return false;
    }
}

bool GeometryClient::IsConnected() const {
    return connected_;
}

std::string GeometryClient::CreateBox(double x, double y, double z, 
                                     double width, double height, double depth,
                                     double r, double g, double b) {
    GRPC_PERF_TIMER("CreateBox");
    
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
        AddClientMetadata(context);
        
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
    GRPC_PERF_TIMER("CreateCone");
    
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
        AddClientMetadata(context);
        
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
        AddClientMetadata(context);
        
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
        AddClientMetadata(context);
        
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
        AddClientMetadata(context);
        
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

std::string GeometryClient::CreateSphere(double x, double y, double z, double radius,
                                        double r, double g, double b) {
    if (!connected_) {
        spdlog::error("GeometryClient::CreateSphere: Not connected to server");
        return "";
    }
    
    try {
        geometry::SphereRequest request;
        request.mutable_center()->set_x(x);
        request.mutable_center()->set_y(y);
        request.mutable_center()->set_z(z);
        request.set_radius(radius);
        *request.mutable_color() = CreateColor(r, g, b);
        
        geometry::ShapeResponse response;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
        grpc::Status status = stub_->CreateSphere(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::CreateSphere: Created sphere with ID: {}", response.shape_id());
            return response.shape_id();
        } else {
            spdlog::error("GeometryClient::CreateSphere: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return "";
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::CreateSphere: Exception: {}", e.what());
        return "";
    }
}

std::string GeometryClient::CreateCylinder(double x, double y, double z, double radius, double height,
                                          double r, double g, double b) {
    if (!connected_) {
        spdlog::error("GeometryClient::CreateCylinder: Not connected to server");
        return "";
    }
    
    try {
        geometry::CylinderRequest request;
        request.mutable_position()->set_x(x);
        request.mutable_position()->set_y(y);
        request.mutable_position()->set_z(z);
        request.set_radius(radius);
        request.set_height(height);
        *request.mutable_color() = CreateColor(r, g, b);
        
        geometry::ShapeResponse response;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
        grpc::Status status = stub_->CreateCylinder(&context, request, &response);
        
        if (status.ok() && response.success()) {
            spdlog::info("GeometryClient::CreateCylinder: Created cylinder with ID: {}", response.shape_id());
            return response.shape_id();
        } else {
            spdlog::error("GeometryClient::CreateCylinder: Failed - {}", 
                         status.ok() ? response.message() : status.error_message());
            return "";
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::CreateCylinder: Exception: {}", e.what());
        return "";
    }
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
    auto _perf_timer = GrpcPerformanceMonitor::getInstance().createTimer("GetAllMeshes");
    
    std::vector<MeshData> meshes;
    size_t total_bytes_received = 0;
    
    if (!connected_) {
        spdlog::error("GeometryClient::GetAllMeshes: Not connected to server");
        return meshes;
    }
    
    try {
        geometry::EmptyRequest request;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
        std::unique_ptr<grpc::ClientReader<geometry::MeshData>> reader = 
            stub_->GetAllMeshes(&context, request);
        
        geometry::MeshData proto_mesh;
        while (reader->Read(&proto_mesh)) {
            // Calculate approximate bytes received for this mesh
            size_t mesh_bytes = proto_mesh.vertices_size() * sizeof(float) + 
                               proto_mesh.indices_size() * sizeof(int32_t) + 
                               proto_mesh.normals_size() * sizeof(float) +
                               proto_mesh.shape_id().size() + 16; // overhead
            total_bytes_received += mesh_bytes;
            
            MeshData mesh_data = ConvertProtoMesh(proto_mesh);
            spdlog::info("GeometryClient::GetAllMeshes: Received mesh for shape: {} ({} vertices, ~{} bytes)", 
                        mesh_data.shape_id, mesh_data.vertices.size() / 3, mesh_bytes);
            meshes.push_back(std::move(mesh_data));
        }
        
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::string error_msg = FormatGrpcError(status, "GetAllMeshes");
            spdlog::error("GeometryClient::GetAllMeshes: {}", error_msg);
            // Clear partially received data on error to maintain consistency
            meshes.clear();
        } else {
            spdlog::info("GeometryClient::GetAllMeshes: Successfully received {} meshes, total ~{} bytes", 
                        meshes.size(), total_bytes_received);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("GeometryClient::GetAllMeshes: Exception: {}", e.what());
    }
    
    // Set bytes received for performance monitoring
    if (_perf_timer) {
        _perf_timer->setBytesReceived(total_bytes_received);
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
        AddClientMetadata(context);
        
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

void GeometryClient::AddClientMetadata(grpc::ClientContext& context) const {
    // Add client ID to metadata so server can identify the client
    context.AddMetadata("client-id", client_id_);
}

// Helper function for consistent gRPC error handling
std::string GeometryClient::FormatGrpcError(const grpc::Status& status, const std::string& operation) const {
    switch (status.error_code()) {
        case grpc::StatusCode::OK:
            return "Success";
        case grpc::StatusCode::UNAVAILABLE:
            return operation + " failed: Server unavailable (check if GeometryServer is running)";
        case grpc::StatusCode::DEADLINE_EXCEEDED:
            return operation + " failed: Timeout (server may be overloaded)";
        case grpc::StatusCode::PERMISSION_DENIED:
            return operation + " failed: Permission denied";
        case grpc::StatusCode::RESOURCE_EXHAUSTED:
            return operation + " failed: Server resources exhausted";
        case grpc::StatusCode::INVALID_ARGUMENT:
            return operation + " failed: Invalid argument - " + status.error_message();
        case grpc::StatusCode::NOT_FOUND:
            return operation + " failed: Resource not found - " + status.error_message();
        case grpc::StatusCode::INTERNAL:
            return operation + " failed: Internal server error - " + status.error_message();
        default:
            return operation + " failed: " + std::to_string(static_cast<int>(status.error_code())) + 
                   " - " + status.error_message();
    }
}


// Unified model file operations implementation

GeometryClient::ModelImportResult GeometryClient::ImportModelFile(const std::string& file_path, const ModelImportOptions& options) {
    ModelImportResult result;
    result.success = false;
    
    if (!connected_) {
        spdlog::error("GeometryClient::ImportModelFile: Not connected to server");
        result.message = "Not connected to server";
        return result;
    }
    
    try {
        geometry::ModelFileRequest request;
        request.set_file_path(file_path);
        
        // Set import options
        auto* proto_options = request.mutable_options();
        proto_options->set_auto_detect_format(options.auto_detect_format);
        proto_options->set_force_format(options.force_format);
        proto_options->set_import_colors(options.import_colors);
        proto_options->set_import_names(options.import_names);
        // proto_options->set_import_materials(options.import_materials); // Field removed in simplified proto
        proto_options->set_precision(options.precision);
        proto_options->set_merge_shapes(options.merge_shapes);
        // proto_options->set_validate_shapes(options.validate_shapes); // Field removed in simplified proto
        // proto_options->set_heal_shapes(options.heal_shapes); // Field removed in simplified proto
        // proto_options->set_linear_tolerance(options.linear_tolerance); // Field removed in simplified proto
        // proto_options->set_angular_tolerance(options.angular_tolerance); // Field removed in simplified proto
        
        geometry::ModelImportResponse response;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
        grpc::Status status = stub_->ImportModelFile(&context, request, &response);
        
        if (status.ok() && response.success()) {
            result.success = true;
            result.message = response.message();
            result.detected_format = response.detected_format();
            
            for (int i = 0; i < response.shape_ids_size(); ++i) {
                result.shape_ids.push_back(response.shape_ids(i));
            }
            
            // Copy file info
            if (response.has_file_info()) {
                const auto& file_info = response.file_info();
                result.filename = file_info.filename();
                result.file_size = file_info.file_size();
                result.shape_count = file_info.shape_count();
                result.format = file_info.format();
                result.creation_time = file_info.creation_time();
                // result.format_version = file_info.format_version(); // Field removed in simplified proto
                
                // for (int i = 0; i < file_info.supported_features_size(); ++i) { // Field removed in simplified proto
                //     result.supported_features.push_back(file_info.supported_features(i));
                // }
            }
            
            spdlog::info("GeometryClient::ImportModelFile: Successfully imported {} shapes from {} (format: {})", 
                        result.shape_ids.size(), file_path, result.detected_format);
        } else {
            result.message = status.ok() ? response.message() : status.error_message();
            spdlog::error("GeometryClient::ImportModelFile: Failed - {}", result.message);
        }
        
    } catch (const std::exception& e) {
        result.message = std::string("Exception: ") + e.what();
        spdlog::error("GeometryClient::ImportModelFile: Exception: {}", e.what());
    }
    
    return result;
}


GeometryClient::ModelExportResult GeometryClient::ExportModelFile(const std::vector<std::string>& shape_ids, const ModelExportOptions& options) {
    ModelExportResult result;
    result.success = false;
    
    if (!connected_) {
        spdlog::error("GeometryClient::ExportModelFile: Not connected to server");
        result.message = "Not connected to server";
        return result;
    }
    
    if (shape_ids.empty()) {
        spdlog::error("GeometryClient::ExportModelFile: No shape IDs provided");
        result.message = "No shape IDs provided";
        return result;
    }
    
    try {
        geometry::ModelExportRequest request;
        for (const auto& shape_id : shape_ids) {
            request.add_shape_ids(shape_id);
        }
        
        // Set export options
        auto* proto_options = request.mutable_options();
        proto_options->set_format(options.format);
        proto_options->set_export_colors(options.export_colors);
        proto_options->set_export_names(options.export_names);
        // proto_options->set_export_materials(options.export_materials); // Field removed in simplified proto
        // proto_options->set_schema_version(options.schema_version); // Field removed in simplified proto
        proto_options->set_units(options.units);
        // proto_options->set_export_as_compound(options.export_as_compound); // Field removed in simplified proto
        // proto_options->set_validate_before_export(options.validate_before_export); // Field removed in simplified proto
        // proto_options->set_precision(options.precision); // Field removed in simplified proto
        proto_options->set_binary_mode(options.binary_mode);
        
        geometry::ModelFileResponse response;
        grpc::ClientContext context;
        AddClientMetadata(context);
        
        grpc::Status status = stub_->ExportModelFile(&context, request, &response);
        
        if (status.ok() && response.success()) {
            result.success = true;
            result.message = response.message();
            result.model_data = response.model_data();
            result.filename = response.filename();
            
            // Copy file info
            const auto& file_info = response.file_info();
            result.file_size = file_info.file_size();
            result.shape_count = file_info.shape_count();
            result.format = file_info.format();
            result.creation_time = file_info.creation_time();
            // result.format_version = file_info.format_version(); // Field removed in simplified proto
            
            // for (int i = 0; i < file_info.supported_features_size(); ++i) { // Field removed in simplified proto
            //     result.supported_features.push_back(file_info.supported_features(i));
            // }
            
            spdlog::info("GeometryClient::ExportModelFile: Successfully exported {} shapes to {} format, size: {} bytes", 
                        shape_ids.size(), options.format, result.model_data.size());
        } else {
            result.message = status.ok() ? response.message() : status.error_message();
            spdlog::error("GeometryClient::ExportModelFile: Failed - {}", result.message);
        }
        
    } catch (const std::exception& e) {
        result.message = std::string("Exception: ") + e.what();
        spdlog::error("GeometryClient::ExportModelFile: Exception: {}", e.what());
    }
    
    return result;
}