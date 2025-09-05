// MIT License
//
// Copyright(c) 2025 Xing Tang <tang.xing1@outlook.com>
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
#include <AIS_Shape.hxx>         // For AIS_Shape (recommended over AIS_Triangulation)
#include <Quantity_Color.hxx>     // For Quantity_Color
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Poly_Triangulation.hxx>
#include <Poly_Triangle.hxx>
#include <Standard_Failure.hxx>
#include <Bnd_Box.hxx>           // For bounding box calculation
#include <Prs3d_Drawer.hxx>      // For presentation attributes
#include <Prs3d_ShadingAspect.hxx> // For shading configuration
#include <Graphic3d_AspectFillArea3d.hxx> // For fill area aspect
#include <TopoDS_Face.hxx>       // For creating TopoDS_Face from triangulation
#include <BRep_Builder.hxx>      // For building topology
#include <BRepBuilderAPI_MakeFace.hxx> // For creating faces from triangulation
#include <memory> // For std::unique_ptr
#include <spdlog/spdlog.h> // For logging
#include <nfd.h> // Native File Dialog for file selection

// Forward declarations
struct GLFWwindow;
class Prs3d_Drawer;
class AIS_Shape;
class AIS_InteractiveObject;

class GeometryClient;
class GrpcPerformancePanel;
class ConsolePanel;

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
  
  //! STEP file import
  void importStepFile();
  
  //! Import model files (STEP, IGES, STL, OBJ, VRML, etc.)
  void importModelFile();

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

  //! Override AIS_ViewController's handleViewRedraw method
  virtual void handleViewRedraw(const Handle(AIS_InteractiveContext)& theCtx,
                                const Handle(V3d_View)& theView) override;

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

  //! Content scale (DPI) change event.
  void onContentScale(float xscale, float yscale);

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

  //! Content scale (DPI) change callback.
  static void onContentScaleCallback(GLFWwindow *theWin, float xscale, float yscale);

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

// Template method implementation - Using TopoDS_Face + AIS_Shape (OCCT recommended approach)
template<typename MeshDataType>
void OcctRenderClient::addMeshAsAisShape(const MeshDataType& mesh_data) {
  if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): Empty mesh data");
    return;
  }
  
  // Validate mesh data integrity
  if (mesh_data.vertices.size() % 3 != 0) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): Invalid vertices size: {}", mesh_data.vertices.size());
    return;
  }
  
  if (mesh_data.indices.size() % 3 != 0) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): Invalid indices size: {}", mesh_data.indices.size());
    return;
  }
  
  const int numVertices = static_cast<int>(mesh_data.vertices.size() / 3);
  const int numTriangles = static_cast<int>(mesh_data.indices.size() / 3);
  
  // Check for index bounds
  for (size_t i = 0; i < mesh_data.indices.size(); ++i) {
    if (mesh_data.indices[i] >= numVertices || mesh_data.indices[i] < 0) {
      spdlog::error("OcctRenderClient::addMeshAsAisShape(): Index out of bounds: {} (max: {})", 
                   mesh_data.indices[i], numVertices - 1);
      return;
    }
  }
  
  try {
    const double scaleFactor = 10.0; // Scale up server shapes for visibility
    spdlog::debug("OcctRenderClient::addMeshAsAisShape(): Creating TopoDS_Face from triangulation with {} vertices, {} triangles ({}x scaling)", 
                  numVertices, numTriangles, scaleFactor);
    
    // Log first few vertices for debugging (original and scaled)
    if (numVertices > 0) {
      spdlog::debug("  First vertex: ({:.3f}, {:.3f}, {:.3f}) -> scaled: ({:.3f}, {:.3f}, {:.3f})", 
                    mesh_data.vertices[0], mesh_data.vertices[1], mesh_data.vertices[2],
                    mesh_data.vertices[0] * scaleFactor, mesh_data.vertices[1] * scaleFactor, mesh_data.vertices[2] * scaleFactor);
      if (numVertices > 1) {
        spdlog::debug("  Second vertex: ({:.3f}, {:.3f}, {:.3f}) -> scaled: ({:.3f}, {:.3f}, {:.3f})", 
                      mesh_data.vertices[3], mesh_data.vertices[4], mesh_data.vertices[5],
                      mesh_data.vertices[3] * scaleFactor, mesh_data.vertices[4] * scaleFactor, mesh_data.vertices[5] * scaleFactor);
      }
    }
    
    // Log first few indices for debugging
    if (numTriangles > 0) {
      spdlog::debug("  First triangle indices: [{}, {}, {}]", 
                    mesh_data.indices[0], mesh_data.indices[1], mesh_data.indices[2]);
    }
    
    // Create triangulation from mesh data with validation
    Handle(Poly_Triangulation) triangulation = new Poly_Triangulation(
        numVertices, numTriangles, Standard_True);
    
    // Add vertices with bounds checking and scaling
    for (int v = 0; v < numVertices; ++v) {
      int idx = v * 3;
      if (idx + 2 < static_cast<int>(mesh_data.vertices.size())) {
        gp_Pnt point(mesh_data.vertices[idx] * scaleFactor, 
                     mesh_data.vertices[idx+1] * scaleFactor, 
                     mesh_data.vertices[idx+2] * scaleFactor);
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
    
    // Validate triangulation creation
    if (triangulation.IsNull()) {
      spdlog::error("OcctRenderClient::addMeshAsAisShape(): Failed to create triangulation");
      return;
    }
    
    // Log triangulation bounding box
    Bnd_Box bbox;
    for (int i = 1; i <= triangulation->NbNodes(); ++i) {
      bbox.Add(triangulation->Node(i));
    }
    if (!bbox.IsVoid()) {
      double xmin, ymin, zmin, xmax, ymax, zmax;
      bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
      spdlog::debug("  Triangulation bounding box: ({:.3f},{:.3f},{:.3f}) to ({:.3f},{:.3f},{:.3f})",
                    xmin, ymin, zmin, xmax, ymax, zmax);
    }
    
    // OCCT recommended approach: Create TopoDS_Face from triangulation then use AIS_Shape
    spdlog::debug("OcctRenderClient::addMeshAsAisShape(): Creating TopoDS_Face from triangulation (OCCT recommended)");
    
    // Create a face from the triangulation using BRepBuilderAPI_MakeFace
    TopoDS_Face face;
    BRep_Builder builder;
    builder.MakeFace(face);
    builder.UpdateFace(face, triangulation);
    
    if (face.IsNull()) {
      spdlog::error("OcctRenderClient::addMeshAsAisShape(): Failed to create TopoDS_Face from triangulation");
      return;
    }
    
    // Create AIS_Shape from the face (much more flexible and efficient than AIS_Triangulation)
    Handle(AIS_Shape) ais_shape = new AIS_Shape(face);
    
    if (ais_shape.IsNull()) {
      spdlog::error("OcctRenderClient::addMeshAsAisShape(): Failed to create AIS_Shape");
      return;
    }
    
    // Set display mode to shaded
    ais_shape->SetDisplayMode(1);  // 1 = Shaded mode
    
    // Set color - use bright red for visibility testing
    if (sizeof(mesh_data.color) >= 4 * sizeof(float)) {
      // Force bright red color for debugging visibility
      Quantity_Color color(1.0, 0.0, 0.0, Quantity_TOC_RGB);  // Bright red
      ais_shape->SetColor(color);
      ais_shape->SetTransparency(0.0);  // Fully opaque
      spdlog::debug("  Set color: FORCED BRIGHT RED (1.0, 0.0, 0.0) for AIS_Shape visibility test");
      spdlog::debug("  Original color would be: ({:.3f}, {:.3f}, {:.3f}, {:.3f})", 
                    mesh_data.color[0], mesh_data.color[1], mesh_data.color[2], mesh_data.color[3]);
    }
    
    // Display the shape using AIS_Shape (should work much better than AIS_Triangulation)
    spdlog::debug("OcctRenderClient::addMeshAsAisShape(): About to add AIS_Shape to display");
    addAisObject(ais_shape);
    
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): Standard exception: {}", e.what());
  } catch (const Standard_Failure& e) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): OCCT exception: {}", e.GetMessageString());
  } catch (...) {
    spdlog::error("OcctRenderClient::addMeshAsAisShape(): Unknown exception caught");
  }
}

#endif // _OcctRenderClient_Header
