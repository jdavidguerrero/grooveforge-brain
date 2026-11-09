#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 - <<'PY'
import io
import json
from pathlib import Path, PurePosixPath
import shutil
import urllib.error
import urllib.request
import zipfile


def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'GrooveForge-font-fetch/0.1'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def google(directories, filename):
    last_error = None
    for directory in directories:
        try:
            listing = json.loads(get(f'https://api.github.com/repos/google/fonts/contents/ofl/{directory}'))
            fonts = [f for f in listing if f['name'].endswith('.ttf') and 'italic' not in f['name'].lower()]
            preferred = next((f for f in fonts if '[' in f['name']), None)
            preferred = preferred or next((f for f in fonts if 'Regular' in f['name']), fonts[0])
            Path(filename).write_bytes(get(preferred['download_url']))
            license_file = next(f for f in listing if f['name'] == 'OFL.txt')
            Path(directory+'-OFL.txt').write_bytes(get(license_file['download_url']))
            return
        except (urllib.error.HTTPError, IndexError, StopIteration) as error:
            last_error = error
    raise RuntimeError(f'Could not fetch {filename}: {last_error}')


google(['bigshouldersdisplay', 'bigshoulders'], 'BigShouldersDisplay.ttf')
listing = json.loads(get('https://api.github.com/repos/google/fonts/contents/ofl/ibmplexmono'))
for weight in ('Regular', 'Medium', 'SemiBold', 'Bold'):
    name = f'IBMPlexMono-{weight}.ttf'
    font = next(f for f in listing if f['name'] == name)
    Path(name).write_bytes(get(font['download_url']))
license_file = next(f for f in listing if f['name'] == 'OFL.txt')
Path('ibmplexmono-OFL.txt').write_bytes(get(license_file['download_url']))
google(['archivo'], 'Archivo.ttf')
google(['jetbrainsmono'], 'JetBrainsMono.ttf')
archive = zipfile.ZipFile(io.BytesIO(get('https://api.fontshare.com/v2/fonts/download/general-sans')))
base = Path('general-sans')
for member in archive.infolist():
    relative = PurePosixPath(member.filename)
    if relative.is_absolute() or '..' in relative.parts:
        raise RuntimeError('Unsafe archive path')
    if not member.is_dir():
        target = base.joinpath(*relative.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(archive.read(member))
fonts = list(base.rglob('*.ttf')) + list(base.rglob('*.otf'))
chosen = next((p for p in fonts if 'variable' in p.name.lower()), None)
chosen = chosen or next((p for p in fonts if 'regular' in p.name.lower()), None)
if chosen is None:
    raise RuntimeError('Fontshare archive has no TTF/OTF; inspect general-sans/ manually')
shutil.copyfile(chosen, 'GeneralSans.ttf')
print('Fonts downloaded. Retain OFL and Fontshare licenses before redistributing.')
PY
