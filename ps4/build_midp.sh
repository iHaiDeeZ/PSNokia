#!/bin/bash
# Builds MIDP (runMidlet) for the PS4 into $PSNOKIA_OUT/midp, on top of the
# VM and PCSL.
. "$(dirname "$0")/env.sh"
F="$PSNOKIA_ROOT/phoneme_feature"
export JDK_DIR="$JDK8_DIR"
export GNU_TOOLS_DIR="$PSNOKIA_PS4/toolchain"
export CLDC_DIST_DIR="$PSNOKIA_OUT/cldc/ps4_c/dist"
export PCSL_OUTPUT_DIR="$PSNOKIA_OUT/pcsl"
export TOOLS_DIR="$F/tools"
export MIDP_OUTPUT_DIR="$PSNOKIA_OUT/midp"
# The link also depends on the VM's exported objects (low heap, stat fix),
# which MIDP's makefiles do not track: always relink.
rm -f "$MIDP_OUTPUT_DIR/bin/c/runMidlet_g"
cd "$F/midp/build/ps4_c"
make USE_DEBUG=true MIDP_OUTPUT_DIR="$(cygpath -m "$MIDP_OUTPUT_DIR")" "$@"
