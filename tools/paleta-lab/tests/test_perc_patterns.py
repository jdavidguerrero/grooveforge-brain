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
        assert p["origen"] == "ritmo tradicional", name
        for role, bars in p["roles"].items():
            assert role in mp.ROLES, (name, role)
            assert len(bars) == 2 and all(len(b) == 16 and set(b) <= set("Xxog.") for b in bars), (name, role)
    for genre, pats in mp.GENRES.items():
        for name, p in pats.items():
            lines = [*[l for bars in p["roles"].values() for l in bars], *p["fill"].values(), *p["peak"].values()]
            assert all(len(l) == 16 and set(l) <= set("Xxog.") for l in lines), (genre, name)
            assert set(p["intro"]) <= set(p["roles"]) and set(p["fill"]) <= set(p["roles"]), (genre, name)
            assert all(r in mp.ROLES for r in [*p["roles"], *p["peak"]]), (genre, name)


def test_graves_fuera_del_kick():
    # Los top loops van encima del kick 4/4 (Kick 3): ningún grave en las semicorcheas 1, 5, 9 y 13. Los patrones de
    # Juan quedan como los escribió (en samba_house el surdo en 2 y 4 refuerza el kick a propósito)
    for genre, pats in mp.GENRES.items():
        for name, p in pats.items():
            if p["origen"].endswith("(Juan)"):
                continue
            for v in mp.VERSIONS:
                for role in ("grave_abierto", "grave_apagado"):
                    for bar in mp.arrange(p, v)["roles"].get(role, []):
                        assert all(bar[i] == "." for i in (0, 4, 8, 12)), (genre, name, v, role, bar)


def test_versiones_intro_main_peak():
    p = mp.GENRES["afro_house"]["djembe_sang"]
    intro, main, peak = (mp.arrange(p, v) for v in mp.VERSIONS)
    assert set(intro["roles"]) == set(p["intro"])                      # intro: solo sacudidos y campana
    assert main["roles"]["agudo_slap"][3] == p["fill"]["agudo_slap"]   # remate en el compás 4
    assert all(len(bars) == 4 for bars in main["roles"].values())
    assert set(peak["roles"]) == set(main["roles"]) | set(p["peak"])   # peak suma densidad


def test_midis_top_perc_y_rack_en_el_nombre(tmp_path):
    mp.main(["--dataset", str(tmp_path)])
    perc = tmp_path / "midi" / "perc"

    def check(folder, stem, kit, version="", variant=""):
        tag = mp.RACK_TAG[kit]
        valid = mp.TRIBE.values() if kit == "tribe_colombia" else mp.ROLES.values()
        found = 0
        for part in ("top", "perc"):
            f = folder / part / version / variant / f"{stem}_{tag}.mid"
            if f.exists():
                notes = read_notes(f)
                assert notes and all(n in valid for _, n, _ in notes), f
                found += 1
        assert found, (folder, stem)

    for name in mp.PATTERNS:
        check(perc / "raices", name, mp.RAICES_KIT[name])
        check(perc / "raices", f"{name}_q", mp.RAICES_KIT[name])
    for genre, pats in mp.GENRES.items():
        for name, p in pats.items():
            for v in (*mp.VERSIONS, "main_q"):
                check(perc / genre, f"{name}_{v}", p["kit"], v, "v1")
            f = json.loads((perc / genre / f"{name}.json").read_text())
            assert f["rack"] == mp.RACK_TAG[p["kit"]] and f["archivos"]
            if p["kit"] != "tribe_colombia":                                  # todos también en Tribe
                check(perc / genre, f"{name}_main", "tribe_colombia", "main", "v1")
    # El top no lleva graves ni medios; el perc no lleva sacudidos ni campanas
    top = read_notes(perc / "afro_house" / "top" / "main" / "v1" / "djembe_sang_main_djembe.mid")
    assert all(n not in (36, 37, 38, 39) for _, n, _ in top)
    pc = read_notes(perc / "afro_house" / "perc" / "main" / "v1" / "djembe_sang_main_djembe.mid")
    assert all(n not in (43, 44, 46, 47) for _, n, _ in pc)
    kits = {p.stem for p in (tmp_path / "kits").glob("*.json")}
    assert kits == {"tribe_colombia", "djembe_dundun", "brasil_samples", "808_perc"}


def test_variaciones_cambian_y_respetan_el_kick():
    for genre, pats in mp.GENRES.items():
        for name, p in pats.items():
            vs = [mp.mutate(p, seed=sum(map(ord, name)) * 10 + k) for k in (2, 3, 4)]
            assert any(v["roles"] != p["roles"] for v in vs), (genre, name)    # alguna cambia de verdad
            for v in vs:
                assert set(v["roles"]) == set(p["roles"])                       # misma idea: mismos roles
                for role in mp.GRAVES:
                    for i, bar in enumerate(v["roles"].get(role, [])):
                        orig = p["roles"][role][i]
                        # ningún grave nuevo en un tiempo del kick (los de Juan conservan los suyos)
                        assert all(bar[k] == "." or orig[k] != "." for k in mp.KICK_STEPS), (genre, name, bar)


def test_colombia_en_todos_los_generos():
    for genre, pats in mp.GENRES.items():
        assert any(p["kit"] == "tribe_colombia" for p in pats.values()), genre


def test_favoritos_solo_lo_elegido(tmp_path):
    mp.main(["--dataset", str(tmp_path)])
    perc = tmp_path / "midi" / "perc"
    f = perc / "afro_house" / "djembe_sang.json"
    d = json.loads(f.read_text())
    d["favoritos"] = [{"version": "peak", "parte": "perc", "rack": "djembe"}]
    f.write_text(json.dumps(d))
    mp.main(["--dataset", str(tmp_path)])                                 # elegir genera las variaciones
    assert mp.build_favorites(perc) == 4                                  # el peak perc y sus 3 variaciones
    fav = perc / "favoritos" / "afro_house" / "perc" / "peak"
    got = sorted(str(p.relative_to(fav)) for p in fav.rglob("*.mid"))
    assert got == ["v1/djembe_sang_peak_djembe.mid"] + [f"v{k}/djembe_sang_v{k}_peak_djembe.mid" for k in (2, 3, 4)]
    assert not (perc / "favoritos" / "afro_house" / "top").exists()


def test_tribe_alterna_manos_y_guache():
    notes = [(0, 100, "grave_abierto", 90), (mp.S16, 100, "grave_abierto", 90),
             (0, 100, "sacudido_corto", 90), (mp.S16, 100, "sacudido_corto", 90), (0, 100, "campana_grave", 9)]
    out, missing = mp.to_tribe(notes)
    got = [n for _, _, n, _ in out]
    assert got[:2] == [mp.TRIBE["Tambora Open L"], mp.TRIBE["Tambora Open R"]]
    assert got[2:4] == [mp.TRIBE["Guache Down"], mp.TRIBE["Guache Up"]]
    assert missing == []
