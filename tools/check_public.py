"""Firmware-free public checks. ARM models require an explicit Zig and dependency directory."""
from __future__ import annotations

import argparse
import ast
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from prepare_public import inspect_files, selected_files

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, help='Directory containing Zig 0.13.0 executable')
    parser.add_argument('--python-deps', type=Path, help='Directory installed from requirements-build.txt')
    args = parser.parse_args()
    if bool(args.toolchain) != bool(args.python_deps):
        parser.error('--toolchain and --python-deps must be provided together')
    if sys.flags.optimize or os.environ.get('PYTHONOPTIMIZE'):
        raise RuntimeError('Assertions must remain enabled')
    names = selected_files(ROOT)
    inspect_files(ROOT, names)
    sdk_files = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'sdk').rglob('*')
                 if p.is_file() and '__pycache__' not in p.parts}
    if sdk_files - set(names):
        raise ValueError('SDK files missing from public selection: ' + ', '.join(sorted(sdk_files - set(names))))
    upstream = ROOT / 'sdk/abi3/reference/upstream'
    from hashlib import sha256
    for item in json.loads((upstream / 'provenance.json').read_text(encoding='utf-8')):
        if sha256((upstream / item['path']).read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Upstream reference changed: ' + item['path'])
    for name in names:
        path = ROOT / name
        if path.suffix == '.py':
            ast.parse(path.read_text(encoding='utf-8-sig'), filename=name)
        elif path.suffix == '.json':
            json.loads(path.read_text(encoding='utf-8-sig'))
    work = ROOT / 'work'
    work.mkdir(exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='public-check-', dir=work))
    report = dict(complete=False, hardware_tested=False, firmware_used=False,
                  public_files=len(names), arm_model='not_run', stages=[])
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONNOUSERSITE='1')

    def stage(name, command):
        result = subprocess.run(list(map(str, command)), env=env, cwd=run, capture_output=True)
        (run / (name + '.stdout.txt')).write_bytes(result.stdout)
        (run / (name + '.stderr.txt')).write_bytes(result.stderr)
        report['stages'].append(dict(name=name, exit_code=result.returncode))
        if result.returncode:
            raise RuntimeError(f'{name} failed; see {run}')

    try:
        stage('acceptance-parser', [sys.executable, '-B', ROOT / 'sdk/abi3/tests/test_acceptance.py'])
        stage('publication-boundaries', [sys.executable, '-B', ROOT / 'tools/tests/test_public.py'])
        stage('recorded-provenance', [sys.executable, '-B', ROOT / 'tools/audit_sources.py',
              '--manifest-only', '--output', run / 'source-audit.json'])
        if args.toolchain:
            from integrate import verify_python_dependencies
            zig = args.toolchain.resolve() / ('zig.exe' if os.name == 'nt' else 'zig')
            deps = args.python_deps.resolve()
            if not zig.is_file() or not deps.is_dir():
                raise FileNotFoundError('Missing configured toolchain or dependency directory')
            if subprocess.check_output([str(zig), 'version'], text=True).strip() != '0.13.0':
                raise ValueError('Requires Zig 0.13.0')
            report['python_dependency_versions'] = verify_python_dependencies(deps)
            shutil.copytree(ROOT / 'sdk', run / 'sdk', ignore=shutil.ignore_patterns('__pycache__'))
            env.update(PYTHONPATH=os.pathsep.join([str(run / 'sdk'), str(deps)]),
                       MIWEAR_BUILD_ROOT=str(run),
                       ZIG_LOCAL_CACHE_DIR=str(run / 'zig-cache/local'),
                       ZIG_GLOBAL_CACHE_DIR=str(run / 'zig-cache/global'))
            stage('abi1-lifecycle-arm-model', [sys.executable, '-B', '-s',
                  run / 'sdk/abi1/tools/verify_contract_arm.py', '--zig', zig])
            model = json.loads((run / 'sdk/abi1/sdk-build/sdk-contract-verification.json').read_text())
            report['arm_model'] = {k: model[k] for k in ['cases', 'checks', 'failure_line']}
        report['complete'] = True
    finally:
        (run / 'run.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('PASS: ' + str(run / 'run.json'))
    if not args.toolchain:
        print('ARM model not run: pass --toolchain and --python-deps. No firmware/device acceptance claimed.')


if __name__ == '__main__':
    main()
