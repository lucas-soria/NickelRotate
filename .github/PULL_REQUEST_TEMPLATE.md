## What and why

<!-- Describe the change and the problem it solves. -->

## Tested on

<!-- State the exact device and firmware tested. Say when hardware testing is pending. -->

- Device and revision:
- Firmware:
- Mod version:

## Logs

<!-- Include relevant NickelRotate / NickelHook syslog output if available.
     The mod does not write a dedicated onboard log file. -->

## Checklist

- [ ] User-visible changes have a CHANGELOG entry.
- [ ] Hooks and lookups covered by NickelBench have up-to-date annotations.
- [ ] Touch hook changes pass the local firmware verifier.
- [ ] Rotation and touch hooks remain one required dependency group on models that use the touch correction.
- [ ] `./build.sh` and `./test.sh` pass.
