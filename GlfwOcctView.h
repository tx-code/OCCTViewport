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

#ifndef _GlfwOcctView_Header
#define _GlfwOcctView_Header

#include <AIS_InteractiveObject.hxx>
#include <AIS_ViewController.hxx> // Base class
#include <Quantity_Color.hxx>     // For Quantity_Color
#include <gp_Pnt.hxx>
#include <memory> // For std::unique_ptr

// Forward declarations
struct GLFWwindow;
class Prs3d_Drawer;
class AIS_Shape;
class AIS_InteractiveObject;

// Using protected inheritance because:
// Public members from AIS_ViewController become protected in GlfwOcctView
// Prevents external code from directly accessing AIS_ViewController's interface
// GlfwOcctView can still use these features internally
class GlfwOcctView : protected AIS_ViewController {
public:
  //! Constructor now takes an existing GLFWwindow.
  GlfwOcctView(GLFWwindow *aGlfwWindow);

  //! Destructor.
  ~GlfwOcctView(); // Definition must be in .cpp where ViewInternal is complete

  //! Main application entry point.
  void run();

  //! Add an ais object to the ais context.
  void addAisObject(const Handle(AIS_InteractiveObject) & theAisObject);

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
  static GlfwOcctView *toView(GLFWwindow *theWin);

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

private:
  struct ViewInternal;
  std::unique_ptr<ViewInternal> internal_;
};

#endif // _GlfwOcctView_Header
