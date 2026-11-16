"""Check frozen origins and classify every active SDK file; never writes imports."""
from pathlib import Path
import argparse
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(manifest_only=False):
    manifest = json.loads((ROOT / 'migration/active-sources.json').read_text(encoding='utf-8'))
    origins = {row['active']: row for row in manifest['files']}
    rows = []
    for name, row in origins.items():
        if not manifest_only and sha(ROOT / row['source']) != row['source_sha256']:
            raise ValueError('Frozen origin changed: ' + row['source'])
        if not (ROOT / name).is_file() and not row.get('retired'):
            raise FileNotFoundError('Mapped active file missing: ' + name)
    for path in sorted((ROOT / 'sdk').rglob('*')):
        if not path.is_file() or '__pycache__' in path.parts:
            continue
        name = path.relative_to(ROOT).as_posix()
        origin = origins.get(name)
        digest = sha(path)
        rows.append(dict(active=name, active_sha256=digest,
                         status=('byte_preserved' if origin['source_sha256'] == digest else 'adapted') if origin else 'new',
                         origin=origin))
    counts = {status: sum(row['status'] == status for row in rows) for status in ['byte_preserved', 'adapted', 'new']}
    return dict(all_origin_hashes_match=None if manifest_only else True,
                frozen_origins_verified=not manifest_only, counts=counts, files=rows,
                retired_origins=[row for row in origins.values() if row.get('retired')])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'work/source-audit.json')
    parser.add_argument('--manifest-only', action='store_true',
                        help='Classify against recorded hashes without claiming to verify private frozen origins')
    args = parser.parse_args()
    result = audit(args.manifest_only)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result['counts']))
