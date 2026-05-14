#!/bin/bash
# Shader compilation script for GreatWar GPU
# Usage: ./compile_shaders.sh [output_directory]

OUTPUT_DIR="${1:-.}"
SHADER_DIR="$(dirname "$0")/shaders"

if [ ! -d "$SHADER_DIR" ]; then
    echo "Error: Shader directory not found at $SHADER_DIR"
    exit 1
fi

# Check if glslc is installed
if ! command -v glslc &> /dev/null; then
    echo "Error: glslc not found. Please install Vulkan SDK."
    exit 1
fi

echo "Compiling shaders to SPIR-V..."
echo "================================"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Compile each shader
SHADERS=(
    "crowd.comp"
    "crowd.vert"
    "crowd.frag"
)

SUCCESS=0
FAILED=0

for shader in "${SHADERS[@]}"; do
    INPUT="$SHADER_DIR/$shader"
    OUTPUT="$OUTPUT_DIR/$shader.spv"
    
    if [ -f "$INPUT" ]; then
        echo "Compiling $shader..."
        if glslc "$INPUT" -o "$OUTPUT"; then
            echo "  ✓ $OUTPUT"
            ((SUCCESS++))
        else
            echo "  ✗ Failed to compile $shader"
            ((FAILED++))
        fi
    else
        echo "  ✗ Shader not found: $INPUT"
        ((FAILED++))
    fi
done

echo "================================"
echo "Compilation summary:"
echo "  Successful: $SUCCESS"
echo "  Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "All shaders compiled successfully!"
    exit 0
else
    echo "Some shaders failed to compile."
    exit 1
fi
