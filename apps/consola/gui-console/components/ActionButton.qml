import QtQuick
import "../theme"
Panel {
    id: root
    property string text: ""
    property string action: ""
    property color tint: Theme.white
    property bool active: false
    property bool display: false
    signal clicked()
    color: active ? Theme.tealDim : Theme.surface2
    border.color: active ? Theme.tealPlus : Theme.line2
    Label { anchors.fill: parent; anchors.margins: Theme.s2; text: root.text; color: root.tint; horizontalAlignment: Text.AlignHCenter; face: root.display ? Theme.displayFont : Theme.monoFont; font.pixelSize: root.display ? Theme.dL : Theme.m12 }
    MouseArea { anchors.fill: parent; onClicked: { if(root.action) console.triggerAction(root.action); root.clicked() } }
}
