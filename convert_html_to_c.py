#!/usr/bin/env python3
"""Convert HTML file to C string literal"""

import sys

def html_to_c_string(html_file):
    with open(html_file, 'r', encoding='utf-8') as f:
        html_content = f.read()

    # Escape special characters
    escaped = html_content.replace('\\', '\\\\')  # Backslash first
    escaped = escaped.replace('"', '\\"')          # Double quotes
    escaped = escaped.replace('\n', '\\n"\n"')     # Newlines

    # Output C code
    print('static const char* HTML_CONTENT =')
    print(f'"{escaped}";')
    print()
    print(f'// Length: {len(html_content)} bytes')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python convert_html_to_c.py web_interface.html')
        sys.exit(1)

    html_to_c_string(sys.argv[1])
