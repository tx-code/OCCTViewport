#!/usr/bin/env python3
"""
Test script for client session isolation in GeometryService.
This script simulates two different clients creating shapes and verifying
that each client only sees their own shapes.
"""

import grpc
import time
import sys
sys.path.append('generated')

import geometry_service_pb2
import geometry_service_pb2_grpc


def create_client(client_id):
    """Create a gRPC client with a specific client ID."""
    channel = grpc.insecure_channel('localhost:50051')
    stub = geometry_service_pb2_grpc.GeometryServiceStub(channel)
    
    # Create metadata with client ID
    metadata = [('client-id', client_id)]
    
    return channel, stub, metadata


def test_session_isolation():
    """Test that different clients have isolated sessions."""
    print("=" * 60)
    print("Testing Client Session Isolation")
    print("=" * 60)
    
    # Create two different clients
    channel1, stub1, metadata1 = create_client('Client-A')
    channel2, stub2, metadata2 = create_client('Client-B')
    
    try:
        # Step 1: Client A creates a box
        print("\n1. Client-A creates a box...")
        box_request = geometry_service_pb2.BoxRequest()
        box_request.width = 100.0
        box_request.height = 100.0
        box_request.depth = 100.0
        box_request.position.x = 0.0
        box_request.position.y = 0.0
        box_request.position.z = 0.0
        box_request.color.r = 1.0
        box_request.color.g = 0.0
        box_request.color.b = 0.0
        box_request.color.a = 1.0
        
        box_response = stub1.CreateBox(box_request, metadata=metadata1)
        client_a_shape = box_response.shape_id
        print(f"   Created: {client_a_shape}")
        
        # Step 2: Client B creates a sphere
        print("\n2. Client-B creates a sphere...")
        sphere_request = geometry_service_pb2.SphereRequest()
        sphere_request.radius = 50.0
        sphere_request.center.x = 0.0
        sphere_request.center.y = 0.0
        sphere_request.center.z = 0.0
        sphere_request.color.r = 0.0
        sphere_request.color.g = 0.0
        sphere_request.color.b = 1.0
        sphere_request.color.a = 1.0
        
        sphere_response = stub2.CreateSphere(sphere_request, metadata=metadata2)
        client_b_shape = sphere_response.shape_id
        print(f"   Created: {client_b_shape}")
        
        # Step 3: Check Client A's shapes count
        print("\n3. Checking Client-A's shape count...")
        info_request = geometry_service_pb2.EmptyRequest()
        info_response = stub1.GetSystemInfo(info_request, metadata=metadata1)
        print(f"   Client-A has {info_response.active_shapes} shapes (expected: 1)")
        
        # Step 4: Check Client B's shapes count
        print("\n4. Checking Client-B's shape count...")
        info_response = stub2.GetSystemInfo(info_request, metadata=metadata2)
        print(f"   Client-B has {info_response.active_shapes} shapes (expected: 1)")
        
        # Step 5: Client B clears all shapes
        print("\n5. Client-B clears all shapes...")
        clear_response = stub2.ClearAll(info_request, metadata=metadata2)
        print(f"   {clear_response.message}")
        
        # Step 6: Verify Client A still has its shape
        print("\n6. Verifying Client-A still has its shape...")
        info_response = stub1.GetSystemInfo(info_request, metadata=metadata1)
        print(f"   Client-A has {info_response.active_shapes} shapes (expected: 1)")
        
        # Step 7: Verify Client B has no shapes
        print("\n7. Verifying Client-B has no shapes...")
        info_response = stub2.GetSystemInfo(info_request, metadata=metadata2)
        print(f"   Client-B has {info_response.active_shapes} shapes (expected: 0)")
        
        # Step 8: Client A tries to delete Client B's shape (should fail)
        print("\n8. Client-A tries to delete Client-B's shape (should fail)...")
        delete_request = geometry_service_pb2.ShapeRequest()
        delete_request.shape_id = client_b_shape
        delete_response = stub1.DeleteShape(delete_request, metadata=metadata1)
        print(f"   Result: {delete_response.message}")
        print(f"   Success: {delete_response.success} (expected: False)")
        
        # Step 9: Create multiple shapes for stress test
        print("\n9. Creating multiple shapes for both clients...")
        for i in range(3):
            # Client A creates boxes
            box_response = stub1.CreateBox(box_request, metadata=metadata1)
            print(f"   Client-A created: {box_response.shape_id}")
            
            # Client B creates spheres
            sphere_response = stub2.CreateSphere(sphere_request, metadata=metadata2)
            print(f"   Client-B created: {sphere_response.shape_id}")
        
        # Step 10: Final shape count
        print("\n10. Final shape counts:")
        info_response = stub1.GetSystemInfo(info_request, metadata=metadata1)
        print(f"   Client-A has {info_response.active_shapes} shapes (expected: 4)")
        
        info_response = stub2.GetSystemInfo(info_request, metadata=metadata2)
        print(f"   Client-B has {info_response.active_shapes} shapes (expected: 3)")
        
        print("\n" + "=" * 60)
        print("Session Isolation Test: COMPLETED")
        print("=" * 60)
        
    finally:
        channel1.close()
        channel2.close()


if __name__ == '__main__':
    test_session_isolation()