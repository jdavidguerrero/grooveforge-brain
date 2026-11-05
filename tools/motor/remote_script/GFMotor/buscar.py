"""Búsqueda en el navegador de Live, sin dependencias de Live para poder probarla fuera."""


def _coincide(item, nombre):
    actual = item.name
    return actual == nombre or actual.rsplit(".", 1)[0] == nombre.rsplit(".", 1)[0]


def buscar_item(raices, partes):
    """Sigue `partes` desde una lista de carpetas; devuelve el item cargable o None si aún no está indexado."""
    actuales = list(raices)
    item = None
    for parte in partes:
        item = next((x for x in actuales if _coincide(x, parte)), None)
        if item is None:
            return None
        actuales = list(item.children)
    return item if item is not None and item.is_loadable else None
