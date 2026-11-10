import QtQuick
import "../theme"
Item {
    id: root
    property var rows
    property int arcSize: Layout.brain.arcSize
    property color tint: Theme.teal
    Repeater { model: root.rows
        Panel { required property int index; required property var entry
            objectName: "column"+index
            x: Theme.columnX(index); width: Theme.columnWidth; height: root.height
            border.color: entry.bd || Theme.line
            ArcKnob { width: root.arcSize; height: width; anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height-height)/2-Theme.s4; text: entry.v; tint: index === 0 ? Theme.tealPlus : root.tint
                value: entry.amount }
            Label { y: parent.height-Theme.s6; width: parent.width; text: entry.n; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.m12; color: Theme.grayN }
        }
    }
}
