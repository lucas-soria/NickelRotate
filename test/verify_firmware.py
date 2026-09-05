#!/usr/bin/env python3
"""Check the PLT hook contracts against local ELF32 ARM firmware and our build."""
import pathlib
import re
import struct
import sys


class Elf:
    def __init__(self, path):
        self.data = pathlib.Path(path).read_bytes()
        assert self.data[:7] == b'\x7fELF\x01\x01\x01', 'expected little-endian ELF32'
        assert struct.unpack_from('<H', self.data, 18)[0] == 40, 'expected ARM'
        offset = struct.unpack_from('<I', self.data, 32)[0]
        size, count, names = struct.unpack_from('<HHH', self.data, 46)
        self.sections = [struct.unpack_from('<10I', self.data, offset + i * size) for i in range(count)]
        self.names = self.section(names)
        self.named = {self.string(self.names, s[0]): i for i, s in enumerate(self.sections)}
        dynsym = self.sections[self.named['.dynsym']]
        strings = self.section(dynsym[6])
        data = self.section(self.named['.dynsym'])
        self.symbols = []
        for i in range(0, len(data), dynsym[9]):
            name, value, size, info, other, section = struct.unpack_from('<IIIBBH', data, i)
            self.symbols.append((self.string(strings, name), section, info))

    @staticmethod
    def string(data, offset):
        return data[offset:data.index(b'\0', offset)].decode('ascii')

    def section(self, index):
        s = self.sections[index]
        return self.data[s[4]:s[4] + s[5]]

    def definitions(self):
        return {name for name, section, _ in self.symbols if section}

    def plt(self):
        data = self.section(self.named['.rel.plt'])
        return {self.symbols[info >> 8][0] for _, info in struct.iter_unpack('<II', data) if info & 255 == 22}


if len(sys.argv) != 4:
    sys.exit('Usage: verify_firmware.py libnickel.so.1.0.0 libkevdevtouch.so libnickelrotate.so')
nickel, touch, plugin = map(Elf, sys.argv[1:])
source = (pathlib.Path(__file__).parent.parent / 'src/nickelrotate.cc').read_text()
hooks = re.findall(r'\.sym = "([^"]+)", \.sym_new = "([^"]+)", \.lib = "([^"]+)"', source)
assert len(hooks) == 8
for symbol, replacement, library in hooks:
    firmware = touch if library.endswith('libkevdevtouch.so') else nickel
    assert symbol in firmware.plt(), f'missing PLT call for {symbol}'
    # NickelHook requires STB_GLOBAL and STT_FUNC, even for defined symbols.
    info = next(info for name, _, info in firmware.symbols if name == symbol)
    assert info >> 4 == 1 and info & 15 == 2, f'NickelHook rejects symbol binding/type for {symbol}: {info:#x}'
    assert replacement in plugin.definitions(), f'missing replacement {replacement}'
assert '_ZTI18AbstractController' in nickel.definitions()
assert 'NickelHook' in plugin.definitions()
assert b'nh.nickelrotate' in plugin.data
assert struct.unpack_from('<I', plugin.data, 36)[0] & 0x400, 'expected hard-float ABI'
print('PASS: all eight firmware PLT hooks, controller RTTI, plugin exports and ARM hard-float ABI')
