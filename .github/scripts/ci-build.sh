#!/usr/bin/env bash
# Configures ns-3 with NORI and builds libnori plus the examples listed in
# examples/CMakeLists.txt (base_examples).
#
# Runs inside the CI image (Dockerfile target "ci"), with this repository
# mounted at /home/developer/ns-3-dev/contrib/nori. Local use:
#   docker compose run --rm nori-dev contrib/nori/.github/scripts/ci-build.sh
set -euo pipefail

cd /home/developer/ns-3-dev
NORI=contrib/nori

mapfile -t EXAMPLES < <(
    sed -n '/set(base_examples/,/)/p' "$NORI/examples/CMakeLists.txt" |
        sed -e '1d;$d' -e 's/#.*//' -e 's/[[:space:]]//g' | grep -v '^$'
)
echo "Examples: ${EXAMPLES[*]}"

if command -v ccache >/dev/null; then ccache -z; fi

# Only nori and its dependencies (nr, lte, ...); examples of the dependencies
# are configured but not built, because only the NORI targets are requested.
./ns3 configure -d default --enable-examples --enable-modules=nori ${NS3_CONFIGURE_EXTRA:-}
./ns3 build nori "${EXAMPLES[@]}"

if command -v ccache >/dev/null; then ccache -s; fi
