#!/usr/bin/env python3
"""
Convert WAV impulse responses to C++ header files for Daisy
- Resamples from 44.1kHz to 48kHz
- Converts to float32 arrays
- Splits stereo to separate L/R arrays
- Stores in SDRAM with DSY_SDRAM_BSS
"""

import numpy as np
import soundfile as sf
from scipy import signal
import os

INPUT_FILES = [
    "Shirobon - PS1 Reverb- Impulse Responses - 01 PS1 Studio IR.wav",
    "Shirobon - PS1 Reverb- Impulse Responses - 02 PS1 Church IR.wav",
    "Shirobon - PS1 Reverb- Impulse Responses - 03 PS1 Dome IR.wav",
    "Shirobon - PS1 Reverb- Impulse Responses - 04 PS1 Hall IR.wav"
]

OUTPUT_NAMES = [
    "PS1_Studio",
    "PS1_Church",
    "PS1_Dome",
    "PS1_Hall"
]

TARGET_SR = 48000
OUTPUT_HEADER = "IRData.h"

def resample_audio(data, orig_sr, target_sr):
    """Resample audio using scipy's high-quality resampler"""
    num_samples = int(len(data) * target_sr / orig_sr)
    return signal.resample(data, num_samples)

def float_to_cpp_array(data, name, values_per_line=8):
    """Convert float array to C++ array initialization string"""
    lines = []
    for i in range(0, len(data), values_per_line):
        chunk = data[i:i+values_per_line]
        values = ", ".join(f"{v:+.8f}f" for v in chunk)
        lines.append(f"    {values}")
    return ",\n".join(lines)

def process_ir(input_file, output_name):
    """Process a single IR file"""
    print(f"Processing {input_file}...")

    # Read WAV file
    data, sr = sf.read(input_file, dtype='float32')
    print(f"  Original: {sr}Hz, {data.shape[0]} samples, {data.shape[1]} channels")

    # Split stereo channels
    if len(data.shape) == 1:
        # Mono - duplicate to stereo
        left = data
        right = data
    else:
        left = data[:, 0]
        right = data[:, 1]

    # Resample to 48kHz if needed
    if sr != TARGET_SR:
        print(f"  Resampling from {sr}Hz to {TARGET_SR}Hz...")
        left = resample_audio(left, sr, TARGET_SR)
        right = resample_audio(right, sr, TARGET_SR)

    # Normalize to prevent clipping
    max_val = max(np.abs(left).max(), np.abs(right).max())
    if max_val > 0.99:
        scale = 0.99 / max_val
        left *= scale
        right *= scale
        print(f"  Normalized by {scale:.3f}")

    length = len(left)
    print(f"  Output: {TARGET_SR}Hz, {length} samples per channel")

    # WORKAROUND: Truncate all IRs to same length to avoid SDRAM initialization issues
    # Use the shortest common length
    max_length = 389646
    if length > max_length:
        print(f"  Truncating from {length} to {max_length} samples")
        left = left[:max_length]
        right = right[:max_length]
        length = max_length

    print(f"  Memory: {length * 4 * 2 / 1024 / 1024:.2f} MB (stereo float32)")

    return left, right, length

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Process all IRs
    ir_data = []
    for input_file, output_name in zip(INPUT_FILES, OUTPUT_NAMES):
        input_path = os.path.join(script_dir, input_file)
        left, right, length = process_ir(input_path, output_name)
        ir_data.append((output_name, left, right, length))

    # Generate header file
    header_path = os.path.join(script_dir, OUTPUT_HEADER)
    print(f"\nGenerating {OUTPUT_HEADER}...")

    with open(header_path, 'w') as f:
        f.write("// Auto-generated IR data for PS1 reverbs\n")
        f.write("// Converted from 44.1kHz to 48kHz\n")
        f.write("// Storage: SDRAM (DSY_SDRAM_BSS)\n\n")
        f.write("#pragma once\n\n")
        f.write("#include \"daisy_seed.h\"\n")
        f.write("#include <cstddef>\n\n")
        f.write("namespace IRData {\n\n")

        # Write length constants
        for name, left, right, length in ir_data:
            f.write(f"constexpr size_t k{name}_Length = {length};\n")
        f.write("\n")

        # Write arrays with SDRAM placement
        # NOTE: Using const to force .data section instead of .bss (ensures initialization)
        for name, left, right, length in ir_data:
            f.write(f"// {name}: {length} samples, {length * 8 / 1024 / 1024:.2f} MB stereo\n")
            f.write(f"__attribute__((section(\".sdram_data\"))) const float {name}_L[{length}] = {{\n")
            f.write(float_to_cpp_array(left, f"{name}_L"))
            f.write(f"\n}};\n\n")

            f.write(f"__attribute__((section(\".sdram_data\"))) const float {name}_R[{length}] = {{\n")
            f.write(float_to_cpp_array(right, f"{name}_R"))
            f.write(f"\n}};\n\n")

        # Write IR descriptor struct
        f.write("struct IRDescriptor {\n")
        f.write("    const char* name;\n")
        f.write("    const float* left;\n")
        f.write("    const float* right;\n")
        f.write("    size_t length;\n")
        f.write("};\n\n")

        # Write IR array
        f.write("constexpr IRDescriptor kAllIRs[] = {\n")
        for name, _, _, length in ir_data:
            f.write(f'    {{"{name}", {name}_L, {name}_R, k{name}_Length}},\n')
        f.write("};\n\n")

        f.write("constexpr size_t kNumIRs = sizeof(kAllIRs) / sizeof(kAllIRs[0]);\n\n")
        f.write("} // namespace IRData\n")

    print(f"✓ Generated {OUTPUT_HEADER}")

    # Calculate total memory
    total_samples = sum(length for _, _, _, length in ir_data)
    total_mb = total_samples * 8 / 1024 / 1024  # stereo float32
    print(f"\nTotal memory: {total_mb:.2f} MB for {len(ir_data)} IRs")

if __name__ == "__main__":
    main()
