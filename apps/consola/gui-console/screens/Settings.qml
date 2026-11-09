import QtQuick
import "../theme"
import "../components"
Item {
    Column { x: Theme.s5; y: Theme.s5; spacing: Theme.s1
        Repeater { model: ["Audio","Connections","Surface","System"]
            Panel { width: Theme.settingsNavWidth; height: Theme.settingsRow; color: Theme.gpBgCard; border.color: console.settingsSection === modelData ? Theme.gpTeal : Theme.gpBorder
                Label { x: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: modelData; font.family: Theme.osFont; font.pixelSize: Theme.m18 }
                Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: "ENC "+(index+1); font.pixelSize: Theme.m11; color: Theme.mon }
                MouseArea { anchors.fill: parent; onClicked: console.triggerAction("settings:"+modelData) }
            }
        }
    }
    Item { x: Theme.settingsBodyX; width: Theme.settingsBodyWidth; height: parent.height; visible: console.settingsSection === "Surface"
        Repeater { model: [{n:"Pad LEDs",v:80,d:"Screen, pads and key LEDs share role colours."},{n:"Key LEDs",v:65,d:"Dim the keys without losing their state."}]
            Panel { x: index*(Theme.settingsBodyWidth+Theme.s4)/2; y: Theme.s5; width: (Theme.settingsBodyWidth-Theme.s4)/2; height: Theme.settingsLedHeight; color: Theme.gpBgCard; border.color: Theme.gpBorder
                Label { x: Theme.s4; y: Theme.s4; text: modelData.n; font.pixelSize: Theme.m18; font.family: Theme.osFont }
                Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s4; text: modelData.v+"%"; font.pixelSize: Theme.m20 }
                Meter { x: Theme.s4; y: Theme.topHeight; width: parent.width-2*x; height: Theme.osSliderHeight; value: modelData.v/100; tint: Theme.gpTeal }
                Label { x: Theme.s4; y: Theme.mixerStripsY; width: parent.width-2*x; text: modelData.d; font.pixelSize: Theme.m11; color: Theme.mon }
            }
        }
        Panel { y: Theme.settingsForceY; width: parent.width; height: Theme.settingsForceHeight; color: Theme.gpBgCard; border.color: Theme.gpBorder
            Label { x: Theme.s4; y: Theme.s3; text: "Force pads"; font.family: Theme.osFont; font.pixelSize: Theme.m18 }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s4; text: "Hit each pad softly, then hard. Marker = trigger threshold."; font.pixelSize: Theme.m11; color: Theme.gpTextSec }
            Repeater { model: [0.18,0.72,0.42,0.08]
                Panel { x: Theme.s4+index*(Theme.fsrTileWidth+Theme.s4); y: Theme.topHeight; width: Theme.fsrTileWidth; height: Theme.settingsForceHeight-Theme.topHeight-Theme.s4; color: Theme.gpBgBase; border.color: index === 1 ? Theme.gpTeal : Theme.gpBorder
                    Label { x: Theme.s3; y: Theme.s2; text: "FSR "+(index+1); font.pixelSize: Theme.m14 }
                    Rectangle { x: Theme.s3; y: Theme.dL; width: parent.width-2*x; height: Theme.fsrHeight; color: Theme.surface2
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*modelData; color: Theme.gpTeal }
                        Rectangle { y: parent.height*.72; width: parent.width; height: Theme.edge; color: Theme.white }
                    }
                    Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; text: "THR 28 · VEL "+Math.round(modelData*127); color: Theme.gpTextSec; font.pixelSize: Theme.m12 }
                }
            }
        }
        Repeater { model: ["Left IR", "Right IR"]
            Panel { x: index*(Theme.settingsBodyWidth+Theme.s4)/2; y: Theme.settingsIrY; width: (Theme.settingsBodyWidth-Theme.s4)/2; height: Theme.settingsIrHeight; color: Theme.gpBgCard; border.color: Theme.gpBorder
                Label { x: Theme.s4; y: Theme.s4; text: modelData; font.family: Theme.osFont; font.pixelSize: Theme.m18 }
                Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s4; text: index ? "17 cm · READY" : "12 cm · READY"; color: Theme.gpTeal; font.pixelSize: Theme.m12 }
                Meter { x: Theme.s4; y: Theme.topHeight; width: parent.width-2*x; height: Theme.s5; value: .77; tint: Theme.gpTeal }
                Label { x: Theme.s4; y: Theme.layerHeader; text: "4 cm        RANGE 6–26 CM        30 cm"; color: Theme.mon; font.pixelSize: Theme.m12 }
            }
        }
    }
    Panel { x: Theme.settingsBodyX; y: Theme.s5; width: Theme.settingsBodyWidth; height: Theme.settingsForceHeight; visible: console.settingsSection !== "Surface"; color: Theme.gpBgCard; border.color: Theme.gpBorder
        Label { x: Theme.s5; y: Theme.s5; text: console.settingsSection; font.family: Theme.osFont; font.pixelSize: Theme.dL }
        Label { x: Theme.s5; y: Theme.layerHeader; text: ({Audio:"48 kHz · 128 samples · Stereo room + cue",Connections:"Brain ready · Link 3 peers · Ethernet 10.0.0.24",System:"GroovePilot OS 0.9.2 · Console GUI 0.1.0 · Demo data"})[console.settingsSection] || ""; color: Theme.gpTextSec; font.pixelSize: Theme.m18 }
        Label { x: Theme.s5; y: Theme.settingsForceY; text: "DEMO · NO HARDWARE CONNECTED"; color: Theme.mon; font.pixelSize: Theme.m12 }
    }
}
