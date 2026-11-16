"""MiWear's offline entrypoint. Builds only in work/, never installs or connects.

python tools/integrate.py --config config/local.example.json
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

# The coordinator also imports pyelftools for archive inspection. Protect the
# configured dependency tree in this process as well as in every child.
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]


def verify_python_dependencies(directory):
    """Public builds must not silently use globally installed model packages."""
    sys.path.insert(0, str(directory.resolve()))
    import elftools
    import unicorn
    modules = {'pyelftools': (elftools, '0.33'), 'unicorn': (unicorn, '2.1.4')}
    for name, (module, version) in modules.items():
        if (module.__version__ != version or
                not Path(module.__file__).resolve().is_relative_to(directory.resolve())):
            raise ValueError(f'{name} {version} must be installed in configured python_deps')
    return {name: module.__version__ for name, (module, _) in modules.items()}


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def files(path):
    return sorted(p for p in path.rglob('*') if p.is_file() and '__pycache__' not in p.parts)


def archive_symbols(path):
    """Inspect every object, including hidden private global definitions."""
    from elftools.elf.elffile import ELFFile
    blob = path.read_bytes()
    if blob[:8] != b'!<arch>\n':
        raise ValueError('Not an ordinary static archive: ' + str(path))
    cursor = 8
    result = {}
    while cursor < len(blob):
        header = blob[cursor:cursor + 60]
        if len(header) != 60 or header[58:] != b'`\n':
            raise ValueError('Invalid ar header')
        size = int(header[48:58])
        data = blob[cursor + 60:cursor + 60 + size]
        cursor += 60 + size + size % 2
        if not data.startswith(b'\x7fELF'):
            continue
        elf = ELFFile(BytesIO(data))
        table = elf.get_section_by_name('.symtab')
        for symbol in table.iter_symbols():
            if (symbol.name and symbol['st_shndx'] != 'SHN_UNDEF' and
                    symbol['st_info']['bind'] in {'STB_GLOBAL', 'STB_WEAK'}):
                result.setdefault(symbol.name, []).append({
                    'binding': symbol['st_info']['bind'],
                    'visibility': symbol['st_other']['visibility'],
                    'type': symbol['st_info']['type'], 'bytes': symbol['st_size'],
                })
    return result


def render_lua(abi1, abi3):
    """Render text hosts for fixture tests without making face packages."""
    out = abi1 / 'device-build'
    out.mkdir()
    module = (abi1 / 'engineering-build/ns_default_v6.elf').read_bytes()
    bootstrap = (abi1 / 'engineering-build/band_default_bootstrap.bin').read_bytes()
    s4 = (abi1 / 'device/s4_default_main.lua.in').read_text(encoding='utf-8')
    s4 = s4.replace('@MODULE_HEX@', module.hex()).replace('@MODULE_SIZE@', str(len(module)))
    rpc = (abi1 / 'device/band_default_rpc.lua.in').read_text(encoding='utf-8')
    rpc = rpc.replace('@NATIVE_HEX@', bootstrap.hex()).replace('@NATIVE_SIZE@', str(len(bootstrap)))
    band = (abi1 / 'reference/band_loader.lua.in').read_text(encoding='utf-8').replace('@RPC@', rpc)
    for before, after in [('B11 NFC / NS1', 'B11 NFC / NS6'),
                          ('Session core checks', 'SDK ABI1 / delay 0'),
                          ('No BLE / no workers', 'Policy 0 / manager held'),
                          ('RESULT: NS1 CORE CHECK', 'RESULT: NS6 PAGE CHECK'),
                          ('/tmp/b11_ns1_word.txt', '/tmp/b11_ns6_word.txt'),
                          ('Tap screen: RUN once', 'Tap: ACTIVATE / READ'),
                          ('READY: tap screen once', 'READY: tap to start/read')]:
        band = band.replace(before, after)
    (out / 's4-main.lua').write_bytes(s4.encode('utf-8'))
    (out / 'band11-main.lua').write_bytes(band.encode('utf-8'))
    out = abi3 / 'device-build'
    out.mkdir()
    bootstrap = (abi3 / 'build/bootstrap.bin').read_bytes()
    rpc = (abi3 / 'device/g1_rpc.lua.in').read_text(encoding='utf-8')
    rpc = rpc.replace('@NATIVE_HEX@', bootstrap.hex()).replace('@NATIVE_SIZE@', str(len(bootstrap)))
    band = (abi3 / 'reference/band_loader.lua.in').read_text(encoding='utf-8').replace('@RPC@', rpc)
    for before, after in [('B11 NFC / NS1', 'B11 NFC / G4R2'),
                          ('Session core checks', 'GATT foreground fixture'),
                          ('No BLE / no workers', 'Fixed bytes / runtime held'),
                          ('RESULT: NS1 CORE CHECK', 'RESULT: G4R2 SDK CHECK'),
                          ('/tmp/b11_ns1_word.txt', '/tmp/b11_g1_word.txt')]:
        band = band.replace(before, after)
    if '@RPC@' in band or '@NATIVE_' in band:
        raise ValueError('Unresolved Lua template placeholder')
    (out / 'band11-main.lua').write_bytes(band.encode('utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=ROOT / 'config/local.example.json')
    parser.add_argument('--compare', type=Path, help='Prior run.json: require identical candidate artifacts')
    parser.add_argument('--mode', choices=['public', 'historical'], default='public',
                        help='Public builds need no imports; historical mode also checks frozen local origins and archives')
    args = parser.parse_args()
    if sys.flags.optimize or os.environ.get('PYTHONOPTIMIZE'):
        raise RuntimeError('Offline proofs require Python assertions enabled')
    config = json.loads(args.config.read_text(encoding='utf-8-sig'))
    inputs = {key: (ROOT / config[key]).resolve() for key in
              (['research'] if args.mode == 'historical' else []) + ['zig', 'python_deps', 'band_ap', 's4_ap', 'lua']}
    for key, path in inputs.items():
        if not (path.is_dir() if key in {'research', 'python_deps'} else path.is_file()):
            raise FileNotFoundError(f'{key}: {path}')
    dependency_versions = verify_python_dependencies(inputs['python_deps']) if args.mode == 'public' else None
    baseline_path = ROOT / ('config/baselines.json' if args.mode == 'historical' else 'config/public-baselines.json')
    baselines = json.loads(baseline_path.read_text(encoding='utf-8'))
    for key in ['band_ap', 's4_ap']:
        if sha(inputs[key]) != baselines['firmware'][key]:
            raise ValueError('Unsupported firmware: ' + key)
    version = subprocess.check_output([str(inputs['zig']), 'version'], text=True).strip()
    if version != '0.13.0':
        raise ValueError('Requires Zig 0.13.0, got ' + version)
    # Verify frozen comparison inputs before using them. No imports script runs.
    for item in baselines['artifacts'] if args.mode == 'historical' else []:
        if sha(ROOT / item['source']) != item['sha256']:
            raise ValueError('Frozen baseline mismatch: ' + item['source'])
    work = ROOT / 'work'
    work.mkdir(exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='integration-', dir=work))
    report = {'schema': 2, 'mode': args.mode, 'complete': False, 'hardware_tested': False,
              'frozen_origins_verified': args.mode == 'historical',
              'historical_archive_definitions_verified': args.mode == 'historical',
              'python_dependency_versions': dependency_versions,
              'started_utc': datetime.now(timezone.utc).isoformat(),
              'run_directory': str(run), 'compiler': version,
              'config_sha256': sha(args.config), 'inputs': {}, 'stages': [],
              'active_sources': {p.relative_to(ROOT).as_posix(): sha(p) for p in files(ROOT / 'sdk')},
              'orchestration': {p.relative_to(ROOT).as_posix(): sha(p) for p in
                                [Path(__file__).resolve(), ROOT / 'tools/audit_sources.py',
                                 ROOT / 'migration/active-sources.json', baseline_path]}}
    report['inputs'] = {key: {'path': str(path), 'sha256': sha(path)} for key, path in inputs.items() if path.is_file()}
    # Hash actual Python dependency files without loading/writing their caches.
    dependency_files = {p.relative_to(inputs['python_deps']).as_posix(): sha(p)
                        for p in files(inputs['python_deps'])}
    write_json(run / 'python-dependencies.json', dependency_files)
    report['python_dependencies_sha256'] = sha(run / 'python-dependencies.json')
    report['python'] = sys.version
    shutil.copytree(ROOT / 'sdk', run / 'sdk', ignore=shutil.ignore_patterns('__pycache__'))
    if any(sha(run / name) != digest for name, digest in report['active_sources'].items()):
        raise RuntimeError('Active sources changed while making the isolated build tree')
    research = run / 'inputs'
    (research / 'band11').mkdir(parents=True)
    shutil.copyfile(inputs['band_ap'], research / 'band11/179-vela_ap.bin')
    shutil.copyfile(inputs['s4_ap'], research / 'vela_ap.bin')
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONNOUSERSITE='1',
               PYTHONPATH=os.pathsep.join([str(run / 'sdk'), str(inputs['python_deps'])]),
               ZIG_LOCAL_CACHE_DIR=str(run / 'zig-cache/local'),
               ZIG_GLOBAL_CACHE_DIR=str(run / 'zig-cache/global'),
               MIWEAR_S4_AP=str(research / 'vela_ap.bin'), MIWEAR_BUILD_ROOT=str(run))
    a1, a3 = run / 'sdk/abi1', run / 'sdk/abi3'
    def stage(name, command, expected_failure=False):
        start = time.monotonic()
        result = subprocess.run([str(x) for x in command], cwd=run, env=env, capture_output=True)
        stdout = run / 'logs' / (name + '.stdout.txt')
        stderr = run / 'logs' / (name + '.stderr.txt')
        stdout.parent.mkdir(exist_ok=True)
        stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
        report['stages'].append({'name': name, 'exit_code': result.returncode,
            'expected_failure': expected_failure, 'seconds': round(time.monotonic() - start, 2),
            'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)})
        write_json(run / 'run.json', report)
        print(f'{name}: exit {result.returncode}', flush=True)
        if (result.returncode != 0) != expected_failure:
            raise RuntimeError(f'{name} failed; see {stderr}')
        return result
    def script(root, name, *options):
        return [sys.executable, '-B', '-s', root / name, *options]
    try:
        stage('source-provenance', script(ROOT, 'tools/audit_sources.py',
              *(['--manifest-only'] if args.mode == 'public' else []), '--output', run / 'source-audit.json'))
        stage('abi1-libraries', script(a1, 'tools/build_library.py', '--research', research, '--zig', inputs['zig']))
        # NS6 shell consumes the rebuilt archives; this work manifest describes candidates.
        libdir = a1 / 'sdk-export/lib'
        libdir.mkdir(parents=True)
        for target in ['band11', 's4']:
            shutil.copyfile(a1 / 'sdk-build' / f'libns_session_sdk_{target}.a', libdir / f'libns_session_sdk_{target}.a')
        write_json(a1 / 'sdk-export/manifest.json', {'hardware_tested': False,
            'files': {'lib/' + p.name: sha(p) for p in libdir.iterdir()}})
        stage('abi1-hosts', script(a1, 'tools/build_default.py', '--research', research, '--zig', inputs['zig']))
        stage('abi1-lifecycle-model', script(a1, 'tools/verify_contract_arm.py', '--zig', inputs['zig']))
        stage('abi1-arm-ui-ble-model', script(a1, 'tools/verify_default.py', '--research', research))
        stage('abi1-band-ui-model', script(a1, 'tests/band_default_ui_model.py', '--research', research))
        stage('abi1-relocations', script(a1, 'tools/verify_local_relocations.py', '--research', research))
        stage('abi3-library-host-model-build', script(a3, 'tools/build.py', '--research', research, '--zig', inputs['zig']))
        for name in ['prove_payload_rom', 'prove_attribute_rom', 'prove_retirement_rom', 'verify_resources', 'verify_sdk']:
            stage('abi3-' + name, script(a3, 'tools/' + name + '.py', '--research', research))
        stage('abi3-public-example', script(a3, 'tools/check_public_example.py', '--research', research, '--zig', inputs['zig']))
        stage('abi3-acceptance-parser', script(a3, 'tests/test_acceptance.py'))
        render_lua(a1, a3)
        stage('abi1-lua', script(a1, 'tools/verify_default_lua.py', '--lua', inputs['lua']))
        entry = a3 / 'build/lua-tests.lua'
        entry.write_text('G1_ROOT=' + json.dumps(a3.as_posix()) + '\nassert(loadfile(G1_ROOT.."/tests/rpc.lua"))()\n', encoding='utf-8')
        stage('abi3-lua', [inputs['lua'], entry])
        sys.path.insert(0, str(inputs['python_deps']))
        comparisons = []
        for item in baselines['artifacts']:
            candidate = run / item['candidate']
            row = {'name': item['name'], 'baseline_sha256': item['sha256'],
                   'candidate_sha256': sha(candidate), 'byte_identical': sha(candidate) == item['sha256']}
            if candidate.suffix == '.a' and args.mode == 'historical':
                old, new = archive_symbols(ROOT / item['source']), archive_symbols(candidate)
                row.update(definitions_identical=old == new, added_symbols=sorted(new.keys() - old.keys()),
                           removed_symbols=sorted(old.keys() - new.keys()),
                           changed_symbols=sorted(k for k in old.keys() & new.keys() if old[k] != new[k]))
                if old != new:
                    raise RuntimeError('Archive definition drift: ' + item['name'])
            comparisons.append(row)
        report['baseline_comparison'] = comparisons
        write_json(run / 'archive-symbols.json', {item['name']: archive_symbols(run / item['candidate'])
                    for item in baselines['artifacts'] if item['candidate'].endswith('.a')})
        b1 = a1 / 'sdk-export/lib/libns_session_sdk_band11.a'
        b3 = a3 / 'sdk-export/lib/libns_link_v3_band11_candidate.a'
        s1, s3 = archive_symbols(b1), archive_symbols(b3)
        collisions = sorted(s1.keys() & s3.keys())
        report['abi_boundary'] = {'mixed_link_supported': False, 'shared_defined_symbols': collisions,
                                  'public_headers_interchangeable': False}
        if collisions:
            result = stage('mixed-link-rejection', [inputs['zig'], 'ld.lld', '-r', '--whole-archive', b1, b3,
                           '--no-whole-archive', '-o', run / 'mixed.o'], expected_failure=True)
            if b'duplicate symbol' not in result.stderr:
                raise RuntimeError('Mixed link failed for an unexpected reason')
        artifact_paths = [item['candidate'] for item in baselines['artifacts']]
        artifact_paths += ['sdk/abi1/sdk-build/band11_component.elf', 'sdk/abi1/sdk-build/s4_component.elf',
                          'sdk/abi1/sdk-build/band11_public.elf', 'sdk/abi1/sdk-build/s4_public.elf',
                          'sdk/abi3/build/public-example-linked.o']
        report['candidate_artifacts'] = {p: sha(run / p) for p in artifact_paths}
        if args.compare:
            previous = json.loads(args.compare.read_text(encoding='utf-8'))
            locked = ['mode', 'active_sources', 'orchestration', 'config_sha256', 'compiler',
                      'inputs', 'python_dependencies_sha256', 'python']
            if not previous.get('complete') or any(previous.get(key) != report[key] for key in locked):
                raise ValueError('Comparison requires a successful run with identical source, configuration and dependency inputs')
            if previous['candidate_artifacts'] != report['candidate_artifacts']:
                raise RuntimeError('Reproducibility check failed')
            report['reproduced_from'] = str(args.compare.resolve())
        report['complete'] = True
        if any(sha(ROOT / name) != digest for name, digest in
               {**report['active_sources'], **report['orchestration']}.items()):
            raise RuntimeError('Sources changed during validation; rerun before exporting a candidate')
        # Local SDK candidates only. No device bundle, firmware or toolchain distributed.
        destination = ROOT / 'outputs' / run.name
        destination.mkdir(parents=True)
        (destination / 'README.md').write_text(
            '# MiWear local SDK candidate\n\nOffline validation only; hardware_tested=false. '
            'ABI1 and ABI3 require separate matching hosts; do not mix archives. '
            'No device package, firmware, toolchain, account state or historical evidence included. '
            'Full active source and third-party notices are in source/. '
            'Canopus Lua combinations retain AGPL-3.0-only obligations.\n', encoding='utf-8')
        for label, source in [('abi1-band11', b1), ('abi1-s4', a1 / 'sdk-export/lib/libns_session_sdk_s4.a'), ('abi3-band11', b3)]:
            target = destination / label
            (target / 'lib').mkdir(parents=True)
            write_json(target / 'component.json', dict(
                abi=1 if label.startswith('abi1') else 3, hardware_tested=False,
                profile='M2312W1 / 3.111.032' if label.endswith('s4') else 'M2617B1 / 4.100.179',
                library_sha256=sha(source), mixed_abi_link_supported=False))
            shutil.copyfile(source, target / 'lib' / source.name)
            if label.startswith('abi1'):
                shutil.copytree(a1 / 'include', target / 'platform-adapter-include')
                for public in ['ns_session.h', 'ns_controller.h', 'ns_sdk_flow.h']:
                    (target / 'include').mkdir(exist_ok=True)
                    shutil.copyfile(a1 / 'include' / public, target / 'include' / public)
                shutil.copytree(a1 / 'templates', target / 'platform-component')
                shutil.copytree(a1 / 'examples', target / 'examples')
            else:
                shutil.copytree(a3 / 'public', target / 'include')
                shutil.copytree(a3 / 'examples', target / 'examples')
        for relative in report['active_sources']:
            target = destination / 'source' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(run / relative, target)
        from prepare_public import selected_files
        for relative in selected_files(ROOT):
            if relative.startswith('sdk/'):
                continue
            target = destination / 'source' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        shutil.copyfile(run / 'source-audit.json', destination / 'source-audit.json')
        report['output_directory'] = str(destination)
        write_json(run / 'run.json', report)
        shutil.copyfile(run / 'run.json', destination / 'run.json')
        write_json(destination / 'files.json', {p.relative_to(destination).as_posix(): sha(p) for p in files(destination)})
        print('PASS: ' + str(run / 'run.json'), flush=True)
    except Exception as error:
        report['complete'] = False
        report['failure'] = str(error)
        write_json(run / 'run.json', report)
        print('FAIL: ' + str(run / 'run.json'), file=sys.stderr)
        raise


if __name__ == '__main__':
    main()
