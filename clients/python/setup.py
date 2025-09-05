#!/usr/bin/env python3
"""
Setup script to generate Python bindings from proto files
Run: python setup.py
"""

import os
import sys
import subprocess
from pathlib import Path


def generate_proto_files():
    """Generate Python files from proto definitions"""
    
    # Paths
    current_dir = Path(__file__).parent
    project_root = current_dir.parent.parent
    proto_dir = project_root / "proto"
    output_dir = current_dir / "generated"
    
    # Create output directory
    output_dir.mkdir(exist_ok=True)
    
    # Proto files to compile
    proto_files = [
        "geometry_types.proto",
        "geometry_service.proto"
    ]
    
    print(f"Generating Python bindings from proto files...")
    print(f"Proto directory: {proto_dir}")
    print(f"Output directory: {output_dir}")
    
    # Generate Python bindings
    for proto_file in proto_files:
        proto_path = proto_dir / proto_file
        if not proto_path.exists():
            print(f"Warning: {proto_path} not found")
            continue
            
        # Run protoc compiler
        cmd = [
            sys.executable, "-m", "grpc_tools.protoc",
            f"--proto_path={proto_dir}",
            f"--python_out={output_dir}",
            f"--grpc_python_out={output_dir}",
            str(proto_path)
        ]
        
        print(f"Compiling {proto_file}...")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Error compiling {proto_file}:")
            print(result.stderr)
            return False
        else:
            print(f"  [OK] Generated {proto_file.replace('.proto', '_pb2.py')}")
            if "service" in proto_file:
                print(f"  [OK] Generated {proto_file.replace('.proto', '_pb2_grpc.py')}")
    
    # Create __init__.py
    init_file = output_dir / "__init__.py"
    init_file.write_text("# Generated proto files\n")
    
    print("\n[SUCCESS] Proto files generated successfully!")
    print("\nYou can now run:")
    print("  python geometry_client.py")
    print("  python pythonocc_viewer.py (if PythonOCC is installed)")
    
    return True


def install_requirements():
    """Install required packages"""
    print("Installing requirements...")
    # Install only basic requirements
    subprocess.run([sys.executable, "-m", "pip", "install", "grpcio", "grpcio-tools", "protobuf", "numpy", "--quiet"])
    

def main():
    """Main setup"""
    print("=== Python gRPC Client Setup ===\n")
    
    # Install requirements
    install_requirements()
    
    # Generate proto files
    if not generate_proto_files():
        print("\n[FAILED] Setup failed")
        sys.exit(1)
        
    print("\n[SUCCESS] Setup complete!")
    

if __name__ == "__main__":
    main()