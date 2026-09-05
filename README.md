# NickelRotate

A [NickelHook](https://github.com/pgaskin/NickelHook) mod that adds manual rotation to the built-in reader on Kobo Clara HD, Clara 2E, Clara BW and Clara Colour. Choose Portrait or Landscape from the reading menu. Home, Library and general settings stay portrait.

NickelRotate is experimental. Rotation and touch have been tested on Clara BW and Clara Colour running firmware 4.45.23697.

> [!IMPORTANT]
> This mod targets Kobo software **4.x only**. See [Compatibility](#compatibility) for model-specific requirements and test results.

## What it does

The reading menu gains Kobo's native rotation button with Portrait and Landscape options. Auto rotation is hidden because these Clara models have no orientation sensor. When no orientation lock is saved, the popup highlights the current orientation. Its width fits both selected icon states, so switching options does not resize it.

Kobo saves the reading orientation in its existing preference. NickelRotate permits landscape in the reader, table of contents, advanced font settings, annotation entry and bookmarks. Kobo's controller restrictions still apply, including for audiobooks. Returning to the reader restores the reading preference.

## Why was this made?

The Clara has no native rotation control. Enabling the existing global rotation patch also affects other views and can leave touch coordinates misaligned. NickelRotate exposes rotation in the reader and corrects the touch transform on models that need it.

## Configuration

There is no configuration file. To disable NickelRotate, create an empty `.adds/nickel-rotate/disabled` file and reboot. Delete that file and reboot to enable it again.

## Compatibility

Clara BW and Colour require **4.45 or newer within 4.x**. Experimental Clara HD and 2E support requires **4.38.23684 or newer within 4.x**. Firmware 5.x is excluded. NickelRotate identifies devices by hardware UUID, not serial prefix.

| Model | Revision | UUID suffix | Device testing |
| --- | --- | --- | --- |
| Clara HD | N249 | `376` | Experimental; not tested on a device |
| Clara 2E | N506 | `386` | Experimental; not tested on a device |
| Clara BW | N365 | `391` | Rotation, touch and final popup sizing and padding confirmed with the v0.1 build on 4.45.23697 |
| Clara BW | P365 | `395` | Enabled, but not tested on a device |
| Clara Colour | N367 | `393` | Rotation and touch confirmed with 0.1.7-dev on 4.45.23697 |

All five model revisions above are enabled. P365 uses the same implementation as N365. HD, 2E and P365 remain unverified without device tests. Devices with other hardware UUIDs stay inactive.

The required firmware hooks have been checked on 4.38.23684, 4.38.23697, 4.45.23697 and 4.46.23836. Later 4.x versions remain unverified. All eight hooks must resolve before the mod activates. The final v0.1 popup fixes have been confirmed on the BW; they have not been retested on the Colour. Broader checks such as text selection, sleep/wake, EPUB and on-device behaviour on 4.46 remain incomplete.

## Safety

### Whole-mod boot failsafe

Before applying hooks, NickelHook renames the plugin to `libnickelrotate.so.failsafe`. After initialization finishes, it waits three seconds before restoring the filename. If initialization crashes or hangs, or the process exits before that restore, the next boot cannot load the plugin.

The startup failsafe does not detect touch problems found later while reading. If touches are misaligned, reconnect by USB and disable or uninstall the mod.

### Required hooks

Rotation and touch correction must work together. All eight hooks and the controller lookup are required. If a target cannot be resolved, NickelHook rolls back the hooks it applied. Unsupported models, firmware versions and unexpected screen geometry keep NickelRotate inactive.

## Build

With Docker or Podman running:

```sh
git submodule update --init --recursive
./build.sh
```

This builds `KoboRoot.tgz` and runs compatibility and package checks in the [NickelBench](https://github.com/nicoverbruggen/nickelbench) image. It does not install anything on a connected device. See [CONTRIBUTING.md](CONTRIBUTING.md) for commands and validation details.

## Install

Remove the old `Allow rotation on all devices` binary patch first. NickelRotate does not undo that patch's changes to the rotation popup.

Copy `KoboRoot.tgz` to the Kobo's hidden `.kobo` folder. Safely eject, unplug and let the Kobo install the package. Reboot if installation does not start. Open a book and its reading menu, tap the rotation button, then choose Portrait or Landscape.

## Uninstall

Delete `.adds/nickel-rotate/uninstall` and reboot. NickelHook removes the plugin, and NickelRotate removes its known support files. Other files in that directory are preserved.

## Development

This repository was developed with assistance from large language models and tested on the devices listed above. Source and firmware checks do not replace device testing.

See [CONTRIBUTING.md](CONTRIBUTING.md) for building, testing and pull requests, [ABOUT.md](ABOUT.md) for implementation details, and [CHANGELOG.md](CHANGELOG.md) for changes.

## License

MIT. See [LICENSE](LICENSE). NickelHook is Copyright © 2020–2025 Patrick Gaskin and includes its own [MIT license](NickelHook/LICENSE).
