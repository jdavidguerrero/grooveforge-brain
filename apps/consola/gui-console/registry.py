"""Ordered review catalog. The first nine IDs remain the phase-1 public interface."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Screen:
    name: str
    title: str
    mode: str = 'STANDALONE'
    keys: str = ''
    source: str = ''
    state: str = 'normal'
    columns: bool = False
    design: str = ''

    @property
    def qml(self):
        return (self.source or self.name) + '.qml'


SCREENS = (
    Screen('Boot', 'GROOVEPILOT', 'COMMON'),
    Screen('ModeSelect', 'Choose a mode', 'COMMON'),
    Screen('Toasts', 'LIVE · SESSION'),
    Screen('Settings', 'Surface', 'COMMON'),
    Screen('LiveSession', 'LIVE · SESSION', columns=True),
    Screen('Layer', 'LAYER · BASS'),
    Screen('Transition', 'LIVE · SESSION'),
    Screen('Mixer', 'MIXER', columns=True),
    Screen('Hands', 'HANDS'),
    Screen('Search', 'SEARCH', 'COMMON', 's.kSearch'),
    Screen('LiveArrange', 'LIVE · ARRANGE', keys='s.kArr'),
    Screen('SequencerSteps', 'SEQ · STEPS', keys='s.kSteps'),
    Screen('SequencerPianoRoll', 'SEQ · PIANO ROLL', keys='s.kRoll'),
    Screen('Pads', 'PADS', keys='s.kPads', columns=True),
    Screen('BrainEngine', 'BRAIN', keys='s.kBrain', columns=True),
    Screen('BrainHybrid', 'BRAIN · HYBRID', keys='s.kHyb', columns=True),
    Screen('Kit', 'KIT', keys='s.kKit', columns=True),
    Screen('Library', 'LIBRARY', keys='s.kLib'),
    Screen('Log', 'SET LOG', keys='s.kLog'),
    Screen('Routing', 'ROUTING', keys='s.kRoute', columns=True),
    Screen('Session', 'SESSION', 'CONTROLLER', 'c.kSess', columns=True),
    Screen('Note', 'NOTE', 'CONTROLLER', 'c.kNote', columns=True),
    Screen('Clip', 'CLIP', 'CONTROLLER', 'c.kClip'),
    Screen('Device', 'DEVICE', 'CONTROLLER', 'c.kDev', columns=True),
    Screen('Mix', 'MIX', 'CONTROLLER', 'c.kMix', columns=True),
    Screen('Browse', 'BROWSE', 'CONTROLLER', 'c.kBrowse'),
    Screen('Scale', 'NOTE', 'SHARED', 'c.kScale'),
    Screen('Repeat', 'NOTE', 'SHARED', 'c.kRep', columns=True),
    Screen('Tempo', 'SESSION', 'SHARED', 'c.kTempo'),
    Screen('Motor', 'ENGINE', 'CONTROLLER · STUDIO', 'c.kEng'),
    Screen('LiveSessionOffline', 'LIVE · SESSION', source='LiveSession', state='offline', columns=True),
    Screen('SessionNoLive', 'SESSION', 'CONTROLLER', 'c.kNoLive', source='Session', state='no-live', columns=True),
    Screen('LiveSessionEmpty', 'LIVE · SESSION', source='LiveSession', state='empty', columns=True),
    Screen('LiveSessionLoading', 'LIVE · SESSION', source='LiveSession', state='loading', columns=True),
    Screen('NoteMelodic', 'NOTE', 'CONTROLLER', 's.kPads', source='Pads', columns=True),
    Screen('SynthParts', 'PARTS', 'STANDALONE', source='SynthParts', design='31'),
    Screen('SynthPartsStates', 'PARTS', 'STANDALONE', source='SynthParts', design='31b'),
    Screen('SynthFilter', 'BRAIN · FILTER', 'STANDALONE', source='SynthParameter', columns=True, design='32'),
    Screen('SynthNotResponding', 'BRAIN · FILTER', 'STANDALONE', source='SynthParameter', columns=True, design='32b'),
    Screen('SynthLoading', 'BRAIN · FILTER', 'STANDALONE', source='SynthParameter', columns=True, design='32c'),
    Screen('SynthOsc', 'BRAIN · OSC', 'STANDALONE', source='SynthParameter', columns=True, design='33'),
    Screen('SynthEnv', 'BRAIN · ENV', 'STANDALONE', source='SynthParameter', columns=True, design='34'),
    Screen('SynthEnvBank2', 'BRAIN · ENV', 'STANDALONE', source='SynthParameter', columns=True, design='34b'),
    Screen('SynthLfo', 'BRAIN · LFO', 'STANDALONE', source='SynthParameter', columns=True, design='35'),
    Screen('SynthFm', 'BRAIN · FM 4-OP', 'STANDALONE', source='SynthParameter', columns=True, design='36'),
    Screen('SynthFmFocus', 'BRAIN · FM 4-OP', 'STANDALONE', source='SynthParameter', columns=True, design='36b'),
    Screen('SynthDx7', 'BRAIN · DX7 6-OP', 'STANDALONE', source='SynthParameter', columns=True, design='36 · DX7'),
    Screen('SynthDx7Bank2', 'BRAIN · DX7 6-OP', 'STANDALONE', source='SynthParameter', columns=True, design='36 · DX7 bank 2'),
    Screen('SynthDx7Focus', 'BRAIN · DX7 6-OP', 'STANDALONE', source='SynthParameter', columns=True, design='36b · DX7'),
    Screen('SynthGroove', 'BRAIN · GROOVE', 'STANDALONE', source='SynthParameter', columns=True, design='37'),
    Screen('SynthFx', 'BRAIN · FX', 'STANDALONE', source='SynthParameter', columns=True, design='38'),
    Screen('SynthMacros', 'BRAIN · MACROS', 'STANDALONE', source='SynthParameter', columns=True, design='39'),
    Screen('SynthHybrid', 'BRAIN · HYBRID', 'STANDALONE', source='SynthHybrid', design='40'),
    Screen('SynthHybridA', 'BRAIN · HYBRID A', 'STANDALONE', source='SynthParameter', columns=True, design='41'),
    Screen('SynthHybridB', 'BRAIN · HYBRID B', 'STANDALONE', source='SynthParameter', columns=True, design='41 · B'),
    Screen('SynthPresets', 'PRESETS', 'STANDALONE', source='SynthPresets', design='42'),
    Screen('SynthSaveAs', 'PRESETS · SAVE AS', 'STANDALONE', source='SynthPresets', design='42b'),
    Screen('SynthSettingsAudio', 'Audio', 'COMMON', source='SynthSettings', design='43'),
    Screen('SynthSettingsConnections', 'Connections', 'COMMON', source='SynthSettings', design='44'),
    Screen('SynthSettingsSystem', 'System', 'COMMON', source='SynthSettings', design='45'),
    Screen('SynthScale', 'SYNTH · SCALE', 'STANDALONE', source='SynthScale'),
)
SCREEN_NAMES = [s.name for s in SCREENS]
SCREEN_BY_NAME = {s.name.casefold(): i for i, s in enumerate(SCREENS, 1)}


def screen_id(value):
    if str(value).isdigit() and 1 <= int(value) <= len(SCREENS):
        return int(value)
    try:
        return SCREEN_BY_NAME[str(value).casefold()]
    except KeyError:
        raise ValueError(f'Unknown screen {value!r}. Choose: {", ".join(SCREEN_NAMES)}') from None
