# About NickelRotate

Technical details for contributors. For installation and compatibility, see [README.md](README.md).

## Reader permissions

`src/nickelrotate.cc` uses eight NickelHook PLT hooks. A thread-local permission scope exposes rotation while Nickel calculates reader permissions or updates the reading menu. Sensor queries outside that scope keep their original behaviour. A leftover `ForceAllowLandscape` setting is ignored inside the permission scope without changing the configuration file.

## Touch mapping

The touch hooks preserve Kobo's panel mapping. BW and Colour use the opposite landscape quarter-turn from Qt to correct the alignment observed in device testing. The experimental HD profile uses that correction too; the 2E profile preserves Nickel's complete native touch transform. These older-model profiles are inferred from the firmware rotation tables and [FBInk's device mappings](https://github.com/NiLuJe/FBInk/blob/master/fbink_device_id.c), and still need hardware validation. Orientation changes are read on the GUI thread and shared with the touch thread through an atomic value. Portrait preserves the panel mapper's output.

## Rotation popup

The popup constructor hook hides the separator above Portrait and prepares the width before Nickel shows the menu. Nickel's native width is the hidden Auto label's text width plus three times the hidden Auto icon's geometry width. The device trace showed that icon changing from 100 to 41 pixels. This changed the native popup width from 463 to 286 pixels, after which the old selection hook expanded it to 316 pixels while visible.

The constructor adds 12 pixels to the right content margin of each visible label. It then measures the visible layout with each option selected, restores the current orientation, and fixes the hidden Auto icon's width so the native formula fits the wider layout. It rounds up by at most two pixels when the visible layout is wider than Auto's text. This keeps the initial size and the later native `setFixedSize` call consistent. Native height and positioning still use Nickel's own calculations. This width formula was inspected on 4.38.23684, 4.38.23697, 4.45.23697 and 4.46.23836.

The selection hook only replaces the hidden Auto highlight with the current orientation. Any mask that allows both a portrait and a landscape orientation, including 0, 3 and 15, selects Auto in Nickel and is replaced. Nickel also supplies the paired Portrait and Landscape masks, 5 and 10, which pass through unchanged. The constructor preserves the ARM return value. Hook targets must be globally bound functions; NickelHook rejects weak symbols such as the generated `setupUi` function.

## Controller pointers

The controller hook uses exported RTTI to cross-cast `AbstractController` to `QObject`. It does not assume the two base pointers have the same address.

## Dependencies

[NickelHook](https://github.com/pgaskin/NickelHook) is a Git submodule pinned to `bcd90ba94d90d22562fb64ef2920ed3323c1dae5`. Its MIT licence is included in the submodule and install package. Firmware binaries are not included in this repository or the package.
