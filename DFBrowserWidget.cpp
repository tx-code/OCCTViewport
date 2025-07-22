#include "DFBrowserWidget.h"

// ImGui
#include <imgui.h>

// OCCT OCAF
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <TDF_AttributeIterator.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Comment.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_IntegerArray.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDataStd_TreeNode.hxx>
#include <TNaming_NamedShape.hxx>
#include <TPrsStd_AISPresentation.hxx>


// OCCT Application/Document
#include <BinDrivers.hxx>
#include <StdDrivers.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XmlDrivers.hxx>


// OCCT Others
#include <Standard.hxx>
#include <Standard_GUID.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopAbs_ShapeEnum.hxx>


// Standard library
#include <algorithm>
#include <iomanip>
#include <spdlog/spdlog.h>
#include <sstream>


// Internal data structure
struct DFBrowserWidget::BrowserData {
  // OCAF document - focus on document-centric view
  Handle(TDocStd_Document) document;

  // Tree structure
  DFBrowserNodePtr rootNode;
  DFBrowserNodePtr selectedNode;

  // UI state
  std::string searchText;
  std::vector<DFBrowserNodePtr> searchResults;
  std::string navigationPath;
  std::string dumpContent;

  // Navigation history
  std::vector<DFBrowserNodePtr> navigationHistory;
  int navigationIndex = -1;

  BrowserData() { searchText.reserve(256); }
};

DFBrowserWidget::DFBrowserWidget() : data_(std::make_unique<BrowserData>()) {}

DFBrowserWidget::~DFBrowserWidget() = default;

void DFBrowserWidget::draw(const Handle(AIS_InteractiveContext) & context,
                           const AddAisObjectCallback &addAisObjectCallback) {
  // Header with action buttons
  if (ImGui::Button("Create Document")) {
    createTestDocument();
  }

  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    updateTree();
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    data_->document.Nullify();
    updateTree();
  }

  if (data_->document.IsNull()) {
    ImGui::Separator();
    ImGui::Text("No OCAF document available");
    ImGui::Text("Click 'Create Document' to generate sample data");
    return;
  }

  // Main layout with splitters
  ImGui::Separator();
  ImGui::Separator();

  // Search bar
  renderSearchBar();

  ImGui::Separator();

  // Navigation line
  renderNavigationLine();

  ImGui::Separator();

  // Main content area - split horizontally
  ImGui::BeginChild("MainContent", ImVec2(0, -200), true);

  // Tree view (left side)
  ImGui::BeginChild("TreeView",
                    ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), true);
  ImGui::Text("OCAF Tree");
  ImGui::Separator();
  renderTreeView();
  ImGui::EndChild();

  ImGui::SameLine();

  // Property panel (right side)
  ImGui::BeginChild("PropertyPanel", ImVec2(0, 0), true);
  ImGui::Text("Properties");
  ImGui::Separator();
  renderPropertyPanel();
  ImGui::EndChild();

  ImGui::EndChild();

  // Dump view (bottom)
  ImGui::BeginChild("DumpView", ImVec2(0, 0), true);
  ImGui::Text("Dump Information");
  ImGui::Separator();
  renderDumpView();
  ImGui::EndChild();
}

void DFBrowserWidget::setDocument(const Handle(TDocStd_Document) & document) {
  data_->document = document;
  updateTree();
}

void DFBrowserWidget::updateTree() {
  spdlog::info("Updating tree...");

  data_->rootNode.reset();
  data_->selectedNode.reset();
  data_->searchResults.clear();
  data_->navigationHistory.clear();
  data_->navigationIndex = -1;

  try {
    if (!data_->document.IsNull()) {
      spdlog::info("Building tree from document");
      buildTreeFromDocument();
    }
    spdlog::info("Tree update completed successfully");
  } catch (const std::exception &e) {
    spdlog::error("Error during tree update: {}", e.what());
    data_->rootNode.reset();
  } catch (...) {
    spdlog::error("Unknown error during tree update");
    data_->rootNode.reset();
  }
}

void DFBrowserWidget::buildTreeFromDocument() {
  if (data_->document.IsNull())
    return;

  try {
    spdlog::info("Creating document tree structure");
    // Make document the root of the tree
    data_->rootNode = createDocumentNode(data_->document);
    if (!data_->rootNode) {
      spdlog::error("Failed to create document node!");
      return;
    }
    spdlog::info("Document tree created successfully with {} children",
                 data_->rootNode->children.size());
  } catch (const std::exception &e) {
    spdlog::error("Error creating document tree: {}", e.what());
    throw;
  }
}

DFBrowserNodePtr
DFBrowserWidget::createDocumentNode(const Handle(TDocStd_Document) & document) {
  if (document.IsNull()) {
    spdlog::error("createDocumentNode: document is null!");
    return nullptr;
  }

  try {
    spdlog::info("createDocumentNode: Creating document node");
    auto node = std::make_shared<DFBrowserNode>();
    node->type = DFBrowserNode::Document;

    spdlog::info("createDocumentNode: Getting root label");
    // Get document name if available
    TDF_Label rootLabel = document->GetData()->Root();
    spdlog::info("createDocumentNode: Root label obtained: {}",
                 getLabelEntry(rootLabel));

    Handle(TDataStd_Name) nameAttr;
    std::string docName = "Document";
    if (rootLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
      spdlog::info("createDocumentNode: Found name attribute");
      try {
        // Use TCollection_AsciiString constructor from ExtendedString
        TCollection_AsciiString asciiName(nameAttr->Get());
        docName = asciiName.ToCString();
        spdlog::info("createDocumentNode: Document name: {}", docName);
      } catch (...) {
        spdlog::warn("createDocumentNode: Failed to convert name attribute, "
                     "using default");
        docName = "Document";
      }
    }

    node->text = "0: " + docName;
    node->entry = "0:";
    spdlog::info("createDocumentNode: Node text set to: {}", node->text);

    // Add root label's children directly to document node (skip redundant root level)
    spdlog::info("createDocumentNode: Adding root label children directly");
    for (TDF_ChildIterator childIt(rootLabel); childIt.More(); childIt.Next()) {
      auto childNode = createLabelNode(childIt.Value(), 0);
      if (childNode) {
        childNode->parent = node;
        node->children.push_back(childNode);
        spdlog::info("createDocumentNode: Added child: {}", childNode->entry);
      }
    }
    
    // Also add root label's attributes directly to document node
    for (TDF_AttributeIterator attrIt(rootLabel); attrIt.More(); attrIt.Next()) {
      auto attrNode = createAttributeNode(attrIt.Value());
      if (attrNode) {
        attrNode->parent = node;
        node->children.push_back(attrNode);
        spdlog::info("createDocumentNode: Added root attribute: {}", attrNode->attributeType);
      }
    }

    spdlog::info("createDocumentNode: Document node creation completed");
    return node;

  } catch (const std::exception &e) {
    spdlog::error("createDocumentNode: Exception: {}", e.what());
    return nullptr;
  } catch (...) {
    spdlog::error("createDocumentNode: Unknown exception");
    return nullptr;
  }
}

DFBrowserNodePtr DFBrowserWidget::createLabelNode(const TDF_Label &label,
                                                  int depth) {
  // Safety check for recursion depth
  if (depth > 100) {
    spdlog::warn("Maximum recursion depth reached for label: {}",
                 getLabelEntry(label));
    return nullptr;
  }

  // Check if label is null or invalid
  if (label.IsNull()) {
    spdlog::warn("Null label encountered at depth {}", depth);
    return nullptr;
  }

  try {
    spdlog::info("createLabelNode: Starting at depth {}", depth);
    auto node = std::make_shared<DFBrowserNode>();
    node->type = DFBrowserNode::Label;
    node->label = label;

    spdlog::info("createLabelNode: Getting label entry");
    node->entry = getLabelEntry(label);
    spdlog::info("createLabelNode: Label entry: {} at depth {}", node->entry,
                 depth);

    // Get label name
    spdlog::info("createLabelNode: Getting label name");
    Handle(TDataStd_Name) nameAttr;
    std::string labelName;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
      spdlog::info("createLabelNode: Found name attribute");
      try {
        TCollection_AsciiString asciiName(nameAttr->Get());
        labelName = ": " + std::string(asciiName.ToCString());
        spdlog::info("createLabelNode: Label name: {}", labelName);
      } catch (...) {
        spdlog::warn("createLabelNode: Failed to convert name attribute");
        labelName = "";
      }
    }

    node->text = node->entry + labelName;
    spdlog::info("createLabelNode: Node text set to: {}", node->text);

    // Process child labels with safety checks
    spdlog::info("createLabelNode: Processing child labels");
    int childCount = 0;
    try {
      for (TDF_ChildIterator childIt(label); childIt.More(); childIt.Next()) {
        if (childCount >= 100) { // Reasonable limit for children count
          spdlog::warn("Too many children for label {}, stopping at {}",
                       node->entry, childCount);
          break;
        }

        spdlog::info("createLabelNode: Creating child {} at depth {}",
                     childCount, depth + 1);
        auto childNode = createLabelNode(childIt.Value(), depth + 1);
        if (childNode) {
          childNode->parent = node;
          node->children.push_back(childNode);
          childCount++;
          spdlog::info("createLabelNode: Child {} added successfully",
                       childCount - 1);
        } else {
          spdlog::warn("createLabelNode: Child {} creation failed", childCount);
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("createLabelNode: Error processing children: {}", e.what());
    } catch (...) {
      spdlog::error("createLabelNode: Unknown error processing children");
    }

    // Add attributes
    spdlog::info("createLabelNode: Processing attributes");
    int attrCount = 0;
    for (TDF_AttributeIterator attrIt(label); attrIt.More(); attrIt.Next()) {
      if (attrCount >= 100) { // Limit attribute count
        spdlog::warn("Too many attributes for label {}, stopping at {}",
                     node->entry, attrCount);
        break;
      }

      spdlog::info("createLabelNode: Creating attribute {}", attrCount);
      auto attrNode = createAttributeNode(attrIt.Value());
      if (attrNode) {
        attrNode->parent = node;
        node->children.push_back(attrNode);
        attrCount++;
        spdlog::info("createLabelNode: Attribute {} added successfully",
                     attrCount - 1);
      } else {
        spdlog::warn("createLabelNode: Attribute {} creation failed",
                     attrCount);
      }
    }

    spdlog::info("createLabelNode: Label node created: {} with {} children and "
                 "{} attributes",
                 node->entry, childCount, attrCount);

    return node;
  } catch (const std::exception &e) {
    spdlog::error("Error creating label node at depth {}: {}", depth, e.what());
    return nullptr;
  }
}

DFBrowserNodePtr
DFBrowserWidget::createAttributeNode(const Handle(TDF_Attribute) & attribute) {
  if (attribute.IsNull()) {
    spdlog::warn("Null attribute encountered");
    return nullptr;
  }

  try {
    auto node = std::make_shared<DFBrowserNode>();
    node->type = DFBrowserNode::Attribute;
    node->attribute = attribute;
    node->attributeType = attribute->DynamicType()->Name();

    std::string attrInfo = getAttributeInfo(attribute);
    node->text = node->attributeType + attrInfo;

    return node;
  } catch (const std::exception &e) {
    spdlog::error("Error creating attribute node: {}", e.what());
    return nullptr;
  }
}

void DFBrowserWidget::renderSearchBar() {
  ImGui::Text("Search:");
  ImGui::SameLine();

  // Search input
  char searchBuffer[256];
  strncpy(searchBuffer, data_->searchText.c_str(), sizeof(searchBuffer) - 1);
  searchBuffer[sizeof(searchBuffer) - 1] = '\0';

  if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) {
    data_->searchText = searchBuffer;
    if (!data_->searchText.empty()) {
      performSearch(data_->searchText);
    } else {
      data_->searchResults.clear();
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Search for label entries, attribute names, or values");
  }

  // Show search results count
  if (!data_->searchResults.empty()) {
    ImGui::SameLine();
    ImGui::Text("(%zu results)", data_->searchResults.size());
  }
}

void DFBrowserWidget::renderTreeView() {
  if (!data_->rootNode) {
    ImGui::Text("No data available");
    return;
  }

  renderTreeNode(data_->rootNode);
}

void DFBrowserWidget::renderNavigationLine() {
  // Back/Forward buttons
  if (ImGui::Button("<")) {
    if (data_->navigationIndex > 0) {
      data_->navigationIndex--;
      data_->selectedNode = data_->navigationHistory[data_->navigationIndex];
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Go back");
  }

  ImGui::SameLine();
  if (ImGui::Button(">")) {
    if (data_->navigationIndex < (int)data_->navigationHistory.size() - 1) {
      data_->navigationIndex++;
      data_->selectedNode = data_->navigationHistory[data_->navigationIndex];
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Go forward");
  }

  ImGui::SameLine();
  if (ImGui::Button("Update")) {
    updateTree();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Refresh tree from OCAF data");
  }

  // Navigation path
  ImGui::SameLine();
  ImGui::Text("Path: %s", data_->navigationPath.c_str());
}

void DFBrowserWidget::renderTreeNode(const DFBrowserNodePtr &node) {
  if (!node)
    return;

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (node->children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf;
  }

  if (node->isSelected || node == data_->selectedNode) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  // Different colors for different node types
  ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White default
  switch (node->type) {
  case DFBrowserNode::Document:
    textColor = ImVec4(0.8f, 1.0f, 0.8f, 1.0f); // Light green
    break;
  case DFBrowserNode::Label:
    textColor = ImVec4(1.0f, 1.0f, 0.8f, 1.0f); // Light yellow
    break;
  case DFBrowserNode::Attribute:
    textColor = ImVec4(1.0f, 0.8f, 0.8f, 1.0f); // Light red
    break;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, textColor);

  bool isOpen = ImGui::TreeNodeEx(node.get(), flags, "%s", node->text.c_str());

  ImGui::PopStyleColor();

  // Handle selection
  if (ImGui::IsItemClicked()) {
    handleNodeSelection(node);
  }

  if (isOpen) {
    for (const auto &child : node->children) {
      if (child) { // Safety check for null children
        renderTreeNode(child);
      }
    }
    ImGui::TreePop();
  }
}

void DFBrowserWidget::handleNodeSelection(const DFBrowserNodePtr &node) {
  data_->selectedNode = node;

  // Update navigation history
  if (data_->navigationHistory.empty() ||
      data_->navigationHistory.back() != node) {

    // Remove forward history if we're not at the end
    if (data_->navigationIndex >= 0 &&
        data_->navigationIndex < (int)data_->navigationHistory.size() - 1) {
      data_->navigationHistory.erase(data_->navigationHistory.begin() +
                                         data_->navigationIndex + 1,
                                     data_->navigationHistory.end());
    }

    data_->navigationHistory.push_back(node);
    data_->navigationIndex = (int)data_->navigationHistory.size() - 1;
  }

  // Update navigation path
  std::string path;
  auto current = node;
  std::vector<std::string> pathElements;

  while (current) {
    if (!current->entry.empty()) {
      pathElements.push_back(current->entry);
    } else if (!current->text.empty()) {
      pathElements.push_back(current->text);
    }
    current = current->parent;
  }

  // Reverse and join
  for (int i = (int)pathElements.size() - 1; i >= 0; i--) {
    if (!path.empty())
      path += " > ";
    path += pathElements[i];
  }

  data_->navigationPath = path;

  // Update dump content
  data_->dumpContent.clear();
  if (node->type == DFBrowserNode::Label && !node->label.IsNull()) {
    // Generate dump for label
    std::ostringstream oss;
    oss << "Label Entry: " << getLabelEntry(node->label) << "\n";
    oss << "Number of Attributes: " << node->label.NbAttributes() << "\n";
    oss << "Number of Children: " << node->label.NbChildren() << "\n";
    data_->dumpContent = oss.str();
  } else if (node->type == DFBrowserNode::Attribute &&
             !node->attribute.IsNull()) {
    // Generate dump for attribute
    std::ostringstream oss;
    oss << "Attribute Type: " << node->attribute->DynamicType()->Name() << "\n";
    oss << "Value: " << getAttributeValue(node->attribute) << "\n";
    data_->dumpContent = oss.str();
  }
}

void DFBrowserWidget::renderPropertyPanel() {
  if (!data_->selectedNode) {
    ImGui::Text("No item selected");
    return;
  }

  const auto &node = data_->selectedNode;

  switch (node->type) {

  case DFBrowserNode::Document:
    ImGui::Text("Document Properties");
    ImGui::Text("Entry: %s", node->entry.c_str());
    break;

  case DFBrowserNode::Label:
    if (!node->label.IsNull()) {
      renderLabelProperties(node->label);
    }
    break;

  case DFBrowserNode::Attribute:
    renderAttributeProperties(node->attribute);
    break;
  }
}

void DFBrowserWidget::renderLabelProperties(const TDF_Label &label) {
  if (label.IsNull())
    return;

  ImGui::Text("Label Properties");
  ImGui::Separator();

  if (ImGui::BeginTable("LabelProps", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Entry");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", getLabelEntry(label).c_str());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Attributes");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", label.NbAttributes());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Children");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", label.NbChildren());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Has Attributes");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", (label.NbAttributes() > 0) ? "Yes" : "No");

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Is Root");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", label.IsRoot() ? "Yes" : "No");

    // Show name if available
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
      try {
        TCollection_AsciiString asciiName(nameAttr->Get());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", asciiName.ToCString());
      } catch (...) {
        // Ignore conversion errors
      }
    }

    ImGui::EndTable();
  }
}

void DFBrowserWidget::renderAttributeProperties(const Handle(TDF_Attribute) &
                                                attribute) {
  if (attribute.IsNull())
    return;

  ImGui::Text("Attribute Properties");
  ImGui::Separator();

  if (ImGui::BeginTable("AttrProps", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Type");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", attribute->DynamicType()->Name());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Value");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", getAttributeValue(attribute).c_str());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("ID");
    ImGui::TableSetColumnIndex(1);
    Standard_GUID id = attribute->ID();
    char guidBuffer[40];
    id.ToCString(guidBuffer);
    ImGui::Text("%s", guidBuffer);

    ImGui::EndTable();
  }
}

void DFBrowserWidget::renderDumpView() {
  if (data_->dumpContent.empty()) {
    ImGui::Text("No dump information available");
    return;
  }

  ImGui::TextWrapped("%s", data_->dumpContent.c_str());
}

std::string DFBrowserWidget::getLabelEntry(const TDF_Label &label) const {
  TCollection_AsciiString entry;
  TDF_Tool::Entry(label, entry);
  return entry.ToCString();
}

std::string DFBrowserWidget::getAttributeInfo(const Handle(TDF_Attribute) &
                                              attribute) const {
  std::string info = getAttributeValue(attribute);
  if (!info.empty()) {
    return " [" + info + "]";
  }
  return "";
}

std::string DFBrowserWidget::getAttributeValue(const Handle(TDF_Attribute) &
                                               attribute) const {
  if (attribute.IsNull())
    return "";

  // Handle different attribute types
  if (attribute->IsKind(STANDARD_TYPE(TDataStd_Name))) {
    Handle(TDataStd_Name) nameAttr = Handle(TDataStd_Name)::DownCast(attribute);
    try {
      TCollection_AsciiString asciiStr(nameAttr->Get());
      return asciiStr.ToCString();
    } catch (...) {
      return "Name conversion error";
    }
  } else if (attribute->IsKind(STANDARD_TYPE(TDataStd_Integer))) {
    Handle(TDataStd_Integer) intAttr =
        Handle(TDataStd_Integer)::DownCast(attribute);
    return std::to_string(intAttr->Get());
  } else if (attribute->IsKind(STANDARD_TYPE(TDataStd_Real))) {
    Handle(TDataStd_Real) realAttr = Handle(TDataStd_Real)::DownCast(attribute);
    return std::to_string(realAttr->Get());
  } else if (attribute->IsKind(STANDARD_TYPE(TDataStd_Comment))) {
    Handle(TDataStd_Comment) commentAttr =
        Handle(TDataStd_Comment)::DownCast(attribute);
    try {
      TCollection_AsciiString asciiStr(commentAttr->Get());
      return asciiStr.ToCString();
    } catch (...) {
      return "Comment conversion error";
    }
  } else if (attribute->IsKind(STANDARD_TYPE(TNaming_NamedShape))) {
    Handle(TNaming_NamedShape) shapeAttr =
        Handle(TNaming_NamedShape)::DownCast(attribute);
    if (!shapeAttr->Get().IsNull()) {
      return "Shape: " +
             std::string(shapeAttr->Get().ShapeType() == TopAbs_SOLID  ? "Solid"
                         : shapeAttr->Get().ShapeType() == TopAbs_FACE ? "Face"
                         : shapeAttr->Get().ShapeType() == TopAbs_EDGE ? "Edge"
                         : shapeAttr->Get().ShapeType() == TopAbs_VERTEX
                             ? "Vertex"
                             : "Other");
    }
  }

  return "";
}

void DFBrowserWidget::performSearch(const std::string &searchText) {
  data_->searchResults.clear();

  if (!data_->rootNode || searchText.empty())
    return;

  // Simple recursive search function
  std::function<void(const DFBrowserNodePtr &)> searchNode =
      [&](const DFBrowserNodePtr &node) {
        if (!node)
          return;

        // Search in text, entry, and attribute type
        std::string lowerSearchText = searchText;
        std::transform(lowerSearchText.begin(), lowerSearchText.end(),
                       lowerSearchText.begin(), ::tolower);

        std::string lowerText = node->text;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                       ::tolower);

        std::string lowerEntry = node->entry;
        std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
                       ::tolower);

        std::string lowerAttrType = node->attributeType;
        std::transform(lowerAttrType.begin(), lowerAttrType.end(),
                       lowerAttrType.begin(), ::tolower);

        if (lowerText.find(lowerSearchText) != std::string::npos ||
            lowerEntry.find(lowerSearchText) != std::string::npos ||
            lowerAttrType.find(lowerSearchText) != std::string::npos) {
          data_->searchResults.push_back(node);
        }

        // Search children
        for (const auto &child : node->children) {
          searchNode(child);
        }
      };

  searchNode(data_->rootNode);
}

void DFBrowserWidget::highlightSearchResults(
    const std::vector<DFBrowserNodePtr> &results) {
  // Clear previous highlights
  std::function<void(const DFBrowserNodePtr &)> clearHighlights =
      [&](const DFBrowserNodePtr &node) {
        if (node) {
          node->isSelected = false;
          for (const auto &child : node->children) {
            clearHighlights(child);
          }
        }
      };

  if (data_->rootNode) {
    clearHighlights(data_->rootNode);
  }

  // Set new highlights
  for (const auto &result : results) {
    if (result) {
      result->isSelected = true;
    }
  }
}

void DFBrowserWidget::createTestDocument() {
  spdlog::info("Creating simple test document");

  try {
    // Create a new document using standard OCAF format
    data_->document = new TDocStd_Document("BinOcaf");

    // Get the root label of the document
    TDF_Label rootLabel = data_->document->GetData()->Root();
    spdlog::info("Root label created: {}", getLabelEntry(rootLabel));

    // Create simple test data to avoid recursion issues
    TDF_Label mainLabel = rootLabel.NewChild();
    TDataStd_Name::Set(mainLabel, "OCAF Demo Document");
    TDataStd_Comment::Set(mainLabel, "Simple OCAF demonstration");
    spdlog::info("Main label created: {}", getLabelEntry(mainLabel));

    // Create minimal test structure
    TDF_Label basicLabel = mainLabel.NewChild();
    TDataStd_Name::Set(basicLabel, "Basic Attributes");
    TDataStd_Integer::Set(basicLabel, 42);
    TDataStd_Real::Set(basicLabel, 3.14159);
    spdlog::info("Basic label created: {}", getLabelEntry(basicLabel));

    TDF_Label simpleLabel = mainLabel.NewChild();
    TDataStd_Name::Set(simpleLabel, "Simple Object");
    TDataStd_Comment::Set(simpleLabel, "A simple test object");
    spdlog::info("Simple label created: {}", getLabelEntry(simpleLabel));

    spdlog::info("Calling updateTree()");
    updateTree();
    spdlog::info("Test document created successfully");

  } catch (const std::exception &e) {
    spdlog::error("Error creating test document: {}", e.what());
    data_->document.Nullify();
  } catch (...) {
    spdlog::error("Unknown error creating test document");
    data_->document.Nullify();
  }
}
