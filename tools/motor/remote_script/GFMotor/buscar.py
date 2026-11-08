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


def buscar_plugin(raiz, nombre, preferir=("VST3", "Audio Units", "VST")):
    """Busca un plugin por nombre en el árbol de Plugins de Live (formato → fabricante → plugin).
    Si está en varios formatos, gana el primero de `preferir`."""
    encontrados = []
    pila = [(hijo, hijo.name) for hijo in raiz.children]
    while pila:
        item, formato = pila.pop()
        if item.is_loadable and _coincide(item, nombre):
            encontrados.append((formato, item))
        pila.extend((h, formato) for h in item.children)
    if not encontrados:
        return None
    orden = {f: i for i, f in enumerate(preferir)}
    return min(encontrados, key=lambda x: orden.get(x[0], len(orden)))[1]
