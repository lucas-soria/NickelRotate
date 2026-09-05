#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
workdir="${PWD}"
scratch="${workdir}/tmp/build"
image="${NICKELTC_IMAGE:-ghcr.io/nicoverbruggen/nickelbench:recent}"
if command -v podman >/dev/null 2>&1; then
    engine=podman
elif command -v docker >/dev/null 2>&1; then
    engine=docker
else
    echo "Building NickelRotate requires Podman or Docker." >&2
    exit 127
fi
if [ "$#" -eq 0 ]; then set -- clean all strip koboroot check; fi
# The source archive excludes .git. Pass the host version to NickelHook.
version="$(git describe --tags --always --dirty 2>/dev/null || true)"
mkdir -p "${scratch}"
export COPYFILE_DISABLE=1
tar_options=()
if [ "$(uname -s)" = Darwin ]; then
    tar_options+=(--no-mac-metadata --no-xattrs --no-acls --no-fflags)
fi
# Stream sources into the VM so Podman needs no host-directory sharing.
tar "${tar_options[@]}" --exclude=.git --exclude=.DS_Store --exclude=tmp --exclude=KoboRoot.tgz \
    --exclude='*.o' --exclude='*.so' --exclude='*.moc' --exclude=nhplugin.json \
    -czf "${scratch}/source.tgz" .
"${engine}" run --rm -i --platform linux/amd64 --entrypoint sh --env "NH_BUILD_VERSION=${version}" "${image}" -lc '
    set -eu
    mkdir /work
    tar -xzf - -C /work
    cd /work
    export PATH="/tc/arm-nickel-linux-gnueabihf/bin:$PATH"
    make "$@" VERSION="${NH_BUILD_VERSION}" \
        CROSS_COMPILE=arm-nickel-linux-gnueabihf- \
        MOC=/tc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/bin/moc \
        RCC=/tc/arm-nickel-linux-gnueabihf/arm-nickel-linux-gnueabihf/sysroot/usr/bin/rcc >&2
    if [ -f KoboRoot.tgz ] && [ -f src/libnickelrotate.so ]; then
        tar -czf - KoboRoot.tgz src/libnickelrotate.so
    fi
' sh "$@" < "${scratch}/source.tgz" > "${scratch}/artifacts.tgz"
if [ -s "${scratch}/artifacts.tgz" ]; then
    tar -xzf "${scratch}/artifacts.tgz" -C "${workdir}" KoboRoot.tgz src/libnickelrotate.so
fi
