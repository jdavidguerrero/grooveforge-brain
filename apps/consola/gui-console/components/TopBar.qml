import QtQuick
import "../theme"
Rectangle {
    id: root
    property bool os: console.screen === 2 || console.screen === 4
    property string view: ({2:"Choose a mode",4:console.settingsSection,6:"LAYER · BASS",8:"MIXER",9:"HANDS"})[console.screen] || "LIVE · SESSION"
    height: Theme.topHeight
    color: os ? Theme.gpBgBase : Theme.bg
    Label { x: Theme.s5; y: Theme.s1; text: root.os ? (console.screen === 4 ? "SETTINGS" : "GROOVEPILOT") : "STANDALONE"; color: Theme.mon; font.pixelSize: Theme.tiny }
    Label { x: Theme.s5; y: Theme.s5; text: root.view; font.family: root.os ? Theme.osFont : Theme.displayFont; font.pixelSize: root.os ? Theme.m20 : Theme.dM; height: Theme.lineM }
    Row {
        anchors.right: connRow.left; anchors.rightMargin: Theme.s5; anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s5; visible: !root.os
        Repeater {
            model: [{l:"KEY",v:console.keyName},{l:"BPM",v:console.bpm.toFixed(1)+" · LINK 3"},{l:"BAR",v:String(console.bar)}]
            Column { spacing: Theme.edge
                Label { text: modelData.l; color: Theme.mon; font.pixelSize: Theme.tiny }
                Label { text: modelData.v; font.pixelSize: Theme.m14 }
            }
        }
        Column { spacing: Theme.s1
            Label { text: "PHRASE "+console.phrase+"/8"; font.pixelSize: Theme.tiny; color: Theme.mon }
            Row { spacing: Theme.edge; Repeater { model: Theme.columns; Rectangle { width: Theme.tiny; height: Theme.tiny; radius: Theme.edge; color: index < console.phrase-1 ? Theme.teal : index === console.phrase-1 ? Theme.white : Theme.line } } }
        }
        Panel { width: Theme.topHeight; height: Theme.s5+Theme.s2; Label { anchors.centerIn: parent; text: console.section; font.family: Theme.displayFont; font.pixelSize: Theme.m20 } }
    }
    Row {
        id: connRow
        anchors.right: parent.right; anchors.rightMargin: Theme.s5; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.m14
        Repeater {
            model: console.connections
            Row { required property string title; required property string status; spacing: Theme.rCell
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: Theme.s2; height: Theme.s2; radius: Theme.s1; color: status === "ok" ? (root.os ? Theme.gpTeal : Theme.teal) : status === "warn" ? Theme.amber : status === "down" ? Theme.red : Theme.off }
                Label { text: title; font.pixelSize: Theme.tiny; color: status === "off" ? Theme.mon : status === "warn" ? Theme.amber : Theme.grayN }
            }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.surface2 }
}
