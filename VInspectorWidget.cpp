#include "VInspectorWidget.h"

// ImGui
#include <imgui.h>

// OCCT AIS
#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <AIS_Shape.hxx>

// OCCT Selection
#include <SelectMgr_Selection.hxx>
#include <SelectMgr_SensitiveEntity.hxx>
#include <SelectMgr_EntityOwner.hxx>

// OCCT Collections
#include <TCollection_AsciiString.hxx>

// Standard
#include <sstream>
#include <iomanip>

// Logging
#include <spdlog/spdlog.h>

//! Private implementation data
struct VInspectorWidget::InspectorData {
    //! Current AIS context
    Handle(AIS_InteractiveContext) context;
    
    //! Root tree node
    VInspectorNodePtr rootNode;
    
    //! Selected node for properties panel
    VInspectorNodePtr selectedNode;
    
    //! Callbacks
    ObjectActionCallback showObjectCallback;
    ObjectActionCallback hideObjectCallback;
    ObjectActionCallback selectObjectCallback;
    
    //! Search filter
    char searchFilter[256] = "";
    
    //! Tree expansion states
    std::vector<bool> expansionStates;
    
    InspectorData() = default;
};

VInspectorWidget::VInspectorWidget() 
    : data_(std::make_unique<InspectorData>()) {
    spdlog::info("VInspectorWidget: Created");
}

VInspectorWidget::~VInspectorWidget() = default;

void VInspectorWidget::draw(const Handle(AIS_InteractiveContext)& context) {
    if (context.IsNull()) {
        ImGui::Text("No AIS_InteractiveContext available");
        return;
    }
    
    // Update context if changed
    if (data_->context != context) {
        data_->context = context;
        updateTree();
    }
    
    // Header with update button
    if (ImGui::Button("Update")) {
        updateTree();
    }
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("Search", data_->searchFilter, sizeof(data_->searchFilter));
    
    ImGui::Separator();
    
    // Main content area with splitter
    if (ImGui::BeginTable("VInspectorContent", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        
        ImGui::TableNextRow();
        
        // Left panel - Tree view
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("TreeView", ImVec2(0, 0), true);
        renderTree();
        ImGui::EndChild();
        
        // Right panel - Properties
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("Properties", ImVec2(0, 0), true);
        renderPropertiesPanel();
        ImGui::EndChild();
        
        ImGui::EndTable();
    }
}

void VInspectorWidget::updateTree() {
    if (data_->context.IsNull()) {
        data_->rootNode.reset();
        return;
    }
    
    spdlog::info("VInspectorWidget: Updating tree from AIS context");
    
    try {
        buildTreeFromContext();
        spdlog::info("VInspectorWidget: Tree update completed");
    } catch (const std::exception& e) {
        spdlog::error("VInspectorWidget: Error updating tree: {}", e.what());
        data_->rootNode.reset();
    } catch (...) {
        spdlog::error("VInspectorWidget: Unknown error updating tree");
        data_->rootNode.reset();
    }
}

void VInspectorWidget::setShowObjectCallback(const ObjectActionCallback& callback) {
    data_->showObjectCallback = callback;
}

void VInspectorWidget::setHideObjectCallback(const ObjectActionCallback& callback) {
    data_->hideObjectCallback = callback;
}

void VInspectorWidget::setSelectObjectCallback(const ObjectActionCallback& callback) {
    data_->selectObjectCallback = callback;
}

void VInspectorWidget::buildTreeFromContext() {
    if (data_->context.IsNull()) return;
    
    spdlog::info("VInspectorWidget: Building tree from context");
    
    // Create root context node
    data_->rootNode = createContextNode(data_->context);
    if (!data_->rootNode) {
        spdlog::error("VInspectorWidget: Failed to create context node");
        return;
    }
    
    // Get all objects (both displayed and erased)
    AIS_ListOfInteractive allObjects;
    data_->context->ObjectsInside(allObjects);
    
    spdlog::info("VInspectorWidget: Found {} total objects", allObjects.Size());
    
    int objectRow = 0;
    for (AIS_ListOfInteractive::Iterator objIt(allObjects); objIt.More(); objIt.Next()) {
        auto objectNode = createInteractiveObjectNode(objIt.Value(), objectRow++);
        if (objectNode) {
            objectNode->parent = data_->rootNode;
            data_->rootNode->children.push_back(objectNode);
        }
    }
    
    spdlog::info("VInspectorWidget: Tree building completed with {} children", 
                 data_->rootNode->children.size());
}

VInspectorNodePtr VInspectorWidget::createContextNode(const Handle(AIS_InteractiveContext)& context) {
    auto node = std::make_shared<VInspectorNode>();
    node->type = VInspectorNode::Context;
    node->name = "AIS_InteractiveContext";
    node->pointer = getPointerString(context.get());
    node->row = 0;
    node->context = context;
    
    // Count selected owners
    node->selectedOwners = 0;
    if (context->HasSelectedShape()) {
        node->selectedOwners = 1; // Simplified count
    }
    
    return node;
}

VInspectorNodePtr VInspectorWidget::createInteractiveObjectNode(const Handle(AIS_InteractiveObject)& object, int row) {
    if (object.IsNull()) return nullptr;
    
    auto node = std::make_shared<VInspectorNode>();
    node->type = VInspectorNode::InteractiveObject;
    node->name = getObjectName(object);
    node->pointer = getPointerString(object.get());
    node->row = row;
    node->interactiveObject = object;
    node->isErased = !data_->context->IsDisplayed(object);
    node->isSelected = data_->context->IsSelected(object);
    
    // Count selected owners
    node->selectedOwners = node->isSelected ? 1 : 0;
    
    // Note: Selection information is typically managed by AIS_InteractiveContext
    // For now, we'll skip adding selection details as it requires deeper OCCT integration
    // This would need to be implemented using context->GetSelectedOwners() or similar APIs
    
    return node;
}

VInspectorNodePtr VInspectorWidget::createSelectionNode(const Handle(SelectMgr_Selection)& selection, int row) {
    if (selection.IsNull()) return nullptr;
    
    auto node = std::make_shared<VInspectorNode>();
    node->type = VInspectorNode::Selection;
    node->name = "Selection";
    node->pointer = getPointerString(selection.get());
    node->row = row;
    node->selection = selection;
    
    return node;
}

VInspectorNodePtr VInspectorWidget::createSensitiveEntityNode(const Handle(SelectMgr_SensitiveEntity)& entity, int row) {
    if (entity.IsNull()) return nullptr;
    
    auto node = std::make_shared<VInspectorNode>();
    node->type = VInspectorNode::SensitiveEntity;
    node->name = "SensitiveEntity";
    node->pointer = getPointerString(entity.get());
    node->row = row;
    node->sensitiveEntity = entity;
    
    return node;
}

VInspectorNodePtr VInspectorWidget::createOwnerNode(const Handle(SelectMgr_EntityOwner)& owner, int row) {
    if (owner.IsNull()) return nullptr;
    
    auto node = std::make_shared<VInspectorNode>();
    node->type = VInspectorNode::Owner;
    node->name = "Owner";
    node->pointer = getPointerString(static_cast<const Standard_Transient*>(owner.get()));
    node->row = row;
    node->owner = owner;
    
    return node;
}

void VInspectorWidget::renderTree() {
    if (!data_->rootNode) {
        ImGui::Text("No tree data available");
        return;
    }
    
    // Table header
    if (ImGui::BeginTable("ObjectTree", 4, 
                          ImGuiTableFlags_Borders | 
                          ImGuiTableFlags_Resizable | 
                          ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Pointer", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("SelectedOwners", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();
        
        // Render tree recursively
        renderNode(data_->rootNode);
        
        ImGui::EndTable();
    }
}

void VInspectorWidget::renderNode(const VInspectorNodePtr& node) {
    if (!node) return;
    
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    
    // Apply color based on node state
    ImVec4 color = getNodeColor(node);
    if (color.w > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }
    
    // Tree node with icon - use unique ID to avoid conflicts
    std::string nodeLabel = std::string(getNodeIcon(node->type)) + " " + node->name;
    if (node->type == VInspectorNode::InteractiveObject && node->isErased) {
        nodeLabel += " (Hidden)";
    }
    std::string uniqueId = nodeLabel + "##" + node->pointer; // Unique ID using pointer
    bool nodeOpen = false;
    
    if (!node->children.empty()) {
        nodeOpen = ImGui::TreeNodeEx(uniqueId.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
    } else {
        ImGui::TreeNodeEx(uniqueId.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
    }
    
    // Handle selection
    if (ImGui::IsItemClicked()) {
        data_->selectedNode = node;
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        renderContextMenu();
        ImGui::EndPopup();
    }
    
    // Other columns
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", node->row);
    
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%s", node->pointer.c_str());
    
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", node->selectedOwners);
    
    if (color.w > 0) {
        ImGui::PopStyleColor();
    }
    
    // Render children
    if (nodeOpen && !node->children.empty()) {
        for (const auto& child : node->children) {
            renderNode(child);
        }
        ImGui::TreePop();
    }
}

void VInspectorWidget::renderPropertiesPanel() {
    if (!data_->selectedNode) {
        ImGui::Text("Select an object to view properties");
        return;
    }
    
    auto node = data_->selectedNode;
    
    ImGui::Text("Object Properties");
    ImGui::Separator();
    
    ImGui::Text("Type: %s", [node]() {
        switch (node->type) {
            case VInspectorNode::Context: return "AIS_InteractiveContext";
            case VInspectorNode::InteractiveObject: return "AIS_InteractiveObject";
            case VInspectorNode::Selection: return "SelectMgr_Selection";
            case VInspectorNode::SensitiveEntity: return "SelectMgr_SensitiveEntity";
            case VInspectorNode::Owner: return "SelectMgr_EntityOwner";
            default: return "Unknown";
        }
    }());
    
    ImGui::Text("Name: %s", node->name.c_str());
    ImGui::Text("Pointer: %s", node->pointer.c_str());
    ImGui::Text("Row: %d", node->row);
    ImGui::Text("Selected Owners: %d", node->selectedOwners);
    
    if (node->type == VInspectorNode::InteractiveObject) {
        ImGui::Text("Is Erased: %s", node->isErased ? "Yes" : "No");
        ImGui::Text("Is Selected: %s", node->isSelected ? "Yes" : "No");
        
        ImGui::Separator();
        ImGui::Text("Actions:");
        
        if (!node->isErased && ImGui::Button("Hide")) {
            if (data_->hideObjectCallback && !node->interactiveObject.IsNull()) {
                data_->hideObjectCallback(node->interactiveObject);
            }
        }
        
        if (node->isErased && ImGui::Button("Show")) {
            if (data_->showObjectCallback && !node->interactiveObject.IsNull()) {
                data_->showObjectCallback(node->interactiveObject);
            }
        }
        
        if (ImGui::Button("Select")) {
            if (data_->selectObjectCallback && !node->interactiveObject.IsNull()) {
                data_->selectObjectCallback(node->interactiveObject);
            }
        }
    }
}

void VInspectorWidget::renderContextMenu() {
    if (!data_->selectedNode) return;
    
    auto node = data_->selectedNode;
    
    if (node->type == VInspectorNode::InteractiveObject && !node->interactiveObject.IsNull()) {
        if (ImGui::MenuItem("Show Object")) {
            if (data_->showObjectCallback) {
                data_->showObjectCallback(node->interactiveObject);
            }
        }
        
        if (ImGui::MenuItem("Hide Object")) {
            if (data_->hideObjectCallback) {
                data_->hideObjectCallback(node->interactiveObject);
            }
        }
        
        if (ImGui::MenuItem("Select Object")) {
            if (data_->selectObjectCallback) {
                data_->selectObjectCallback(node->interactiveObject);
            }
        }
    }
}

std::string VInspectorWidget::getObjectName(const Handle(AIS_InteractiveObject)& object) {
    if (object.IsNull()) return "NULL";
    
    try {
        // Try to get meaningful name
        if (Handle(AIS_Shape) shapeObj = Handle(AIS_Shape)::DownCast(object)) {
            return "AIS_Shape";
        }
        
        // Generic name based on type
        return "AIS_InteractiveObject";
    } catch (...) {
        return "AIS_InteractiveObject";
    }
}

std::string VInspectorWidget::getPointerString(const Standard_Transient* object) {
    if (!object) return "0x00000000";
    
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << reinterpret_cast<uintptr_t>(object);
    return oss.str();
}

ImVec4 VInspectorWidget::getNodeColor(const VInspectorNodePtr& node) {
    if (!node) return ImVec4(1.0f, 1.0f, 1.0f, 0.0f); // Default/transparent
    
    if (node->isErased) {
        return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Dark gray for erased objects
    }
    
    if (node->isSelected) {
        return ImVec4(0.2f, 0.4f, 0.8f, 1.0f); // Dark blue for selected objects
    }
    
    return ImVec4(1.0f, 1.0f, 1.0f, 0.0f); // Default color
}

const char* VInspectorWidget::getNodeIcon(VInspectorNode::NodeType type) {
    switch (type) {
        case VInspectorNode::Context: return "[C]";
        case VInspectorNode::InteractiveObject: return "[O]";
        case VInspectorNode::Selection: return "[S]";
        case VInspectorNode::SensitiveEntity: return "[E]";
        case VInspectorNode::Owner: return "[W]";
        default: return "[?]";
    }
}