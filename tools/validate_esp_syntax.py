#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

from esp_mock_sdk import MOCKS

with tempfile.TemporaryDirectory(prefix='homeguard-esp-mocks-') as temp:
    mock = Path(temp)
    for relative, text in MOCKS.items():
        path = mock / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    includes = ['-I' + str(mock), '-I' + str(root / 'firmware/include'), '-I' + str(root / 'firmware/esp-idf/main')]
    includes += ['-I' + str(path) for path in sorted((root / 'firmware/esp-idf/components').glob('*/include'))]
    sources = sorted((root / 'firmware/esp-idf').rglob('*.cpp'))
    for source in sources:
        subprocess.run([
            'g++', '-std=c++20', '-fsyntax-only', '-include', 'sdkconfig.h',
            *includes, str(source)
        ], check=True)
    print(len(sources))
