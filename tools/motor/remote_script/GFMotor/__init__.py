"""Superficie de control de GFMotor: carga racks del motor en una pista. Teoría: motor-primera-entrega.md §5."""


def create_instance(c_instance):
    from .superficie import SuperficieGFMotor

    return SuperficieGFMotor(c_instance)
