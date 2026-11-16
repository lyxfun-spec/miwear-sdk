"""Strict ELF inspection shared by the ABI1 offline builders."""
def inspect_elf(path, raw=False):
    from elftools.elf.elffile import ELFFile
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        symbols = {s.name: s['st_value'] for s in table.iter_symbols() if s.name}
        assert not any(s.name and s['st_shndx'] == 'SHN_UNDEF' for s in table.iter_symbols())
        if raw:
            assert not any(s['sh_size'] and (
                (s['sh_flags'] & 2 and s.name != '.text') or
                s['sh_type'] in {'SHT_REL', 'SHT_RELA'}
            ) for s in elf.iter_sections()), 'Raw PIC requires .text only, without relocations'
        return elf.get_section_by_name('.text').data(), symbols
