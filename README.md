# NickelRotate

Manual rotation for the built-in reader on Kobo Clara BW and Clara Colour. Choose Portrait or Landscape from the reading menu. Home, Library and general settings stay portrait.

NickelRotate is experimental. Rotation and touch have been tested on Clara BW and Clara Colour running firmware 4.45.23697.

## Compatibility

NickelRotate accepts firmware **4.45 and newer within 4.x**. Firmware 5.x is excluded. It identifies devices by hardware UUID, not serial prefix.

| Model | Revision | UUID suffix | Device testing |
| --- | --- | --- | --- |
| Clara BW | N365 | `391` | Rotation, touch and popup fixes confirmed with 0.1.8-dev on 4.45.23697 |
| Clara BW | P365 | `395` | Enabled, but not tested on a device |
| Clara Colour | N367 | `393` | Rotation and touch confirmed with 0.1.7-dev on 4.45.23697 |

Both Clara BW revisions are enabled. P365 uses the same mod implementation, but compatibility remains unverified without a device test. Devices with other hardware UUIDs stay inactive.

The required firmware hooks have been checked on 4.45.23697 and 4.46.23836. Later 4.x versions remain unverified. All eight hooks must resolve before the mod activates. The popup fixes added in 0.1.8-dev have been confirmed on the BW; they have not been retested on the Colour. Broader checks such as text selection, sleep/wake, EPUB and on-device behaviour on 4.46 remain incomplete.

## Installation and use

Remove the old `Allow rotation on all devices` binary patch before testing. NickelRotate does not undo that patch's changes to the rotation popup.

1. Copy the built `KoboRoot.tgz` to `.kobo` on a supported Kobo.
2. Safely eject and unplug the Kobo. Let it install the package.
3. Open a book and its reading menu, then tap the rotation button.
4. Choose Portrait or Landscape.

Auto rotation is hidden because these Clara models have no orientation sensor. When no orientation lock is saved, the popup highlights the current orientation. The popup expands to fit its visible labels and selected icons.

Kobo saves the reading orientation in its existing preference. NickelRotate permits landscape in the reader, table of contents, advanced font settings, annotation entry and bookmarks. Kobo's controller restrictions still apply, including for audiobooks. Home, Library, general settings and unknown controllers stay portrait. Returning to the reader restores the reading preference.

## Disable or uninstall

To disable NickelRotate, create an empty `.adds/nickel-rotate/disabled` file and reboot. Delete that file and reboot to enable it again.

To uninstall, delete `.adds/nickel-rotate/uninstall` and reboot. NickelRotate removes its plugin and known support files on startup. It preserves other files in that directory.

NickelHook provides a startup failsafe. It does not detect touch problems found later while reading. If touches are misaligned, reconnect by USB and disable or uninstall the mod.

The old `diagnostics` and `observe-touch` markers are ignored. The current build does not write touch logs. Old diagnostic logs remain until uninstall.

## Build

Clone this repository with `--recurse-submodules`. For an existing checkout, run `git submodule update --init --recursive` to fetch the pinned NickelHook dependency.

Run `./build.sh` with a running Podman or Docker engine. It uses a pinned NickelBench image containing NickelTC and Qt 5.2.1. The outputs are `KoboRoot.tgz` and `src/libnickelrotate.so`. The build does not install anything on a connected device.

## Checks

Run `make test` for local C++ policy tests with address and undefined-behaviour sanitizers. Run `./test.sh` for those tests plus the production hooks tested against simulated Nickel callers and host Qt. These tests cover device and firmware gates, reader permissions, touch transforms, popup selection and sizing, translated labels, disabled passthrough and exception cleanup. They do not emulate Kobo's firmware or touch hardware.

Run `python3 test/verify_package.py` to check archive paths, file modes and payload bytes. Run `python3 test/verify_firmware.py PATH_TO_LIBNICKEL PATH_TO_LIBKEVDEVTOUCH src/libnickelrotate.so` to check the eight hook call sites, symbol bindings, controller RTTI, plugin exports and ARM ABI against extracted firmware.

On a device, check taps near every corner, page turns, swipes, text selection and reading controls in both EPUB and KEPUB. Check the table of contents and reading dialogs, then return to Home, Library and settings. Repeat after sleep, closing the book and rebooting. Check taps again after returning to portrait.

## Implementation

`src/nickelrotate.cc` uses eight NickelHook PLT hooks. A thread-local permission scope exposes rotation while Nickel calculates reader permissions or updates the reading menu. Sensor queries outside that scope keep their original behaviour. A leftover `ForceAllowLandscape` setting is ignored inside the permission scope without changing the configuration file.

The touch hooks preserve Kobo's panel mapping and replace its final orientation transform. Landscape uses the opposite quarter-turn from Qt to correct the alignment observed in device testing. Orientation changes are read on the GUI thread and shared with the touch thread through an atomic value. Portrait preserves the panel mapper's output.

The popup constructor hook hides the separator above Portrait. The selection hook highlights the current orientation when Auto would be selected, then expands the popup to fit its visible layout. The constructor hook preserves the ARM return value. Hook targets must be globally bound functions; NickelHook rejects weak symbols such as the generated `setupUi` function.

The controller hook uses exported RTTI to cross-cast `AbstractController` to `QObject`. It does not assume the two base pointers have the same address.

[NickelHook](https://github.com/pgaskin/NickelHook) is a Git submodule pinned to `bcd90ba94d90d22562fb64ef2920ed3323c1dae5`. Its MIT licence is included in the submodule and install package. The build container is pinned by digest in `build.sh`. Firmware binaries are not included in this repository or the package.
