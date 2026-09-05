# About NickelRotate

Technical details for contributors. For installation and compatibility, see [README.md](README.md).

## Reader permissions

`src/nickelrotate.cc` uses eight NickelHook PLT hooks. A thread-local permission scope exposes rotation while Nickel calculates reader permissions or updates the reading menu. Sensor queries outside that scope keep their original behaviour. A leftover `ForceAllowLandscape` setting is ignored inside the permission scope without changing the configuration file.

## Touch mapping

The touch hooks preserve Kobo's panel mapping. BW and Colour use the opposite landscape quarter-turn from Qt to correct the alignment observed in device testing. The experimental HD profile uses that correction too; the 2E profile preserves Nickel's complete native touch transform. These older-model profiles are inferred from the firmware rotation tables and [FBInk's device mappings](https://github.com/NiLuJe/FBInk/blob/master/fbink_device_id.c), and still need hardware validation. Orientation changes are read on the GUI thread and shared with the touch thread through an atomic value. Portrait preserves the panel mapper's output.

## Rotation popup

The popup constructor hook hides the separator above Portrait. The selection hook highlights the current orientation when Auto would be selected, then expands the popup to fit its visible layout. The constructor hook preserves the ARM return value. Hook targets must be globally bound functions; NickelHook rejects weak symbols such as the generated `setupUi` function.

## Controller pointers

The controller hook uses exported RTTI to cross-cast `AbstractController` to `QObject`. It does not assume the two base pointers have the same address.

## Dependencies

[NickelHook](https://github.com/pgaskin/NickelHook) is a Git submodule pinned to `bcd90ba94d90d22562fb64ef2920ed3323c1dae5`. Its MIT licence is included in the submodule and install package. Firmware binaries are not included in this repository or the package.
