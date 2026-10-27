"""Tests de la biblioteca de percusión (apps/docs/produccion/percusion-groove.md §7)."""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_perc_patterns as mp  # noqa: E402


def read_notes(path: Path) -> list[tuple[int, int, int]]:
    """(tick, nota, velocity) de los NOTE ON de un MIDI tipo 0."""
    data = path.read_bytes()
    assert data[:4] == b"MThd" and data[14:18] == b"MTrk"
    n = struct.unpack(">I", data[18:22])[0]
    trk, i, t, out = data[22:22 + n], 0, 0, []
    while i < len(trk):
        d = 0
        while True:
            b = trk[i]; i += 1
            d = (d << 7) | (b & 0x7F)
            if not b & 0x80:
                break
        t += d
        st = trk[i]
        if st == 0xFF:
            ln = trk[i + 2]; i += 3 + ln
        else:
            if st & 0xF0 == 0x90:
                out.append((t, trk[i + 1], trk[i + 2]))
            i += 3
    return out


def test_patrones_bien_formados():
    for name, p in mp.PATTERNS.items():
        assert p["origen"] in ("ritmo tradicional", "propio"), name
        for role, bars in p["roles"].items():
            assert role in mp.ROLES, (name, role)
            assert len(bars) == 2 and all(len(b) == 16 and set(b) <= set("Xxg.") for b in bars), (name, role)


def test_midis_en_el_mapa_y_en_tribe(tmp_path):
    mp.main(["--dataset", str(tmp_path)])
    perc = tmp_path / "midi" / "perc"
    for name in mp.PATTERNS:
        notes = read_notes(perc / f"{name}.mid")
        assert notes and all(n in mp.ROLES.values() for _, n, _ in notes)
        q = read_notes(perc / f"{name}_q.mid")
        assert all(t % mp.S16 == 0 for t, _, _ in q)                     # la cuantizada cae en la grilla
        tribe = read_notes(perc / "tribe_colombia" / f"{name}.mid")
        assert all(n in mp.TRIBE.values() for _, n, _ in tribe)
        ficha = json.loads((perc / f"{name}.json").read_text())
        assert ficha["version_cuantizada"] == f"{name}_q.mid"
    kits = {p.stem for p in (tmp_path / "kits").glob("*.json")}
    assert kits == {"tribe_colombia", "djembe_dundun", "brasil_samples", "808_perc"}


def test_tribe_alterna_manos_y_guache():
    notes = [(0, 100, "grave_abierto", 90), (mp.S16, 100, "grave_abierto", 90),
             (0, 100, "sacudido_corto", 90), (mp.S16, 100, "sacudido_corto", 90), (0, 100, "campana_grave", 9)]
    out, missing = mp.to_tribe(notes)
    got = [n for _, _, n, _ in out]
    assert got[:2] == [mp.TRIBE["Tambora Open L"], mp.TRIBE["Tambora Open R"]]
    assert got[2:4] == [mp.TRIBE["Guache Down"], mp.TRIBE["Guache Up"]]
    assert missing == []
