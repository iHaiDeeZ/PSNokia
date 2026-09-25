# PSNokia build environment. Source it from MSYS2 bash:  . ps4/env.sh
# Override any of the tool locations by exporting them first.
PSNOKIA_PS4="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PSNOKIA_PS4
export PSNOKIA_ROOT="$(dirname "$PSNOKIA_PS4")"
# Build output (outside version control)
export PSNOKIA_OUT="${PSNOKIA_OUT:-$PSNOKIA_PS4/out}"
# release: optimized, for playing (the default). debug: much slower, but
# checks more and logs every Java exception with its stack.
export PSNOKIA_BUILD="${PSNOKIA_BUILD:-release}"
if [ "$PSNOKIA_BUILD" = debug ]; then
  export PSNOKIA_MIDP_OUT="$PSNOKIA_OUT/midp" PSNOKIA_MIDP_ELF=runMidlet_g
else
  export PSNOKIA_MIDP_OUT="$PSNOKIA_OUT/midp_release" PSNOKIA_MIDP_ELF=runMidlet
fi

# OpenOrbis PS4 toolchain and the LLVM it is used with
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/ps4/OpenOrbis/PS4Toolchain}"
export PS4_LLVM="${PS4_LLVM:-$HOME/ps4/llvm-18}"
# phoneME's Java build: JDK 6 for CLDC, JDK 8 for MIDP
export JDK6_DIR="${JDK6_DIR:-$HOME/zulu6.22.0.3-jdk6.0.119-win_x64}"
export JDK8_DIR="${JDK8_DIR:-$HOME/jdk8u504-b01}"

export PATH="$PS4_LLVM/bin:$OO_PS4_TOOLCHAIN/bin/windows:$PATH"
# PkgTool.Core targets netcoreapp3.0; let it run on newer .NET runtimes
export DOTNET_ROLL_FORWARD=Major
