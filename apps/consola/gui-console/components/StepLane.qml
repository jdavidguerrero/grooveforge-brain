import QtQuick
import "../theme"
Item {
    id: root
    property var entry
    Panel { width: Layout.steps.labelW; height: parent.height; color: root.entry.d
        Label { x: Theme.s3; y: Theme.rCell; text: root.entry.n; font.pixelSize: Theme.keyLabel }
        Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.rCell; text: root.entry.role; color: root.entry.c; font.pixelSize: Theme.tiny }
    }
    Repeater { model: root.entry.steps
        Panel { required property int index; required property var modelData
            x: Layout.steps.gridX+index*Layout.steps.stepPitch; width: Layout.steps.stepW; height: root.height; color: modelData.bg
            border.color: modelData.bd.indexOf("#F2F2F2") >= 0 ? Theme.white : modelData.bg
            Rectangle { x: Theme.s1; width: parent.width-2*x; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s1; height: (parent.height-Theme.s2)*modelData.h/100; visible: modelData.on; color: modelData.c; radius: Theme.edge }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.s1; y: Theme.edge; text: modelData.prob; font.pixelSize: Theme.tagSize }
            Rectangle { x: parent.width/2+modelData.mi; y: Theme.edge; height: Theme.s1; width: Theme.edge; color: modelData.mc; visible: modelData.on }
        }
    }
}
