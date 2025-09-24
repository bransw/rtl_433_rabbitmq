#!/usr/bin/env python3
"""
Script to test rtl_433 client-server pipeline with real signal files
"""

import subprocess
import time
import sys
import os

def run_client_with_real_signals():
    """Run rtl_433_client with real signal file and send to RabbitMQ"""
    
    print("🚀 Testing rtl_433 client-server with REAL signals...")
    print("=" * 60)
    
    # Check if files exist
    client_path = "build/rtl_433_client"
    signal_file = "tests/signals/test.cu8"
    
    if not os.path.exists(client_path):
        print(f"❌ Client not found: {client_path}")
        return False
        
    if not os.path.exists(signal_file):
        print(f"❌ Signal file not found: {signal_file}")
        return False
    
    print(f"✅ Using signal file: {signal_file}")
    print(f"✅ Using client: {client_path}")
    print()
    
    # Run client with real signal file
    cmd = [
        "./build/rtl_433_client",
        "-T", "amqp://guest:guest@localhost:5672/rtl_433",
        "-r", signal_file,
        "-v"
    ]
    
    print(f"🔧 Running command: {' '.join(cmd)}")
    print("=" * 60)
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        print("📤 CLIENT OUTPUT:")
        print("-" * 40)
        print(result.stdout)
        if result.stderr:
            print("⚠️  CLIENT ERRORS:")
            print(result.stderr)
        
        print(f"📊 Client exit code: {result.returncode}")
        
        if result.returncode == 0:
            print("✅ Client successfully processed real signals!")
            return True
        else:
            print("❌ Client failed to process signals")
            return False
            
    except subprocess.TimeoutExpired:
        print("⏰ Client timed out after 30 seconds")
        return False
    except Exception as e:
        print(f"❌ Error running client: {e}")
        return False

def run_server_test():
    """Run server to receive and decode the signals"""
    
    print("\n🖥️  Starting server to receive real signals...")
    print("=" * 60)
    
    cmd = ["./build/rtl_433_server", "-v"]
    
    try:
        print(f"🔧 Running server: {' '.join(cmd)}")
        print("📥 SERVER OUTPUT:")
        print("-" * 40)
        
        # Run server for limited time
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("⚠️  SERVER ERRORS:")
            print(result.stderr)
            
        print(f"📊 Server exit code: {result.returncode}")
        
        # Parse results
        output = result.stdout
        if "Devices decoded:" in output:
            lines = output.split('\n')
            for line in lines:
                if "Devices decoded:" in line:
                    decoded = line.split(":")[-1].strip()
                    print(f"🎯 Devices decoded: {decoded}")
                if "Signals received:" in line:
                    received = line.split(":")[-1].strip()
                    print(f"📡 Signals received: {received}")
                if "Recognition rate:" in line:
                    rate = line.split(":")[-1].strip()
                    print(f"📈 Recognition rate: {rate}")
        
        return True
        
    except subprocess.TimeoutExpired:
        print("⏰ Server stopped after 10 seconds (normal)")
        return True
    except Exception as e:
        print(f"❌ Error running server: {e}")
        return False

def main():
    """Main test function"""
    
    print("🧪 RTL_433 CLIENT-SERVER REAL SIGNAL TEST")
    print("=" * 60)
    
    # Step 1: Run client with real signals
    client_success = run_client_with_real_signals()
    
    if not client_success:
        print("\n❌ Client test failed, skipping server test")
        return False
    
    # Wait a moment for messages to be queued
    print("\n⏳ Waiting 2 seconds for RabbitMQ message processing...")
    time.sleep(2)
    
    # Step 2: Run server to process the signals
    server_success = run_server_test()
    
    print("\n" + "=" * 60)
    if client_success and server_success:
        print("🎉 REAL SIGNAL TEST COMPLETED!")
        print("✅ Client processed real signals and sent to RabbitMQ")
        print("✅ Server received and attempted to decode signals")
        print("\n💡 Check the server output above for decoding results!")
    else:
        print("❌ Test completed with errors")
    
    return client_success and server_success

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
