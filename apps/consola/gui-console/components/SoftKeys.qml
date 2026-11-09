import QtQuick
import "../theme"
Rectangle {
    id: root
    color: Theme.bg
    height: Theme.keysHeight
    Repeater {
        model: console.softKeys
        Rectangle {
            id: cap
            required property int index
            required property string label
            required property string value
            required property string roleName
            required property bool lit
            required property bool turning
            required enabled
            required property real position
            property color tint: label === "Generate" ? Theme.purpleBright : Theme.roleColor(roleName)
            objectName: "keycap"+index
            x: Theme.columnX(index); y: Theme.s2; width: Theme.columnWidth; height: Theme.keyHeight
            radius: Theme.rCell
            border.width: Theme.hairline
            border.color: turning ? tint : Theme.line2
            gradient: Gradient { GradientStop { position: 0; color: Theme.keyTop } GradientStop { position: 1; color: Theme.keyBottom } }
            opacity: enabled ? 1 : 0.4
            Image { x: Theme.s1; y: 0; width: parent.width-2*x; height: Theme.glowLedHeight; source: Theme.ledGlow(cap.label === "Generate" ? "PURPLE" : cap.roleName); visible: cap.lit || cap.turning || cap.label === "Generate" }
            Rectangle { x: Theme.keyInset; y: Theme.keyLedY; width: parent.width-2*Theme.keyInset; height: Theme.keyLedHeight; radius: Theme.edge; color: cap.lit || cap.turning || cap.label === "Generate" ? cap.tint : Theme.roleDim(cap.roleName) }
            Label { x: Theme.keyInset; y: Theme.keyLabelY; width: parent.width-2*x; text: cap.label.toUpperCase(); font.pixelSize: Theme.keyLabel; color: cap.lit ? Theme.white : Theme.grayN }
            Label { x: Theme.keyInset; y: Theme.keyValueY; width: parent.width-2*x; text: cap.value; font.pixelSize: Theme.m18; color: cap.turning ? cap.tint : Theme.white }
            Rectangle { visible: cap.position >= 0; x: Theme.keyInset; y: Theme.keyPositionY; width: parent.width-2*x; height: Theme.edge; color: Theme.line2
                Rectangle { width: parent.width * Math.max(0, cap.position); height: parent.height; color: cap.tint }
            }
            MouseArea { anchors.fill: parent; enabled: cap.enabled; onClicked: console.pressEncoder(cap.index); onWheel: function(event) { console.turnEncoder(cap.index, event.angleDelta.y > 0 ? 1 : -1) } }
        }
    }
}
