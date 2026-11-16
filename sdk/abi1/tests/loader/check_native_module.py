"""Run exact firmware insmod/rmmod logic against a generated tiny module.

External file IO, allocation, cache maintenance and locks are explicit stubs.
This is not retail execution and does not simulate hardware or radio.
"""
from pathlib import Path
import json
import struct
from collections import deque
from emulate_entrypoints import Trace, DATA, STATE, ERRNO, STOP
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR, UC_ARM_REG_R11
ROOT = Path(__file__).resolve().parent

def run_case(reject=False, blob_override=None, setup_extra=None, expected_unload=0, after_init=None):
    if blob_override is None:
        raise ValueError('Supply the explicitly built NS6 candidate ELF')
    blob = blob_override
    t = Trace()
    heap = 0x20020000
    allocations = {}
    events = []
    recent = deque(maxlen=12)
    def allocate(size, align=8):
        nonlocal heap
        heap = (heap + align - 1) & -align
        ptr = heap
        heap += max(size, 8)
        assert heap < 0x200d0000
        allocations[ptr] = size
        return ptr
    def zero(t):
        t.uc.mem_write(t.reg(0), bytes([t.reg(1) & 255]) * t.reg(2))
        return t.reg(0)
    def compare(t):
        a = bytes(t.uc.mem_read(t.reg(0), t.reg(2)))
        b = bytes(t.uc.mem_read(t.reg(1), t.reg(2)))
        return 0 if a == b else (1 if a > b else 0xffffffff)
    def copy_name(t):
        n = t.reg(2)
        s = t.string(t.reg(1)).encode()
        t.uc.mem_write(t.reg(0), (s + b'\0' * n)[:n])
        return t.reg(0)
    def stat(t):
        ptr = t.reg(1)
        t.uc.mem_write(ptr, b'\0' * 0x60)
        t.uc.mem_write(ptr + 8, struct.pack('<I', 0x8000))
        t.uc.mem_write(ptr + 24, struct.pack('<Q', len(blob)))
        return 0
    def read_part(t):
        sp = t.uc.reg_read(UC_ARM_REG_SP)
        offset = struct.unpack('<Q', bytes(t.uc.mem_read(sp, 8)))[0]
        size = t.reg(2)
        assert offset + size <= len(blob), (offset, size, len(blob))
        t.uc.mem_write(t.reg(1), blob[offset:offset+size])
        return 0
    def observe(uc, address, size, user):
        recent.append(hex(address))
        if address == 0x2c33e09c:
            events.append({'native_entry': hex(t.reg(3) & ~1),
                           'module_info': hex(t.uc.reg_read(UC_ARM_REG_R11) + 0x68)})
    t.uc.hook_add(UC_HOOK_CODE, observe)
    t.stubs = {
        0x2c34e50c: lambda t: 0, 0x2c34f670: lambda t: 0,
        0x2c34a0cc: lambda t: ERRNO,
        0x2c6a74c4: lambda t: 42, 0x2c6a730c: stat,
        0x2c6a6c2c: lambda t: 0, 0x2c350150: read_part,
        0x2c35a12c: lambda t: allocate(t.reg(0)),
        0x2c35a16c: lambda t: allocate(t.reg(0)),
        0x2c35a0c8: lambda t: allocate(t.reg(1), t.reg(0)),
        0x2c358260: lambda t: 0,
        0x2c8e3cb0: zero, 0x2c355a44: compare,
        0x2c355cc0: copy_name,
        0x2c8e3d00: lambda t: 0 if t.string(t.reg(0)) == t.string(t.reg(1)) else 1,
        0x2c357166: lambda t: 0,
        0x2c359780: lambda t: 0,
    }
    t.uc.mem_write(DATA, b'/tmp/vela_p3.elf\0')
    t.uc.mem_write(DATA + 0x100, b'vela_route_p3\0')
    if setup_extra:
        setup_extra(t, events, allocations)
    try:
        handle = t.run(0x2c33d3ac, [DATA, DATA + 0x100])
        assert events, 'module entry not reached'
        assert bool(handle) == (not reject), (handle, t.word(ERRNO))
        result = {'variant': 'negative_init' if reject else 'success',
                  'init_called': True, 'handle_returned': bool(handle), 'events': events}
        if handle:
            if after_init:
                after_init(t, events)
            t.stopped = False
            t.uc.reg_write(UC_ARM_REG_LR, STOP | 1)
            status = t.run(0x2c33a4a0, [handle])
            assert status == expected_unload
            assert (t.word(0x3c2ab8ec) == 0) == (expected_unload == 0)
            result['rmmod_return'] = status
            result['registry_empty_after_unload'] = expected_unload == 0
        else:
            assert t.word(0x3c2ab8ec) == 0
            result['registry_empty_after_rejection'] = True
        return result
    except Exception:
        print('recent instructions:', list(recent))
        raise

if __name__ == '__main__':
    results = {'evidence': 'Exact firmware loader and generated entry; external OS services stubbed',
               'device_tested': False, 'cases': [run_case(), run_case(True)]}
    (ROOT / 'native-p3' / 'loader-checks.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(json.dumps(results, indent=2))
