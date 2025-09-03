// MIT License
//
// Copyright(c) 2023 Shing Liu
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef _OcctRenderClient_Header
#define _OcctRenderClient_Header

#include <AIS_InteractiveObject.hxx>
#include <AIS_ViewController.hxx> // Base class
#include <AIS_Triangulation.hxx>
#include <Quantity_Color.hxx>     // For Quantity_Color
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Poly_Triangulation.hxx>
#include <Poly_Triangle.hxx>
#include <Standard_Failure.hxx>
#include <memory> // For std::unique_ptr
#include <iostream> // For std::cerr

// Forward declarations
struct GLFWwindow;
class Prs3d_Drawer;
class AIS_Shape;
class AIS_InteractiveObject;

class GeometryClient;

// Using protected inheritance because:
// Public members from AIS_ViewController become protected in OcctRenderClient
// Prevents external code from directly accessing AIS_ViewController's interface
// OcctRenderClient can still use these features internally
class OcctRenderClient : protected AIS_ViewController {
public:
  //! Constructor now takes an existing GLFWwindow.
  OcctRenderClient(GLFWwindow *aGlfwWindow);

  //! Destructor.
  ~OcctRenderClient(); // Definition must be in .cpp where ViewInternal is complete

  //! Main application entry point.
  void run();

  //! Add an ais object to the ais context.
  void addAisObject(const Handle(AIS_InteractiveObject) & theAisObject);

  //! Connect to geometry server and initialize gRPC client (synchronous)
  bool initGeometryClient();
  
  //! Start async connection to geometry server
  void startAsyncConnection();

  //! Safely disconnect gRPC client
  void shutdownGeometryClient();

  //! Create geometry via gRPC and add to viewer
  void createRandomBox();
  void createRandomCone();
  void createDemoScene();
  void refreshMeshes();
  void clearAllShapes();

protected:
  //! Initialize OCCT Rendering System.
  void initOCCTRenderingSystem();

  //! Init ImGui.
  void initGui();

  //! Render ImGUI.
  void renderGui();

  //! Fill 3D Viewer with a DEMO items.
  void initDemoScene();

  //! Initialize OCCT 3D Viewer.
  void initV3dViewer();

  //! Initialize OCCT AIS Context.
  void initAisContext();

  //! Initialize OCCT Visual Settings.
  void initVisualSettings();

  //! Initialize off-screen rendering.
  void initOffscreenRendering();

  //! Resize off-screen framebuffer if needed.
  bool resizeOffscreenFramebuffer(int theWidth, int theHeight);

  //! Application event loop.
  void mainloop();

  //! Clean up before .
  void cleanup();

  //! Adjust mouse position based on current viewport
  Graphic3d_Vec2i adjustMousePosition(int thePosX, int thePosY) const;

  //! @name GLWF callbacks
private:
  //! Window resize event.
  void onResize(int theWidth, int theHeight);

  //! Mouse scroll event.
  void onMouseScroll(double theOffsetX, double theOffsetY);

  //! Mouse click event.
  void onMouseButton(int theButton, int theAction, int theMods);

  //! Mouse move event.
  void onMouseMove(int thePosX, int thePosY);

  //! @name GLWF callbacks (static functions)
private:
  //! GLFW callback redirecting messages into Message::DefaultMessenger().
  static void errorCallback(int theError, const char *theDescription);

  //! Wrapper for glfwGetWindowUserPointer() returning this class instance.
  static OcctRenderClient *toView(GLFWwindow *theWin);

  //! Window resize callback.
  static void onResizeCallback(GLFWwindow *theWin, int theWidth, int theHeight);

  //! Frame-buffer resize callback.
  static void onFBResizeCallback(GLFWwindow *theWin, int theWidth,
                                 int theHeight);

  //! Mouse scroll callback.
  static void onMouseScrollCallback(GLFWwindow *theWin, double theOffsetX,
                                    double theOffsetY);

  //! Mouse click callback.
  static void onMouseButtonCallback(GLFWwindow *theWin, int theButton,
                                    int theAction, int theMods);

  //! Mouse move callback.
  static void onMouseMoveCallback(GLFWwindow *theWin, double thePosX,
                                  double thePosY);

  // Helper functions
private:
  gp_Pnt screenToViewCoordinates(int theX, int theY) const;

  //! Configure the highlight style for the given drawer
  void configureHighlightStyle(const Handle(Prs3d_Drawer) & theDrawer);

  //! Get the default AIS drawer for nice shape display (shaded with edges)
  Handle(Prs3d_Drawer) getDefaultAISDrawer();

  //! Convert mesh data to AIS_Triangulation and display it
  template<typename MeshDataType>
  void addMeshAsAisShape(const MeshDataType& mesh_data);

private:
  struct ViewInternal;
  std::unique_ptr<ViewInternal> internal_;
};

// Template method implementation
template<typename MeshDataType>
void OcctRenderClient::addMeshAsAisShape(const MeshDataType& mesh_data) {
  if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): Empty mesh data" << std::endl;
    return;
  }
  
  // Validate mesh data integrity
  if (mesh_data.vertices.size() % 3 != 0) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): Invalid vertices size: " << mesh_data.vertices.size() << std::endl;
    return;
  }
  
  if (mesh_data.indices.size() % 3 != 0) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): Invalid indices size: " << mesh_data.indices.size() << std::endl;
    return;
  }
  
  const int numVertices = static_cast<int>(mesh_data.vertices.size() / 3);
  const int numTriangles = static_cast<int>(mesh_data.indices.size() / 3);
  
  // Check for index bounds
  for (size_t i = 0; i < mesh_data.indices.size(); ++i) {
    if (mesh_data.indices[i] >= numVertices || mesh_data.indices[i] < 0) {
      std::cerr << "OcctRenderClient::addMeshAsAisShape(): Index out of bounds: " << mesh_data.indices[i] 
                << " (max: " << numVertices - 1 << ")" << std::endl;
      return;
    }
  }
  
  try {
    // Create triangulation from mesh data with validation
    Handle(Poly_Triangulation) triangulation = new Poly_Triangulation(
        numVertices, numTriangles, Standard_True);
    
    // Add vertices with bounds checking
    for (int v = 0; v < numVertices; ++v) {
      int idx = v * 3;
      if (idx + 2 < static_cast<int>(mesh_data.vertices.size())) {
        gp_Pnt point(mesh_data.vertices[idx], mesh_data.vertices[idx+1], mesh_data.vertices[idx+2]);
        triangulation->SetNode(v + 1, point); // OCCT uses 1-based indexing
      }
    }
    
    // Add normals if available with bounds checking
    if (mesh_data.normals.size() == mesh_data.vertices.size()) {
      for (int v = 0; v < numVertices; ++v) {
        int idx = v * 3;
        if (idx + 2 < static_cast<int>(mesh_data.normals.size())) {
          try {
            gp_Dir normal(mesh_data.normals[idx], mesh_data.normals[idx+1], mesh_data.normals[idx+2]);
            triangulation->SetNormal(v + 1, normal); // OCCT uses 1-based indexing
          } catch (...) {
            // Skip invalid normals
          }
        }
      }
    }
    
    // Add triangles with bounds checking
    for (int t = 0; t < numTriangles; ++t) {
      int idx = t * 3;
      if (idx + 2 < static_cast<int>(mesh_data.indices.size())) {
        // Convert from 0-based to 1-based indexing for OCCT
        int v1 = mesh_data.indices[idx] + 1;
        int v2 = mesh_data.indices[idx + 1] + 1;
        int v3 = mesh_data.indices[idx + 2] + 1;
        
        // Ensure indices are within valid range
        if (v1 > 0 && v1 <= numVertices && 
            v2 > 0 && v2 <= numVertices && 
            v3 > 0 && v3 <= numVertices) {
          Poly_Triangle triangle(v1, v2, v3);
          triangulation->SetTriangle(t + 1, triangle); // OCCT uses 1-based indexing
        }
      }
    }
    
    // Create AIS_Triangulation object
    Handle(AIS_Triangulation) ais_triangulation = new AIS_Triangulation(triangulation);
    
    // Set color with bounds checking
    if (sizeof(mesh_data.color) >= 4 * sizeof(float)) {
      Quantity_Color color(mesh_data.color[0], mesh_data.color[1], mesh_data.color[2], Quantity_TOC_RGB);
      ais_triangulation->SetColor(color);
      ais_triangulation->SetTransparency(1.0f - mesh_data.color[3]);
    }
    
    // Display the triangulation
    addAisObject(ais_triangulation);
    
  } catch (const std::exception& e) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): Standard exception: " << e.what() << std::endl;
  } catch (const Standard_Failure& e) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): OCCT exception: " << e.GetMessageString() << std::endl;
  } catch (...) {
    std::cerr << "OcctRenderClient::addMeshAsAisShape(): Unknown exception caught" << std::endl;
  }
}

#endif // _OcctRenderClient_Header
