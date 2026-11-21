#!/bin/bash
# Builds SD Bench for the Nintendo Switch 2 and checks the result.
# Needs devkitPro with the Switch 2 devkit (see BUILDING.md).
set -e
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
[ -f "$DEVKITPRO/switch2vars.sh" ] && . "$DEVKITPRO/switch2vars.sh"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS LIBS
cd "$R"
make -j"$(nproc 2>/dev/null || echo 4)"
bash tools/verify_elf.sh sdbench.elf
python3 -I tools/verify_nro.py sdbench.nro build/sdbench.map
echo "built $R/sdbench.nro"
