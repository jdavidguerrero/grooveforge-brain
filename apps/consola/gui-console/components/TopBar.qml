import QtQuick
import "../theme"
Rectangle {
    id: root
    property bool os: console.screen === 2 || console.screen === 4 || console.screenName.indexOf("SynthSettings") === 0
    property string view: console.screen === 4 ? console.settingsSection : console.screenTitle
    property string sectionName: console.screenName === 'SessionNoLive' ? '—' : console.screenMode.indexOf('CONTROLLER') === 0 ? 'SCENE 4' : console.section
    height: Theme.topHeight
    color: os ? Theme.gpBgBase : Theme.bg
    Label { x: Theme.m20; y: Theme.s1; text: root.os ? (console.screen === 4 || console.screenName.indexOf("SynthSettings") === 0 ? "SETTINGS" : "GROOVEPILOT") : console.screenMode; color: Theme.mon; font.pixelSize: Theme.tiny; weight: Font.Normal }
    Label { x: Theme.m20; y: Theme.s5; text: root.view; face: root.os ? Theme.osFont : Theme.displayFont; font.pixelSize: root.os ? Theme.m20 : Theme.dM; weight: Font.Bold; height: Theme.lineM }
    Row {
        anchors.right: separator.left; anchors.rightMargin: Theme.m28; anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s5; visible: !root.os && !console.synthMode && !console.drumMode
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
    Row { x:480;anchors.verticalCenter:parent.verticalCenter;spacing:24;visible:console.synthMode
        Label {text:(synth.partInfo.role || 'PART')+' · '+surface.roots[surface.root]+' '+surface.scale; font.pixelSize:14;color:Theme.tealPlus}
        Label {text:surface.status;font.pixelSize:11;color:Theme.grayN}
    }
    Row { x:430;anchors.verticalCenter:parent.verticalCenter;spacing:20;visible:console.drumMode && console.screenName.indexOf('Drum')===0
        Label {text:drum.rack.root+' '+drum.rack.mode+' · '+Number(drum.clock.bpm).toFixed(1)+' BPM';font.pixelSize:13;color:Theme.tealPlus}
        Label {text:drum.clock.running?'PLAY · '+drum.clock.bar+'.'+(Math.floor(drum.clock.step/4)+1):'STOPPED · PRESS PLAY';font.pixelSize:12;color:drum.clock.running?Theme.tealPlus:Theme.grayN}
        Label {text:drum.demo?'PREVIEW · NO AUDIO':drum.status.toUpperCase();font.pixelSize:10;color:Theme.grayN}
    }
    TouchTarget {x:430;y:0;width:208;height:56;anchors.fill:null;enabled:console.drumMode && console.screenName.indexOf("Drum")===0;hardwareAction:"choice";callback:function(){drum.action("tempo")} }
    Rectangle { id: separator; anchors.right: connRow.left; anchors.rightMargin: Theme.m28; anchors.verticalCenter: parent.verticalCenter; width: Theme.hairline; height: Theme.m28; color: Theme.line; visible: !root.os && !console.synthMode && !console.drumMode }
    Row {
        id: connRow
        anchors.right: parent.right; anchors.rightMargin: Theme.m20; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.m14
        Row {visible:console.screenName.indexOf('Synth')===0 && !console.synthMode;spacing:6
            Rectangle {anchors.verticalCenter:parent.verticalCenter;width:8;height:8;radius:4;color:Theme.teal}
            Label {text:'ENGINE';font.pixelSize:10;color:Theme.grayN}
        }
        Row { visible:console.drumMode;spacing:12
            Label {text:'HOST '+drum.status.toUpperCase();font.pixelSize:10;color:drum.status==='ready'?Theme.tealPlus:Theme.amber}
            Label {text:drum.demo?'SURFACE PREVIEW':drumSurface.status.indexOf('ERROR')>=0?'SURFACE ERROR':'SURFACE';font.pixelSize:10;color:drumSurface.status.indexOf('ERROR')>=0?Theme.amber:Theme.grayN}
        }
        Repeater {
            model: console.drumMode ? null : console.connections
            Row { required property string title; required property string status; spacing: Theme.rCell
                Rectangle { property string effectiveStatus: title === "BRAIN" && console.screenName.indexOf("Synth") === 0 ? (synth.status === "not_responding" ? "warn" : synth.status === "ready" || synth.status === "preview" || synth.status === "loading" ? "ok" : "off") : status; anchors.verticalCenter: parent.verticalCenter; width: Theme.s2; height: Theme.s2; radius: Theme.s1; color: effectiveStatus === "ok" ? (root.os ? Theme.gpTeal : Theme.teal) : effectiveStatus === "warn" ? Theme.amber : effectiveStatus === "down" ? Theme.red : Theme.off }
                Label { text: title; face: root.os ? Theme.osMonoFont : Theme.monoFont; font.pixelSize: Theme.tiny; color: status === "off" ? Theme.mon : status === "warn" ? Theme.amber : Theme.grayN }
            }
        }
    }
    TouchTarget { width:180;height:56;anchors.fill:null;hardwareAction:"navigation";callback:function(){console.navigate('ModeSelect')} }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.surface2 }
}
