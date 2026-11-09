import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: 2
        Panel {
            required property int index
            property bool standalone: index === 1
            x: Theme.modeX+index*(Theme.modeWidth+Theme.modeGap); y: Theme.modeY
            width: Theme.modeWidth; height: Theme.modeHeight; color: Theme.gpBgCard; radius: Theme.rOs
            border.width: Theme.edge; border.color: console.selectedMode === index ? Theme.gpTeal : Theme.gpBorder
            Label { x: Theme.modePad; y: Theme.modePad; text: "ENC "+(index+1); color: Theme.mon; font.pixelSize: Theme.m12 }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.modePad; y: Theme.modePad; text: parent.standalone ? "READY" : "LIVE NOT FOUND"; color: parent.standalone ? Theme.gpTeal : Theme.amber; font.pixelSize: Theme.m11 }
            Label { x: Theme.modePad; y: Theme.modeTitleY; text: parent.standalone ? "Standalone" : "Controller"; font.family: Theme.osFont; font.pixelSize: Theme.modeTitleSize }
            Label { x: Theme.modePad; y: Theme.modeDescY; width: parent.width-2*x; height: Theme.infoHeight; wrapMode: Text.WordWrap; font.family: Theme.osFont; font.pixelSize: Theme.m18; color: Theme.gpTextSec; text: parent.standalone ? "No computer. The Brain generates every layer; you play it like a DJ set." : "Push-style control of Ableton Live, plus the Mac engine in studio mode." }
            Column { x: Theme.modePad; y: Theme.modeStatusY; spacing: Theme.s3
                Repeater { model: index === 1 ? ["Brain      ready · 6/8 models", "Library    42 tracks · 6 sets", "Network    Ethernet 10.0.0.24"] : ["Live       not found on network", "Link       3 peers", "USB / MIDI host ready"]
                    Row { spacing: Theme.s3
                        Rectangle { width: Theme.s2; height: width; radius: width/2; color: Theme.gpTeal; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: modelData; color: Theme.gpTextSec; font.pixelSize: Theme.m14 }
                    }
                }
            }
            MouseArea { anchors.fill: parent; onClicked: console.triggerAction("mode:"+parent.index) }
        }
    }
    Label { y: Theme.modeHintY; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "TURN ENC 1–2 OR TAP · PRESS TO ENTER · LAST MODE STARTS AFTER 8 S"; color: Theme.mon; font.pixelSize: Theme.m12 }
}
