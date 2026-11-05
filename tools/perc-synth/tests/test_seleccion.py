import numpy as np

from gf_perc.kit import sintetizar_voz
from gf_perc.seleccion import huella, mas_distintas


def test_empieza_por_la_base_y_no_repite():
    h = [np.array([float(i)]) for i in range(10)]
    elegidos = mas_distintas(h, 4)
    assert elegidos[0] == 0
    assert len(set(elegidos)) == 4


def test_prefiere_lo_distinto_a_un_duplicado():
    base, _ = sintetizar_voz("conga_media", "piel")
    otro, _ = sintetizar_voz("hat_cerrado", "madera")
    assert mas_distintas([huella(base), huella(base), huella(otro)], 2) == [0, 2]


def test_pedir_mas_de_las_que_hay_no_falla():
    assert len(mas_distintas([np.zeros(3), np.ones(3)], 5)) == 2
