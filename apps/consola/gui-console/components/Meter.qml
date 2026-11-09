import QtQuick
import "../theme"
Rectangle {
    id: root
    property real value: 0.7
    property color tint: Theme.tealPlus
    color: Theme.rollBg
    radius: Theme.edge
    clip: true
    Rectangle {
        width: parent.width * Math.max(0, Math.min(1, root.value))
        height: parent.height
        color: root.tint
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Theme.tealDim }
            GradientStop { position: 0.6; color: Theme.teal }
            GradientStop { position: 1; color: root.tint }
        }
    }
}
