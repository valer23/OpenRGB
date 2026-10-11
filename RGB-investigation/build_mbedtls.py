"""Build the matching official mbedTLS C sources using the portable compiler."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess
import os

root = Path(__file__).resolve().parent
source = root / 'mbedtls-source' / 'mbedtls-3.2.1'
output = root / 'mbedtls-mingw'
output.mkdir(exist_ok=True)
compiler = root / 'toolchain' / 'Tools' / 'mingw1310_64' / 'bin'
os.environ['PATH'] = str(compiler) + os.pathsep + os.environ['PATH']

def compile_one(path):
    obj = output / (path.stem + '.o')
    if not obj.exists() or obj.stat().st_mtime < path.stat().st_mtime:
        subprocess.run([str(compiler / 'gcc.exe'), '-O2', '-I' + str(source / 'include'),
                        '-I' + str(source / 'library'), '-c', str(path), '-o', str(obj)], check=True)
    return str(obj)

with ThreadPoolExecutor(max_workers=6) as pool:
    objects = list(pool.map(compile_one, sorted((source / 'library').glob('*.c'))))
subprocess.run([str(compiler / 'ar.exe'), 'rcs', str(output / 'libmbedall.a'), *objects], check=True)
print(f'Built {len(objects)} C sources into {output / "libmbedall.a"}')
