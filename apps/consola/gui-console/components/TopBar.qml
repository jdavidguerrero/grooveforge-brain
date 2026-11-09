import QtQuick
import "../theme"
Rectangle {
    id: root
    property bool os: console.screen === 2 || console.screen === 4
    property string view: console.screen === 4 ? console.settingsSection : console.screenTitle
    property string sectionName: console.screenName === 'SessionNoLive' ? '—' : console.screenMode.indexOf('CONTROLLER') === 0 ? 'SCENE 4' : console.section
    height: Theme.topHeight
    color: os ? Theme.gpBgBase : Theme.bg
    Label { x: Theme.m20; y: Theme.s1; text: root.os ? (console.screen === 4 ? "SETTINGS" : "GROOVEPILOT") : console.screenMode; color: Theme.mon; font.pixelSize: Theme.tiny; weight: Font.Normal }
    Label { x: Theme.m20; y: Theme.s5; text: root.view; face: root.os ? Theme.osFont : Theme.displayFont; font.pixelSize: root.os ? Theme.m20 : Theme.dM; weight: Font.Bold; height: Theme.lineM }
    Row {
        anchors.right: separator.left; anchors.rightMargin: Theme.m28; anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s5; visible: !root.os
        Column { spacing: Theme.edge
            Label { text: 'KEY'; color: Theme.mon; font.pixelSize: Theme.tiny; weight: Font.Normal }
            Label { text: console.keyName; font.pixelSize: Theme.s4 }
        }
        Column { spacing: Theme.edge
            Label { text: 'BPM'; color: Theme.mon; font.pixelSize: Theme.tiny; weight: Font.Normal }
            Row { spacing: Theme.rCell
                Label { id: bpmValue; text: console.bpm.toFixed(1); font.pixelSize: Theme.s4 }
                Label { text: 'LINK 3'; font.pixelSize: Theme.tiny; color: Theme.tealPlus; anchors.baseline: bpmValue.baseline }
            }
        }
        Column { spacing: Theme.edge
            Label { text: 'BAR'; color: Theme.mon; font.pixelSize: Theme.tiny; weight: Font.Normal }
            Label { text: String(console.bar); font.pixelSize: Theme.s4 }
        }
        Column { spacing: Theme.s1
            Label { text: "PHRASE "+console.phrase+"/8"; font.pixelSize: Theme.tiny; color: Theme.mon; weight: Font.Normal }
            Row { spacing: Theme.rCell/2; Repeater { model: Theme.columns; Rectangle { width: Theme.tiny; height: Theme.tiny; radius: Theme.edge; color: index < console.phrase-1 ? Theme.teal : index === console.phrase-1 ? Theme.white : Theme.line } } }
        }
        Panel { width: sectionText.implicitWidth+Theme.s5; height: Theme.s5+Theme.s2; color: Theme.surface2; border.color: Theme.line2
            Label { id: sectionText; anchors.centerIn: parent; text: root.sectionName; face: Theme.displayFont; weight: Font.Bold; font.pixelSize: Theme.m20 }
        }
    }
    Rectangle { id: separator; anchors.right: connRow.left; anchors.rightMargin: Theme.m28; anchors.verticalCenter: parent.verticalCenter; width: Theme.hairline; height: Theme.m28; color: Theme.line; visible: !root.os }
    Row {
        id: connRow
        anchors.right: parent.right; anchors.rightMargin: Theme.m20; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.m14
        Repeater {
            model: console.connections
            Row { required property string title; required property string status; spacing: Theme.rCell
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: Theme.s2; height: Theme.s2; radius: Theme.s1; color: status === "ok" ? (root.os ? Theme.gpTeal : Theme.teal) : status === "warn" ? Theme.amber : status === "down" ? Theme.red : Theme.off }
                Label { text: title; face: root.os ? Theme.osMonoFont : Theme.monoFont; font.pixelSize: Theme.tiny; color: status === "off" ? Theme.mon : status === "warn" ? Theme.amber : Theme.grayN }
            }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.surface2 }
}
