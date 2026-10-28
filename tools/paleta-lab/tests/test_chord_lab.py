"""Tests del lab de acordes (apps/docs/produccion/lab-acordes.md §5)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_chord_lab as mc  # noqa: E402
from make_lead_lab import A3  # noqa: E402
from make_test_clips import BAR, S16  # noqa: E402
from test_adn_clips import read_events  # noqa: E402


def pc(n: int) -> int:
    return (n - A3) % 12


def test_voicing_cerca_y_en_registro():
    for name in mc.PROGRESSIONS:
        prog = mc.progression(name)
        vs = mc.voice_lead(prog)
        for chord, v in zip(prog, vs):
            assert sorted(map(pc, v)) == sorted(mc.pcs(chord)) and mc.LOW <= v[0] and v[-1] <= mc.HIGH
        for a, b, ca, cb in zip(vs, vs[1:] + vs[:1], prog, prog[1:] + prog[:1]):
            if set(mc.pcs(ca)) & set(mc.pcs(cb)):                       # con notas comunes, se quedan
                assert sum(x != y for x, y in zip(a, b)) <= 2, (name, ca, cb)
            assert sum(abs(x - y) for x, y in zip(a, b)) <= 6, (name, ca, cb)


def test_cierre_por_semitono_en_la_tension():
    vs = mc.voice_lead(mc.progression("menor_v_prestado"))
    e, am = vs[3], vs[0]
    assert any(pc(x) == 11 and pc(y) == 0 and y - x == 1 for x, y in zip(e, am))   # Sol# → La dentro del acorde


def test_patrones_en_el_acorde_y_en_el_ritmo():
    for name in mc.PROGRESSIONS:
        prog = mc.progression(name)
        for order in mc.ORDERS:
            for rhythm, hits in mc.RHYTHMS.items():
                notes = mc.pattern(name, order, rhythm)
                steps = {s for s, _, _ in hits}
                for t, d, n, _ in notes:
                    bar, step = t // BAR, (t % BAR) // S16
                    last = bar == 3 and step == max(steps) - 1
                    chord = prog[0] if last else prog[bar]
                    assert pc(n) in mc.pcs(chord), (name, order, rhythm, bar, n)
                    assert step in steps or last, (name, order, rhythm, step)
                    if order in mc.BASS_ORDERS:
                        assert mc.BASS_LOW <= n <= mc.HIGH
                    else:
                        assert mc.LOW <= n <= mc.HIGH, (name, order, n)
                assert any((t % BAR) // S16 == max(steps) - 1 and t // BAR == 3 for t, *_ in notes)   # anticipa


def test_bajo_solo_en_158_y_decimas():
    for order in mc.ORDERS:
        notes = mc.pattern("kumoi", order, "corcheas")
        has_bass = any(n < mc.LOW for _, _, n, _ in notes)
        assert has_bass == (order in mc.BASS_ORDERS), order


def test_ordenes_quebrados():
    v = mc.voice_lead(mc.progression("kumoi"))[0]
    first = lambda o: [n for t, _, n, _ in mc.pattern("kumoi", o, "corcheas") if t < BAR]   # noqa: E731
    assert first("alberti")[:4] == [v[0], v[2], v[1], v[2]]
    arp = first("arpegio")[:4]
    assert arp == sorted(arp) and arp[3] - arp[0] == 12
    arco = first("arco")[:6]
    assert arco[:4] == arp and arco[4:] == arp[2:0:-1]
    assert [len([n for t, _, n, _ in mc.pattern("kumoi", "decimas", "respiro") if t == 0])] == [2]


def test_escribe(tmp_path):
    mc.main(["--dataset", str(tmp_path)])
    root = tmp_path / "midi" / "adn" / "chords" / "lab"
    files = list(root.glob("*.mid"))
    assert len(files) == len(mc.PROGRESSIONS) * len(mc.ORDERS) * len(mc.RHYTHMS)
    notes, _ = read_events(root / "lidia__alberti__tresillo.mid")
    assert [n for _, n, _ in notes] == [n for _, _, n, _ in mc.pattern("lidia", "alberti", "tresillo")]


def test_escenas_del_lab_de_acordes(tmp_path):
    import fill_lab_set as fl
    mc.main(["--dataset", str(tmp_path)])
    rows = fl.chord_rows(tmp_path / "midi" / "adn" / "chords" / "lab")
    n = len(mc.PROGRESSIONS)
    assert len(rows) == n + n * (len(fl.CHORD_ROUNDS) - 1)
    assert [r[4].stem.split("__")[0] for r in rows[:n]] == list(mc.PROGRESSIONS)          # ronda 1: progresiones
    for dialect, scene, tribe, lead, chords in rows:
        assert dialect == fl.CHORD_DIALECT and tribe and lead is None and chords.exists()
        assert scene.split(" · ")[0] in fl.DIALECTS
    for o, r in fl.CHORD_ROUNDS:
        assert o in mc.ORDERS and r in mc.RHYTHMS
