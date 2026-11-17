#!/usr/bin/env bash
# Smoke test: runs the examples that work without a Near-RT RIC for a few
# simulated seconds (E2 and realtime off). Fails if any of them crashes,
# aborts on an NS_ASSERT/NS_FATAL_ERROR or exceeds the timeout.
#
# Not covered (always install E2 or use the realtime clock): nori-sample,
# nori-rc-slicing-demo, nori-ccc-slicing-demo, nori-mimo-demo.
#
# Expects ci-build.sh to have run in the same container.
set -euo pipefail

cd /home/developer/ns-3-dev
TIMEOUT="${SMOKE_TIMEOUT:-600}"
failed=()

run() {
    local cmd="$*"
    echo "::group::$cmd"
    local start=$SECONDS
    if timeout --kill-after=30 "$TIMEOUT" ./ns3 run --no-build "$cmd"; then
        echo "OK ($((SECONDS - start)) s)"
    else
        echo "::error::'$cmd' failed (exit $?)"
        failed+=("$cmd")
    fi
    echo "::endgroup::"
}

run "nori-print-attributes"
run "nori-testbed-scenario --simTime=2 --enableE2=false --enableRealtime=false"
run "nori-benchmark-scenario --simTime=2 --enableE2=false --enableRealtime=false"
run "nori-handover-scenario --simTime=2 --enableE2=false --enableRealtime=false"

if ((${#failed[@]})); then
    printf 'Failed: %s\n' "${failed[@]}"
    exit 1
fi
