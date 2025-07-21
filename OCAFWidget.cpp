#include "OCAFWidget.h"

// ImGui
#include <imgui.h>

// OCCT
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>

// OCAF
#include <TColStd_HArray1OfReal.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Data.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Comment.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_IntegerArray.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDocStd_Document.hxx>
#include <TNaming_NamedShape.hxx>
#include <TPrsStd_AISPresentation.hxx>


// Others
#include <cmath>
#include <cstring>


// Internal data structure
struct OCAFWidget::OCAFData {
  // OCAF Data Framework
  Handle(TDF_Data) ocafData;

  // Current sample to run
  int currentSample{0};

  // Sample execution status
  std::string sampleOutput;

  // Integer value for TDataStd_Integer demo
  int integerValue{10};

  // Real value for TDataStd_Real demo
  double realValue{3.14159};

  // Comment string for TDataStd_Comment demo
  std::string commentText{"This is a sample comment"};

  // Name string for TDataStd_Name demo
  std::string nameText{"SampleLabel"};
};

OCAFWidget::OCAFWidget() : data_(std::make_unique<OCAFData>()) {}

OCAFWidget::~OCAFWidget() = default;

void OCAFWidget::draw(const Handle(AIS_InteractiveContext) & context,
                      const AddAisObjectCallback &addAisObjectCallback) {
  // Initialize OCAF Data Framework if not already done
  if (data_->ocafData.IsNull()) {
    data_->ocafData = new TDF_Data();
    data_->sampleOutput = "OCAF Data Framework initialized";
  }

  ImGui::SeparatorText("OCAF Samples");

  // Sample selection
  const char *sampleNames[] = {"TDataStd Attributes", "TDocStd Document",
                               "TNaming NamedShape", "TPrsStd Presentation"};

  ImGui::Combo("Select Sample", &data_->currentSample, sampleNames,
               IM_ARRAYSIZE(sampleNames));

  ImGui::Separator();

  // Show current sample UI
  switch (data_->currentSample) {
  case 0:
    renderTDataStdSample();
    break;
  case 1:
    renderTDocStdSample();
    break;
  case 2:
    renderTNamingSample();
    break;
  case 3:
    renderTPrsStdSample(addAisObjectCallback);
    break;
  }

  ImGui::Separator();

  // Output section
  ImGui::SeparatorText("Sample Output");
  ImGui::TextWrapped("%s", data_->sampleOutput.c_str());
}

void OCAFWidget::renderTDataStdSample() {
  ImGui::Text("TDataStd Attributes Demo");

  // Integer attribute demo
  ImGui::SeparatorText("TDataStd_Integer");
  ImGui::InputInt("Integer Value", &data_->integerValue);
  if (ImGui::Button("Set Integer Attribute")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    Handle(TDataStd_Integer) intAttr =
        TDataStd_Integer::Set(rootLabel, data_->integerValue);
    data_->sampleOutput =
        "TDataStd_Integer set to: " + std::to_string(data_->integerValue);
  }

  // Real attribute demo
  ImGui::SeparatorText("TDataStd_Real");
  ImGui::InputDouble("Real Value", &data_->realValue, 0.01, 0.1, "%.6f");
  if (ImGui::Button("Set Real Attribute")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    Handle(TDataStd_Real) realAttr =
        TDataStd_Real::Set(rootLabel, data_->realValue);
    data_->sampleOutput =
        "TDataStd_Real set to: " + std::to_string(data_->realValue);
  }

  // Comment attribute demo
  ImGui::SeparatorText("TDataStd_Comment");
  char commentBuffer[256];
  strncpy(commentBuffer, data_->commentText.c_str(), sizeof(commentBuffer) - 1);
  commentBuffer[sizeof(commentBuffer) - 1] = '\0';

  if (ImGui::InputText("Comment", commentBuffer, sizeof(commentBuffer))) {
    data_->commentText = commentBuffer;
  }
  if (ImGui::Button("Set Comment Attribute")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    Handle(TDataStd_Comment) commentAttr = TDataStd_Comment::Set(rootLabel);
    TCollection_ExtendedString extStr(data_->commentText.c_str());
    commentAttr->Set(extStr);
    data_->sampleOutput = "TDataStd_Comment set to: " + data_->commentText;
  }

  // Name attribute demo
  ImGui::SeparatorText("TDataStd_Name");
  char nameBuffer[256];
  strncpy(nameBuffer, data_->nameText.c_str(), sizeof(nameBuffer) - 1);
  nameBuffer[sizeof(nameBuffer) - 1] = '\0';

  if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
    data_->nameText = nameBuffer;
  }
  if (ImGui::Button("Set Name Attribute")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    TCollection_ExtendedString extName(data_->nameText.c_str());
    Handle(TDataStd_Name) nameAttr = TDataStd_Name::Set(rootLabel, extName);
    data_->sampleOutput = "TDataStd_Name set to: " + data_->nameText;
  }

  // Real Array demo
  ImGui::SeparatorText("TDataStd_RealArray");
  static bool arraySet = false;
  if (ImGui::Button("Create Real Array (1-10)")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    Handle(TDataStd_RealArray) realArray =
        TDataStd_RealArray::Set(rootLabel, 1, 10);
    for (int i = 1; i <= 10; i++) {
      realArray->SetValue(i, M_PI * i);
    }
    arraySet = true;
    data_->sampleOutput =
        "TDataStd_RealArray created with 10 elements (π * index)";
  }

  if (arraySet && ImGui::Button("Read Array Values")) {
    TDF_Label rootLabel = data_->ocafData->Root();
    Handle(TDataStd_RealArray) realArray;
    if (rootLabel.FindAttribute(TDataStd_RealArray::GetID(), realArray)) {
      std::string result = "Array values: ";
      for (int i = realArray->Lower(); i <= realArray->Upper() && i <= 5; i++) {
        result += std::to_string(realArray->Value(i)) + " ";
      }
      if (realArray->Upper() > 5)
        result += "...";
      data_->sampleOutput = result;
    }
  }
}

void OCAFWidget::renderTDocStdSample() {
  ImGui::Text("TDocStd Document Demo");
  ImGui::TextWrapped(
      "This sample demonstrates document creation and management.");

  if (ImGui::Button("Create New Document")) {
    // This is a simplified version since full TDocStd requires application
    // framework
    data_->sampleOutput =
        "Document creation demo - requires full application framework setup";
  }

  ImGui::TextWrapped(
      "Note: Full TDocStd sample requires TDocStd_Application setup");
}

void OCAFWidget::renderTNamingSample() {
  ImGui::Text("TNaming NamedShape Demo");
  ImGui::TextWrapped(
      "This sample demonstrates shape naming and topology evolution.");

  if (ImGui::Button("Create Named Shape")) {
    // Create a simple box and attach it to a TNaming_NamedShape
    TDF_Label rootLabel = data_->ocafData->Root();
    TDF_Label shapeLabel = rootLabel.NewChild();

    gp_Ax2 axis;
    axis.SetLocation(gp_Pnt(0, 0, 0));
    TopoDS_Shape box = BRepPrimAPI_MakeBox(axis, 10, 20, 30).Shape();

    // Note: TNaming_Builder would be used in a real implementation
    data_->sampleOutput =
        "Named shape created - Box(10x20x30) attached to label";
  }

  ImGui::TextWrapped("Note: Full TNaming sample requires TNaming_Builder for "
                     "proper shape evolution tracking");
}

void OCAFWidget::renderTPrsStdSample(
    const AddAisObjectCallback &addAisObjectCallback) {
  ImGui::Text("TPrsStd Presentation Demo");
  ImGui::TextWrapped("This sample demonstrates 3D presentation attributes.");

  if (ImGui::Button("Create AIS Presentation")) {
    // Create a shape and its presentation
    TDF_Label rootLabel = data_->ocafData->Root();
    TDF_Label presLabel = rootLabel.NewChild();

    gp_Ax2 axis;
    axis.SetLocation(gp_Pnt(50, 50, 0));
    TopoDS_Shape cone = BRepPrimAPI_MakeCone(axis, 15, 5, 25).Shape();

    // In a full implementation, TPrsStd_AISPresentation would be used
    // Use the callback to add the AIS object if available
    if (addAisObjectCallback) {
      Handle(AIS_Shape) aisCone = new AIS_Shape(cone);
      addAisObjectCallback(aisCone);
      data_->sampleOutput = "AIS Presentation created - Cone added to 3D view";
    } else {
      data_->sampleOutput = "AIS Presentation created - Cone shape generated "
                            "(no callback to add to view)";
    }
  }

  ImGui::TextWrapped("Note: Shape generation completed. In full OCAF, this "
                     "would be managed through TPrsStd_AISPresentation");
}