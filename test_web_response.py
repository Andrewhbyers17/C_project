#!/usr/bin/env python3
"""Test script to verify /api/fft response"""

import socket
import time

def test_api_response(port=8080):
    """Send HTTP GET to /api/fft and show raw response"""

    # Wait a moment for server to be ready
    time.sleep(1)

    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('127.0.0.1', port))

        # Send HTTP GET request
        request = b"GET /api/fft HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        sock.sendall(request)

        # Receive response
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            except socket.timeout:
                break

        sock.close()

        # Parse response
        response_str = response.decode('utf-8', errors='replace')

        print("="*60)
        print("HTTP RESPONSE:")
        print("="*60)

        # Split header and body
        if "\r\n\r\n" in response_str:
            header, body = response_str.split("\r\n\r\n", 1)
            print("HEADERS:")
            print(header)
            print("\nBODY:")
            print(body[:500])  # First 500 chars
            print("\nBODY LENGTH:", len(body), "bytes")

            # Check Content-Length
            for line in header.split("\r\n"):
                if line.startswith("Content-Length:"):
                    expected_len = int(line.split(":")[1].strip())
                    actual_len = len(body)
                    print(f"\nContent-Length header: {expected_len}")
                    print(f"Actual body length: {actual_len}")
                    if expected_len != actual_len:
                        print(f"❌ MISMATCH! Missing {expected_len - actual_len} bytes")
                    else:
                        print("✅ Length matches")
        else:
            print(response_str)

        print("="*60)

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    print("Testing /api/fft endpoint...")
    print("Make sure FFT analyzer is running on port 8080")
    test_api_response()
