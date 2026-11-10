import QtQuick
import "../theme"
Item {
    id: root
    property var rows
    property real rowHeight: Layout.note.padH
    property real rowPitch: Layout.note.padPitch
    Repeater { model: root.rows
        Panel { required property int index; required property var entry
            objectName: index < Theme.columns ? "column"+index : "pad"+index
            x: Theme.columnX(index%Theme.columns); y: Math.floor(index/Theme.columns)*root.rowPitch
            width: Theme.columnWidth; height: root.rowHeight; color: entry.bg; border.color: entry.bd; border.width: Theme.edge
            Label { x: Theme.s3; y: Theme.s2; text: entry.tag || entry.t || ''; color: entry.fg; font.pixelSize: Theme.tiny }
            Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s3; width: parent.width-2*x; text: entry.n; color: entry.fg; font.pixelSize: Theme.m20 }
        }
    }
}
