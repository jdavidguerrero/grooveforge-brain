"""Tests del lab de leads (apps/docs/produccion/lab-leads.md §8)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_lead_lab as ml  # noqa: E402
from make_test_clips import BAR, S16  # noqa: E402
from test_adn_clips import read_events  # noqa: E402

DIALECTS = {"afro", "organic", "melodic", "indie", "disco"}


def pc(n: int, k: int = 0) -> int:
    return (n - ml.A3 - k) % 12


def test_doce_escalas_en_cuatro_familias_con_dialecto():
    assert len(ml.SCALES) == 12
    fams = {}
    for sc in ml.SCALES:
        fams.setdefault(sc.family, []).append(sc.name)
        assert sc.dialects and set(sc.dialects) <= DIALECTS, sc.name
        assert len(sc.prog) == 4
    assert sorted(len(v) for v in fams.values()) == [3, 3, 3, 3]


def test_progresiones_dentro_de_su_escala():
    for sc in ml.SCALES:
        for bar, chord in enumerate(sc.prog):
            assert ml.chord_pcs(chord) <= set(sc.bar_parent(bar)), (sc.name, chord)
            assert set(sc.bar_notes(bar)) <= set(sc.bar_parent(bar)), sc.name
            assert len(ml.chord_pcs(chord) & set(sc.bar_notes(bar))) >= 2, (sc.name, chord)


def test_notas_en_escala_registro_y_largo():
    for k in (0, 2):
        for sc in ml.SCALES:
            for r in ml.RHYTHMS:
                low, high = (x + k for x in ml.RANGE[r])
                for t, d, n, v in ml.lead(sc, r, k):
                    bar = t // BAR
                    assert 0 <= t and t + d <= 4 * BAR, (sc.name, r)
                    assert low <= n <= high and 0 < v < 128, (sc.name, r, n)
                    assert pc(n, k) in sc.bar_notes(bar), (sc.name, r, bar, n)


def test_tiempo_fuerte_y_notas_largas_en_el_acorde():
    for sc in ml.SCALES:
        for r in ml.RHYTHMS:
            for t, d, n, v in ml.lead(sc, r):
                step, dur = (t % BAR) // S16, (d + 10) // S16
                if ml.is_strong(step, dur):
                    assert pc(n) in ml.chord_pcs(sc.prog[t // BAR]), (sc.name, r, t, n)


def test_silencio_y_mezcla_de_largas_y_cortas():
    for r, top in ml.OCCUPANCY.items():
        for sc in ml.SCALES:
            notes = ml.lead(sc, r)
            assert sum(d + 10 for _, d, _, _ in notes) <= top * 4 * BAR, (sc.name, r)
    durs = {r: {(d + 10) // S16 for _, d, _, _ in ml.lead(ml.SCALES[0], r)} for r in ml.RHYTHMS}
    assert durs["corta"] == {1}
    for r in ("mixta_a", "mixta_b", "larga"):
        assert min(durs[r]) == 1 and max(durs[r]) >= 3, r


def test_misma_ritmica_en_todas_las_escalas():
    for r in ml.RHYTHMS:
        grids = {tuple((t, d, v) for t, d, _, v in ml.lead(sc, r)) for sc in ml.SCALES}
        assert len(grids) == 1, r


def test_escribe_leads_y_acordes(tmp_path):
    ml.main(["--dataset", str(tmp_path)])
    root = tmp_path / "midi" / "adn" / "lead" / "lab"
    for sc in ml.SCALES:
        for r in ml.RHYTHMS:
            f = root / sc.family / f"{ml.clip_name(sc, r)}.mid"
            notes, _ = read_events(f)
            assert [n for _, n, _ in notes] == [n for _, _, n, _ in ml.lead(sc, r)]
            assert f.stem.split("__")[2].split("_") == list(sc.dialects)
        notes, _ = read_events(root / sc.family / f"{sc.name}__acordes.mid")
        for bar, chord in enumerate(sc.prog):
            assert {pc(n) for t, n, _ in notes if t // BAR == bar} == ml.chord_pcs(chord), (sc.name, bar)


def test_orden_de_escenas_del_lab(tmp_path):
    import fill_lab_set as fl
    ml.main(["--dataset", str(tmp_path)])
    rows = fl.lead_lab_rows(tmp_path / "midi" / "adn" / "lead" / "lab")
    assert len(rows) == 48
    first = [f.stem.split("__") for _, f, _ in rows[:12]]
    assert all(r == "mixta_a" for _, r, _ in first)
    assert [s for s, _, _ in first[:3]] == ["eolica", "dorica", "menor_v_prestado"]   # eólica primero (ronda 1)
    for scene, lead, chords in rows:
        assert chords.exists() and chords.stem == lead.stem.split("__")[0] + "__acordes"
        assert scene.startswith(lead.parent.name.split("_")[0])
