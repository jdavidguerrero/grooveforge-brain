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
