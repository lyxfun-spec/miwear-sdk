"""Resolve an archived repository-relative path without changing historical reports."""
from pathlib import Path, PurePosixPath
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', help='Old repository-relative path using forward slashes')
    args = parser.parse_args()
    old = PurePosixPath(args.path)
    if old.is_absolute() or '..' in old.parts or '\\' in args.path or ':' in args.path:
        parser.error('Provide a repository-relative path using forward slashes')
    manifest = ROOT / 'local-private/project/layout-moves.json'
    if not manifest.is_file():
        parser.error('Private path map is unavailable in this checkout')
    moves = json.loads(manifest.read_text(encoding='utf-8'))['moves']
    resolved = old
    for name in sorted(moves, key=len, reverse=True):
        if old.is_relative_to(PurePosixPath(name)):
            resolved = PurePosixPath(moves[name]['destination']) / old.relative_to(name)
            break
    target = ROOT / str(resolved)
    if not target.exists():
        parser.error('Resolved path does not exist: ' + resolved.as_posix())
    print(target)


if __name__ == '__main__':
    main()
