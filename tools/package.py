"""Makes release/reo-first-person-<version>.zip: the mod folder reo-first-person/ with mod.json, mod.elf (build/mod.elf
from build.sh), README.md and window_layout.json (the camera window's layout), packed with Deflate as the program's Mods tab installs it."""
import json
import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
manifest = json.load(open(os.path.join(ROOT, 'mod.json'), encoding='utf-8'))
out_dir = os.path.join(ROOT, 'release')
os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, '%s-%s.zip' % (manifest['id'], manifest['version']))
files = [('mod.json', 'mod.json'), ('build/mod.elf', 'mod.elf'), ('README.md', 'README.md'),
         ('window_layout.json', 'window_layout.json')]
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for src, name in files:
        info = zipfile.ZipInfo('%s/%s' % (manifest['id'], name), date_time=(2026, 10, 10, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        with open(os.path.join(ROOT, src), 'rb') as f:
            z.writestr(info, f.read())
print(out, os.path.getsize(out))
