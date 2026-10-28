#!/usr/bin/env python3
"""Biblioteca de patrones de percusión sobre el GF Perc Map (apps/docs/produccion/percusion-groove.md §3, §7).

Escribe en $GF_DATASET (por defecto ~/GROOVEFORGE/GrooveForge_Dataset_v1):
  kits/<kit>.json                     qué toca cada rol en cada kit (Tribe Colombian, djembes/dununs, Brasil, 808)
  midi/perc/<genero>/top/<version>/<patron>_<version>_<rack>.mid    top loop (sacudidos, campanas, aros, agudos)
  midi/perc/<genero>/perc/<version>/<patron>_<version>_<rack>.mid   loop de percusión (graves, medios, caja, efectos)
  midi/perc/<genero>/<patron>.json                        ficha (§5)
  versiones: intro · main · peak · main_q (cuantizada); rack: djembe · brazilian · 808 · tribe (ya convertido)
  midi/perc/raices/…                                      los ritmos tradicionales, igual, de referencia

    python3 tools/paleta-lab/make_perc_patterns.py
    python3 tools/paleta-lab/make_perc_patterns.py --bpm 122
"""
from __future__ import annotations

import argparse
import json
import os
import random
import shutil
from pathlib import Path

from make_test_clips import BAR, S16, TPQ, write_mid

# GF Perc Map v1: un rol rítmico por nota (C1 = 36, convención de Live)
ROLES = {"grave_abierto": 36, "grave_apagado": 37, "medio_abierto": 38, "medio_apagado": 39, "agudo_abierto": 40,
         "agudo_slap": 41, "aro_madera": 42, "sacudido_corto": 43, "sacudido_largo": 44, "raspado": 45,
         "campana_grave": 46, "campana_aguda": 47, "caja": 48, "pandereta": 49, "efecto": 50}

# Tribe Colombian Percussion, del teclado del plugin (capturas de Juan, 2026-10-27), en la numeración de Live
TRIBE = {"Bombo Open": 36, "Bombo Cascara L": 38, "Bombo Cascara R": 40, "Tambora Open L": 41, "Tambora Open R": 43,
         "Tambora Cascara L": 44, "Tambora Damped L": 45, "Tambora Cascara R": 46, "Tambora Damped R": 47,
         "Llamador Bajoneo": 48, "Llamador Abierto": 50, "Alegre Bajoneo": 52, "Alegre Abierto": 53,
         "Alegre Quemao": 55, "Alegre Latigazo": 57, "Guache Up": 59, "Guache Down": 60, "Guache Rain": 62,
         "Guache Tremolo": 64, "Maracon Up": 65, "Maracon Down": 67, "Maracon Tremolo": 69, "Caja Fondo L": 71,
         "Caja Fondo R": 72, "Caja Quemao L": 74, "Caja Quemao R": 76, "Caja Latigazo": 77, "Guacharaca Shot": 79,
         "Guacharaca Short": 81, "Guacharaca Mid": 83, "Guacharaca Long": 84, "Guacharaca Longer": 86}

# Rol → articulaciones de Tribe. Dos = [semicorchea par, impar] (manos L / R; guache y maracón: Down / Up).
TRIBE_ROLES = {
    "grave_abierto": ["Tambora Open L", "Tambora Open R"],
    "grave_apagado": ["Tambora Damped L", "Tambora Damped R"],
    "aro_madera": ["Tambora Cascara L", "Tambora Cascara R"],
    "medio_abierto": ["Alegre Abierto"],
    "medio_apagado": ["Alegre Bajoneo"],
    "agudo_abierto": ["Llamador Abierto"],
    "agudo_slap": ["Alegre Quemao"],
    "sacudido_corto": ["Guache Down", "Guache Up"],
    "sacudido_largo": ["Maracon Down", "Maracon Up"],
    "raspado": ["Guacharaca Short"],
    "caja": ["Caja Fondo L", "Caja Fondo R"],
    "pandereta": ["Caja Quemao L", "Caja Quemao R"],
    "efecto": ["Alegre Latigazo"],
    "campana_grave": ["Bombo Cascara L", "Bombo Cascara R"],   # sin campanas: el aro del bombo hace de madera grave
    "campana_aguda": ["Llamador Bajoneo"],
}

# Drum Racks: el sample de cada rol, los mismos que pone make_gf_racks.py en "GF Djembe Dunun", "GF Brasil" y
# "GF 808 Perc" (vacío = ese kit no tiene sample para el rol)
DJEMBE = {"grave_abierto": "dundumba ab", "grave_apagado": "Djembe bass", "medio_abierto": "Sangban ab",
          "medio_apagado": "Sangban pres", "agudo_abierto": "kenke ab", "agudo_slap": "Djembe slap seco",
          "aro_madera": "kenke pres", "sacudido_corto": "Xeke Xeke", "sacudido_largo": "Tchekere cabaz",
          "raspado": "", "campana_grave": "campana sang", "campana_aguda": "campana kenke", "caja": "Djembe tono",
          "pandereta": "Djembe agudo", "efecto": "Djembe long"}
BRASIL = {"grave_abierto": "Surdo", "grave_apagado": "Surdo (2)", "medio_abierto": "Atabaque",
          "medio_apagado": "Zabumba", "agudo_abierto": "Repinique", "agudo_slap": "Tamborim", "aro_madera": "Coco",
          "sacudido_corto": "Caxixi", "sacudido_largo": "Ganza", "raspado": "Reco reco",
          "campana_grave": "Agogo", "campana_aguda": "Agogo (whistle) (2)", "caja": "Caixa (snare)",
          "pandereta": "Pandeiro", "efecto": "Apito (whistle)"}
TR808 = {"grave_abierto": "Tom 808 Low", "grave_apagado": "Tom 808 Mid", "medio_abierto": "Conga 808 Low",
         "medio_apagado": "Conga 808 Mid", "agudo_abierto": "Conga 808 Hi", "agudo_slap": "Tom 808 Hi",
         "aro_madera": "Rim 808", "sacudido_corto": "Maracas 808", "sacudido_largo": "", "raspado": "",
         "campana_grave": "Cowbell 808", "campana_aguda": "Clave 808", "caja": "Snare 808", "pandereta": "Clap 808",
         "efecto": "Cymbal 808"}

# Raíces: ritmos tradicionales (dominio público) como referencia; rol → [compás 1, compás 2] en semicorcheas.
# X acento (112) · x normal (88) · o suave (70) · g ghost (52) · . silencio.
# swing: % del contratiempo de semicorchea (50 = recto) · micro: ms de adelanto (−) o atraso (+) por rol.
PATTERNS = {
    "cumbia_base": dict(estilo="cumbia", origen="ritmo tradicional", densidad="media", swing=53,
                        micro={"agudo_abierto": 6, "medio_abierto": -4}, roles={
        "grave_abierto":  ["X.......x..g....", "X.......x..g..x."],
        "aro_madera":     ["..g.x.g...g.x.g.", "..g.x.g...g.x..."],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "medio_abierto":  ["x..x..x...x..x..", "x..x..x.x.x....x"],
        "medio_apagado":  ["...g......g.....", "...g............"],
        "sacudido_corto": ["XgxgXgxgXgxgXgxg", "XgxgXgxgXgxgXgxg"]}),
    "puya_vallenata": dict(estilo="vallenato", origen="ritmo tradicional", densidad="alta", swing=52,
                           micro={"raspado": -3}, roles={
        "grave_abierto": ["x.......x.......", "x.......x...x..."],
        "caja":          ["x.gxx.gxx.gxx.gx", "x.gxx.gxx.gxxxgx"],
        "raspado":       ["XgxgXgxgXgxgXgxg", "XgxgXgxgXgxgXxxx"]}),
    "bullerengue": dict(estilo="bullerengue", origen="ritmo tradicional", densidad="media", swing=54,
                        micro={"medio_abierto": -5}, roles={
        "agudo_abierto":  ["x...x...x...x...", "x...x...x...x..."],
        "medio_abierto":  ["x..x...x.x..x...", "x..x...x.x..x.xx"],
        "grave_apagado":  ["....x.......x...", "....x.......x..x"],
        "sacudido_largo": ["x.......x.......", "x.......x......."]}),
    "mapale": dict(estilo="mapalé", origen="ritmo tradicional", densidad="alta", swing=50,
                   micro={"medio_abierto": -3, "agudo_abierto": 3}, roles={
        "grave_abierto":  ["X..xX..xX..xX..x", "X..xX..xX..xXxxx"],
        "medio_abierto":  ["xxx.xxx.xxx.xx.x", "xxx.xxx.xxx.xxxx"],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "sacudido_corto": ["xgxgxgxgxgxgxgxg", "xgxgxgxgxgxgxgxg"]}),
    "kuku_afro": dict(estilo="afro (kuku)", origen="ritmo tradicional", densidad="media", swing=55,
                      micro={"agudo_slap": -6, "campana_grave": 2}, roles={
        "grave_abierto":  ["x.....x...x.....", "x.....x...x...x."],
        "grave_apagado":  ["x.........x.....", "x.........x....."],
        "campana_grave":  ["x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x."],
        "medio_abierto":  ["....x.x.....x.x.", "....x.x.....x.xx"],
        "agudo_abierto":  ["..x...x...x...x.", "..x...x...x...x."],
        "agudo_slap":     ["...x..x....x.x..", "...x..x....xxx.x"],
        "sacudido_corto": ["x.xxx.xxx.xxx.xx", "x.xxx.xxx.xxx.xx"]}),
    "samba_reggae": dict(estilo="samba reggae", origen="ritmo tradicional", densidad="media", swing=52,
                         micro={"agudo_slap": -4}, roles={
        "grave_abierto": ["X......xX.......", "X......xX......."],
        "grave_apagado": ["....x.......x...", "....x.......x.x."],
        "agudo_slap":    ["x..x..x...x.x...", "x..x..x...x.x.xx"],
        "caja":          ["x.gx.gx.x.gx.gxg", "x.gx.gx.x.gxxxxx"]}),
    "maracatu": dict(estilo="maracatu", origen="ritmo tradicional", densidad="alta", swing=50,
                     micro={"campana_aguda": -3}, roles={
        "grave_abierto": ["x.......x......x", "x.......x....x.x"],
        "medio_apagado": ["x..x..x.x.x.x...", "x..x..x.x.x.xxx."],
        "campana_grave": ["x.x.....x.x.....", "x.x.....x.x....."],
        "campana_aguda": ["....x.xx....x.xx", "....x.xx....x.xx"],
        "caja":          ["xgxgxgxgxgxgxgxg", "xgxgxgxgxgxgXXXX"]}),
    "baiao": dict(estilo="baião", origen="ritmo tradicional", densidad="baja", swing=52,
                  micro={"campana_aguda": -2}, roles={
        "grave_abierto": ["X..x....X..x....", "X..x....X..x..x."],
        "aro_madera":    ["....x.......x...", "....x.......x..."],
        "campana_aguda": ["xgxXxgxXxgxXxgxX", "xgxXxgxXxgxXxgxX"]}),
}

VEL = {"X": 112, "x": 88, "o": 70, "g": 52}

# Top loops por género (percusion-groove.md §7): núcleo de 1 compás (o 2) que se arma en 4 compases en tres versiones.
# Los graves evitan las semicorcheas 1, 5, 9 y 13: ahí va el kick (Kick 3, en su pista).
#   intro: solo esos roles · fill: rol → compás 4 (remate) · peak: rol → línea que se suma (más densidad)
GENRES = {
    "afro_house": {
        "tambora_afro": dict(kit="tribe_colombia", origen="propio", swing=56,
                             micro={"agudo_abierto": 5, "agudo_slap": -4}, roles={
            "aro_madera":     ["..x.x...x.x..x.."],            # Tambora Cáscara
            "grave_abierto":  ["......x...x....."],            # Tambora Open
            "agudo_abierto":  ["..X...X...X...X."],            # Llamador Abierto
            "sacudido_corto": ["goXogoXogoXogoXo"],            # Guache
            "agudo_slap":     ["...........x..x."]},           # Alegre Quemao
            intro=["sacudido_corto", "agudo_abierto"], fill={"agudo_slap": "...........xxxXX"},
            peak={"medio_abierto": "...x.....x...x.."}),
        "djembe_sang": dict(kit="GF Djembe Dunun", origen="propio (Juan)", swing=56,
                                micro={"agudo_slap": -5, "sacudido_corto": 2}, roles={
            "medio_abierto":  ["X.....o...X....."],
            "campana_grave":  ["X.o..X.o..X.o.X."],
            "caja":           ["...o...o.o...o.."],
            "agudo_slap":     ["......X....X..Xo"],
            "sacudido_corto": [".oX..oX..oX..oX."]},
            intro=["campana_grave", "sacudido_corto"], fill={"agudo_slap": "......X....XxxXX"},
            peak={"grave_abierto": "......x...x....."}),
        "afro_house_groove": dict(kit="GF Djembe Dunun", origen="propio", swing=56,
                               micro={"agudo_slap": -5, "medio_abierto": 3}, roles={
            "sacudido_corto": ["gxXxgxXxgxXxgxXx", "gxXxgxXxgxXxgxXx"],
            "medio_abierto":  ["..x.....x.x.....", "..x.....x.x..x.."],
            "agudo_slap":     ["......x.......x.", "......x....x.xx."],
            "grave_abierto":  ["..x.......x.....", "..x.......x....x"],
            "campana_grave":  ["..x...x...x...x.", "..x...x...x...x."]},
            intro=["sacudido_corto", "campana_grave"], fill={"medio_abierto": "..x.....x.x.xxXX"},
            peak={"campana_aguda": "x...x...x...x..."}),
        "afro_dunun_roll": dict(kit="GF Djembe Dunun", origen="propio", swing=55,
                                micro={"medio_abierto": -3}, roles={
            "grave_abierto":  ["......x...x....."],
            "medio_abierto":  ["..x.x...x.....x."],
            "campana_grave":  ["x.x.x.x.x.x.x.x."],
            "sacudido_corto": ["goXogoXogoXogoXo"],
            "agudo_slap":     ["...........x..x."]},
            intro=["campana_grave", "sacudido_corto"], fill={"medio_abierto": "..x.x...x.xxXxXX"},
            peak={"agudo_abierto": "..o...o...o...o."}),
    },
    "organic_house": {
        "bullerengue_organic": dict(kit="tribe_colombia", origen="propio", swing=57,
                                    micro={"medio_abierto": -4, "sacudido_largo": 3}, roles={
            "campana_aguda":  ["o...o...o...o..."],            # Llamador Bajoneo
            "medio_abierto":  ["...x...x.x..x..."],            # Alegre Abierto
            "sacudido_largo": ["x.......x......."],            # Maracón
            "agudo_slap":     [".....x.......x.."],            # Alegre Quemao
            "grave_apagado":  ["......x.......x."]},           # Tambora Damped
            intro=["sacudido_largo", "campana_aguda"], fill={"medio_abierto": "...x...x.x.xxxXX"},
            peak={"sacudido_corto": "..g...g...g...g."}),
        "cumbia_afro": dict(kit="tribe_colombia", origen="propio (Juan)", swing=55,
                                micro={"agudo_abierto": 5, "medio_abierto": -4}, roles={
            "campana_aguda":  ["o...o...o...o..."],            # Llamador Bajoneo
            "agudo_abierto":  ["..X...X...X...X."],            # Llamador Abierto
            "aro_madera":     ["X..o..X.X..o.oX."],            # Tambora Cáscara
            "medio_abierto":  ["...X...o...X.X.."],            # Alegre Abierto
            "agudo_slap":     [".....X......X..o"],            # Alegre Quemao
            "sacudido_largo": ["o.Xoo.Xoo.Xoo.Xo"]},           # Maracón
            intro=["sacudido_largo", "agudo_abierto"], fill={"medio_abierto": "...X...o...XxXXX"},
            peak={"sacudido_corto": "gogogogogogogogo"}),
        "organic_shaker_wood": dict(kit="GF Brasil", origen="propio", swing=57,
                                    micro={"raspado": -4, "aro_madera": 3}, roles={
            "sacudido_largo": ["x.......x.......", "x.......x......."],
            "sacudido_corto": ["..x...x...x...xg", "..x...x...x..gxg"],
            "raspado":        ["....x.......x...", "....x.......x.x."],
            "aro_madera":     ["...x.....x...x..", "...x.....x..x..."]},
            intro=["sacudido_largo", "sacudido_corto"], fill={"aro_madera": "...x.....x.xxxXx"},
            peak={"campana_aguda": "......x.......x."}),
        "organic_soft_conga": dict(kit="GF 808 Perc", origen="propio", swing=58,
                                   micro={"agudo_abierto": 4}, roles={
            "medio_abierto":  ["x.....g...x..g.."],
            "agudo_abierto":  ["....g.x.....g.x."],
            "sacudido_corto": ["..o...o...o...og"],
            "aro_madera":     ["...........x...."]},
            intro=["sacudido_corto"], fill={"agudo_abierto": "....g.x.....xxXx"},
            peak={"medio_apagado": "..g.......g....."}),
    },
    "melodic_progressive": {
        "guache_rolling": dict(kit="tribe_colombia", origen="propio", swing=52,
                               micro={"agudo_abierto": 3}, roles={
            "sacudido_corto": ["gxXxgxXxgxXxgxXx"],            # Guache
            "agudo_abierto":  ["..x...x...x...x."],            # Llamador Abierto
            "medio_apagado":  ["...g......g...g."],            # Alegre Bajoneo
            "aro_madera":     [".......x.......x"]},           # Tambora Cáscara
            intro=["sacudido_corto"], fill={"aro_madera": ".......x....xxXX"},
            peak={"raspado": "..x...x...x...x."}),
        "prog_rolling16": dict(kit="GF Brasil", origen="propio", swing=52, micro={"pandereta": -2}, roles={
            "sacudido_corto": ["goXogoXogoXogoXo"],
            "pandereta":      ["..x...x...x...x."],
            "agudo_abierto":  ["...g......g...g."],
            "medio_apagado":  ["......g.......g."]},
            intro=["sacudido_corto"], fill={"agudo_abierto": "...g......g.xxXX"},
            peak={"campana_aguda": "x.......x......."}),
        "prog_offbeat_rim": dict(kit="GF 808 Perc", origen="propio", swing=53, micro={"aro_madera": 3}, roles={
            "aro_madera":     ["..x...x...x...x."],
            "agudo_abierto":  [".....x.......x.."],
            "sacudido_corto": ["gogogogogogogogo"],
            "medio_abierto":  ["..........x....."]},
            intro=["sacudido_corto", "aro_madera"], fill={"agudo_abierto": ".....x.....xxxXX"},
            peak={"campana_grave": "......x.......x."}),
    },
    "disco_house": {
        "cumbia_disco": dict(kit="tribe_colombia", origen="propio", swing=54,
                             micro={"agudo_abierto": 4, "raspado": -2}, roles={
            "agudo_abierto":  ["..X...X...X...X."],            # Llamador Abierto (como un hat abierto)
            "raspado":        ["xgxgxgxgxgxgxgxg"],            # Guacharaca
            "sacudido_largo": ["....x.......x..."],            # Maracón
            "medio_abierto":  [".x....x..x....x."],            # Alegre Abierto
            "grave_abierto":  ["..........x....."]},           # Tambora Open
            intro=["raspado", "agudo_abierto"], fill={"medio_abierto": ".x....x..x.xxxXX"},
            peak={"pandereta": "....x.......x..."}),
        "house_conga": dict(kit="GF 808 Perc", origen="propio", swing=56,
                                micro={"agudo_abierto": 4, "pandereta": -2}, roles={
            "sacudido_corto": ["gxXxgxXxgxXxgxXx", "gxXxgxXxgxXxgxXx"],
            "agudo_abierto":  ["......x..x....x.", "..x...x..x..x..."],
            "medio_abierto":  ["x.........x.....", "x.........x...x."],
            "aro_madera":     ["....x.......x..x", "....x.......x..."],
            "pandereta":      ["..x...x...x...x.", "..x...x...x...x."]},
            intro=["sacudido_corto", "pandereta"], fill={"agudo_abierto": "..x...x..x.xxxXX"},
            peak={"campana_grave": "..x.......x....."}),
        "samba_house": dict(kit="GF Brasil", origen="propio (Juan)", swing=54,
                                micro={"agudo_slap": -3, "sacudido_corto": 2}, roles={
            "grave_apagado":  ["....X.......X..o"],            # Surdo abafado
            "agudo_slap":     ["...X..X..X....X."],            # Tamborim
            "campana_grave":  ["X..o....X..o...."],            # Agogô grave
            "campana_aguda":  [".....X.o.....X.X"],            # Agogô agudo
            "aro_madera":     [".........o.....o"],            # Caixa rim / ghost
            "sacudido_corto": ["Xo.oXo.oXo.oXoXo"]},           # Caxixi
            intro=["sacudido_corto", "campana_grave"], fill={"agudo_slap": "...X..X..X.xxxXX"},
            peak={"pandereta": "..x...x...x...x."}),
        "disco_tamb_cowbell": dict(kit="GF 808 Perc", origen="propio", swing=53, micro={"campana_grave": 2}, roles={
            "pandereta":      ["XgxgXgxgXgxgXgxg"],
            "campana_grave":  ["..x...x...x.x.x."],
            "agudo_abierto":  [".x....x..x....x."],
            "medio_abierto":  ["........x......."]},
            intro=["pandereta"], fill={"agudo_abierto": ".x....x..x.xxxXX"},
            peak={"sacudido_corto": "gogogogogogogogo"}),
    },
    "indie_tech": {
        "mapale_tech": dict(kit="tribe_colombia", origen="propio", swing=50,
                            micro={"agudo_slap": -3}, roles={
            "medio_abierto":  ["x.x.xx.x.x.xx.x."],            # Alegre Abierto
            "agudo_slap":     ["...x..x....x..x."],            # Alegre Quemao
            "caja":           ["..x...x...x...x."],            # Caja Fondo
            "grave_apagado":  ["......x.......x."],            # Tambora Damped
            "efecto":         ["................"]},           # Alegre Latigazo (en el remate)
            intro=["caja"], fill={"agudo_slap": "...x..x....xxxXX", "efecto": "...............X"},
            peak={"sacudido_corto": "x.x.x.x.x.x.x.x."}),
        "tribal_toms": dict(kit="GF 808 Perc", origen="propio", swing=51, micro={"agudo_abierto": -3}, roles={
            "medio_abierto": ["x..x..x.x..x..x.", "x..x..x.x..x.xxx"],
            "agudo_abierto": ["..x.....x.x.....", "..x.....x.x.x..."],
            "grave_abierto": ["......x.......x.", "......x.......xx"],
            "efecto":        ["................", "...............x"]},
            intro=["agudo_abierto"], fill={"medio_abierto": "x..x..x.x..xxxXX"},
            peak={"aro_madera": "..x...x...x...x."}),
        "indie_metal_rim": dict(kit="GF 808 Perc", origen="propio", swing=50, micro={"aro_madera": -2}, roles={
            "aro_madera":    ["x..x..x.x..x..x."],             # 3-3-2
            "campana_aguda": ["..x..x....x..x.."],
            "campana_grave": ["......x.......x."],
            "agudo_slap":    [".x......x......."]},
            intro=["aro_madera"], fill={"agudo_slap": ".x......x...xxXX"},
            peak={"sacudido_corto": "xgxgxgxgxgxgxgxg"}),
    },
}
VERSIONS = ("intro", "main", "peak")

# Top loop vs loop de percusión (percusion-groove.md §7 rev. 3): el top va arriba de ~300 Hz (brillo, encima de todo);
# el perc lleva los tambores medios y graves, la zona del kick y del bajo (otra mezcla: HPF ~90 Hz, sidechain, afinar)
TOP = {"agudo_abierto", "agudo_slap", "aro_madera", "sacudido_corto", "sacudido_largo", "raspado", "campana_grave",
       "campana_aguda", "pandereta"}
PERC = {"grave_abierto", "grave_apagado", "medio_abierto", "medio_apagado", "caja", "efecto"}
# Rack de cada kit, para el nombre del archivo
RACK_TAG = {"GF Djembe Dunun": "djembe", "GF Brasil": "brazilian", "GF 808 Perc": "808", "tribe_colombia": "tribe"}
RAICES_KIT = {"cumbia_base": "tribe_colombia", "puya_vallenata": "tribe_colombia", "bullerengue": "tribe_colombia",
              "mapale": "tribe_colombia", "kuku_afro": "GF Djembe Dunun", "samba_reggae": "GF Brasil",
              "maracatu": "GF Brasil", "baiao": "GF Brasil"}


KICK_STEPS = (0, 4, 8, 12)
GRAVES = ("grave_abierto", "grave_apagado")


def mutate(p: dict, seed: int) -> dict:
    """Variación con semilla de un patrón (para los que Juan eligió): mueve algún golpe una semicorchea, cambia
    acentos y suma o quita ghosts; los graves nunca entran en los tiempos del kick."""
    rnd = random.Random(seed)

    def vary(role: str, line: str) -> str:
        c = list(line)
        hits = [i for i, ch in enumerate(c) if ch != "."]
        if hits and rnd.random() < 0.7:                                   # mover un golpe
            i = rnd.choice(hits)
            j = i + rnd.choice((-1, 1))
            if 0 <= j < 16 and c[j] == "." and not (role in GRAVES and j in KICK_STEPS):
                c[i], c[j] = ".", c[i]
        if rnd.random() < 0.6:                                            # acentos
            xs = [i for i, ch in enumerate(c) if ch == "x"]
            if xs:
                c[rnd.choice(xs)] = "X"
            big = [i for i, ch in enumerate(c) if ch == "X"]
            if len(big) > 1:
                c[rnd.choice(big)] = "x"
        if role not in GRAVES and rnd.random() < 0.6:                     # un ghost que entra o sale
            free = [i for i in range(1, 16, 2) if c[i] == "."]
            ghosts = [i for i, ch in enumerate(c) if ch == "g"]
            if free and (not ghosts or rnd.random() < 0.5):
                c[rnd.choice(free)] = "g"
            elif ghosts:
                c[rnd.choice(ghosts)] = "."
        return "".join(c)

    q = dict(p)
    q["roles"] = {role: [vary(role, l) for l in bars] for role, bars in p["roles"].items()}
    q["peak"] = {role: vary(role, l) for role, l in p["peak"].items()}
    return q


def arrange(p: dict, version: str) -> dict:
    """4 compases del núcleo: intro (solo sus roles) · main (remate en el compás 4) · peak (main + densidad)."""
    roles = {}
    for role, core in p["roles"].items():
        if version == "intro" and role not in p["intro"]:
            continue
        bars = [core[i % len(core)] for i in range(4)]
        if version != "intro" and role in p["fill"]:
            bars[3] = p["fill"][role]
        roles[role] = bars
    if version == "peak":
        for role, line in p["peak"].items():
            roles[role] = [line] * 3 + [roles.get(role, [line] * 4)[3] if role in p["fill"] else line]
    return {**p, "roles": roles}


def notes_of(p: dict, bpm: float, micro: bool, seed: int) -> list[tuple[int, int, str, int]]:
    """(tick, duración, rol, velocity). Con micro: swing en las semicorcheas impares, ms por rol y velocity ±5."""
    rnd = random.Random(seed)
    ms = TPQ * bpm / 60000.0                         # ticks por milisegundo
    swing = int((p["swing"] - 50) / 50.0 * S16) if micro else 0
    out = []
    for role, bars in p["roles"].items():
        shift = int(p["micro"].get(role, 0) * ms) if micro else 0
        for b, line in enumerate(bars):
            assert len(line) == 16 and set(line) <= set(VEL) | {"."}, (role, line)
            for step, ch in enumerate(line):
                if ch == ".":
                    continue
                t = b * BAR + step * S16 + (swing if step % 2 else 0) + shift
                v = VEL[ch] + (rnd.randint(-5, 5) if micro else 0)
                out.append((max(0, t), S16 - 20, role, max(1, min(127, v))))
    return out


def to_tribe(notes, octave: int = 0) -> tuple[list, list]:
    """Al teclado de Tribe: dos articulaciones = mano / dirección por la semicorchea (par / impar)."""
    out, missing = [], set()
    for t, d, role, v in notes:
        arts = TRIBE_ROLES.get(role)
        if not arts:
            missing.add(role)
            continue
        step = int(round(t / S16))
        art = arts[step % 2] if len(arts) == 2 else arts[0]
        out.append((t, d, TRIBE[art] + 12 * octave, v))
    return out, sorted(missing)


def kit_files() -> dict[str, dict]:
    tribe_roles = {r: {"articulaciones": a, "notas": [TRIBE[x] for x in a]} for r, a in TRIBE_ROLES.items()}
    return {
        "tribe_colombia": {"kit": "tribe_colombia", "tipo": "plugin", "instrumento": "Tribe · Colombian Percussion",
                           "origen": "tribe-instruments", "octava": 0,
                           "verificar": "tocar C1 (MIDI 36) en Live: si suena Bombo Open la octava es 0",
                           "permisos": {"entrenar": "revisar licencia", "publicar": "sí (en tu música)",
                                        "samplear": "revisar licencia"},
                           "teclado": TRIBE, "roles": tribe_roles},
        "djembe_dundun": {"kit": "djembe_dundun", "tipo": "samples (Drum Rack)", "origen": "otro",
                          "permisos": {"entrenar": "revisar", "publicar": "revisar"},
                          "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in DJEMBE.items()},
                          "extras": ["Djembe bass con xeke", "Dundumba doble", "YGK_Perc_Dundun_03_Dry",
                                     "Campana sang", "TCP_Conga_Bell_5"]},
        "brasil_samples": {"kit": "brasil_samples", "tipo": "samples (Drum Rack)", "origen": "otro",
                           "permisos": {"entrenar": "revisar", "publicar": "revisar"},
                           "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in BRASIL.items()},
                           "extras": ["Zabumba", "Rum / Rumpi / Le", "Tantan", "Rebolo", "Tamborim", "Xequerê",
                                      "Rocar", "Triangle (abierto / cerrado)", "Cowbell", "Bell", "Caxixi", "Coco",
                                      "Berimbau", "Apito"],
                           "notas": "baião y maracatu suenan mejor con Zabumba y Alfaia en grave_abierto"},
        "808_perc": {"kit": "808_perc", "tipo": "samples (Drum Rack)", "origen": "otro",
                     "permisos": {"entrenar": "sí", "publicar": "sí"},
                     "roles": {r: {"nota": ROLES[r], "sample": s} for r, s in TR808.items()}},
    }


def build_favorites(perc: Path) -> int:
    """Copias de lo que Juan eligió (campo "favoritos" de las fichas) en midi/perc/favoritos/<genero>/<top|perc>/:
    el archivo exacto si dio versión / parte / rack, si no las tres versiones en las dos partes con el rack del
    patrón; y lo mismo de sus variaciones _v2…_v4. Se rearma en cada corrida (son copias: el resto no se toca)."""
    fav_root = perc / "favoritos"
    if fav_root.exists():
        shutil.rmtree(fav_root)
    count = 0
    for genre in GENRES:
        for f in sorted((perc / genre).glob("*.json")):
            d = json.loads(f.read_text())
            if not d.get("favoritos") or d.get("variacion_de"):
                continue
            base = d["patron"]
            for fav in d["favoritos"]:
                versions = [fav["version"]] if fav.get("version") else list(VERSIONS)
                parts = [fav["parte"]] if fav.get("parte") else ["top", "perc"]
                rack = fav.get("rack", d["rack"])
                for name in (base, f"{base}_v2", f"{base}_v3", f"{base}_v4"):
                    for v in versions:
                        for part in parts:
                            src = perc / genre / part / v / f"{name}_{v}_{rack}.mid"
                            if src.exists():
                                dst = fav_root / genre / part / v / src.name
                                dst.parent.mkdir(parents=True, exist_ok=True)
                                shutil.copy2(src, dst)
                                count += 1
    return count


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bpm", type=float, default=124.0)
    ap.add_argument("--dataset", default=os.environ.get("GF_DATASET",
                                                        str(Path.home() / "GROOVEFORGE" / "GrooveForge_Dataset_v1")))
    a = ap.parse_args(argv)
    root = Path(a.dataset)
    for name, k in kit_files().items():
        path = root / "kits" / f"{name}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(k, indent=2, ensure_ascii=False) + "\n")
    perc = root / "midi" / "perc"
    octave = kit_files()["tribe_colombia"]["octava"]

    def ficha(path: Path, base: dict):
        if path.exists():                                   # conservar lo que Juan anotó
            old = json.loads(path.read_text())
            for key in ("kits_probados", "notas", "favoritos"):
                base[key] = old.get(key, base.get(key, []))
        path.write_text(json.dumps(base, indent=2, ensure_ascii=False) + "\n")

    def write_split(folder: Path, stem: str, notes: list, kit: str, title: str, version: str = "") -> list[str]:
        """Un MIDI para top/ y otro para perc/ (si hay golpes); el rack en el nombre; Tribe ya convertido."""
        tag, files = RACK_TAG[kit], []
        for part, roles in (("top", TOP), ("perc", PERC)):
            sel = [nt for nt in notes if nt[2] in roles]
            if not sel:
                continue
            if kit == "tribe_colombia":
                out, _ = to_tribe(sel, octave)
            else:
                out = [(t, d, ROLES[r], v) for t, d, r, v in sel]
            fname = f"{stem}_{tag}.mid"
            rel = Path(part) / version / fname                # <top|perc>/<intro|main|peak|main_q>/
            write_mid(folder / rel, out, a.bpm, f"{title} {part} {tag}")
            files.append(str(rel))
        return files

    n = 0
    for i, (name, p) in enumerate(PATTERNS.items()):          # raíces: 2 compases, como referencia
        kit = RAICES_KIT[name]
        files = write_split(perc / "raices", name, notes_of(p, a.bpm, True, seed=i + 1), kit, name)
        files += write_split(perc / "raices", f"{name}_q", notes_of(p, a.bpm, False, seed=i + 1), kit, name + " q")
        ficha(perc / "raices" / f"{name}.json",
              {"patron": name, "estilo": p["estilo"], "bpm": a.bpm, "compases": 2, "rack": RACK_TAG[kit],
               "roles": list(p["roles"]), "archivos": files, "swing": f"{p['swing']} %",
               "microtiming_ms": p["micro"], "densidad": p["densidad"], "origen": p["origen"],
               "kits_probados": [], "notas": ""})
        n += 1
    # Lo que Juan eligió (campo "favoritos" de las fichas) genera 3 variaciones: _v2, _v3, _v4
    plan = {}
    for genre, pats in GENRES.items():
        plan[genre] = dict(pats)
        for name, p in pats.items():
            f = perc / genre / f"{name}.json"
            if f.exists() and json.loads(f.read_text()).get("favoritos"):
                for k in (2, 3, 4):
                    v = mutate(p, seed=sum(map(ord, name)) * 10 + k)   # semilla fija: la misma variación siempre
                    v["origen"] = f"variación de {name}" + (" (Juan)" if p["origen"].endswith("(Juan)") else "")
                    v["variacion_de"] = name
                    plan[genre][f"{name}_v{k}"] = v
    for genre, pats in plan.items():                          # top loops por género: 4 compases × 3 versiones
        for j, (name, p) in enumerate(pats.items()):
            seed = 100 + 10 * n + j
            files = []
            # Su rack y, para ver la parte colombiana en todos los géneros, también Tribe
            for kit in (p["kit"],) if p["kit"] == "tribe_colombia" else (p["kit"], "tribe_colombia"):
                for v in VERSIONS:
                    files += write_split(perc / genre, f"{name}_{v}", notes_of(arrange(p, v), a.bpm, True, seed=seed),
                                         kit, f"{name} {v}", v)
                files += write_split(perc / genre, f"{name}_main_q",
                                     notes_of(arrange(p, "main"), a.bpm, False, seed=seed), kit, f"{name} main q",
                                     "main_q")
            ficha(perc / genre / f"{name}.json",
                  {"patron": name, "genero": genre, "bpm": a.bpm, "compases": 4, "versiones": list(VERSIONS),
                   "rack": RACK_TAG[p["kit"]], "kit": p["kit"], "archivos": files, "roles": list(p["roles"]),
                   "intro": p["intro"], "remate_compas_4": list(p["fill"]), "peak_suma": list(p["peak"]),
                   "swing": f"{p['swing']} %", "microtiming_ms": p["micro"], "origen": p["origen"],
                   "variacion_de": p.get("variacion_de", ""), "kits_probados": [], "notas": "", "favoritos": []})
            n += 1
            print(f"{genre:20} {name:22} rack: {RACK_TAG[p['kit']]}")
    print(f"{n} patrones en {perc}")
    print(f"favoritos: {build_favorites(perc)} archivos en {perc / 'favoritos'}")


if __name__ == "__main__":
    main()
