"""Tests de los patrones de la sesión por ADN (apps/docs/produccion/sesion-paleta-adn.md §7)."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_adn_clips as ma  # noqa: E402
from make_test_clips import BAR, S16  # noqa: E402


def read_events(path: Path) -> tuple[list, list]:
    """(NOTE ON (tick, nota, vel), CC (tick, cc, valor)) de un MIDI tipo 0."""
    data = path.read_bytes()
    n = struct.unpack(">I", data[18:22])[0]
    trk, i, t, notes, ccs = data[22:22 + n], 0, 0, [], []
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
            i += 3 + trk[i + 2]
            continue
        if st & 0xF0 == 0x90:
            notes.append((t, trk[i + 1], trk[i + 2]))
        elif st & 0xF0 == 0xB0:
            ccs.append((t, trk[i + 1], trk[i + 2]))
        i += 3
    return notes, ccs


def layer_from_cc(value: int, count: int) -> int:
    """Copia de studio::layer_from_cc (firmware ui/layers.h): 1…count."""
    if value == 0 or count == 0:
        return 0
    return min(count, 1 + (value - 1) * count // 127)


def test_patrones_en_la_menor_y_en_su_largo():
    scale = {0, 2, 3, 5, 7, 8, 10}                                # La menor natural, desde La
    for rel, notes in ma.clips(0).items():
        assert notes, rel
        span = 8 * BAR if rel == "pad/pad_adn_dron" else 4 * BAR
        for t, d, n, v in notes:
            assert 0 <= t and t + d <= span, rel
            assert 0 < n < 128 and 0 < v < 128, rel
            assert (n - ma.A1) % 12 in scale, (rel, n)


def test_bajos_fuera_del_kick():
    for rel in ("bass/bass_adn_tambora", "bass/bass_adn_rodante"):
        steps = {(t % BAR) // S16 for t, *_ in ma.clips(0)[rel]}
        assert not steps & set(ma.KICK_STEPS), rel


def test_capas_eligen_su_capa_y_morph(tmp_path):
    for i in range(len(ma.LAYERS)):
        assert layer_from_cc(ma.layer_cc(i), len(ma.LAYERS)) == i + 1
    sel = tmp_path / "ds" / "midi" / "perc" / "seleccion"
    for perc, top in ma.CONTEXTS.values():
        for rel in (perc, top):
            (sel / rel).parent.mkdir(parents=True, exist_ok=True)
            (sel / rel).write_bytes(b"MThd-" + rel.encode())
    ma.main(["--dataset", str(tmp_path / "ds")])
    root = tmp_path / "ds" / "midi" / "adn"
    for i, (name, rel, shift) in enumerate(ma.LAYERS):
        notes, ccs = read_events(root / "capas" / f"capa_{i + 1}_{name}.mid")
        assert ccs[0] == (0, ma.CC_LAYER, ma.layer_cc(i))
        assert [(t, v) for t, c, v in ccs if c == ma.CC_MORPH] == [(0, 0), (4 * BAR, 64), (8 * BAR, 127)]
        assert notes and max(t for t, *_ in notes) < 12 * BAR
    for ctx, (perc, top) in ma.CONTEXTS.items():
        assert (root / "contexto" / ctx / "perc_tribe.mid").read_bytes() == (sel / perc).read_bytes()
        assert (root / "contexto" / ctx / "top_tribe.mid").read_bytes() == (sel / top).read_bytes()
    kick, _ = read_events(root / "contexto" / "kick_4x4.mid")
    assert [t for t, *_ in kick] == [q * BAR // 4 for q in range(16)]
