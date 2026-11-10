"""Build export and fake-host snapshot must be identical, including routing flags."""
import json
import sys
from pathlib import Path
actual, snapshot = (json.loads(Path(p).read_text()) for p in sys.argv[1:])
assert actual == snapshot, "Metadata drift: rebuild host and copy build/synth-metadata.json to gui-console/synth-metadata.json"
print("All eight engines: compiled metadata/routing equals committed fake-host snapshot")
