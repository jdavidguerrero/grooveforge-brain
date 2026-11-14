import QtQuick
import "../theme"
Panel {
    id: root
    property string text: ""
    property string hardwareAction: "navigation"
    property var callback: null
    property string action: ""
    property color tint: Theme.white
    property bool active: false
    property bool display: false
    signal clicked()
    color: active || reviewSelected ? Theme.tealDim : Theme.surface2
    border.color: active || reviewSelected ? Theme.tealPlus : Theme.line2
    Label { anchors.fill: parent; anchors.margins: Theme.s2; text: root.text; color: root.tint; horizontalAlignment: Text.AlignHCenter; face: root.display ? Theme.displayFont : Theme.monoFont; font.pixelSize: root.display ? Theme.dL : Theme.m12 }
    TouchTarget { hardwareAction: root.hardwareAction; anchors.fill: parent; callback: function() { if(typeof root.callback === "function") root.callback(); else if(root.action) console.triggerAction(root.action); else {root.reviewSelected=!root.reviewSelected;touch.inspect(root.text+" · design demo · service unavailable")} root.clicked() } }
}
