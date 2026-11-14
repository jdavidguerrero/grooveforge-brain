import QtQuick
import "../theme"
Flickable {
    id: root
    property var rows
    property real rowHeight: Layout.browse.rowH
    property real pitch: Layout.browse.pitch
    property int selected: 0
    signal chosen(int row)
    contentWidth:width
    contentHeight:repeater.count*root.pitch
    boundsBehavior:Flickable.StopAtBounds
    clip: true
    Repeater { id:repeater; model: root.rows
        Panel { required property int index; required property var entry
            y: index*root.pitch; width: root.width; height: root.rowHeight
            color: index === root.selected ? Theme.surface2 : Theme.surface
            border.color: index === root.selected ? Theme.white : Theme.surface
            Label { x: Theme.s3; y: Theme.s2; width: parent.width-2*x; text: entry.n; font.pixelSize: Theme.m14 }
            Label { x: Theme.s3; y: Theme.s2+Theme.m20; width: parent.width-2*x; text: entry.m || entry.k || ""; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
            TouchTarget { hardwareAction: "list"; anchors.fill: parent; callback: function() {root.selected=index;root.chosen(index);touch.inspect(entry.n+" · selected")} }
        }
    }
}
