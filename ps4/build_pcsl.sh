#!/bin/bash
# Builds PCSL for the PS4 into $PSNOKIA_OUT/pcsl/ps4_c.
. "$(dirname "$0")/env.sh"
export JDK_DIR="$JDK6_DIR"
export PATH="$JDK_DIR/bin:$PATH"
cd "$PSNOKIA_ROOT/phoneme_feature/pcsl"
make PCSL_PLATFORM=ps4_c_gcc PCSL_OS=ps4 PCSL_CPU=c PCSL_OUTPUT_DIR="$PSNOKIA_OUT/pcsl" \
     GNU_TOOLS_DIR="$PSNOKIA_PS4/toolchain" "$@" all
