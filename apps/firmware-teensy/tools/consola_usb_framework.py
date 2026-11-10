"""Private framework overlay for consola_usb; never mutate the installed package."""
import hashlib
import json
from pathlib import Path
import shutil

Import("env")

platform = env.PioPlatform()
original_get_package_dir = platform.get_package_dir
base = Path(original_get_package_dir("framework-arduinoteensy"))
version = json.loads((base / "package.json").read_text())["version"]
if version != "1.160.0":
    raise RuntimeError("consola_usb requires framework-arduinoteensy 1.160.0; audit overlay before upgrading")

project = Path(env.subst("$PROJECT_DIR"))
overlay = project / "vendor/usb-audio-alex6679/changedCorefiles"
private = Path(env.subst("$PROJECT_BUILD_DIR")) / "framework-consola-usb"
# Include the installed core and Audio library in the key: local package edits must
# not silently leave an older private framework in use.
digest = hashlib.sha256()
for root in (base / "cores/teensy4", base / "libraries/Audio", overlay):
    for source in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(str(source.relative_to(root)).encode())
        digest.update(source.read_bytes())
digest.update((base / "package.json").read_bytes())
key = digest.hexdigest()
stamp = private / ".gf-overlay-sha256"
if not stamp.exists() or stamp.read_text() != key:
    if private.exists():
        shutil.rmtree(private)
    shutil.copytree(base, private)
    shutil.copytree(overlay, private / "cores/teensy4", dirs_exist_ok=True)
    stamp.write_text(key)

# PlatformIO's Teensy builder and LDF both obtain paths through this instance.
# Redirection is process-local and only this environment loads the script.
def private_package_dir(name, *args, **kwargs):
    if name == "framework-arduinoteensy":
        return str(private)
    return original_get_package_dir(name, *args, **kwargs)

platform.get_package_dir = private_package_dir
print("consola_usb: private framework 1.160.0 + alex6679 f731821 (async, PCM16, 4ch)")
