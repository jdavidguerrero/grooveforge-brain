import QtQuick
import "../theme"
Rectangle {
    id: root
    property real value: 0
    property color tint: Theme.tealPlus
    color: Theme.rollBg
    radius: Theme.edge
    clip: true
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*Math.max(0,Math.min(1,root.value)); color: root.tint }
}
