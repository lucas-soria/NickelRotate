# Contributing to NickelRotate

Technical guide for building, testing and changing the mod. This repository follows the documentation and contribution structure of [NickelTypeFix](https://github.com/nicoverbruggen/NickelTypeFix).

## Building

Initialize the pinned NickelHook submodule and start Docker or Podman:

```sh
git submodule update --init --recursive
./build.sh
```

The default targets are `clean all strip koboroot check`. The build uses `ghcr.io/nicoverbruggen/nickelbench:recent`, which includes NickelTC and the compatibility database. `NICKELTC_IMAGE` overrides the image. Use an immutable image digest when reproducing a build.

`./build.sh <targets>` passes other make targets through. The script produces `KoboRoot.tgz` and `src/libnickelrotate.so` when the requested targets build them. It streams the source into the container, so Podman does not need host-directory sharing.

NickelHook stamps `git describe --tags --always --dirty` into `NH_VERSION`. The build script reads this on the host because its source archive excludes `.git`. CI checks out the full history and builds directly. A source copy without Git history falls back to `dev`.

With NickelTC installed on the host, use `make CROSS_COMPILE=/path/to/nickeltc/bin/arm-nickel-linux-gnueabihf- all koboroot`.

## Tests

- `make test` runs the C++ policy tests with address and undefined-behaviour sanitizers.
- `./test.sh` runs the policy tests and production hook tests using host Qt in a separate Debian container.
- `./build.sh` builds with Kobo's Qt 5.2.1 toolchain and runs `make check` inside NickelBench.
- `python3 test/verify_package.py` checks archive paths, file modes, ownership and payload bytes.

The production hook tests use simulated Nickel callers and real host Qt widgets. They cover device and firmware gates, reader permissions, touch transforms, popup selection and native sizing before show, paired orientation masks, translated labels, disabled passthrough and exception cleanup. They do not emulate Kobo firmware or touch hardware.

## Firmware compatibility

Each hook and lookup in a library recorded by NickelBench has a `//nb <kind> <role|*> <first> <last|*> <symbol>...` annotation. `nickelbench check-source src` checks these against every database firmware from 4.38.23684 onward. The runtime policy applies the separate 4.45 floor for BW and Colour and rejects 5.x.

NickelBench currently does not record `libkevdevtouch.so`. The two touch hooks therefore have an explicit source comment pointing to the local firmware verifier. Do not annotate a different library to make the check pass.

For a complete check, extract `libnickel.so.1.0.0` and `generic/libkevdevtouch.so` from the same firmware, then run:

```sh
python3 test/verify_firmware.py PATH_TO_LIBNICKEL PATH_TO_LIBKEVDEVTOUCH src/libnickelrotate.so
```

This checks all eight PLT call sites, global function bindings, controller RTTI, plugin exports and the ARM hard-float ABI. Firmware checks have passed on 4.38.23684, 4.38.23697, 4.45.23697 and 4.46.23836. Keep firmware binaries out of Git and release artifacts.

The six reader hooks and the lookup are always required. The two touch hooks are marked optional only so Clara 2E, which keeps Nickel's own touch transform, does not depend on them; `init()` rolls back every hook when a model that needs the touch correction is missing either one. Rotation without its touch correction is unsafe to use, so on those models these remain one dependency group. When NickelRotate stays inactive (unsupported model or firmware, disable marker, unexpected geometry), it restores every applied hook instead of leaving them in passthrough. This differs from NickelTypeFix's independent optional fixes. NickelRotate has no instruction-byte patches and needs no byte-anchor validation job.

## Testing on a device

Copy `KoboRoot.tgz` into the Kobo's hidden `.kobo` folder, safely eject and let the package install. Reboot if necessary. See the README for disabling, uninstalling and the startup failsafe.

Check taps near every corner, page turns, swipes, text selection and reading controls in both EPUB and KEPUB. Check the table of contents and reading dialogs, then return to Home, Library and settings. Repeat after sleep, closing the book and rebooting. Check taps again after returning to portrait.

Record the exact hardware revision, firmware and mod version. Clara HD, Clara 2E and Clara BW P365 have not been tested on hardware. A successful build or symbol check does not change that status.

## Logs and debugging

Startup and loader messages go to syslog through `nh_log`. If you have shell access, use `logread` and include the NickelRotate and NickelHook messages relevant to the failure. NickelRotate does not write a dedicated onboard log or per-touch traces.

Old `diagnostics` and `observe-touch` markers are ignored. Known legacy log files, including the temporary popup-width trace, are removed during uninstall.

## Pull requests

- Add user-visible changes to `CHANGELOG.md`.
- Update the annotations for every changed hook or lookup covered by NickelBench. Check touch-library changes against extracted firmware.
- Keep rotation and touch hooks in the same required dependency group on every model that uses the touch correction.
- Run `./build.sh` and `./test.sh`.
- State the device and firmware actually tested, or state that hardware testing is pending. Include relevant syslog output when available.

## Releases

Move the changes from `Unreleased` into a complete `## vX.Y` section before tagging that version. The release workflow runs the checks, extracts the section matching the tag and publishes `KoboRoot.tgz`. It fails if that section is missing or empty.

The checks workflow also uploads the library for inspection. The install package contains only the plugin, the onboard instructions and the uninstall marker.
