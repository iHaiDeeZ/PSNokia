#!/bin/bash
# Builds MIDP (runMidlet) for the PS4 into $PSNOKIA_MIDP_OUT, on top of the
# VM and PCSL: optimized by default, or with PSNOKIA_BUILD=debug the debug
# build (see env.sh).
. "$(dirname "$0")/env.sh"
F="$PSNOKIA_ROOT/phoneme_feature"
export JDK_DIR="$JDK8_DIR"
export GNU_TOOLS_DIR="$PSNOKIA_PS4/toolchain"
export CLDC_DIST_DIR="$PSNOKIA_OUT/cldc/ps4_c/dist"
export PCSL_OUTPUT_DIR="$PSNOKIA_OUT/pcsl"
export TOOLS_DIR="$F/tools"
export MIDP_OUTPUT_DIR="$PSNOKIA_MIDP_OUT"
if [ "$PSNOKIA_BUILD" = debug ]; then USE_DEBUG=true; else USE_DEBUG=false; fi
# The link also depends on the VM's exported objects (low heap, stat fix),
# which MIDP's makefiles do not track: always relink.
rm -f "$MIDP_OUTPUT_DIR/bin/c/$PSNOKIA_MIDP_ELF"
cd "$F/midp/build/ps4_c"
run_make() {
    make USE_DEBUG=$USE_DEBUG MIDP_OUTPUT_DIR="$(cygpath -m "$MIDP_OUTPUT_DIR")" "$@"
}
run_make "$@" || exit $?

# romgen exits 0 even when it fails, and the link then silently reuses the
# previous ROMImage.cpp. The usual cause is an inner class left behind by a
# source that no longer declares it (Outer$2.class older than Outer.class):
# it still goes into classes.zip and fails to link.
rom_stale() {
    [ "$MIDP_OUTPUT_DIR/ROMImage.cpp" -ot "$MIDP_OUTPUT_DIR/classes.zip" ]
}
if rom_stale; then
    echo "ROMImage.cpp is older than classes.zip: removing stale inner classes"
    find "$MIDP_OUTPUT_DIR/classes" -name '*$*.class' | while read -r f; do
        outer="${f%%\$*}.class"
        if [ -f "$outer" ] && [ "$f" -ot "$outer" ] &&
           [ $(( $(stat -c %Y "$outer") - $(stat -c %Y "$f") )) -gt 60 ]; then
            echo "  $f"
            rm -f "$f"
        fi
    done
    rm -f "$MIDP_OUTPUT_DIR/classes.zip" "$MIDP_OUTPUT_DIR/bin/c/$PSNOKIA_MIDP_ELF"
    run_make "$@" || exit $?
    if rom_stale; then
        echo "ERROR: romgen failed; see the ROMizing error above" >&2
        exit 1
    fi
fi
