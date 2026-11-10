import QtQuick
import "../theme"
Canvas {
    id: root
    property real decay: .5
    property color tint: Theme.teal
    onPaint: {let c=getContext('2d');c.reset();c.fillStyle=root.tint;c.beginPath();c.moveTo(0,height);c.lineTo(width*.04,0);c.lineTo(width*root.decay,height);c.closePath();c.fill()}
    onDecayChanged: requestPaint()
    onTintChanged: requestPaint()
}
