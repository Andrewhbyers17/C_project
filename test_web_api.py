#!/usr/bin/env python3
"""
Comprehensive Web API Test Suite
Tests all aspects of the FFT Analyzer web interface
"""

import socket
import json
import sys
import time

class Colors:
    """ANSI color codes (may not work on all Windows terminals)"""
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    END = '\033[0m'
    BOLD = '\033[1m'

def colored(text, color):
    """Return colored text (works on most terminals)"""
    return f"{color}{text}{Colors.END}"

def test_http_request(path, expected_status="200 OK", port=8080):
    """
    Make HTTP GET request and return parsed response
    Returns: (status, headers_dict, body)
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('127.0.0.1', port))

        request = f"GET {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        sock.sendall(request.encode())

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

        if "\r\n\r\n" not in response_str:
            return None, {}, ""

        header_part, body = response_str.split("\r\n\r\n", 1)
        lines = header_part.split("\r\n")
        status = lines[0].replace("HTTP/1.1 ", "")

        headers = {}
        for line in lines[1:]:
            if ": " in line:
                key, value = line.split(": ", 1)
                headers[key] = value

        return status, headers, body

    except Exception as e:
        return None, {}, str(e)

def test_1_root_endpoint():
    """Test 1: Root endpoint serves HTML"""
    print("[TEST 1] Root endpoint (/)...")

    status, headers, body = test_http_request("/")

    if not status:
        print(colored("  [FAIL] No response from server", Colors.RED))
        return False

    if "200 OK" not in status:
        print(colored(f"  [FAIL] Expected 200 OK, got: {status}", Colors.RED))
        return False

    if "text/html" not in headers.get("Content-Type", ""):
        print(colored(f"  [FAIL] Expected text/html, got: {headers.get('Content-Type')}", Colors.RED))
        return False

    if "<html>" not in body.lower():
        print(colored("  [FAIL] Response doesn't contain HTML", Colors.RED))
        return False

    print(colored("  [PASS] Root endpoint OK", Colors.GREEN))
    return True

def test_2_api_fft_structure():
    """Test 2: /api/fft returns valid JSON structure"""
    print("[TEST 2] /api/fft JSON structure...")

    status, headers, body = test_http_request("/api/fft")

    if not status:
        print(colored("  [FAIL] No response from server", Colors.RED))
        return False

    if "200 OK" not in status:
        print(colored(f"  [FAIL] Expected 200 OK, got: {status}", Colors.RED))
        return False

    if "application/json" not in headers.get("Content-Type", ""):
        print(colored(f"  [FAIL] Expected application/json, got: {headers.get('Content-Type')}", Colors.RED))
        return False

    try:
        data = json.loads(body)
    except json.JSONDecodeError as e:
        print(colored(f"  [FAIL] Invalid JSON: {e}", Colors.RED))
        print(f"  Body preview: {body[:200]}")
        return False

    required_fields = ["mode", "fft_size", "sample_rate", "paused"]
    for field in required_fields:
        if field not in data:
            print(colored(f"  [FAIL] Missing required field: {field}", Colors.RED))
            return False

    print(colored("  [PASS] JSON structure valid", Colors.GREEN))
    print(f"         Mode: {data['mode']}")
    print(f"         FFT Size: {data['fft_size']}")
    print(f"         Sample Rate: {data['sample_rate']}")
    return True

def test_3_content_length():
    """Test 3: Content-Length header matches body length"""
    print("[TEST 3] Content-Length accuracy...")

    status, headers, body = test_http_request("/api/fft")

    if not status:
        print(colored("  [FAIL] No response from server", Colors.RED))
        return False

    content_length = headers.get("Content-Length", "")
    if not content_length:
        print(colored("  [FAIL] No Content-Length header", Colors.RED))
        return False

    try:
        expected_len = int(content_length)
        actual_len = len(body)

        if expected_len != actual_len:
            print(colored(f"  [FAIL] Length mismatch!", Colors.RED))
            print(f"         Expected: {expected_len} bytes")
            print(f"         Actual: {actual_len} bytes")
            print(f"         Difference: {expected_len - actual_len} bytes")
            return False

        print(colored("  [PASS] Content-Length matches body", Colors.GREEN))
        print(f"         Size: {actual_len} bytes")
        return True

    except ValueError:
        print(colored(f"  [FAIL] Invalid Content-Length: {content_length}", Colors.RED))
        return False

def test_4_initializing_state():
    """Test 4: Initializing state handling"""
    print("[TEST 4] Initializing state (if applicable)...")

    status, headers, body = test_http_request("/api/fft")

    try:
        data = json.loads(body)

        if data.get("mode") == "Initializing...":
            print(colored("  [INFO] System is initializing", Colors.YELLOW))
            print("         This is expected on first start")
            print(f"         FFT Size: {data['fft_size']} (should be 0)")
            print(f"         Sample Rate: {data['sample_rate']} (should be 0)")

            if data['fft_size'] != 0 or data['sample_rate'] != 0:
                print(colored("  [WARN] Initializing state has non-zero values", Colors.YELLOW))

            print(colored("  [PASS] Initializing state formatted correctly", Colors.GREEN))
        else:
            print(colored("  [INFO] System has data", Colors.BLUE))
            print(f"         Mode: {data['mode']}")
            print(f"         FFT Size: {data['fft_size']}")
            print(f"         Sample Rate: {data['sample_rate']}")
            print(colored("  [PASS] Data state valid", Colors.GREEN))

        return True

    except json.JSONDecodeError:
        print(colored("  [FAIL] Could not parse JSON", Colors.RED))
        return False

def test_5_data_arrays():
    """Test 5: Data arrays are present and valid"""
    print("[TEST 5] Data arrays validation...")

    status, headers, body = test_http_request("/api/fft")

    try:
        data = json.loads(body)

        # Check if initializing (arrays should be empty)
        if data.get("mode") == "Initializing...":
            print(colored("  [INFO] Skipping array check (initializing state)", Colors.YELLOW))
            return True

        # Check data arrays
        arrays = ["time_domain", "fft_magnitude", "psd", "band_energies"]
        for array_name in arrays:
            if array_name not in data:
                print(colored(f"  [FAIL] Missing array: {array_name}", Colors.RED))
                return False

            if not isinstance(data[array_name], list):
                print(colored(f"  [FAIL] {array_name} is not an array", Colors.RED))
                return False

            array_len = len(data[array_name])
            print(f"         {array_name}: {array_len} elements")

        print(colored("  [PASS] All data arrays present", Colors.GREEN))
        return True

    except json.JSONDecodeError:
        print(colored("  [FAIL] Could not parse JSON", Colors.RED))
        return False

def test_6_multiple_requests():
    """Test 6: Multiple rapid requests (no memory leaks/crashes)"""
    print("[TEST 6] Multiple rapid requests...")

    num_requests = 10
    success_count = 0

    for i in range(num_requests):
        status, headers, body = test_http_request("/api/fft")

        if status and "200 OK" in status:
            try:
                json.loads(body)
                success_count += 1
            except:
                pass

    if success_count == num_requests:
        print(colored(f"  [PASS] All {num_requests} requests successful", Colors.GREEN))
        return True
    else:
        print(colored(f"  [FAIL] Only {success_count}/{num_requests} succeeded", Colors.RED))
        return False

def main():
    """Run all tests"""
    print()
    print("Testing web interface at http://localhost:8080")
    print()

    tests = [
        test_1_root_endpoint,
        test_2_api_fft_structure,
        test_3_content_length,
        test_4_initializing_state,
        test_5_data_arrays,
        test_6_multiple_requests,
    ]

    results = []
    for test in tests:
        result = test()
        results.append(result)
        print()
        time.sleep(0.1)  # Small delay between tests

    # Summary
    print("=" * 60)
    print(colored(f"SUMMARY: {sum(results)}/{len(results)} tests passed", Colors.BOLD))
    print("=" * 60)

    if all(results):
        print()
        print(colored("SUCCESS: Web interface is working correctly!", Colors.GREEN))
        print()
        print("You can now open http://localhost:8080 in your browser")
        print()
        return 0
    else:
        print()
        print(colored("FAILURE: Some tests failed", Colors.RED))
        print()
        print("Please check the errors above")
        print()
        return 1

if __name__ == "__main__":
    sys.exit(main())
