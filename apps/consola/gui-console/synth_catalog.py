"""Development-host metadata only. The GUI receives this via list_params."""
from copy import deepcopy

ENGINES = ('prophet', 'dx7', 'dx7_6op', 'juno', 'ob6', 'model_d', 'arp2600', 'hybrid')
ENGINE_LABELS = dict(zip(ENGINES, ('VINTAGE POLY', 'FM 4-OP', 'DX7 6-OP', 'CHORUS POLY',
                                  'SEM POLY', 'LADDER MONO', 'MODULAR', 'HYBRID')))


def param(id, name, default=50, min=0, max=100, step=1, unit='%', kind='cont', options=None):
    p = dict(id=id, name=name, unit=unit, min=min, max=max, default=default, step=step, kind=kind)
    if options is not None:
        p.update(options=options, min=0, max=len(options)-1, step=1, kind='enum', unit='')
    return p


def enum(id, name, options, default=0):
    return param(id, name, default, options=options)


def bipolar(id, name, default=0, limit=100, unit='%'):
    return param(id, name, default, -limit, limit, 1, unit, 'bipolar')


def switch(id, name, default=0):
    return param(id, name, default, 0, 1, 1, '', 'bool')


def page(name, params):
    return dict(name=name, banks=[params[i:i+8] for i in range(0, len(params), 8)])


def envelope(prefix='amp'):
    return [param(f'{prefix}.{n}', f'{prefix.upper()} {n.upper()}', d, 0, hi, 1, u)
            for n, d, hi, u in [('attack', 4, 4000, 'ms'), ('decay', 220, 4000, 'ms'),
                                ('sustain', 70, 100, '%'), ('release', 620, 8000, 'ms')]]


def analog_pages(sem=False):
    osc = []
    for op in (1, 2):
        prefix = f'osc.{op}'
        osc += [enum(prefix+'.wave', f'VCO {op} WAVE', ['SAW', 'TRI', 'SINE', 'PULSE'], op-1),
                enum(prefix+'.range', f'VCO {op} RANGE', ["32′", "16′", "8′", "4′", "2′"], 2),
                param(prefix+'.level', f'VCO {op} LEVEL', 80 if op == 1 else 72),
                bipolar(prefix+'.tune', f'VCO {op} TUNE', 0 if op == 1 else 7, 24, 'st')]
    osc += [switch('osc.sync', 'SYNC', 1), param('osc.ring', 'RING', 35),
            param('osc.noise', 'NOISE', 10), param('osc.pwm', 'PULSE WIDTH', 50)]
    flt = [param('filter.cutoff', 'CUTOFF', 1100, 20, 20000, 10, 'Hz'),
           param('filter.resonance', 'RESONANCE', 40)]
    flt += [bipolar('filter.morph', 'SEM MORPH', -40), switch('filter.bp', 'BAND PASS')] if sem else [
        enum('filter.mode', 'TYPE', ['LP 24', 'LP 12', 'HP', 'BP']), param('filter.hpf', 'HPF', 30, 20, 2000, 10, 'Hz')]
    flt += [bipolar('filter.env', 'ENV AMOUNT', 35), param('filter.keytrack', 'KEY TRACK', 30),
            param('filter.xmod', 'X-MOD', 15), param('filter.drive', 'DRIVE', 10)]
    env = envelope() + envelope('filter') + [param('env.vel_amp', 'VEL → AMP', 65),
          param('env.vel_filter', 'VEL → FILTER', 50), bipolar('env.amount', 'ENV AMOUNT', 50),
          enum('env.curve', 'CURVE', ['LINEAR', 'EXP'], 1)]
    return [page('OSC', osc), page('FILTER', flt), page('ENV', env)]


def common_pages():
    return [page('LFO', [enum('lfo.shape', 'SHAPE', ['SINE', 'TRI', 'SAW', 'SQUARE', 'S&H'], 1),
            enum('lfo.rate', 'RATE', ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', '1/8T', '1/16T'], 3),
            switch('lfo.sync', 'SYNC', 1), param('lfo.delay', 'DELAY', 120, 0, 2000, 10, 'ms'),
            bipolar('lfo.cutoff', '→ CUTOFF', 40), bipolar('lfo.pitch', '→ PITCH'),
            bipolar('lfo.pwm', '→ PWM', 12)]),
        page('GROOVE', [switch('arp.on', 'ARP', 1), enum('arp.mode', 'MODE', ['UP', 'DOWN', 'UP-DN', 'RANDOM', 'ORDER'], 2),
            enum('arp.rate', 'RATE', ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'], 3),
            param('arp.gate', 'GATE', 60), enum('chord.type', 'CHORD', ['OFF', 'MAJ', 'MIN', 'MIN 9', 'SUS', '7', '9', 'OCT'], 3),
            param('chord.strum', 'STRUM', 30, 0, 100, 1, 'ms'), param('pump.depth', 'PUMP', 60),
            enum('pump.rate', 'PUMP RATE', ['1/2', '1/4', '1/8', '1/16'], 1)]),
        fx_page(),
        page('MACROS', [param('macro.'+n.lower(), n, d) for n,d in
                        [('DRIFT',30),('GRIT',45),('POCKET',20),('MOVE',60)]])]


def fx_page():
    banks = []
    controls = {
        'drive': [('gain',35),('tone',50),('bias',10),('sag',20),('low',40),('high',60)],
        'tape': [('wow',20),('flutter',12),('saturate',35),('age',40),('hiss',5),('tone',10)],
        'phaser': [('rate',35),('depth',70),('feedback',30),('stages',50),('spread',60),('tone',50)],
        'chorus': [('rate',20),('depth',60),('delay',30),('feedback',10),('spread',80),('tone',50)],
        'delay': [('time',35),('feedback',40),('filter',60),('spread',80),('wow',10),('duck',20)],
        'reverb': [('size',70),('decay',60),('damp',40),('predelay',15),('diffuse',70),('tone',50)],
    }
    for name, fields in controls.items():
        bank = [param(f'fx.{name}.{key}',key.upper(),value) for key,value in fields]
        bank += [param(f'fx.{name}.wet',name.upper()+' WET',30),switch(f'fx.{name}.on',name.upper(),1)]
        banks.append(bank)
    return dict(name='FX',banks=banks)



def fm_pages(count):
    overview = []
    focus = []
    for op in range(1, count+1):
        p = f'op.{op}'
        overview += [param(p+'.ratio', f'OP {op} RATIO', 14 if op == 2 else 1, .5, 32, .5, '×'),
                     param(p+'.level', f'OP {op} LEVEL', 90-op*5, 0, 99, 1, '')]
        controls = [deepcopy(overview[-2]), bipolar(p+'.fine', 'FINE', 0, 99, 'ct'), deepcopy(overview[-1]),
                    param(p+'.velocity', 'VELOCITY', 40, 0, 7 if count == 6 else 100, 1, '' if count == 6 else '%')]
        if count == 6:
            controls[3]['default'] = 3
            controls += [param(p+'.breakpoint', 'KEY BREAKPOINT', 60, 0, 127, 1, 'note'),
                         param(p+'.left_depth', 'LEFT DEPTH', 0, 0, 99, 1, ''),
                         param(p+'.right_depth', 'RIGHT DEPTH', 0, 0, 99, 1, ''),
                         enum(p+'.left_curve', 'LEFT CURVE', ['-LIN','-EXP','+EXP','+LIN'])]
            controls += [param(p+f'.rate.{i}', f'R{i}', d, 0, 99, 1, '') for i,d in enumerate((95,65,45,55),1)]
            controls += [param(p+f'.level.{i}', f'L{i}', d, 0, 99, 1, '') for i,d in enumerate((99,80,60,0),1)]
            controls += [enum(p+'.right_curve', 'RIGHT CURVE', ['-LIN','-EXP','+EXP','+LIN']),
                         param(p+'.rate_scaling', 'RATE SCALING', 0, 0, 7, 1, ''),
                         enum(p+'.mode', 'OSC MODE', ['RATIO','FIXED']), param(p+'.amp_mod', 'AMP MOD SENS',0,0,3,1,'')]
        else:
            controls += envelope(p)
        focus.append(page(f'OP {op}', controls))
    overview += [param('fm.algorithm', 'ALGORITHM', 5, 1, 32 if count == 6 else 8, 1, ''),
                 param('fm.feedback', 'FEEDBACK', 5, 0, 7, 1, '')]
    return [page('OPS', overview), *focus]


def list_pages(engine):
    if engine not in ENGINES:
        raise ValueError('unknown engine')
    if engine in ('dx7', 'dx7_6op'):
        return fm_pages(6 if engine == 'dx7_6op' else 4) + common_pages()
    if engine == 'hybrid':
        home = page('HYBRID', [bipolar('hybrid.a.transpose','A TRANSPOSE',0,24,'st'),
            param('hybrid.a.gain','A GAIN',-6,-60,6,.5,'dB'), param('hybrid.a.delay','A DELAY',0,0,100,1,'ms'),
            param('hybrid.morph','MORPH',38), enum('hybrid.layer','LAYER',['F'+str(i) for i in range(1,9)]+['CUSTOM'],2),
            bipolar('hybrid.b.transpose','B TRANSPOSE',12,24,'st'), param('hybrid.b.gain','B GAIN',0,-60,6,.5,'dB'),
            param('hybrid.b.delay','B DELAY',10,0,100,1,'ms')])
        halves = []
        for half in ('A','B'):
            for pg in analog_pages(half == 'A') + common_pages()[:1]:
                pg['name'] = half+' '+pg['name']
                for bank in pg['banks']:
                    for p in bank:
                        p['id'] = half.lower()+'.'+p['id']
                halves.append(pg)
        return [home, *halves, *common_pages()[1:]]
    return analog_pages(engine == 'ob6') + common_pages()


def all_params(engine):
    return {p['id']: p for pg in list_pages(engine) for bank in pg['banks'] for p in bank}
