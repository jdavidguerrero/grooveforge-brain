"""Bake gradient blooms so the scene graph never needs a blur pass."""
from pathlib import Path
from PySide6.QtGui import QImage, QPainter, QRadialGradient, QColor
from PySide6.QtCore import Qt

ROOT = Path(__file__).resolve().parents[1]/'assets'
COLORS = dict(KICK='#FF4560', DRUMS='#FF9A2E', PERC='#F5E04A', TOP='#E8F0FF',
              BASS='#3D8CFF', CHORDS='#FF6FC8', LEAD='#B6F23D', PAD='#F2C9A0',
              TEAL='#5DCAA5', PURPLE='#7A6FE0', WHITE='#F2F2F2')


def glow(path, width, height, color, side=False):
    image = QImage(width, height, QImage.Format_ARGB32_Premultiplied)
    image.fill(Qt.transparent)
    painter = QPainter(image)
    painter.scale(width, height)
    gradient = QRadialGradient(0 if side else .5, .5, .5)
    tint = QColor(color)
    tint.setAlphaF(.45)
    gradient.setColorAt(0, tint)
    tint.setAlphaF(0)
    gradient.setColorAt(1, tint)
    painter.fillRect(0, 0, 1, 1, gradient)
    painter.end()
    image.save(str(path))


if __name__ == '__main__':
    ROOT.mkdir(exist_ok=True)
    for role,color in COLORS.items():
        glow(ROOT/f'led-{role}.png', 128, 20, color)
    glow(ROOT/'halo.png', 60, 220, COLORS['TEAL'], side=True)
    glow(ROOT/'boot.png', 152, 152, '#00D4AA')
