"""Boundary tests for source publication and public/historical provenance separation."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from prepare_public import inspect_files, export
import audit_sources
import sync_public


class Publication(unittest.TestCase):
    def test_git_selection_is_derived_from_reviewed_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'config').mkdir()
            manifest = root / 'config/public-files.json'
            manifest.write_text(json.dumps({'files': ['sdk/a.c']}))
            (root / '.gitignore').write_text(sync_public.expected_gitignore(root))
            sync_public.check(root)
            self.assertIn('!/sdk/a.c\n', (root / '.gitignore').read_text())
            manifest.write_text(json.dumps({'files': ['sdk/a.c', 'sdk/b.c']}))
            with self.assertRaises(ValueError):
                sync_public.check(root)
            manifest.write_text(json.dumps({'files': ['sdk/*.c']}))
            with self.assertRaises(ValueError):
                sync_public.expected_gitignore(root)

    def test_forbidden_and_secret_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ['../outside.py', '/absolute.py', 'imports/code.c', 'work/log.txt',
                         'local-private/project/state.json', 'local-assets/firmware.c',
                         'outputs/source/result.c', 'sdk/image.bin', 'C:/local.py']:
                with self.subTest(name=name), self.assertRaises(ValueError):
                    inspect_files(root, [name])
            path = root / 'source.py'
            for data in [b'\0binary', ('gh' + 'p_' + 'A' * 36).encode(), ('D:' + '/Files/private/code').encode()]:
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    inspect_files(root, ['source.py'])

    def test_export_is_allowlisted_and_release_gate_works(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'config').mkdir()
            (root / 'config/public-files.json').write_text(json.dumps({'files': ['source.c']}))
            (root / 'config/publication.json').write_text(json.dumps({'license_decision_complete': False}))
            (root / 'source.c').write_text('/* public */\n')
            (root / 'private.txt').write_text('private')
            with self.assertRaises(ValueError):
                export(root, release=True)
            out = export(root)
            self.assertEqual({p.name for p in out.iterdir()}, {'source.c', 'PUBLIC_EXPORT.json'})
            self.assertFalse(json.loads((out / 'PUBLIC_EXPORT.json').read_text())['hardware_tested'])

    def test_manifest_audit_does_not_claim_private_origin_verification(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'sdk').mkdir()
            (root / 'migration').mkdir()
            (root / 'sdk/a.c').write_text('/* active */')
            (root / 'migration/active-sources.json').write_text(json.dumps({'files': [
                dict(active='sdk/a.c', source='imports/missing.c', source_sha256='0' * 64)]}))
            old = audit_sources.ROOT
            try:
                audit_sources.ROOT = root
                result = audit_sources.audit(manifest_only=True)
                self.assertIsNone(result['all_origin_hashes_match'])
                self.assertFalse(result['frozen_origins_verified'])
                with self.assertRaises(FileNotFoundError):
                    audit_sources.audit()
            finally:
                audit_sources.ROOT = old


if __name__ == '__main__':
    unittest.main()
