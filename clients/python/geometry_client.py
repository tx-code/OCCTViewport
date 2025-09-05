#!/usr/bin/env python3
"""
Python gRPC client for GeometryServer
Can be used with PythonOCC-core for visualization
"""

import sys
import os
import grpc
import numpy as np
from typing import List, Tuple, Optional

# Add generated proto files to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'generated'))

import geometry_service_pb2
import geometry_service_pb2_grpc
import geometry_types_pb2


class GeometryClient:
    """Python client for GeometryServer"""
    
    def __init__(self, server_address: str = "localhost:50051"):
        """Initialize client with server address"""
        self.server_address = server_address
        self.channel = None
        self.stub = None
        
    def connect(self) -> bool:
        """Connect to the gRPC server"""
        try:
            # Create channel with options
            options = [
                ('grpc.max_receive_message_length', 100 * 1024 * 1024),  # 100MB
                ('grpc.max_send_message_length', 100 * 1024 * 1024),
            ]
            self.channel = grpc.insecure_channel(self.server_address, options=options)
            self.stub = geometry_service_pb2_grpc.GeometryServiceStub(self.channel)
            
            # Test connection with GetSystemInfo
            request = geometry_service_pb2.EmptyRequest()
            response = self.stub.GetSystemInfo(request, timeout=2.0)
            print(f"Connected to GeometryServer v{response.version}")
            print(f"OCCT Version: {response.occt_version}")
            return True
        except grpc.RpcError as e:
            print(f"Failed to connect: {e}")
            return False
            
    def disconnect(self):
        """Close the connection"""
        if self.channel:
            self.channel.close()
            
    def create_box(self, x: float, y: float, z: float, 
                   dx: float, dy: float, dz: float) -> str:
        """Create a box primitive"""
        request = geometry_service_pb2.BoxRequest(
            position=geometry_types_pb2.Point3D(x=x, y=y, z=z),
            width=dx,
            height=dy,
            depth=dz
        )
        response = self.stub.CreateBox(request)
        if response.success:
            return response.shape_id
        else:
            raise Exception(f"Failed to create box: {response.message}")
            
    def create_sphere(self, x: float, y: float, z: float, radius: float) -> str:
        """Create a sphere primitive"""
        request = geometry_service_pb2.SphereRequest(
            center=geometry_types_pb2.Point3D(x=x, y=y, z=z),
            radius=radius
        )
        response = self.stub.CreateSphere(request)
        if response.success:
            return response.shape_id
        else:
            raise Exception(f"Failed to create sphere: {response.message}")
            
    def get_mesh_data(self, shape_id: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        Get mesh data for a shape
        Returns: (vertices, normals, indices) as numpy arrays
        """
        request = geometry_service_pb2.ShapeRequest(shape_id=shape_id)
        response = self.stub.GetMeshData(request)
        
        # Convert to numpy arrays for easy use with visualization libraries
        vertices = np.array([[v.x, v.y, v.z] for v in response.vertices], dtype=np.float32)
        normals = np.array([[n.x, n.y, n.z] for n in response.normals], dtype=np.float32)
        indices = np.array(response.indices, dtype=np.uint32)
        
        return vertices, normals, indices
        
    def get_all_meshes(self) -> List[dict]:
        """
        Get all meshes using streaming
        Returns list of mesh dictionaries
        """
        request = geometry_service_pb2.EmptyRequest()
        meshes = []
        
        # Server streaming RPC
        for mesh_data in self.stub.GetAllMeshes(request):
            vertices = np.array([[v.x, v.y, v.z] for v in mesh_data.vertices], dtype=np.float32)
            normals = np.array([[n.x, n.y, n.z] for n in mesh_data.normals], dtype=np.float32)
            indices = np.array(mesh_data.indices, dtype=np.uint32)
            
            meshes.append({
                'shape_id': mesh_data.shape_id,
                'vertices': vertices,
                'normals': normals,
                'indices': indices
            })
            print(f"Received mesh for shape {mesh_data.shape_id}: "
                  f"{len(vertices)} vertices")
                  
        return meshes
        
    def import_model(self, file_path: str, auto_detect: bool = True) -> List[str]:
        """Import a model file"""
        request = geometry_service_pb2.ModelFileRequest(
            file_path=file_path,
            options=geometry_service_pb2.ModelImportOptions(
                auto_detect_format=auto_detect
            )
        )
        response = self.stub.ImportModelFile(request)
        
        if response.success:
            print(f"Imported {response.detected_format} file: {len(response.shape_ids)} shapes")
            return list(response.shape_ids)
        else:
            raise Exception(f"Import failed: {response.message}")
            
    def export_step(self, shape_ids: List[str], output_path: str = None) -> bytes:
        """Export shapes to STEP format"""
        request = geometry_service_pb2.ModelExportRequest(
            shape_ids=shape_ids,
            options=geometry_service_pb2.ModelExportOptions(
                format="STEP"
            )
        )
        response = self.stub.ExportModelFile(request)
        
        if response.success:
            if output_path:
                with open(output_path, 'wb') as f:
                    f.write(response.model_data)
                print(f"Exported to {output_path}")
            return response.model_data
        else:
            raise Exception(f"Export failed: {response.message}")
            
    def clear_all(self):
        """Clear all shapes on server"""
        request = geometry_service_pb2.EmptyRequest()
        response = self.stub.ClearAll(request)
        if response.success:
            print(f"Cleared all shapes: {response.message}")
        else:
            print(f"Clear failed: {response.message}")
        
    def create_demo_scene(self):
        """Create a demo scene with various primitives"""
        request = geometry_service_pb2.EmptyRequest()
        response = self.stub.CreateDemoScene(request)
        if response.success:
            print(f"Created demo scene: {response.message}")
        else:
            print(f"Demo scene failed: {response.message}")


def main():
    """Example usage"""
    client = GeometryClient()
    
    if not client.connect():
        print("Failed to connect to server")
        return
        
    try:
        # Clear existing shapes
        client.clear_all()
        
        # Create some primitives
        box_id = client.create_box(0, 0, 0, 10, 10, 10)
        print(f"Created box: {box_id}")
        
        sphere_id = client.create_sphere(15, 0, 0, 5)
        print(f"Created sphere: {sphere_id}")
        
        # Get mesh data
        vertices, normals, indices = client.get_mesh_data(box_id)
        print(f"Box mesh: {len(vertices)} vertices, {len(indices)} indices")
        
        # Create demo scene
        client.create_demo_scene()
        
        # Get all meshes using streaming
        meshes = client.get_all_meshes()
        print(f"Total meshes: {len(meshes)}")
        
        # Import a model file (if exists)
        try:
            shape_ids = client.import_model("tests/test_data/models/3boxes.brep")
            print(f"Imported shapes: {shape_ids}")
            
            # Export to STEP
            client.export_step(shape_ids, "exported.step")
        except Exception as e:
            print(f"Import/Export example skipped: {e}")
            
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()