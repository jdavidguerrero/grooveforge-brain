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


def test_agrega_debajo_del_ultimo_clip():
    import xml.etree.ElementTree as ET
    import fill_lab_set as fl
    slot = '<ClipSlot><ClipSlot><Value>{}</Value></ClipSlot></ClipSlot>'
    track = '<MidiTrack><DeviceChain><MainSequencer><ClipSlotList>{}</ClipSlotList></MainSequencer></DeviceChain></MidiTrack>'
    ls = ET.fromstring('<LiveSet><Tracks>'
                       + track.format(slot.format('<MidiClip Id="1"/>') + slot.format('') * 3)
                       + track.format(slot.format('') * 2 + slot.format('<MidiClip Id="2"/>') + slot.format(''))
                       + '</Tracks></LiveSet>')
    assert fl.first_free_row(ls) == 3


def test_ronda_de_dialectos(tmp_path):
    import fill_lab_set as fl
    ml.main(["--dataset", str(tmp_path)])
    liked = ["kumoi__corta__organic", "eolica__mixta_b__afro_melodic", "eolica__mixta_a__afro_melodic"]
    rows = fl.dialect_rows(tmp_path / "midi" / "adn" / "lead" / "lab", liked)
    assert len(rows) == len(liked) * len(fl.DIALECTS)
    assert [d for d, *_ in rows[::len(liked)]] == list(fl.DIALECTS)              # un bloque por dialecto
    assert [f.stem for _, _, f, _ in rows[:3]] == [liked[2], liked[1], liked[0]]  # eólica antes que kumoi; mixta A antes que B
    for _, _, f, chords in rows:
        assert chords.exists()
    for perc, top in fl.DIALECTS.values():
        assert {perc, top} <= {c for c, _ in fl.CONTEXTS}


def test_ronda_con_kits_y_dosis_de_tribe(tmp_path):
    import fill_lab_set as fl
    import make_tribe_dosis as td
    ml.main(["--dataset", str(tmp_path)])
    liked = ["kumoi__corta__organic", fl.REFERENCE]
    rows = fl.kit_rows(tmp_path / "midi" / "adn" / "lead" / "lab", liked)
    assert len(rows) == len(fl.DIALECTS) * len(liked) + 2 * (len(fl.DIALECTS) - 1)    # organic sin referencias
    for dialect in fl.DIALECTS:
        block = [r for r in rows if r[0] == dialect]
        if dialect == "organic":
            assert not any(dose for _, _, dose, _, _ in block)
            continue
        assert [(s.split(" · ")[1], dose) for _, s, dose, _, _ in block[:2]] == [("REF kit solo", False),
                                                                                  ("REF kit + tribe", True)]
        assert block[0][3].stem == block[1][3].stem == fl.REFERENCE
        assert all(dose for _, _, dose, _, _ in block[2:])
        assert dialect in td.DOSIS
    assert set(fl.DIALECT_KITS) == set(fl.DIALECTS)
    for parts in fl.DIALECT_KITS.values():
        assert {t for t, _, _ in parts} <= {"PERC", "TOP", *fl.KIT_TRACKS}


def test_dosis_de_tribe_es_un_elemento_suave():
    import make_tribe_dosis as td
    from make_perc_patterns import TRIBE, TRIBE_ROLES
    for dialect, (role, line, *_ ) in td.DOSIS.items():
        notes = td.dosis_notes(dialect)
        assert {n for _, _, n, _ in notes} <= {TRIBE[a] for a in TRIBE_ROLES[role]}, dialect   # un solo instrumento
        assert max(v for *_, v in notes) <= 80 and set(line) <= {".", "g", "o"}, dialect
        assert max(t for t, *_ in notes) < 4 * BAR and len(notes) == 4 * sum(c != "." for c in line)


def color_notes(sc, notes):
    return [(t, d, n) for t, d, n, _ in notes if pc(n) in ml.color_pcs(sc, t // BAR)]


def test_dosis_de_color():
    by_name = {sc.name: sc for sc in ml.SCALES}
    for scale, rhythm, dialect in ml.DOSE_TESTS:
        sc = by_name[scale]
        assert dialect in DIALECTS
        assert not color_notes(sc, ml.lead(ml.without_color(sc), rhythm))                 # sin color: ninguna
        full = ml.dose(sc, rhythm, "completa")
        assert full == ml.lead(sc, rhythm)
        low, high = ml.RANGE[rhythm]
        for kind in ml.DOSES[1:]:
            notes = ml.dose(sc, rhythm, kind)
            assert [(t, d, v) for t, d, _, v in notes] == [(t, d, v) for t, d, _, v in full], kind   # misma rítmica
            for t, _, n, _ in notes:
                assert pc(n) in sc.bar_notes(t // BAR), (scale, kind, n)
            (t, d, n), = color_notes(sc, notes)                                            # una sola
            bar, step = t // BAR, (t % BAR) // S16
            if kind == "remate":
                assert bar == 3 and pc(n) in ml.chord_pcs(sc.prog[3]) and low - 2 <= n <= high
            else:
                assert d < 2 * S16 and not ml.is_strong(step, 1) and low <= n <= high
        assert ml.dose(sc, rhythm, "remate") != ml.dose(sc, rhythm, "de_paso"), scale


def test_escenas_de_dosis(tmp_path):
    import fill_lab_set as fl
    ml.main(["--dataset", str(tmp_path)])
    rows = fl.dose_rows(tmp_path / "midi" / "adn" / "lead" / "lab")
    assert len(rows) == len(ml.DOSE_TESTS) * len(ml.DOSES)
    for k, (dialect, scene, tribe, f, chords) in enumerate(rows):
        scale, rhythm, d = ml.DOSE_TESTS[k // len(ml.DOSES)]
        assert dialect == d and scene.endswith(ml.DOSES[k % len(ml.DOSES)].replace("_", " "))
        assert tribe and f.exists() and chords.exists() and f.stem.startswith(f"{scale}__{rhythm}")


def test_recorta_escenas_vacias_de_abajo():
    import xml.etree.ElementTree as ET
    import fill_lab_set as fl
    slot = '<ClipSlot Id="{}"><ClipSlot><Value>{}</Value></ClipSlot></ClipSlot>'
    lst = lambda full: '<ClipSlotList>' + ''.join(slot.format(i, '<MidiClip/>' if i in full else '')   # noqa: E731
                                                  for i in range(5)) + '</ClipSlotList>'
    scene = '<Scene Id="{}"><Name Value="{}"/></Scene>'
    ls = ET.fromstring('<LiveSet><Scenes>' + ''.join(scene.format(i, 'x' if i == 1 else '') for i in range(5))
                       + '</Scenes><Tracks><MidiTrack>' + lst({0, 2}) + lst(set()) + '</MidiTrack></Tracks></LiveSet>')
    fl.trim_scenes(ls, 1)
    assert len(ls.find("Scenes")) == 3                                  # la 3.ª tiene clip: no se toca de ahí para arriba
    assert all(len(l) == 3 for l in ls.iter("ClipSlotList"))
