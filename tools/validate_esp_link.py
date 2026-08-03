#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
from esp_mock_sdk import MOCKS
mocks = MOCKS

with tempfile.TemporaryDirectory(prefix='homeguard-esp-link-') as temporary:
    temp = Path(temporary)
    mock = temp / 'mock-sdk'
    for relative, text in mocks.items():
        path = mock / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    entry = temp / 'link_main.cpp'
    entry.write_text('int main(){return 0;}\n', encoding='utf-8')
    includes = [
        '-I' + str(mock),
        '-I' + str(root / 'firmware/include'),
        '-I' + str(root / 'firmware/esp-idf/main'),
    ]
    includes.extend('-I' + str(path) for path in sorted((root / 'firmware/esp-idf/components').glob('*/include')))
    host_library = root / '_build/libhomeguard_core.a'
    if not host_library.is_file():
        subprocess.run(['cmake', '-S', str(root / 'firmware'), '-B', str(root / '_build'), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'], check=True)
        subprocess.run(['cmake', '--build', str(root / '_build'), '-j2'], check=True)
    sources = sorted((root / 'firmware/esp-idf').rglob('*.cpp'))
    output = temp / 'homeguard-esp-link-check'
    subprocess.run([
        'g++', '-std=c++20', '-pthread', '-include', 'sdkconfig.h',
        *includes,
        *map(str, sources),
        str(entry), str(host_library),
        '-o', str(output),
    ], check=True)
    subprocess.run([str(output)], check=True)
print(f'HomeGuard ESP mock link: {len(sources)} translation units PASS')
