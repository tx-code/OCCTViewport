#ifndef VINSPECTORWIDGET_H
#define VINSPECTORWIDGET_H

#include <memory>
#include <vector>
#include <string>
#include <functional>

// ImGui forward declaration
struct ImVec4;

// OCCT Forward declarations
#include <Standard_Handle.hxx>

// Forward declarations
class AIS_InteractiveContext;
class AIS_InteractiveObject;
class SelectMgr_Selection;
class SelectMgr_SensitiveEntity;
class SelectMgr_EntityOwner;

//! Tree node structure for VInspector hierarchy display
struct VInspectorNode {
    enum NodeType {
        Context,           // AIS_InteractiveContext root
        InteractiveObject, // AIS_InteractiveObject
        Selection,         // SelectMgr_Selection
        SensitiveEntity,   // SelectMgr_SensitiveEntity  
        Owner             // SelectBasics_EntityOwner
    };
    
    NodeType type;
    std::string name;
    std::string pointer;
    int row = 0;
    int selectedOwners = 0;
    bool isErased = false;
    bool isSelected = false;
    
    // Object references
    Handle(AIS_InteractiveContext) context;
    Handle(AIS_InteractiveObject) interactiveObject;
    Handle(SelectMgr_Selection) selection;
    Handle(SelectMgr_SensitiveEntity) sensitiveEntity;
    Handle(SelectMgr_EntityOwner) owner;
    
    // Tree structure
    std::shared_ptr<VInspectorNode> parent;
    std::vector<std::shared_ptr<VInspectorNode>> children;
    
    VInspectorNode() = default;
};

using VInspectorNodePtr = std::shared_ptr<VInspectorNode>;

//! ImGui-based VInspector widget for browsing AIS Interactive Context
//! Provides tree view of AIS objects similar to OCCT Inspector VInspector plugin
class VInspectorWidget {
public:
    //! Callback function type for AIS object interaction
    using ObjectActionCallback = std::function<void(const Handle(AIS_InteractiveObject)&)>;

private:
    //! Private implementation data
    struct InspectorData;
    std::unique_ptr<InspectorData> data_;

public:
    //! Constructor
    VInspectorWidget();
    
    //! Destructor  
    ~VInspectorWidget();

    //! Main draw function - call this in ImGui context
    void draw(const Handle(AIS_InteractiveContext)& context);
    
    //! Update tree from current AIS context
    void updateTree();
    
    //! Set object action callbacks
    void setShowObjectCallback(const ObjectActionCallback& callback);
    void setHideObjectCallback(const ObjectActionCallback& callback);
    void setSelectObjectCallback(const ObjectActionCallback& callback);

private:
    //! Build tree structure from AIS context
    void buildTreeFromContext();
    
    //! Create tree nodes
    VInspectorNodePtr createContextNode(const Handle(AIS_InteractiveContext)& context);
    VInspectorNodePtr createInteractiveObjectNode(const Handle(AIS_InteractiveObject)& object, int row);
    VInspectorNodePtr createSelectionNode(const Handle(SelectMgr_Selection)& selection, int row);
    VInspectorNodePtr createSensitiveEntityNode(const Handle(SelectMgr_SensitiveEntity)& entity, int row);
    VInspectorNodePtr createOwnerNode(const Handle(SelectMgr_EntityOwner)& owner, int row);
    
    //! Render functions
    void renderTree();
    void renderNode(const VInspectorNodePtr& node);
    void renderPropertiesPanel();
    void renderContextMenu();
    
    //! Utility functions
    std::string getObjectName(const Handle(AIS_InteractiveObject)& object);
    std::string getPointerString(const Standard_Transient* object);
    ImVec4 getNodeColor(const VInspectorNodePtr& node);
    
    //! Get node icon based on type
    const char* getNodeIcon(VInspectorNode::NodeType type);
};

#endif // VINSPECTORWIDGET_H