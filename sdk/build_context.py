"""Internal builders must run on the disposable tree made by integrate.py."""
import os
from pathlib import Path


def require_build_tree(module_root):
    configured = os.environ.get('MIWEAR_BUILD_ROOT')
    if not configured or not module_root.resolve().is_relative_to(Path(configured).resolve()):
        raise RuntimeError('Use python tools/integrate.py from the project root')
