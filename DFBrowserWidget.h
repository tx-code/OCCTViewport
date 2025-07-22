#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>


// OCCT OCAF
#include <AIS_InteractiveObject.hxx>
#include <TDF_Attribute.hxx>
#include <TDF_Data.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>


// Forward declarations
class AIS_InteractiveContext;

//! Tree node structure for OCAF hierarchy display
struct DFBrowserNode {
  enum NodeType { Document, Label, Attribute };

  NodeType type;
  std::string text;
  std::string entry;
  std::string attributeType;

  // OCAF handles
  TDF_Label label;
  Handle(TDF_Attribute) attribute;

  // Tree structure
  std::vector<std::shared_ptr<DFBrowserNode>> children;
  std::shared_ptr<DFBrowserNode> parent;

  // UI state
  bool isExpanded = false;
  bool isSelected = false;
};

using DFBrowserNodePtr = std::shared_ptr<DFBrowserNode>;
using AddAisObjectCallback =
    std::function<void(const Handle(AIS_InteractiveObject) &)>;

//! Widget for browsing OCAF data structure using ImGui
class DFBrowserWidget {
public:
  DFBrowserWidget();
  ~DFBrowserWidget();

  //! Main draw function - call this in ImGui context
  void draw(const Handle(AIS_InteractiveContext) & context,
            const AddAisObjectCallback &addAisObjectCallback = nullptr);

  //! Set the document to browse
  void setDocument(const Handle(TDocStd_Document) & document);

  //! Update the tree structure from current OCAF data
  void updateTree();

  //! Create test OCAF document for demonstration
  void createTestDocument();

private:
  struct BrowserData;
  std::unique_ptr<BrowserData> data_;

  // Tree building functions
  void buildTreeFromDocument();
  DFBrowserNodePtr createDocumentNode(const Handle(TDocStd_Document) &
                                      document);
  DFBrowserNodePtr createLabelNode(const TDF_Label &label, int depth = 0);
  DFBrowserNodePtr createAttributeNode(const Handle(TDF_Attribute) & attribute);

  // UI rendering functions
  void renderSearchBar();
  void renderTreeView();
  void renderNavigationLine();
  void renderPropertyPanel();
  void renderDumpView();

  // Tree rendering helpers
  void renderTreeNode(const DFBrowserNodePtr &node);
  void handleNodeSelection(const DFBrowserNodePtr &node);

  // Property panel rendering for different types
  void renderLabelProperties(const TDF_Label &label);
  void renderAttributeProperties(const Handle(TDF_Attribute) & attribute);

  // Utility functions
  std::string getLabelEntry(const TDF_Label &label) const;
  std::string getAttributeInfo(const Handle(TDF_Attribute) & attribute) const;
  std::string getAttributeValue(const Handle(TDF_Attribute) & attribute) const;

  // Search functionality
  void performSearch(const std::string &searchText);
  void highlightSearchResults(const std::vector<DFBrowserNodePtr> &results);
};