import QtQuick
import "../theme"
Item {
    id: root
    property bool hostDriven: false
    property var rows
    property int arcSize: Layout.brain.arcSize
    property color tint: Theme.teal
    Repeater { model: root.hostDriven ? synth.cells : root.rows
        Panel { required property int index; required property var entry
            objectName: "column"+index
            x: Theme.columnX(index); width: Theme.columnWidth; height: root.height
            border.color: root.hostDriven ? Theme.line : entry.bd || Theme.line
            ParameterCell { anchors.fill: parent; visible: root.hostDriven; entry: root.hostDriven ? parent.entry : ({empty:true,name:"—",text:"—",unit:"",kind:"empty",amount:0}); tint: root.tint; focused: parent.index === synth.focusedCell; loading: synth.status === "loading" }
            ArcKnob { visible: !root.hostDriven; width: root.arcSize; height: width; anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height-height)/2-Theme.s4; text: root.hostDriven ? "" : entry.v; tint: index === 0 ? Theme.tealPlus : root.tint
                value: entry.amount || 0 }
            Label { visible: !root.hostDriven; y: parent.height-Theme.s6; width: parent.width; text: root.hostDriven ? "" : entry.n; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.m12; color: Theme.grayN }
        }
    }
}
