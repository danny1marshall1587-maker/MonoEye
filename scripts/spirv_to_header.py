#!/usr/bin/env python3
"""
Converts a SPIR-V binary file to a C++ header with embedded byte array.
Usage: spirv_to_header.py input.spv output.h array_name
"""

import sys
import os

def main():
    if len(sys.argv) != 4:
        print("Usage: spirv_to_header.py <input.spv> <output.h> <array_name>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]
    array_name = sys.argv[3]

    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)

    with open(input_path, 'rb') as f:
        data = f.read()

    # Convert to uint32_t array
    words = []
    for i in range(0, len(data), 4):
        word = int.from_bytes(data[i:i+4], byteorder='little')
        words.append(word)

    with open(output_path, 'w') as f:
        f.write(f"// Copyright (c) 2026 MonoEye Contributors\n")
        f.write(f"// SPDX-License-Identifier: Apache-2.0\n")
        f.write(f"//\n")
        f.write(f"// AUTO-GENERATED from {os.path.basename(input_path)}\n")
        f.write(f"// Do not edit manually\n\n")
        f.write(f"#pragma once\n\n")
        f.write(f"#include <cstdint>\n\n")
        f.write(f"// SPIR-V shader binary ({len(data)} bytes)\n")
        f.write(f"static const uint32_t {array_name}_data[] = {{\n")

        for i in range(0, len(words), 8):
            chunk = words[i:i+8]
            hex_values = ', '.join(f'0x{w:08x}' for w in chunk)
            f.write(f"    {hex_values},\n")

        f.write(f"}};\n\n")
        f.write(f"static const size_t {array_name}_size = {len(data)};\n")

    print(f"Generated {output_path} ({len(data)} bytes, {len(words)} words)")

if __name__ == '__main__':
    main()
