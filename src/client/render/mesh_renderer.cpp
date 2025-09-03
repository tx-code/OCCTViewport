#include "mesh_renderer.h"
#include <spdlog/spdlog.h>
#include <iostream>

MeshRenderer::MeshRenderer() {
    spdlog::info("MeshRenderer: Initializing...");
}

MeshRenderer::~MeshRenderer() {
    Shutdown();
}

bool MeshRenderer::Initialize() {
    spdlog::info("MeshRenderer: Initializing OpenGL resources...");
    
    // Initialize OpenGL function pointers (assuming gl3w)
    // if (gl3wInit() != 0) {
    //     spdlog::error("MeshRenderer: Failed to initialize gl3w");
    //     return false;
    // }
    
    // Create and compile shaders
    if (!CreateShaders()) {
        spdlog::error("MeshRenderer: Failed to create shaders");
        return false;
    }
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Set clear color
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    
    spdlog::info("MeshRenderer: OpenGL initialization completed successfully");
    return true;
}

void MeshRenderer::Shutdown() {
    spdlog::info("MeshRenderer: Shutting down...");
    
    ClearAllMeshes();
    DestroyShaders();
}

void MeshRenderer::AddMesh(const std::string& mesh_id, const std::vector<float>& vertices,
                          const std::vector<float>& normals, const std::vector<int>& indices,
                          const float color[4]) {
    if (vertices.empty() || indices.empty()) {
        spdlog::warn("MeshRenderer::AddMesh: Empty mesh data for {}", mesh_id);
        return;
    }
    
    MeshData mesh_data;
    mesh_data.index_count = static_cast<int>(indices.size());
    mesh_data.color = glm::vec4(color[0], color[1], color[2], color[3]);
    
    // Generate OpenGL objects
    glGenVertexArrays(1, &mesh_data.vao);
    glGenBuffers(1, &mesh_data.vbo);
    glGenBuffers(1, &mesh_data.ebo);
    if (!normals.empty()) {
        glGenBuffers(1, &mesh_data.normal_vbo);
    }
    
    // Bind VAO
    glBindVertexArray(mesh_data.vao);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, mesh_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Upload normal data if available
    if (!normals.empty() && mesh_data.normal_vbo != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh_data.normal_vbo);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
    }
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_data.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);
    
    // Unbind VAO
    glBindVertexArray(0);
    
    // Store mesh data
    if (meshes_.find(mesh_id) != meshes_.end()) {
        DestroyMesh(meshes_[mesh_id]);
    }
    meshes_[mesh_id] = mesh_data;
    
    spdlog::info("MeshRenderer::AddMesh: Added mesh {} ({} vertices, {} triangles)", 
                mesh_id, vertices.size() / 3, indices.size() / 3);
}

void MeshRenderer::UpdateMesh(const std::string& mesh_id, const std::vector<float>& vertices,
                             const std::vector<float>& normals, const std::vector<int>& indices,
                             const float color[4]) {
    // For simplicity, just remove and re-add
    RemoveMesh(mesh_id);
    AddMesh(mesh_id, vertices, normals, indices, color);
}

void MeshRenderer::RemoveMesh(const std::string& mesh_id) {
    auto it = meshes_.find(mesh_id);
    if (it != meshes_.end()) {
        DestroyMesh(it->second);
        meshes_.erase(it);
        spdlog::info("MeshRenderer::RemoveMesh: Removed mesh {}", mesh_id);
    }
}

void MeshRenderer::ClearAllMeshes() {
    for (auto& [mesh_id, mesh_data] : meshes_) {
        DestroyMesh(mesh_data);
    }
    meshes_.clear();
    spdlog::info("MeshRenderer::ClearAllMeshes: Cleared all meshes");
}

void MeshRenderer::SetViewMatrix(const glm::mat4& view) {
    view_matrix_ = view;
}

void MeshRenderer::SetProjectionMatrix(const glm::mat4& projection) {
    projection_matrix_ = projection;
}

void MeshRenderer::Render() {
    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (meshes_.empty() || shader_program_ == 0) {
        return;
    }
    
    // Use shader program
    glUseProgram(shader_program_);
    
    // Set matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection_matrix_ * view_matrix_ * model;
    
    if (mvp_location_ != -1) {
        glUniformMatrix4fv(mvp_location_, 1, GL_FALSE, glm::value_ptr(mvp));
    }
    if (model_location_ != -1) {
        glUniformMatrix4fv(model_location_, 1, GL_FALSE, glm::value_ptr(model));
    }
    if (view_location_ != -1) {
        glUniformMatrix4fv(view_location_, 1, GL_FALSE, glm::value_ptr(view_matrix_));
    }
    if (projection_location_ != -1) {
        glUniformMatrix4fv(projection_location_, 1, GL_FALSE, glm::value_ptr(projection_matrix_));
    }
    
    // Set lighting
    if (light_dir_location_ != -1) {
        glm::vec3 light_dir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
        glUniform3f(light_dir_location_, light_dir.x, light_dir.y, light_dir.z);
    }
    if (ambient_location_ != -1) {
        glUniform3f(ambient_location_, 0.3f, 0.3f, 0.3f);
    }
    
    // Render each mesh
    for (const auto& [mesh_id, mesh_data] : meshes_) {
        if (!mesh_data.visible || mesh_data.vao == 0) {
            continue;
        }
        
        // Set mesh color
        if (color_location_ != -1) {
            glUniform4f(color_location_, mesh_data.color.r, mesh_data.color.g, 
                       mesh_data.color.b, mesh_data.color.a);
        }
        
        // Render mesh
        glBindVertexArray(mesh_data.vao);
        glDrawElements(GL_TRIANGLES, mesh_data.index_count, GL_UNSIGNED_INT, 0);
    }
    
    // Cleanup
    glBindVertexArray(0);
    glUseProgram(0);
}

int MeshRenderer::GetTriangleCount() const {
    int triangle_count = 0;
    for (const auto& [mesh_id, mesh_data] : meshes_) {
        triangle_count += mesh_data.index_count / 3;
    }
    return triangle_count;
}

// Camera implementation
glm::mat4 MeshRenderer::Camera::GetViewMatrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 MeshRenderer::Camera::GetProjectionMatrix(float aspect_ratio) const {
    return glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
}

void MeshRenderer::Camera::RotateAroundTarget(float delta_yaw, float delta_pitch) {
    glm::vec3 dir = position - target;
    float radius = glm::length(dir);
    
    // Convert to spherical coordinates
    float theta = atan2(dir.x, dir.y);  // Yaw
    float phi = acos(dir.z / radius);    // Pitch
    
    // Apply deltas
    theta += delta_yaw;
    phi += delta_pitch;
    
    // Clamp pitch to avoid gimbal lock
    phi = glm::clamp(phi, 0.1f, glm::pi<float>() - 0.1f);
    
    // Convert back to Cartesian
    position.x = target.x + radius * sin(phi) * sin(theta);
    position.y = target.y + radius * sin(phi) * cos(theta);
    position.z = target.z + radius * cos(phi);
}

void MeshRenderer::Camera::Zoom(float delta) {
    glm::vec3 dir = glm::normalize(position - target);
    float distance = glm::length(position - target);
    distance += delta;
    distance = glm::max(distance, 1.0f);  // Minimum distance
    position = target + dir * distance;
}

void MeshRenderer::Camera::Pan(float delta_x, float delta_y) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 camera_up = glm::cross(right, forward);
    
    glm::vec3 offset = right * delta_x + camera_up * delta_y;
    position += offset;
    target += offset;
}

// Private helper methods
bool MeshRenderer::CreateShaders() {
    // Create vertex shader
    vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
    if (!CompileShader(vertex_shader_, GetVertexShaderSource())) {
        return false;
    }
    
    // Create fragment shader
    fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
    if (!CompileShader(fragment_shader_, GetFragmentShaderSource())) {
        return false;
    }
    
    // Create and link program
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertex_shader_);
    glAttachShader(shader_program_, fragment_shader_);
    
    if (!LinkProgram()) {
        return false;
    }
    
    // Get uniform locations
    mvp_location_ = glGetUniformLocation(shader_program_, "u_MVP");
    model_location_ = glGetUniformLocation(shader_program_, "u_Model");
    view_location_ = glGetUniformLocation(shader_program_, "u_View");
    projection_location_ = glGetUniformLocation(shader_program_, "u_Projection");
    color_location_ = glGetUniformLocation(shader_program_, "u_Color");
    light_dir_location_ = glGetUniformLocation(shader_program_, "u_LightDir");
    ambient_location_ = glGetUniformLocation(shader_program_, "u_Ambient");
    
    return true;
}

bool MeshRenderer::CompileShader(GLuint shader, const char* source) {
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        spdlog::error("MeshRenderer: Shader compilation failed: {}", info_log);
        return false;
    }
    
    return true;
}

bool MeshRenderer::LinkProgram() {
    glLinkProgram(shader_program_);
    
    GLint success;
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(shader_program_, 512, nullptr, info_log);
        spdlog::error("MeshRenderer: Program linking failed: {}", info_log);
        return false;
    }
    
    return true;
}

void MeshRenderer::DestroyShaders() {
    if (shader_program_ != 0) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
    if (vertex_shader_ != 0) {
        glDeleteShader(vertex_shader_);
        vertex_shader_ = 0;
    }
    if (fragment_shader_ != 0) {
        glDeleteShader(fragment_shader_);
        fragment_shader_ = 0;
    }
}

void MeshRenderer::DestroyMesh(MeshData& mesh) {
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.vao = 0;
    }
    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vbo = 0;
    }
    if (mesh.ebo != 0) {
        glDeleteBuffers(1, &mesh.ebo);
        mesh.ebo = 0;
    }
    if (mesh.normal_vbo != 0) {
        glDeleteBuffers(1, &mesh.normal_vbo);
        mesh.normal_vbo = 0;
    }
}

const char* MeshRenderer::GetVertexShaderSource() {
    return R"(
        #version 330 core
        
        layout (location = 0) in vec3 a_Position;
        layout (location = 1) in vec3 a_Normal;
        
        uniform mat4 u_MVP;
        uniform mat4 u_Model;
        uniform mat4 u_View;
        uniform mat4 u_Projection;
        
        out vec3 v_FragPos;
        out vec3 v_Normal;
        
        void main() {
            v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
            v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
            
            gl_Position = u_MVP * vec4(a_Position, 1.0);
        }
    )";
}

const char* MeshRenderer::GetFragmentShaderSource() {
    return R"(
        #version 330 core
        
        in vec3 v_FragPos;
        in vec3 v_Normal;
        
        out vec4 FragColor;
        
        uniform vec4 u_Color;
        uniform vec3 u_LightDir;
        uniform vec3 u_Ambient;
        
        void main() {
            // Normalize normal
            vec3 norm = normalize(v_Normal);
            
            // Calculate diffuse lighting
            float diff = max(dot(norm, normalize(u_LightDir)), 0.0);
            vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
            
            // Combine lighting
            vec3 result = (u_Ambient + diffuse) * u_Color.rgb;
            FragColor = vec4(result, u_Color.a);
        }
    )";
}