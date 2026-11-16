"""Run selected real firmware instructions with explicit host-only API stubs.

No radio, device, filesystem, shell, or module loader operation is performed.
Passing traces prove only the selected machine-code dispatch/argument paths.
They do not prove watchface reachability or on-device compatibility.
"""
from pathlib import Path
import hashlib
import os
import json
import struct
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'python-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2
from unicorn.arm_const import UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

IMAGE = Path(os.environ['MIWEAR_S4_AP']).read_bytes()
BASE = 0x2C300000
STOP = 0x20000000
STATE = 0x20001000
DATA = 0x20002000
ERRNO = 0x20003000
EXPECTED_SHA = '86a8580db2f603101de017607b509b1968c10122c62c33323684bc13a54b9158'
assert hashlib.sha256(IMAGE).hexdigest() == EXPECTED_SHA

class Trace:
    def __init__(self):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.uc.mem_map(BASE, (len(IMAGE) + 4095) & ~4095)
        self.uc.mem_write(BASE, IMAGE)
        self.uc.mem_map(0x20000000, 0x100000)
        # Exact startup copy recovered at file 0x43c..0x4d8:
        # ROM [0xd660,0x31e60) -> PSRAM [0x3c200000,0x3c224800).
        self.uc.mem_map(0x3C200000, 0x300000)
        self.uc.mem_write(0x3C200000, IMAGE[0xD660:0x31E60])
        self.uc.reg_write(UC_ARM_REG_SP, 0x200FF000)
        self.uc.reg_write(UC_ARM_REG_LR, STOP | 1)
        self.stubs = {}
        self.events = []
        self.stopped = False
        self.uc.hook_add(UC_HOOK_CODE, self.hook)

    def reg(self, n):
        return self.uc.reg_read([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][n])

    def string(self, address):
        data = bytearray()
        while address and len(data) < 512:
            value = bytes(self.uc.mem_read(address + len(data), 1))
            if value == b'\0':
                break
            data.extend(value)
        return data.decode('utf-8', errors='replace')

    def word(self, address):
        return struct.unpack('<I', self.uc.mem_read(address, 4))[0]

    def hook(self, uc, address, size, user):
        if address == STOP:
            self.stopped = True
            uc.emu_stop()
        elif address in self.stubs:
            result = self.stubs[address](self)
            if result is not None:
                uc.reg_write(UC_ARM_REG_R0, result)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def run(self, entry, args):
        for register, value in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args):
            self.uc.reg_write(register, value)
        self.uc.emu_start(entry | 1, 0, count=getattr(self, 'instruction_budget', 20000))
        assert self.stopped, 'instruction budget exhausted before return'
        return self.reg(0)

