#!/bin/bash
# Wrapper to run model_to_md3.py with Blender's Python (has pygltflib)
BLENDER_PYTHON="/home/ruben/Descargas/blender-5.0.1-linux-x64/5.0/python/bin/python3.11"

if [ -f "$BLENDER_PYTHON" ]; then
    $BLENDER_PYTHON "$(dirname "$0")/model_to_md3.py" "$@"
else
    # Fallback to system python
    python3 "$(dirname "$0")/model_to_md3.py" "$@"
fi
