import QtQuick
import "../theme"
Item {
    id: root
    property var bars: []
    property bool snapshot: false
    property color tint: Theme.tealPlus
    Repeater { id: repeated; model: root.bars
        Rectangle { required property int index; required property var model; property var datum: root.snapshot ? model.entry : model.modelData
            width: Math.max(Theme.hairline, (root.width-(repeated.count-1)*Theme.edge)/Math.max(1,repeated.count))
            x: index*(width+Theme.edge); height: root.height*(typeof datum === "number" ? datum : datum.h)/100
            anchors.verticalCenter: parent.verticalCenter; color: typeof datum === "number" ? root.tint : (datum.c || root.tint)
            opacity: typeof datum === "number" ? 1 : (datum.o === undefined ? 1 : datum.o); radius: Theme.hairline
        }
    }
}
