"""LED27 intentions in provisional PCB-K chain order (doc27); no animation timer."""
from dataclasses import dataclass

OFF, DIM, ON, BLINK, PULSE = range(5)
TEAL, VIOLET, BLUE, AMBER, WHITE = range(5)
KEY_REFS = ('PLAY', 'STOP', 'REC', 'LOOP', 'METRO',
            'SCENE4_PAGE_R', 'SCENE3_PAGE_L', 'SCENE2_OCT-', 'SCENE1_OCT+',
            'DELETE', 'DUPLICATE', 'QUANTIZE', 'NEW', 'UNDO', 'SHIFT',
            'BUILD', 'DROP', 'BREAK', 'TAP', 'FILL', 'CUE_ENG',
            'BROWSE', 'NAV_R', 'NAV_L', 'MENU', 'BACK')


@dataclass(frozen=True)
class KeyState:
    state: int = DIM
    palette: int = TEAL


def key_messages(index, state):
    from pad_leds import cc
    if (type(index) is not int or not 0 <= index < 26 or
            type(state.state) is not int or not OFF <= state.state <= PULSE or
            type(state.palette) is not int or not TEAL <= state.palette <= WHITE):
        raise ValueError('LED27 key/state/palette outside protocol')
    return [cc(103, index), cc(104, state.state), cc(105, state.palette), cc(106, 1)]


def key_frame(console, clock, fresh):
    buttons = console.buttons
    scene = {'#5DCAA5': TEAL, '#3D8CFF': BLUE, '#E9AD50': AMBER}[buttons.sceneColor]
    frame = []
    for ref in KEY_REFS:
        if ref.startswith('SCENE'):
            state = KeyState(ON, scene)
        elif ref in ('DELETE', 'DUPLICATE', 'QUANTIZE', 'NEW', 'UNDO',
                     'BROWSE', 'NAV_R', 'NAV_L', 'MENU', 'BACK'):
            state = KeyState(ON, VIOLET)
        elif ref == 'SHIFT':
            state = KeyState(ON if buttons.shift else DIM, VIOLET)
        elif ref in ('BUILD', 'DROP', 'BREAK'):
            state = KeyState(BLINK if buttons.armed == ref else DIM)
        elif fresh and ref in ('PLAY', 'STOP', 'REC'):
            active = (clock.get('running', False) if ref == 'PLAY' else
                      not clock.get('running', False) if ref == 'STOP' else clock.get('armed', False))
            state = KeyState(ON if active else DIM)
        else:
            state = KeyState()
        frame.append(state)
    return tuple(frame)
