"""Export only individually selected UTF-8 source files; never publish or copy private assets."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = {'imports', 'work', 'outputs', 'local-assets', 'local-private', '.git'}
BINARY = {'.bin', '.elf', '.a', '.o', '.exe', '.dll', '.pdb', '.face', '.zip', '.png', '.jpg', '.pyc'}
PATTERNS = {
    'private-key': re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
    'github-credential': re.compile(r'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b'),
    'local-windows-path': re.compile(r'(?i)\b[A-Z]:[\\/](?:Users|Files|Codex|Python)[\\/]'),
    'local-unix-home': re.compile(r'/(?:Users|home)/[A-Za-z0-9_.-]+/'),
}


def selected_files(root):
    names = json.loads((root / 'config/public-files.json').read_text(encoding='utf-8'))['files']
    if not names or names != sorted(set(names)):
        raise ValueError('Public file list must be sorted, unique and nonempty')
    return names


def inspect_files(root, names):
    root = root.resolve()
    digests = {}
    for name in names:
        relative = PurePosixPath(name)
        if (relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name
                or name != relative.as_posix() or any(p in PRIVATE for p in relative.parts)
                or relative.suffix.lower() in BINARY):
            raise ValueError('Forbidden public path: ' + name)
        path = root / name
        if not path.resolve().is_relative_to(root) or any(p.is_symlink() for p in [path, *path.parents] if p != root):
            raise ValueError('Symlink or path escape: ' + name)
        blob = path.read_bytes()
        if b'\0' in blob:
            raise ValueError('Binary content: ' + name)
        content = blob.decode('utf-8-sig')
        for label, pattern in PATTERNS.items():
            match = pattern.search(content)
            if match:
                line = content.count('\n', 0, match.start()) + 1
                raise ValueError(f'{label}: {name}:{line} (content withheld)')
        digests[name] = hashlib.sha256(blob).hexdigest()
    return digests


def export(root, release=False):
    names = selected_files(root)
    digests = inspect_files(root, names)
    publication = json.loads((root / 'config/publication.json').read_text(encoding='utf-8'))
    if release and not publication['license_decision_complete']:
        raise ValueError('License choice is pending; use a local review candidate until it is resolved')
    outputs = root / 'outputs'
    outputs.mkdir(exist_ok=True)
    destination = Path(tempfile.mkdtemp(prefix='public-source-', dir=outputs))
    for name in names:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        blob = (root / name).read_bytes()
        if hashlib.sha256(blob).hexdigest() != digests[name]:
            raise RuntimeError('Source changed during export: ' + name)
        target.write_bytes(blob)
    actual = inspect_files(destination, names)
    if actual != digests:
        raise RuntimeError('Export verification failed')
    report = dict(schema=1, source_only=True, hardware_tested=False,
                  license_decision_complete=publication['license_decision_complete'],
                  release_requested=release, scanner_scope='Selected UTF-8 text, known credential and local-path patterns; manual review remains required',
                  files=digests)
    (destination / 'PUBLIC_EXPORT.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', action='store_true', help='Require a completed license decision; does not publish')
    parser.add_argument('--check', action='store_true', help='Check selected files without exporting')
    args = parser.parse_args()
    if args.check:
        print(f'PASS: {len(inspect_files(ROOT, selected_files(ROOT)))} selected public files')
    else:
        print(export(ROOT, args.release))


if __name__ == '__main__':
    main()
