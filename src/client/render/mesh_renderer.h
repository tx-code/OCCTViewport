#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// OpenGL includes
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

// GLM for math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Forward declarations
struct MeshData;

class MeshRenderer {
public:
    MeshRenderer();
    ~MeshRenderer();

    // Initialize OpenGL resources
    bool Initialize();
    void Shutdown();

    // Mesh management
    void AddMesh(const std::string& mesh_id, const std::vector<float>& vertices,
                const std::vector<float>& normals, const std::vector<int>& indices,
                const float color[4]);
    void UpdateMesh(const std::string& mesh_id, const std::vector<float>& vertices,
                   const std::vector<float>& normals, const std::vector<int>& indices,
                   const float color[4]);
    void RemoveMesh(const std::string& mesh_id);
    void ClearAllMeshes();

    // Rendering
    void SetViewMatrix(const glm::mat4& view);
    void SetProjectionMatrix(const glm::mat4& projection);
    void Render();

    // Camera utilities
    struct Camera {
        glm::vec3 position{0.0f, 0.0f, 100.0f};
        glm::vec3 target{0.0f, 0.0f, 0.0f};
        glm::vec3 up{0.0f, 0.0f, 1.0f};
        float fov{45.0f};
        float near_plane{1.0f};
        float far_plane{1000.0f};
        
        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix(float aspect_ratio) const;
        
        // Camera controls
        void RotateAroundTarget(float delta_yaw, float delta_pitch);
        void Zoom(float delta);
        void Pan(float delta_x, float delta_y);
    };

    Camera& GetCamera() { return camera_; }
    const Camera& GetCamera() const { return camera_; }

    // Statistics
    int GetMeshCount() const { return static_cast<int>(meshes_.size()); }
    int GetTriangleCount() const;

private:
    struct MeshData {
        GLuint vao{0};
        GLuint vbo{0};
        GLuint ebo{0};
        GLuint normal_vbo{0};
        int index_count{0};
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        bool visible{true};
    };

    // OpenGL resources
    GLuint shader_program_{0};
    GLuint vertex_shader_{0};
    GLuint fragment_shader_{0};
    
    // Uniform locations
    GLint mvp_location_{-1};
    GLint model_location_{-1};
    GLint view_location_{-1};
    GLint projection_location_{-1};
    GLint color_location_{-1};
    GLint light_dir_location_{-1};
    GLint ambient_location_{-1};

    // Render data
    std::unordered_map<std::string, MeshData> meshes_;
    glm::mat4 view_matrix_{1.0f};
    glm::mat4 projection_matrix_{1.0f};
    Camera camera_;

    // Helper methods
    bool CreateShaders();
    bool CompileShader(GLuint shader, const char* source);
    bool LinkProgram();
    void DestroyShaders();
    void DestroyMesh(MeshData& mesh);
    
    // Default shader sources
    static const char* GetVertexShaderSource();
    static const char* GetFragmentShaderSource();
};