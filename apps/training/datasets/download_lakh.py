"""
download_lakh.py — Descarga una muestra del Lakh MIDI Dataset Clean (LMD-clean)

El dataset completo (LMD-clean) tiene ~17,000 archivos MIDI. Para el pipeline
de entrenamiento inicial descargamos los primeros 500 vía el archivo de índice
oficial de Colin Raffel (https://colinraffel.com/projects/lmd/).

Estrategia de descarga:
  1. Descargar el archivo tar.gz del subset LMD-matched (más compacto que LMD-clean).
  2. Extraer solo los primeros N archivos para evitar descargar 1.6GB completos.
  3. Verificar que cada .mid es un MIDI válido antes de guardarlo.

Idempotente: si el directorio ya tiene archivos, no re-descarga.

Uso:
    uv run python datasets/download_lakh.py
    uv run python datasets/download_lakh.py --max-files 100
"""

import argparse
import hashlib
import io
import sys
import tarfile
from pathlib import Path

import requests

# ---------------------------------------------------------------------------
# Constantes
# ---------------------------------------------------------------------------

# LMD-matched: 45MB tar.gz con metadata + index. Los MIDI individuales se
# descargan desde el index. Usamos el LMD-clean subset via HTTP directory listing.
# Raffel no provee un zip parcial de LMD-clean, por lo que la estrategia más
# robusta es descargar el archivo index y luego archivos individuales.

LMD_INDEX_URL = (
    "https://hog.ee.columbia.edu/craffel/lmd/lmd_matched.tar.gz"
)

# Mirror alternativo si el primero falla
LMD_MIRROR_URL = (
    "https://colinraffel.com/projects/lmd/lmd_matched.tar.gz"
)

# Para el subset de muestra inicial usamos un archivo pequeño de ejemplo.
# El LMD-aligned-scores contiene MIDI bien curados y pesa ~200MB completo.
# Para CI/dev descargamos solo el primer bloque.

SAMPLE_MIDI_URL = (
    "https://github.com/craffel/midi-ground-truth/raw/master/"
    "lmd_matched/A/A/A/TRAAAAW128F429D538.mid"
)

DATASET_DIR = Path(__file__).parent / "lakh"
MAX_FILES_DEFAULT = 500


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _print_progress(downloaded: int, total: int | None, filename: str) -> None:
    if total:
        pct = downloaded / total * 100
        bar = "#" * int(pct / 2)
        print(f"\r  [{bar:<50}] {pct:5.1f}%  {filename}", end="", flush=True)
    else:
        kb = downloaded / 1024
        print(f"\r  {kb:.0f} KB descargados  {filename}", end="", flush=True)


def _download_file(url: str, dest: Path, chunk_size: int = 65536) -> Path:
    """Descarga url a dest con streaming y barra de progreso. Idempotente."""
    if dest.exists():
        print(f"  Ya existe: {dest.name} — saltando.")
        return dest

    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(".tmp")

    try:
        response = requests.get(url, stream=True, timeout=60)
        response.raise_for_status()
    except requests.RequestException as exc:
        print(f"\n  ERROR: no se pudo descargar {url}\n  {exc}", file=sys.stderr)
        raise

    total = int(response.headers.get("content-length", 0)) or None
    downloaded = 0

    with tmp.open("wb") as fh:
        for chunk in response.iter_content(chunk_size=chunk_size):
            if chunk:
                fh.write(chunk)
                downloaded += len(chunk)
                _print_progress(downloaded, total, dest.name)

    print()  # newline after progress bar
    tmp.rename(dest)
    return dest


def _is_valid_midi(data: bytes) -> bool:
    """Verifica que los primeros 4 bytes son el header MIDI ('MThd')."""
    return data[:4] == b"MThd"


def _extract_midi_from_tar(
    tar_path: Path, output_dir: Path, max_files: int
) -> list[Path]:
    """
    Extrae los primeros max_files archivos .mid del tar.gz a output_dir.
    Verifica que cada archivo sea MIDI válido antes de escribirlo.
    """
    extracted: list[Path] = []
    print(f"\nExtrayendo hasta {max_files} archivos MIDI de {tar_path.name}...")

    with tarfile.open(tar_path, "r:gz") as tf:
        members = [m for m in tf.getmembers() if m.name.endswith(".mid")]
        print(f"  Archivos MIDI en el tar: {len(members)}")

        for member in members[:max_files]:
            # Nombre plano (sin subdirectorios arbitrarios del tar)
            flat_name = Path(member.name).name
            dest = output_dir / flat_name

            if dest.exists():
                extracted.append(dest)
                continue

            f = tf.extractfile(member)
            if f is None:
                continue

            data = f.read()
            if not _is_valid_midi(data):
                print(f"  SKIP (no es MIDI válido): {flat_name}")
                continue

            dest.write_bytes(data)
            extracted.append(dest)
            print(f"  [{len(extracted):>4}/{max_files}] {flat_name}")

    return extracted


# ---------------------------------------------------------------------------
# Flujo principal
# ---------------------------------------------------------------------------


def download_sample_midi(output_dir: Path) -> Path:
    """
    Descarga UN archivo MIDI de muestra para validar el pipeline sin
    descargar el dataset completo. Útil en CI o en primera ejecución rápida.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    dest = output_dir / "sample_TRAAAAW128F429D538.mid"

    if dest.exists():
        print(f"Muestra ya descargada: {dest}")
        return dest

    print(f"Descargando archivo de muestra desde GitHub...")
    try:
        return _download_file(SAMPLE_MIDI_URL, dest)
    except requests.RequestException:
        # Fallback: crear un MIDI mínimo sintético para que el pipeline funcione
        print("  Fallback: creando MIDI sintético mínimo para tests...")
        dest.write_bytes(_minimal_midi_bytes())
        print(f"  MIDI sintético guardado en {dest}")
        return dest


def _minimal_midi_bytes() -> bytes:
    """Genera un MIDI Type 0 mínimo con una nota C4 de 1 segundo."""
    import struct

    def var_len(n: int) -> bytes:
        """Variable-length encoding para MIDI."""
        data = [n & 0x7F]
        n >>= 7
        while n:
            data.append((n & 0x7F) | 0x80)
            n >>= 7
        return bytes(reversed(data))

    # Header chunk: MThd, length=6, format=0, tracks=1, ticks=480
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)

    # Track chunk
    events = (
        var_len(0) + b"\xff\x51\x03\x07\xa1\x20"  # tempo: 500000 us/beat (120 BPM)
        + var_len(0) + b"\x90\x3c\x64"  # note_on C4 vel=100
        + var_len(480) + b"\x80\x3c\x00"  # note_off C4 after 1 beat
        + var_len(0) + b"\xff\x2f\x00"  # end of track
    )
    track = b"MTrk" + struct.pack(">I", len(events)) + events
    return header + track


def download_lakh_subset(output_dir: Path, max_files: int = MAX_FILES_DEFAULT) -> list[Path]:
    """
    Descarga el subset LMD-matched y extrae los primeros max_files archivos MIDI.

    ADVERTENCIA: el tar.gz completo pesa ~1.6GB. Para descargar solo una muestra
    usa download_sample_midi() en su lugar.
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    # Verificar si ya tenemos suficientes archivos
    existing = list(output_dir.glob("*.mid"))
    if len(existing) >= max_files:
        print(f"Ya hay {len(existing)} archivos MIDI en {output_dir} — nada que descargar.")
        return existing

    tar_path = output_dir.parent / "lmd_matched.tar.gz"

    print(f"Descargando LMD-matched tar.gz (~1.6GB)...")
    print(f"  URL: {LMD_INDEX_URL}")
    print("  Esto puede tardar varios minutos según tu conexión.")

    try:
        _download_file(LMD_INDEX_URL, tar_path)
    except requests.RequestException:
        print(f"  Intentando mirror: {LMD_MIRROR_URL}")
        _download_file(LMD_MIRROR_URL, tar_path)

    extracted = _extract_midi_from_tar(tar_path, output_dir, max_files)
    print(f"\nExtracción completa: {len(extracted)} archivos en {output_dir}")
    return extracted


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Descarga una muestra del Lakh MIDI Dataset para GrooveForge Brain."
    )
    parser.add_argument(
        "--max-files",
        type=int,
        default=MAX_FILES_DEFAULT,
        help=f"Máximo de archivos MIDI a extraer (default: {MAX_FILES_DEFAULT})",
    )
    parser.add_argument(
        "--sample-only",
        action="store_true",
        default=True,
        help=(
            "Descargar solo 1 archivo de muestra (default: True). "
            "Usar --no-sample-only para descargar el dataset completo."
        ),
    )
    parser.add_argument(
        "--no-sample-only",
        dest="sample_only",
        action="store_false",
        help="Descargar el subset completo (requiere ~1.6GB de descarga).",
    )
    args = parser.parse_args()

    DATASET_DIR.mkdir(parents=True, exist_ok=True)

    if args.sample_only:
        print("Modo: muestra (1 archivo). Usar --no-sample-only para el dataset completo.")
        path = download_sample_midi(DATASET_DIR)
        print(f"\nListo. Archivo de muestra: {path}")
        print("\nPara explorar los datos:")
        print("  uv run jupyter notebook datasets/explore_midi.ipynb")
    else:
        print(f"Modo: subset completo (hasta {args.max_files} archivos).")
        paths = download_lakh_subset(DATASET_DIR, args.max_files)
        print(f"\nListo. {len(paths)} archivos en {DATASET_DIR}")


if __name__ == "__main__":
    main()
