#!/usr/bin/env python3
"""
Enhanced Python gRPC client for GeometryServer
Includes model import functionality similar to C++ client
"""

import sys
import os
import grpc
import numpy as np
import concurrent.futures
import threading
import time
from typing import List, Tuple, Optional, Dict, Any, Callable
from dataclasses import dataclass, field
from pathlib import Path

# Add generated proto files to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'generated'))

import geometry_service_pb2
import geometry_service_pb2_grpc
import geometry_types_pb2


@dataclass
class ImportTask:
    """Represents an async import task"""
    id: str
    file_path: str
    file_name: str
    format: str = ""
    future: Optional[concurrent.futures.Future] = None
    progress: float = 0.0
    is_active: bool = True
    status_message: str = "Starting..."
    start_time: float = field(default_factory=time.time)
    shape_ids: List[str] = field(default_factory=list)
    error: Optional[str] = None


@dataclass 
class ModelImportOptions:
    """Model import options matching C++ client"""
    auto_detect_format: bool = True
    force_format: str = ""
    import_colors: bool = True
    import_names: bool = True
    precision: float = 0.01
    merge_shapes: bool = False


@dataclass
class ModelImportResult:
    """Result of model import operation"""
    success: bool = False
    message: str = ""
    detected_format: str = ""
    shape_ids: List[str] = field(default_factory=list)
    filename: str = ""
    file_size: int = 0
    shape_count: int = 0
    format: str = ""
    creation_time: str = ""


class EnhancedGeometryClient:
    """Enhanced geometry client with import functionality"""
    
    def __init__(self, server_address: str = "localhost:50051", client_id: str = "PythonClient"):
        """Initialize enhanced client"""
        self.server_address = server_address
        self.channel = None
        self.stub = None
        self.executor = concurrent.futures.ThreadPoolExecutor(max_workers=5)
        self.import_tasks: Dict[str, ImportTask] = {}
        self.task_counter = 0
        self.progress_callbacks: Dict[str, Callable[[ImportTask], None]] = {}
        self.client_id = client_id
        self.metadata = [('client-id', client_id)]  # gRPC metadata for all requests
        
    def connect(self) -> bool:
        """Connect to the gRPC server"""
        try:
            options = [
                ('grpc.max_receive_message_length', 100 * 1024 * 1024),  # 100MB
                ('grpc.max_send_message_length', 100 * 1024 * 1024),
            ]
            self.channel = grpc.insecure_channel(self.server_address, options=options)
            self.stub = geometry_service_pb2_grpc.GeometryServiceStub(self.channel)
            
            # Test connection
            request = geometry_service_pb2.EmptyRequest()
            response = self.stub.GetSystemInfo(request, timeout=2.0, metadata=self.metadata)
            print(f"Connected to GeometryServer v{response.version}")
            print(f"OCCT Version: {response.occt_version}")
            return True
        except grpc.RpcError as e:
            print(f"Failed to connect: {e}")
            return False
            
    def disconnect_from_server(self):
        """Notify server of disconnection"""
        if self.channel and self.stub:
            try:
                request = geometry_service_pb2.EmptyRequest()
                response = self.stub.DisconnectClient(request, timeout=2.0, metadata=self.metadata)
                if response.success:
                    print(f"Successfully notified server of disconnection: {response.message}")
                    return True
                else:
                    print(f"Failed to notify server of disconnection: {response.message}")
                    return False
            except grpc.RpcError as e:
                print(f"Failed to notify server of disconnection: {e}")
                return False
        return False
            
    def disconnect(self):
        """Close the connection"""
        if self.channel:
            # Notify server before disconnecting
            self.disconnect_from_server()
            
            # Cancel all active tasks
            for task_id in list(self.import_tasks.keys()):
                self.cancel_import_task(task_id)
            
            self.executor.shutdown(wait=True)
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
        response = self.stub.CreateBox(request, metadata=self.metadata)
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
        response = self.stub.CreateSphere(request, metadata=self.metadata)
        if response.success:
            return response.shape_id
        else:
            raise Exception(f"Failed to create sphere: {response.message}")
            
    def clear_all(self):
        """Clear all shapes on server"""
        request = geometry_service_pb2.EmptyRequest()
        response = self.stub.ClearAll(request, metadata=self.metadata)
        if response.success:
            print(f"Cleared all shapes: {response.message}")
        else:
            print(f"Clear failed: {response.message}")
            
    def get_all_meshes(self) -> List[dict]:
        """Get all meshes using streaming"""
        request = geometry_service_pb2.EmptyRequest()
        meshes = []
        
        for mesh_data in self.stub.GetAllMeshes(request, metadata=self.metadata):
            vertices = np.array([[v.x, v.y, v.z] for v in mesh_data.vertices], dtype=np.float32)
            normals = np.array([[n.x, n.y, n.z] for n in mesh_data.normals], dtype=np.float32)
            indices = np.array(mesh_data.indices, dtype=np.uint32)
            
            meshes.append({
                'shape_id': mesh_data.shape_id,
                'vertices': vertices,
                'normals': normals,
                'indices': indices
            })
            print(f"Received mesh for shape {mesh_data.shape_id}: {len(vertices)} vertices")
                  
        return meshes
        
    def import_model_async(self, file_path: str, options: ModelImportOptions = None, 
                          progress_callback: Callable[[ImportTask], None] = None) -> str:
        """
        Import a model file asynchronously
        Returns task ID for tracking progress
        """
        if options is None:
            options = ModelImportOptions()
            
        # Create task
        self.task_counter += 1
        task_id = f"import_{self.task_counter}"
        
        file_path = Path(file_path)
        task = ImportTask(
            id=task_id,
            file_path=str(file_path),
            file_name=file_path.name,
            format=file_path.suffix.upper().replace('.', '')
        )
        
        self.import_tasks[task_id] = task
        
        # Set progress callback
        if progress_callback:
            self.progress_callbacks[task_id] = progress_callback
            
        # Submit async task
        task.future = self.executor.submit(self._import_model_worker, task, options)
        
        print(f"Started async import of {task.file_name} (ID: {task_id})")
        return task_id
        
    def _import_model_worker(self, task: ImportTask, options: ModelImportOptions) -> ModelImportResult:
        """Worker function for async import"""
        try:
            task.status_message = "Preparing import..."
            task.progress = 0.1
            self._notify_progress(task)
            
            # Prepare request
            request = geometry_service_pb2.ModelFileRequest(
                file_path=task.file_path,
                options=geometry_service_pb2.ModelImportOptions(
                    auto_detect_format=options.auto_detect_format,
                    force_format=options.force_format,
                    import_colors=options.import_colors,
                    import_names=options.import_names,
                    precision=options.precision,
                    merge_shapes=options.merge_shapes
                )
            )
            
            task.status_message = "Sending to server..."
            task.progress = 0.3
            self._notify_progress(task)
            
            # Make gRPC call
            response = self.stub.ImportModelFile(request, metadata=self.metadata)
            
            task.progress = 0.8
            self._notify_progress(task)
            
            # Process result
            result = ModelImportResult()
            if response.success:
                result.success = True
                result.message = response.message
                result.detected_format = response.detected_format
                result.shape_ids = list(response.shape_ids)
                
                if hasattr(response, 'file_info') and response.file_info:
                    file_info = response.file_info
                    result.filename = file_info.filename
                    result.file_size = file_info.file_size
                    result.shape_count = file_info.shape_count
                    result.format = file_info.format
                    result.creation_time = file_info.creation_time
                
                task.shape_ids = result.shape_ids
                task.status_message = f"Import completed: {len(result.shape_ids)} shapes"
                task.progress = 1.0
                task.is_active = False
                
            else:
                result.message = response.message
                task.error = response.message
                task.status_message = f"Import failed: {response.message}"
                task.is_active = False
                
            self._notify_progress(task)
            return result
            
        except Exception as e:
            task.error = str(e)
            task.status_message = f"Exception: {str(e)}"
            task.is_active = False
            self._notify_progress(task)
            raise
            
    def _notify_progress(self, task: ImportTask):
        """Notify progress callback if registered"""
        if task.id in self.progress_callbacks:
            try:
                self.progress_callbacks[task.id](task)
            except Exception as e:
                print(f"Progress callback error for {task.id}: {e}")
                
    def get_import_task_status(self, task_id: str) -> Optional[ImportTask]:
        """Get status of import task"""
        return self.import_tasks.get(task_id)
        
    def cancel_import_task(self, task_id: str) -> bool:
        """Cancel an import task"""
        if task_id in self.import_tasks:
            task = self.import_tasks[task_id]
            if task.future and not task.future.done():
                task.future.cancel()
                task.status_message = "Cancelled"
                task.is_active = False
                return True
        return False
        
    def get_active_tasks(self) -> List[ImportTask]:
        """Get all active import tasks"""
        return [task for task in self.import_tasks.values() if task.is_active]
        
    def import_multiple_files(self, file_paths: List[str], 
                            options: ModelImportOptions = None) -> Dict[str, str]:
        """
        Import multiple files concurrently
        Returns dict mapping file_path to task_id
        """
        if options is None:
            options = ModelImportOptions()
            
        task_map = {}
        
        for file_path in file_paths:
            if os.path.exists(file_path):
                task_id = self.import_model_async(file_path, options)
                task_map[file_path] = task_id
            else:
                print(f"File not found: {file_path}")
                
        return task_map
        
    def wait_for_import_completion(self, task_id: str, timeout: float = 30.0) -> ModelImportResult:
        """Wait for import task to complete"""
        if task_id not in self.import_tasks:
            raise ValueError(f"Task {task_id} not found")
            
        task = self.import_tasks[task_id]
        if task.future:
            try:
                return task.future.result(timeout=timeout)
            except concurrent.futures.TimeoutError:
                raise TimeoutError(f"Import task {task_id} timed out")
            except Exception as e:
                raise RuntimeError(f"Import task {task_id} failed: {e}")
        else:
            raise RuntimeError(f"Task {task_id} has no associated future")
            
    def cleanup_completed_tasks(self):
        """Remove completed tasks from memory"""
        completed_tasks = [
            task_id for task_id, task in self.import_tasks.items()
            if not task.is_active and (not task.future or task.future.done())
        ]
        
        for task_id in completed_tasks:
            if task_id in self.import_tasks:
                del self.import_tasks[task_id]
            if task_id in self.progress_callbacks:
                del self.progress_callbacks[task_id]
                
        if completed_tasks:
            print(f"Cleaned up {len(completed_tasks)} completed tasks")


def main():
    """Example usage of enhanced client"""
    client = EnhancedGeometryClient()
    
    if not client.connect():
        print("Failed to connect to server")
        return
        
    try:
        # Clear existing shapes
        client.clear_all()
        
        # Create some basic shapes
        box_id = client.create_box(0, 0, 0, 10, 10, 10)
        print(f"Created box: {box_id}")
        
        sphere_id = client.create_sphere(15, 0, 0, 5)
        print(f"Created sphere: {sphere_id}")
        
        # Test async import
        def progress_callback(task: ImportTask):
            print(f"Task {task.id}: {task.progress:.1%} - {task.status_message}")
            
        print("\n=== Testing Async Import ===")
        # Use absolute paths for reliable file access
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        test_files = [
            os.path.join(base_dir, "tests/test_data/models/3boxes.brep"),
            os.path.join(base_dir, "tests/test_data/models/179_synthetic_case.stp")
        ]
        
        # Import multiple files
        task_map = client.import_multiple_files(test_files)
        
        # Wait for completion
        for file_path, task_id in task_map.items():
            client.progress_callbacks[task_id] = progress_callback
            try:
                result = client.wait_for_import_completion(task_id, timeout=10.0)
                print(f"\n[OK] {Path(file_path).name}: {len(result.shape_ids)} shapes imported")
            except Exception as e:
                print(f"\n[ERROR] {Path(file_path).name}: {e}")
                
        # Get final mesh data
        meshes = client.get_all_meshes()
        print(f"\nTotal meshes: {len(meshes)}")
        
        # Cleanup
        client.cleanup_completed_tasks()
        
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()