"""Local, offline rack builder. Audio rendering/decoding belongs in this worker, never the host callback."""
from __future__ import annotations

import argparse
from copy import deepcopy
from hashlib import sha256
import json
import math
import os
from pathlib import Path
import sys
import struct
import tempfile
from uuid import uuid4

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

PERC = Path(__file__).resolve().parents[1] / 'perc-synth'
if str(PERC) not in sys.path:
    sys.path.insert(0, str(PERC))
from gf_perc.core import SR
from gf_perc.kit import VOCES, NOTAS, KITS, sintetizar_voz
from gf_perc.generos import PERFILES, notas_de, sintetizar_en_genero
from gf_perc.tonalidad import hz_de, clase_de, MODOS
from gf_perc.caracteres import CARACTERES_KICK, sintetizar_caracter

RATE = 48000
RAM = 256 * 1024 * 1024
GROUPS = ('KICKS', 'SKINS', 'WOODS', 'METALS')
PARAMS = dict(tune=0., cents=0., attack=0., hold=0., decay=2000., env_mode=0,
              filter_mode=0, cutoff=20000., resonance=0., start=0., end=1., fade_in=0.,
              fade_out=2., pan=0., send=0., velocity_curve=0, velocity_level=1.,
              velocity_tone=0., choke=0, group=0, level=.5, mute=0, solo=0)


def default_data_dir():
    return Path(os.environ.get('GF_DRUM_DATA', Path.home()/'.local/share/gf-drum'))


def atomic_json(path, value, overwrite=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix='.drum-')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            json.dump(value, stream, ensure_ascii=False, allow_nan=False, separators=(',', ':'))
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        if overwrite:
            os.replace(temporary, path)
        else:
            os.link(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        Path(temporary).unlink(missing_ok=True)


def empty_rack(name, root='A', mode='menor', source=None):
    return dict(schema_version=1, id=uuid4().hex, name=name, root=root, mode=mode, tags=[],
                bpm=122., swing=0., sources=[source] if source else [],
                pads=[dict(name='EMPTY', category='perc', source_id='', layers=[],
                           layer_mode='velocity', params=deepcopy(PARAMS)) for _ in range(16)],
                patterns=[dict(lanes=[dict(length=16, steps=[dict(velocity=0, probability=1., micro=0.)
                                                         for _ in range(32)]) for _ in range(16)]) for _ in range(8)],
                groups=[dict(name=name, level=1., mute=False) for name in GROUPS])


def category(name):
    name = name.lower()
    for cat, words in (('kick', ('kick', 'bombo', 'bassdrum', 'bd_', 'bd-')),
                       ('snare', ('snare', 'caja', 'sd_', 'sd-')),
                       ('clap', ('clap', 'palma')),
                       ('hat', ('hat', 'hh_', 'hh-', 'shaker', 'maraca', 'guache')),
                       ('wood', ('rim', 'clave', 'wood', 'guadua')),
                       ('skin', ('conga', 'bongo', 'tom', 'alegre', 'llamador')),
                       ('metal', ('ride', 'crash', 'cymbal', 'bell'))):
        if any(word in name for word in words):
            return cat
    return 'perc'


def group_for(cat):
    return 0 if cat == 'kick' else 2 if cat == 'wood' else 3 if cat in ('hat', 'metal') else 1


def convert(pcm, rate, trim=False):
    pcm = np.asarray(pcm, dtype=np.float32)
    if pcm.ndim == 1:
        pcm = pcm[:, None]
    if pcm.ndim != 2 or pcm.shape[1] not in (1, 2) or len(pcm) < 2:
        raise ValueError('expected nonempty mono/stereo audio')
    if not np.isfinite(pcm).all():
        raise ValueError('nonfinite audio')
    if not 8000 <= rate <= 192000:
        raise ValueError('sample rate must be 8000..192000')
    if math.ceil(len(pcm)*RATE/rate)*pcm.shape[1]*4 > RAM:
        raise ValueError('sample exceeds 256 MiB RAM budget')
    if trim:
        active = np.flatnonzero(np.max(np.abs(pcm), axis=1) > .001)
        if active.size:
            pad = round(.002*rate)
            pcm = pcm[max(0, active[0]-pad):min(len(pcm), active[-1]+pad+1)]
    if rate != RATE:
        common = math.gcd(int(rate), RATE)
        pcm = resample_poly(pcm, RATE//common, int(rate)//common, axis=0).astype(np.float32)
    return pcm


def store_sample(data_dir, pcm, rate=RATE, trim=False):
    data_dir = Path(data_dir)
    pcm = convert(pcm, rate, trim)
    samples = data_dir/'samples'
    samples.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=samples, suffix='.wav', prefix='.drum-')
    os.close(fd)
    try:
        # libsndfile adds a wall-clock PEAK timestamp to float WAVs. A canonical
        # IEEE-float header makes the content hash depend only on the audio.
        channels=pcm.shape[1]
        payload=pcm.astype('<f4',copy=False).tobytes()
        fmt=struct.pack('<HHIIHH',3,channels,RATE,RATE*channels*4,channels*4,32)
        with open(temporary,'wb') as stream:
            stream.write(b'RIFF'+struct.pack('<I',48+len(payload))+b'WAVE')
            stream.write(b'fmt '+struct.pack('<I',16)+fmt)
            stream.write(b'fact'+struct.pack('<II',4,len(pcm)))
            stream.write(b'data'+struct.pack('<I',len(payload))+payload)
            stream.flush()
            os.fsync(stream.fileno())
        digest = sha256(Path(temporary).read_bytes()).hexdigest()
        target = samples/(digest+'.wav')
        try:
            os.link(temporary, target)
        except FileExistsError:
            if sha256(target.read_bytes()).hexdigest() != digest:
                raise ValueError('corrupt content-addressed sample already exists')
        directory = os.open(samples, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        Path(temporary).unlink(missing_ok=True)
    # Compact visual summary is generated by the decoder, not by the GUI.
    peaks = [round(float(np.max(np.abs(chunk)))*100, 2) if len(chunk) else 0.
             for chunk in np.array_split(pcm, 64)]
    return dict(path=f'samples/{digest}.wav', hash=digest, velocity_min=1, velocity_max=127,
                frames=len(pcm), channels=pcm.shape[1], sample_rate=RATE, waveform=peaks)


def save_rack(data_dir, rack):
    path = Path(data_dir)/'racks'/(rack['id']+'.json')
    atomic_json(path, rack)
    return str(path.relative_to(data_dir))


def render_recipe(recipe, velocity=.85, hit=1, seed=None, spread=None):
    seed = int(recipe.get('seed', 1) if seed is None else seed)
    spread = float(recipe.get('spread', 0.) if spread is None else spread)
    root, mode, genre = recipe['root'], recipe.get('mode', 'menor'), recipe.get('genre', 'organic')
    clase_de(root)
    if mode not in MODOS:
        raise ValueError('unknown mode')
    voice = recipe['voice']
    if recipe.get('character') and voice == 'kick':
        return sintetizar_caracter(recipe['character'], genre, root, mode, seed, velocity, spread)
    if recipe.get('kit'):
        material = KITS[recipe['kit']][voice]
        note = notas_de(genre, root, mode).get(voice)
        audio, meta = sintetizar_voz(voice, material, velocity, seed, hit, spread,
                                    afinar_hz=hz_de(note) if note is not None else None)
        return audio, dict(meta, raiz=root, modo=mode, genero=genre, nota_midi=note)
    return sintetizar_en_genero(voice, genre, root, mode, velocity, seed, hit, spread)


def render_engine(data_dir, *, name=None, root='A', mode='menor', genre='organic', kit=None,
                  character=None, seed=1, layers=1, round_robin=False, progress=lambda **kw: None):
    if layers not in range(1, 5):
        raise ValueError('layers must be 1..4')
    clase_de(root)
    if mode not in MODOS or genre not in PERFILES:
        raise ValueError('unknown genre/mode')
    source = dict(id=uuid4().hex, name=name or kit or genre, kind='ENGINE',
                  licence_note='GrooveForge · generated locally', root=root, mode=mode,
                  genre=genre, seed=seed, generator='gf_perc', generator_version='0.4.0')
    rack = empty_rack(name or f'{genre.title()} {root}', root, mode, source)
    for i, voice in enumerate(VOCES):
        recipe = dict(voice=voice, root=root, mode=mode, genre=genre, kit=kit,
                      character=character if voice == 'kick' else None, seed=seed, spread=0.)
        cat = category(voice)
        pad = rack['pads'][i]
        pad.update(name=voice.upper(), category=cat, source_id=source['id'], gf_note=NOTAS[voice],
                   layer_mode='round_robin' if round_robin else 'velocity', recipe=recipe)
        pad['params'].update(group=group_for(cat), choke=1 if voice.startswith('hat_') else 0)
        for k in range(layers):
            low = (k*127)//layers+1
            high = ((k+1)*127)//layers
            velocity = .85 if round_robin else (low+high)/(2*127)
            pcm, meta = render_recipe(recipe, velocity, k+1)
            layer = store_sample(data_dir, pcm, SR)
            layer.update(velocity_min=1 if round_robin else low,
                         velocity_max=127 if round_robin else high, render=meta)
            pad['layers'].append(layer)
        progress(status='progress', done=i+1, total=len(VOCES), current=voice)
    atomic_json(Path(data_dir)/'sources'/(source['id']+'.json'), source)
    path = save_rack(data_dir, rack)
    return dict(status='done', racks=[path], rack_id=rack['id'], samples=len(VOCES)*layers)


def import_folder(data_dir, folder, *, licence_note, trim=False, name=None,
                  progress=lambda **kw: None):
    folder = Path(folder).expanduser().resolve(strict=True)
    if not folder.is_dir():
        raise ValueError('source must be a local folder')
    data_dir = Path(data_dir).expanduser().resolve()
    if folder == data_dir or data_dir in folder.parents or folder in data_dir.parents:
        raise ValueError('source and data directories must be separate')
    source = dict(id=uuid4().hex, name=name or folder.name, kind='IMPORTED',
                  licence_note=str(licence_note), original_folder=str(folder))
    extensions = {'.wav', '.aif', '.aiff', '.flac', '.ogg'}
    files = sorted(p for p in folder.rglob('*') if p.suffix.lower() in extensions)
    if len(files) > 16384:
        raise ValueError('folder exceeds 16384 audio files')
    racks, skipped, rack, count = [], [], None, 0
    ram = 0
    for i, path in enumerate(files):
        try:
            if path.is_symlink() or not path.resolve().is_relative_to(folder):
                raise ValueError('symlink outside source or symlink file')
            info = sf.info(path)
            if math.ceil(info.frames*RATE/info.samplerate)*info.channels*4 > RAM:
                raise ValueError('sample exceeds 256 MiB RAM budget')
            if info.channels not in (1, 2):
                raise ValueError('only mono/stereo supported')
            pcm, rate = sf.read(path, dtype='float32', always_2d=True)
            layer = store_sample(data_dir, pcm, rate, trim)
            size = layer['frames']*layer['channels']*4
            if rack is None or count == 16 or ram+size > RAM:
                if rack is not None:
                    racks.append(save_rack(data_dir, rack))
                rack = empty_rack(f'{source["name"]} · {len(racks)+1}', source=source)
                count, ram = 0, 0
            cat = category(path.stem)
            pad = rack['pads'][count]
            pad.update(name=path.stem, category=cat, source_id=source['id'], layers=[layer],
                       original_file=str(path.relative_to(folder)))
            pad['params'].update(group=group_for(cat), choke=1 if 'hat' in path.stem.lower() else 0,
                                 decay=max(2000., layer['frames']/RATE*1000))
            count += 1
            ram += size
        except (OSError, RuntimeError, ValueError) as exc:
            skipped.append(dict(file=str(path.relative_to(folder)), reason=str(exc)))
        progress(status='progress', done=i+1, total=len(files), current=path.name,
                 imported=i+1-len(skipped), skipped=deepcopy(skipped[-16:]), ram_bytes=ram)
    if rack is not None:
        racks.append(save_rack(data_dir, rack))
    source['skipped'] = skipped
    atomic_json(data_dir/'sources'/(source['id']+'.json'), source)
    return dict(status='done', racks=racks, imported=len(files)-len(skipped), skipped=skipped,
                source_id=source['id'])


def generate_variations(data_dir, recipe, *, count=8, spread=.35, progress=lambda **kw: None):
    if not isinstance(recipe, dict) or count not in range(1, 9) or not 0 <= spread <= 1:
        raise ValueError('ENGINE recipe, count 1..8 and spread 0..1 required')
    # New files and a candidate manifest only. The original rack is never opened for writing.
    variants = []
    for k in range(count):
        candidate = deepcopy(recipe)
        candidate.update(seed=int(recipe.get('seed', 1))+1000*(k+1), spread=spread)
        pcm, metadata = render_recipe(candidate, seed=candidate['seed'], spread=spread)
        layer = store_sample(data_dir, pcm, SR)
        variants.append(dict(name=f'{recipe["voice"]} {chr(65+k)}', layer=layer,
                             recipe=candidate, render=metadata, state='ready'))
        progress(status='progress', done=k+1, total=count, current=variants[-1]['name'])
    result = dict(status='done', variants=variants, root=recipe['root'])
    atomic_json(Path(data_dir)/'jobs'/('variants-'+uuid4().hex+'.json'), result)
    return result


def emit(**message):
    print(json.dumps(message, ensure_ascii=False, allow_nan=False), flush=True)


def worker(job_path):
    job = json.loads(Path(job_path).read_text())
    data_dir = Path(job['data_dir'])
    if job['cmd'] == 'import_folder':
        result = import_folder(data_dir, job['path'], licence_note=job['licence_note'],
                               trim=job.get('trim', False), progress=emit)
    elif job['cmd'] == 'generate_variations':
        result = generate_variations(data_dir, job['recipe'], count=job['count'],
                                     spread=job['spread'], progress=emit)
    elif job['cmd'] == 'render_rack':
        fields = {k: job[k] for k in ('name', 'root', 'mode', 'genre', 'kit', 'character', 'seed', 'layers', 'round_robin') if k in job}
        result = render_engine(data_dir, **fields, progress=emit)
    else:
        raise ValueError('unknown job command')
    atomic_json(Path(job_path).with_suffix('.result.json'), result)
    report=dict(result)
    if "skipped" in report:
        report["skipped_count"]=len(report["skipped"])
        report["skipped"]=report["skipped"][-32:]
    emit(**report)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, default=default_data_dir())
    sub = parser.add_subparsers(dest='command', required=True)
    render = sub.add_parser('render')
    render.add_argument('--name')
    render.add_argument('--root', default='A')
    render.add_argument('--mode', choices=MODOS, default='menor')
    render.add_argument('--genre', choices=PERFILES, default='organic')
    render.add_argument('--kit', choices=KITS)
    render.add_argument('--character', choices=CARACTERES_KICK)
    render.add_argument('--seed', type=int, default=1)
    render.add_argument('--layers', type=int, choices=range(1, 5), default=1)
    render.add_argument('--round-robin', action='store_true')
    importer = sub.add_parser('import')
    importer.add_argument('folder', type=Path)
    importer.add_argument('--licence-note', required=True)
    importer.add_argument('--trim', action='store_true')
    variants = sub.add_parser('variations')
    variants.add_argument('--rack', type=Path, required=True)
    variants.add_argument('--pad', type=int, required=True, help='0-based pad')
    variants.add_argument('--count', type=int, default=8)
    variants.add_argument('--spread', type=float, default=.35)
    background = sub.add_parser('worker')
    background.add_argument('--job', type=Path, required=True)
    args = vars(parser.parse_args(argv))
    command, data_dir = args.pop('command'), args.pop('data_dir')
    try:
        if command == 'worker':
            worker(args['job'])
            return 0
        if command == 'render':
            result = render_engine(data_dir, **args, progress=emit)
        elif command == 'import':
            result = import_folder(data_dir, **args, progress=emit)
        else:
            rack = json.loads(args.pop('rack').read_text())
            pad = args.pop('pad')
            if not 0 <= pad < 16:
                raise ValueError('pad must be 0..15')
            result = generate_variations(data_dir, rack['pads'][pad]['recipe'], **args, progress=emit)
        emit(**result)
        return 0
    except (OSError, ValueError, RuntimeError, KeyError) as exc:
        emit(status='error', error=str(exc))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
