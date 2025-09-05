#!/usr/bin/env python3
"""
PythonOCC Client for GeometryServer
Professional AIS-based 3D visualization using PythonOCC-core
"""

import sys
import os
from pathlib import Path
from typing import List, Optional
import numpy as np
import tkinter as tk
from tkinter import filedialog

# Add generated proto files to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'generated'))

from enhanced_geometry_client import EnhancedGeometryClient, ModelImportOptions

# PyQt5 imports for log window
try:
    from PyQt5.QtWidgets import QWidget, QTextEdit, QVBoxLayout, QDockWidget, QMainWindow
    from PyQt5.QtCore import Qt, QDateTime
    from PyQt5.QtGui import QTextCursor, QColor, QTextCharFormat
    PYQT5_AVAILABLE = True
except ImportError:
    PYQT5_AVAILABLE = False
    print("Warning: PyQt5 not available, log window disabled")

# PythonOCC imports
try:
    from OCC.Display.SimpleGui import init_display
    from OCC.Core.gp import gp_Pnt, gp_Dir, gp_Ax2
    from OCC.Core.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeSphere, BRepPrimAPI_MakeCylinder
    from OCC.Core.BRepBuilderAPI import BRepBuilderAPI_MakeVertex
    from OCC.Core.BRepMesh import BRepMesh_IncrementalMesh
    from OCC.Core.StlAPI import StlAPI_Reader
    from OCC.Core.AIS import AIS_ColoredShape, AIS_Trihedron
    from OCC.Core.Geom import Geom_Axis2Placement
    from OCC.Core.Quantity import (
        Quantity_Color, Quantity_TOC_RGB, 
        Quantity_NOC_RED, Quantity_NOC_BLUE, Quantity_NOC_GREEN,
        Quantity_NOC_YELLOW, Quantity_NOC_CYAN, Quantity_NOC_ORANGE
    )
    from OCC.Core.Graphic3d import (
        Graphic3d_NOM_BRASS, Graphic3d_NOM_BRONZE,
        Graphic3d_NOM_COPPER, Graphic3d_NOM_GOLD,
        Graphic3d_NOM_PEWTER, Graphic3d_NOM_PLASTER,
        Graphic3d_NOM_PLASTIC, Graphic3d_NOM_SILVER
    )
    PYTHONOCC_AVAILABLE = True
except ImportError:
    print("Error: PythonOCC-core is required but not installed")
    print("Install with: conda install -c conda-forge pythonocc-core")
    sys.exit(1)


class PythonOCCClient:
    """Professional PythonOCC client for GeometryServer with AIS rendering"""
    
    def __init__(self):
        self.client = EnhancedGeometryClient(client_id="PythonOCC-AIS")
        self.connected = False
        self.display = None
        self.start_display = None
        self.add_menu = None
        self.add_function = None
        self.shapes = {}  # Store AIS shapes by ID
        self.colors = [
            Quantity_NOC_RED, Quantity_NOC_BLUE, Quantity_NOC_GREEN,
            Quantity_NOC_YELLOW, Quantity_NOC_CYAN, Quantity_NOC_ORANGE
        ]
        self.materials = [
            Graphic3d_NOM_BRASS, Graphic3d_NOM_BRONZE, Graphic3d_NOM_COPPER,
            Graphic3d_NOM_GOLD, Graphic3d_NOM_SILVER, Graphic3d_NOM_PLASTIC
        ]
        self.log_widget = None  # Will hold the log text widget
        self.log_window = None  # Will hold the log window
        
    def init_display(self):
        """Initialize PythonOCC display"""
        self.display, self.start_display, self.add_menu, self.add_function = init_display()
        
        # Set professional background gradient
        self.display.View.SetBgGradientColors(
            Quantity_Color(0.2, 0.2, 0.3, Quantity_TOC_RGB),  # Dark blue-gray
            Quantity_Color(0.8, 0.8, 0.9, Quantity_TOC_RGB),  # Light blue-gray
            2, True
        )
        
        # Add coordinate system trihedron
        axis = Geom_Axis2Placement(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)))
        trihedron = AIS_Trihedron(axis)
        trihedron.SetSize(50)
        self.display.Context.Display(trihedron, False)
        
        # Set rendering parameters for quality
        try:
            self.display.View.SetAntialiasingOn()
        except:
            pass  # Some versions don't support this
            
        # Initialize log window
        self.init_log_window()
        
        # Store log dock reference - will be added to window after start_display
        self.pending_log_dock = self.log_window if (self.log_window and PYQT5_AVAILABLE) else None
        
        self.log("PythonOCC display initialized")
    
    def init_log_window(self):
        """Initialize log display window using Qt"""
        if not PYQT5_AVAILABLE:
            self.log_widget = None
            self.log_window = None
            return
            
        # Create QTextEdit widget for log display
        self.log_widget = QTextEdit()
        self.log_widget.setReadOnly(True)
        self.log_widget.setStyleSheet("""
            QTextEdit {
                background-color: #1e1e1e;
                color: #00ff00;
                font-family: 'Consolas', 'Monaco', monospace;
                font-size: 10pt;
                border: 1px solid #333;
            }
        """)
        
        # Create dock widget to hold the log
        self.log_dock = QDockWidget("Client Log")
        self.log_dock.setWidget(self.log_widget)
        self.log_dock.setMinimumHeight(150)
        
        # Store references but don't show yet - will be added to main window later
        self.log_window = self.log_dock
        
    def toggle_log_window(self):
        """Toggle log window visibility"""
        if self.log_window and isinstance(self.log_window, QDockWidget):
            if self.log_window.isVisible():
                self.log_window.hide()
            else:
                self.log_window.show()
    
    def log(self, message: str, level: str = "INFO"):
        """Add message to log display"""
        if self.log_widget and PYQT5_AVAILABLE:
            from datetime import datetime
            timestamp = datetime.now().strftime("%H:%M:%S")
            
            # Color codes for different log levels
            if level == "ERROR":
                color = "#ff6666"
                prefix = "[ERROR]"
            elif level == "WARNING":
                color = "#ffaa00"
                prefix = "[WARN] "
            elif level == "SUCCESS":
                color = "#66ff66"
                prefix = "[OK]   "
            else:
                color = "#00ff00"
                prefix = "[INFO] "
            
            # Format and append message with color
            formatted_message = f'<span style="color: {color}">{timestamp} {prefix} {message}</span>'
            self.log_widget.append(formatted_message)
            
            # Auto-scroll to bottom
            cursor = self.log_widget.textCursor()
            cursor.movePosition(QTextCursor.End)
            self.log_widget.setTextCursor(cursor)
            
            # Ensure updates are processed
            if hasattr(self.log_widget, 'repaint'):
                self.log_widget.repaint()
        
        # Also print to console
        print(f"[{level}] {message}")
        
    def connect(self) -> bool:
        """Connect to GeometryServer"""
        if self.client.connect():
            self.connected = True
            self.log("Connected to GeometryServer", "SUCCESS")
            return True
        self.connected = False
        self.log("Failed to connect to GeometryServer", "ERROR")
        return False
        
    def create_demo_geometry(self):
        """Create demo geometry on server and display locally"""
        self.log("Creating Demo Geometry")
        
        # Clear both server and local display
        self.clear_all()
        
        # Create shapes on server
        shapes_created = []
        
        # Box
        box_id = self.client.create_box(0, 0, 0, 50, 30, 20)
        shapes_created.append((box_id, "box", (0, 0, 0), (50, 30, 20)))
        
        # Sphere
        sphere_id = self.client.create_sphere(80, 0, 0, 20)
        shapes_created.append((sphere_id, "sphere", (80, 0, 0), 20))
        
        # Create local representations for display
        for i, shape_data in enumerate(shapes_created):
            shape_id = shape_data[0]
            shape_type = shape_data[1]
            
            if shape_type == "box":
                pos = shape_data[2]
                dims = shape_data[3]
                local_shape = BRepPrimAPI_MakeBox(
                    gp_Pnt(pos[0], pos[1], pos[2]),
                    dims[0], dims[1], dims[2]
                ).Shape()
            elif shape_type == "sphere":
                pos = shape_data[2]
                radius = shape_data[3]
                local_shape = BRepPrimAPI_MakeSphere(
                    gp_Pnt(pos[0], pos[1], pos[2]),
                    radius
                ).Shape()
                
            # Create AIS shape with professional appearance
            ais_shape = AIS_ColoredShape(local_shape)
            
            # Set color and transparency
            color = Quantity_Color(self.colors[i % len(self.colors)])
            ais_shape.SetColor(color)
            ais_shape.SetTransparency(0.2)
            
            # Display shape
            self.display.Context.Display(ais_shape, True)
            self.shapes[shape_id] = ais_shape
            
            self.log(f"Created and displayed: {shape_id} ({shape_type})", "SUCCESS")
            
        # Fit all in view
        self.display.FitAll()
        
    def import_models(self, file_paths: List[str]):
        """Import and display CAD models"""
        self.log(f"Importing {len(file_paths)} model(s)")
        
        for file_path in file_paths:
            if not os.path.exists(file_path):
                self.log(f"File not found: {file_path}", "ERROR")
                continue
                
            self.log(f"Importing: {Path(file_path).name}")
            
            # Import on server
            task_id = self.client.import_model_async(
                file_path,
                ModelImportOptions(auto_detect_format=True)
            )
            
            # Wait for completion
            try:
                result = self.client.wait_for_import_completion(task_id, timeout=30.0)
                if result.success:
                    self.log(f"Imported {len(result.shape_ids)} shapes on server", "SUCCESS")
                    
                    # Also load and display locally for immediate visualization
                    self.load_and_display_local_file(file_path, result.shape_ids)
                    
            except Exception as e:
                self.log(f"Import failed: {e}", "ERROR")
    
    def load_and_display_local_file(self, file_path: str, shape_ids: List[str]):
        """Load a CAD file locally and display it"""
        try:
            file_ext = Path(file_path).suffix.lower()
            shape = None
            
            if file_ext in ['.step', '.stp']:
                # Read STEP file
                from OCC.Core.STEPControl import STEPControl_Reader
                reader = STEPControl_Reader()
                status = reader.ReadFile(file_path)
                if status == 1:  # Success
                    reader.TransferRoots()
                    shape = reader.OneShape()
                    
            elif file_ext == '.brep':
                # Read BREP file
                from OCC.Core.BRepTools import breptools
                from OCC.Core.TopoDS import TopoDS_Shape
                from OCC.Core.BRep import BRep_Builder
                shape = TopoDS_Shape()
                builder = BRep_Builder()
                success = breptools.Read(shape, file_path, builder)
                if not success:
                    self.log(f"Failed to read BREP file: {file_path}", "ERROR")
                    return
                    
            elif file_ext == '.stl':
                # Read STL file
                from OCC.Core.StlAPI import StlAPI_Reader
                reader = StlAPI_Reader()
                shape = TopoDS_Shape()
                if not reader.Read(shape, file_path):
                    self.log(f"Failed to read STL file: {file_path}", "ERROR")
                    return
                    
            elif file_ext in ['.iges', '.igs']:
                # Read IGES file
                from OCC.Core.IGESControl import IGESControl_Reader
                reader = IGESControl_Reader()
                status = reader.ReadFile(file_path)
                if status == 1:  # Success
                    reader.TransferRoots()
                    shape = reader.OneShape()
            
            if shape:
                # Create AIS shape and display
                color_idx = len(self.shapes) % len(self.colors)
                ais_shape = AIS_ColoredShape(shape)
                ais_shape.SetColor(Quantity_Color(self.colors[color_idx]))
                ais_shape.SetTransparency(0.1)
                
                # Display the shape
                self.display.Context.Display(ais_shape, True)
                
                # Store it with the server shape ID
                if shape_ids:
                    self.shapes[shape_ids[0]] = ais_shape
                    
                self.log(f"Displayed locally: {Path(file_path).name}", "SUCCESS")
                self.display.FitAll()
                
        except Exception as e:
            self.log(f"Error loading local file: {e}", "WARNING")
            # Fall back to mesh display
            self.display_server_meshes()
                
    def display_server_meshes(self):
        """Fetch and display meshes from server as points"""
        self.log("Displaying Server Meshes")
        
        meshes = self.client.get_all_meshes()
        self.log(f"Received {len(meshes)} meshes from server")
        
        for i, mesh_data in enumerate(meshes):
            shape_id = mesh_data['shape_id']
            vertices = mesh_data['vertices']
            
            if len(vertices) > 0:
                # Create point cloud for visualization
                # (In production, you'd create proper triangulated shape)
                for vertex in vertices[::10]:  # Sample every 10th vertex
                    point = BRepBuilderAPI_MakeVertex(
                        gp_Pnt(vertex[0], vertex[1], vertex[2])
                    ).Shape()
                    self.display.DisplayShape(point, update=False)
                    
        self.display.FitAll()
        self.display.Repaint()
        
    def clear_all(self):
        """Clear both server and local display"""
        self.log("Clearing All")
        # Clear server
        self.client.clear_all()
        # Clear local display
        self.display.EraseAll()
        self.shapes.clear()
        self.log("Cleared server and local display", "SUCCESS")
    
    def import_models_dialog(self):
        """Open file dialog to import models (supports multiple selection)"""
        # Create a hidden root window for the dialog
        root = tk.Tk()
        root.withdraw()
        
        file_paths = filedialog.askopenfilenames(
            title="Select CAD Models",
            filetypes=[
                ("All CAD Files", "*.step;*.stp;*.brep;*.iges;*.igs;*.stl"),
                ("STEP Files", "*.step;*.stp"),
                ("BREP Files", "*.brep"),
                ("IGES Files", "*.iges;*.igs"),
                ("STL Files", "*.stl"),
                ("All Files", "*.*")
            ]
        )
        
        root.destroy()
        
        if file_paths:
            self.import_models(list(file_paths))
        
    def setup_menus(self):
        """Setup application menus"""
        if self.add_menu and self.add_function:
            # File menu
            self.add_menu("File")
            self.add_function("File", self.import_models_dialog)
            self.add_function("File", self.clear_all)
            
            # View menu
            self.add_menu("View")
            self.add_function("View", self.display.FitAll)
            self.add_function("View", self.toggle_log_window)
            
            
    def run(self):
        """Run the PythonOCC client application"""
        # Initialize display
        self.init_display()
        
        # Connect to server
        if not self.connect():
            self.log("Could not connect to GeometryServer", "WARNING")
            self.log("Running in offline mode", "WARNING")
            
        # Setup menus
        self.setup_menus()
        
        # Create initial demo
        if self.connected:
            self.create_demo_geometry()
        
        self.log("PythonOCC Client Ready", "SUCCESS")
        self.log("Controls:")
        self.log("  - Left mouse: Rotate")
        self.log("  - Middle mouse: Pan")
        self.log("  - Right mouse: Zoom")
        self.log("  - Menu: Server operations")
        
        # Add log dock after Qt app is initialized
        if self.pending_log_dock and PYQT5_AVAILABLE:
            try:
                from PyQt5.QtWidgets import QApplication
                app = QApplication.instance()
                if app:
                    # Get all main windows
                    for widget in app.topLevelWidgets():
                        if isinstance(widget, QMainWindow):
                            widget.addDockWidget(Qt.BottomDockWidgetArea, self.log_dock)
                            self.log_dock.show()
                            self.log("Log window attached to main window", "SUCCESS")
                            break
            except Exception as e:
                self.log(f"Could not attach log dock: {e}", "WARNING")
        
        # Start event loop
        self.start_display()
        
        # Cleanup on exit
        if self.connected:
            self.client.disconnect()


def main():
    """Main entry point"""
    client = PythonOCCClient()
    client.run()


if __name__ == "__main__":
    main()