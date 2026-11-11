"""Local surface preferences shared by every mode; never sent to the audio host."""
import json
import os
from pathlib import Path
from PySide6.QtCore import QObject, Property, Signal, Slot, QSaveFile, QIODevice


class LocalSettings(QObject):
    changed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.path = Path(os.environ.get('GF_CONSOLE_SETTINGS',
            str(Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'gf-console/settings.json')))
        self._hands = (False, False, False)  # master, left, right
        self._velocity = 100
        self._led_brightness = 28
        self._led_animations = True
        self._error = ''
        try:
            data = json.loads(self.path.read_text())
            self._hands = tuple(data.get(k) is True for k in ('hands_enabled','left_hand','right_hand'))
            value = data.get('drum_pad_velocity', 100)
            if type(value) is int and 1 <= value <= 127: self._velocity = value
            brightness = data.get('led_brightness', 28)
            if type(brightness) is int and 0 <= brightness <= 31: self._led_brightness = brightness
            animations = data.get('led_animations', True)
            if type(animations) is bool: self._led_animations = animations
        except FileNotFoundError:
            pass
        except (OSError, ValueError, TypeError, AttributeError) as exc:
            self._error = 'Settings: '+str(exc)

    handsEnabled = Property(bool, lambda s:s._hands[0], notify=changed)
    leftHand = Property(bool, lambda s:s._hands[1], notify=changed)
    rightHand = Property(bool, lambda s:s._hands[2], notify=changed)
    padVelocity = Property(int, lambda s:s._velocity, notify=changed)
    ledBrightness = Property(int, lambda s:s._led_brightness, notify=changed)
    ledAnimations = Property(bool, lambda s:s._led_animations, notify=changed)
    error = Property(str, lambda s:s._error, notify=changed)

    def hand_allowed(self, index):
        return index in (0, 1) and self._hands[0] and self._hands[index+1]

    def save(self):
        data = dict(zip(('hands_enabled','left_hand','right_hand'), self._hands))
        data['drum_pad_velocity'] = self._velocity
        data.update(led_brightness=self._led_brightness, led_animations=self._led_animations)
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            output = QSaveFile(str(self.path))
            payload = (json.dumps(data, indent=2)+'\n').encode()
            if not output.open(QIODevice.WriteOnly): raise OSError(output.errorString())
            if output.write(payload) != len(payload) or not output.commit():
                raise OSError(output.errorString())
            self._error = ''
        except OSError as exc:
            self._error = 'Settings not saved: '+str(exc)
        self.changed.emit()

    @Slot(int)
    def toggleHand(self, index):
        if index not in (0, 1, 2): return
        values = list(self._hands)
        values[index] = not values[index]
        self._hands = tuple(values)
        self.save()

    @Slot(int)
    def setPadVelocity(self, value):
        if 1 <= value <= 127 and value != self._velocity:
            self._velocity = value
            self.save()

    @Slot()
    def cyclePadVelocity(self):
        choices = (64, 100, 127)
        self.setPadVelocity(choices[(choices.index(self._velocity)+1)%3] if self._velocity in choices else 100)

    @Slot(int)
    def setLedBrightness(self, value):
        self._led_brightness = max(0, min(31, value))
        self.save()

    @Slot()
    def cycleLedBrightness(self):
        choices = (0, 16, 28, 31)
        self.setLedBrightness(choices[(choices.index(self._led_brightness)+1)%4] if self._led_brightness in choices else 28)

    @Slot()
    def toggleLedAnimations(self):
        self._led_animations = not self._led_animations
        self.save()
