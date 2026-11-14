import QtQuick
import "../theme"
Item {
    id: root
    property var rows
    property int columns: 1
    property real tileHeight: Layout.modal.tileH
    property bool display: false
    property bool centered: true
    property string selection: ""
    signal chosen(string name)
    Repeater { model: root.rows
        Panel { required property int index; required property var entry
            x: (index % root.columns) * ((root.width+Theme.rCell)/root.columns)
            y: Math.floor(index/root.columns)*(root.tileHeight+Theme.rCell)
            width: (root.width-(root.columns-1)*Theme.rCell)/root.columns; height: root.tileHeight
            color: root.selection ? (root.selection === entry.n ? Theme.tealDim : Theme.surface2) : entry.bg
            border.color: root.selection ? (root.selection === entry.n ? Theme.tealPlus : Theme.line) : (entry.bd || Theme.line)
            Label { anchors.fill: parent; anchors.margins: Theme.s3; text: entry.n; color: root.selection ? Theme.white : (entry.fg || Theme.white); horizontalAlignment: root.centered ? Text.AlignHCenter : Text.AlignLeft; face: root.display ? Theme.displayFont : Theme.monoFont; font.pixelSize: root.display ? Theme.dL : Theme.m14 }
            TouchTarget { hardwareAction: "choice"; anchors.fill: parent; callback: function() {root.selection = entry.n; root.chosen(entry.n);touch.inspect(entry.n+" · selected")} }
        }
    }
}
