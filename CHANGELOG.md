# Changelog

## Unreleased

### Added

- Manual reader rotation for Clara BW and Clara Colour on firmware 4.45 and newer within 4.x.
- Experimental support for Clara HD and Clara 2E on firmware 4.38.23684 and newer within 4.x. These models have not been tested on hardware.
- Support for the Clara BW P365 revision, also untested on hardware.
- A disable marker and NickelHook uninstall and startup recovery support.
- NickelBench annotations and build checks, GitHub check and release workflows, and contribution documentation.

### Fixed

- Misaligned landscape touch coordinates on Clara BW and Clara Colour.
- The extra separator above Portrait when Auto rotation is hidden.
- The initial Portrait selection and popup width for selected icons and translated labels, with 12 pixels of extra padding after each label.
- Popup width changing on open or after switching orientation. The constructor now stabilizes the hidden geometry used by Nickel's native width calculation.
