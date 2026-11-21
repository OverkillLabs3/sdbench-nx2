"""Builds the SD-card zip of the release: python3 tools/package.py [version]
Needs sdbench.nro (tools/build.sh). Writes dist/sdbench-nx2-<version>.zip and dist/SHA256SUMS.txt."""
import hashlib
import os
import sys
import zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = sys.argv[1] if len(sys.argv) > 1 else '1.0.0'
nro = os.path.join(root, 'sdbench.nro')
if not os.path.exists(nro):
    sys.exit('sdbench.nro is missing: run tools/build.sh first')

os.makedirs(os.path.join(root, 'dist'), exist_ok=True)
out = os.path.join(root, 'dist', 'sdbench-nx2-%s.zip' % version)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    i = zipfile.ZipInfo('switch/sdbench.nro', (2026, 1, 1, 0, 0, 0))
    i.compress_type = zipfile.ZIP_DEFLATED
    i.external_attr = 0o644 << 16
    z.writestr(i, open(nro, 'rb').read())
sha = hashlib.sha256(open(out, 'rb').read()).hexdigest()
open(os.path.join(root, 'dist', 'SHA256SUMS.txt'), 'w').write('%s  %s\n' % (sha, os.path.basename(out)))
print(out)
print(sha)
