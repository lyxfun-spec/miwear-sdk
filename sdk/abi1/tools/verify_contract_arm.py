"""Run the original 13 lifecycle contract cases as freestanding ARM code."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0

R = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', type=Path, required=True)
    args = parser.parse_args()
    out = R / 'sdk-build'
    out.mkdir(exist_ok=True)
    sources = ['src/ns_session.c', 'src/ns_controller.c', 'src/ns_sdk_flow.c',
               'src/freestanding.c', 'tests/sdk_contract.c', 'tests/contract_model_runtime.c']
    objects = []
    for source in sources:
        obj = out / ('contract_' + Path(source).stem + '.o')
        subprocess.run([str(args.zig), 'cc', '-target', 'thumb-freestanding-eabi',
            '-mcpu=cortex_m33', '-mfloat-abi=soft', '-std=c11', '-O2',
            '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
            '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
            '-Wall', '-Wextra', '-Werror', '-I', str(R / 'tests/model-libc'),
            '-I', str(R / 'include'), '-c', str(R / source), '-o', str(obj)], check=True)
        objects.append(obj)
    elf_path = out / 'contract-model.elf'
    subprocess.run([str(args.zig), 'ld.lld', '-T', str(R / 'tests/contract_model.ld'),
                    *map(str, objects), '-o', str(elf_path)], check=True)
    machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    machine.mem_map(0x100000, 0x100000)
    machine.mem_map(0x300000, 0x10000)
    machine.mem_map(0x400000, 0x1000)
    with elf_path.open('rb') as stream:
        elf = ELFFile(stream)
        symbols = {s.name: s for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name}
        assert not any(s['st_shndx'] == 'SHN_UNDEF' for s in symbols.values())
        for section in elf.iter_sections():
            if section['sh_flags'] & 2 and section['sh_type'] != 'SHT_NOBITS':
                machine.mem_write(section['sh_addr'], section.data())
        entry = elf['e_entry']
        values = [symbols[n]['st_value'] for n in ['contract_cases', 'contract_checks', 'contract_failure']]
    stopped = []
    def hook(uc, address, size, data):
        if address == 0x400000:
            stopped.append(True)
            uc.emu_stop()
    machine.hook_add(UC_HOOK_CODE, hook)
    machine.reg_write(UC_ARM_REG_SP, 0x30fff0)
    machine.reg_write(UC_ARM_REG_LR, 0x400001)
    machine.emu_start(entry, 0, count=2000000)
    cases, checks, failure = [struct.unpack('<I', machine.mem_read(address, 4))[0] for address in values]
    assert stopped and machine.reg_read(UC_ARM_REG_R0) == 0 and failure == 0 and cases == 13
    result = dict(cases=cases, checks=checks, failure_line=failure, hardware_tested=False,
                  scope='Original lifecycle contract assertions executed as ARM; backend evidence modeled',
                  sources={name: hashlib.sha256((R / name).read_bytes()).hexdigest() for name in sources})
    (out / 'sdk-contract-verification.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
