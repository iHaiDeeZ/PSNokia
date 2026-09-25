#!/bin/bash
# Builds the CLDC VM for the PS4 into $PSNOKIA_OUT/cldc (needs PCSL built).
. "$(dirname "$0")/env.sh"
set -e
# The low heap, malloc replacement, stat fix and render log, linked as
# objects into the VM and MIDP (see cldc/build/ps4_c/ps4_c.cfg)
mkdir -p "$PSNOKIA_OUT/common"
for f in lowheap lowheap_malloc psn_marker ps4_stat_fix; do
  "$PS4_LLVM/bin/clang" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 \
    -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" \
    -c "$PSNOKIA_PS4/common/$f.c" -o "$PSNOKIA_OUT/common/$f.o"
done
set +e
export JDK_DIR="$JDK6_DIR"
export PATH="$JDK_DIR/bin:$PATH"
export JVMWorkSpace="$PSNOKIA_ROOT/phoneme_feature/cldc"
export JVMBuildSpace="$PSNOKIA_OUT/cldc"
export ROMIZING=true
mkdir -p "$JVMBuildSpace"
cd "$JVMWorkSpace/build/ps4_c"
make GNU_TOOLS_DIR="$PSNOKIA_PS4/toolchain" PCSL_OUTPUT_DIR="$PSNOKIA_OUT/pcsl" "$@"
