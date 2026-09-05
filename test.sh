#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
make test
if command -v podman >/dev/null 2>&1; then engine=podman; else engine=docker; fi
"${engine}" build -t localhost/nickelrotate-tests -f test/Dockerfile test
mkdir -p tmp
export COPYFILE_DISABLE=1
tar_options=()
if [ "$(uname -s)" = Darwin ]; then tar_options+=(--no-mac-metadata --no-xattrs --no-acls --no-fflags); fi
tar "${tar_options[@]}" --exclude=.git --exclude=tmp --exclude='*.so' --exclude=KoboRoot.tgz -czf tmp/test-source.tgz .
"${engine}" run --rm -i --entrypoint sh localhost/nickelrotate-tests -lc '
    set -eu
    mkdir /work
    tar -xzf - -C /work
    cd /work
    ./test/hooks.sh
' < tmp/test-source.tgz
