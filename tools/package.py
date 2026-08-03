#!/usr/bin/env python3
from pathlib import Path
import hashlib
import json
import zipfile

root = Path(__file__).resolve().parents[1]
out = root.parent
secret_names = {'setup-key.pem', 'factory-private.json', 'label-qr-uri.txt'}
excluded_parts = {'_build', 'build', '.gradle', '.tools', 'artifacts', '__pycache__'}


def included_files(base: Path, include_manifest: bool = True):
    for file in sorted(base.rglob('*')):
        if not file.is_file():
            continue
        relative = file.relative_to(base)
        if any(part in excluded_parts for part in relative.parts):
            continue
        if file.name in secret_names:
            continue
        if not include_manifest and file.name == 'MANIFEST.sha256':
            continue
        yield file


def write_manifest(base: Path) -> Path:
    lines = []
    for file in included_files(base, include_manifest=False):
        digest = hashlib.sha256(file.read_bytes()).hexdigest()
        lines.append(f'{digest}  {file.relative_to(base).as_posix()}')
    manifest = base / 'MANIFEST.sha256'
    manifest.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return manifest


write_manifest(root)
write_manifest(root / 'android')

artifacts = {}
for name, base in [
    ('HomeGuard-S3-Build-0013.zip', root),
    ('HomeGuard-S3-Android-Build-0013.zip', root / 'android'),
]:
    path = out / name
    if path.exists():
        path.unlink()
    prefix = Path(root.name) if base == root else Path('HomeGuard-S3-Android-Build-0013')
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for file in included_files(base):
            archive.write(file, prefix / file.relative_to(base))
    with zipfile.ZipFile(path) as archive:
        bad = archive.testzip()
        if bad is not None:
            raise RuntimeError(f'ZIP CRC failure: {bad}')
        names = archive.namelist()
        forbidden = [entry for entry in names if Path(entry).name in secret_names]
        if forbidden:
            raise RuntimeError(f'Factory secret leaked into ZIP: {forbidden}')
        if not any(Path(entry).name == 'MANIFEST.sha256' for entry in names):
            raise RuntimeError(f'MANIFEST.sha256 missing from {name}')
        leaked_build = [entry for entry in names if any(part in excluded_parts for part in Path(entry).parts)]
        if leaked_build:
            raise RuntimeError(f'Generated build directory leaked into ZIP: {leaked_build[:5]}')
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    (out / (name + '.sha256')).write_text(f'{digest}  {name}\n', encoding='utf-8')
    artifacts[name] = {'sha256': digest, 'size': path.stat().st_size, 'zip_integrity': True}
    print(name, digest, path.stat().st_size)

validation_path = out / 'HomeGuard-S3-Build-0013-validation.json'
validation = json.loads(validation_path.read_text(encoding='utf-8'))
validation['artifacts'] = artifacts
validation_path.write_text(json.dumps(validation, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
