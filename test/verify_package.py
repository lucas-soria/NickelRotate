#!/usr/bin/env python3
"""Ensure our install archive contains only the intended plugin and marker files."""
import pathlib
import tarfile

root = pathlib.Path(__file__).parent.parent
expected = {
    'usr/local/Kobo/imageformats/libnickelrotate.so': root / 'src/libnickelrotate.so',
    'mnt/onboard/.adds/nickel-rotate/doc': root / 'res/doc',
    'mnt/onboard/.adds/nickel-rotate/uninstall': root / 'res/uninstall',
}
seen = set()
with tarfile.open(root / 'KoboRoot.tgz', 'r:gz') as archive:
    for item in archive:
        name = item.name.removeprefix('./')
        assert item.isfile() and name in expected and name not in seen, item.name
        expected_mode = 0o755 if name.endswith('.so') else 0o644
        assert item.mode == expected_mode and item.uid == 0 and item.gid == 0
        assert archive.extractfile(item).read() == expected[name].read_bytes()
        seen.add(name)
assert seen == set(expected)
print('PASS: archive paths, modes, ownership and exact payload bytes')
