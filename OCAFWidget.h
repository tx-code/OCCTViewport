#pragma once

#include <functional>
#include <memory>
#include <string>


// OCCT includes for Handle
#include <Standard_Handle.hxx>

// Forward declarations
class AIS_InteractiveContext;
class AIS_InteractiveObject;

class OCAFWidget {
public:
  OCAFWidget();
  ~OCAFWidget();

  // Callback type for adding AIS objects to the scene
  using AddAisObjectCallback =
      std::function<void(const Handle(AIS_InteractiveObject) &)>;

  // Main draw function for ImGui
  void draw(const Handle(AIS_InteractiveContext) & context,
            const AddAisObjectCallback &addAisObjectCallback = nullptr);

private:
  struct OCAFData;
  std::unique_ptr<OCAFData> data_;

  // Sample rendering methods
  void renderTDataStdSample();
  void renderTDocStdSample();
  void renderTNamingSample();
  void renderTPrsStdSample(const AddAisObjectCallback &addAisObjectCallback);
};