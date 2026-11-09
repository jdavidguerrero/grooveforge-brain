import QtQuick
import "../theme"
import "../components"
Rectangle {
    color: Theme.gpBgBase
    Image { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.bootGlyphY-(Theme.bootGlowSize-Theme.bootGlyphSize)/2; width: Theme.bootGlowSize; height: width; source: "../assets/boot.png" }
    Rectangle { id: glyph; anchors.horizontalCenter: parent.horizontalCenter; y: Theme.bootGlyphY; width: Theme.bootGlyphSize; height: width; radius: width/2; color: "transparent"; border.color: Theme.gpTeal; border.width: Theme.edge+Theme.hairline
        Rectangle { anchors.centerIn: parent; width: Theme.s4; height: width; radius: width/2; color: Theme.gpTeal }
        OpacityAnimator on opacity { from: 0.4; to: 1; duration: Theme.exitMs; loops: 1 }
    }
    Label { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.bootWordY; text: "GroovePilot"; face: Theme.osFont; font.pixelSize: Theme.bootWordSize }
    Label { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.bootSubY; text: "GrooveForge Console"; weight: Font.Medium; face: Theme.osFont; font.pixelSize: Theme.m18; color: Theme.gpTextSec }
    Meter { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.bootProgressY; width: Theme.bootProgressWidth; height: Theme.edge; value: 0.62; tint: Theme.gpTeal }
    Column { x: Theme.bootLogX; y: Theme.bootLogY; spacing: Theme.rCell
        Repeater { model: [{n:"BRAIN 2.4.1",v:"OK"},{n:"ENGINE MODELS",v:"6 / 8"},{n:"SURFACE · PADS · FSR · IR",v:"OK"},{n:"ABLETON LINK",v:"SEARCHING…"}]
            Row { spacing: Theme.s4
                Label { width: Theme.modeWidth/2; text: modelData.n; color: Theme.mon; weight: Font.Normal; face: Theme.osMonoFont; font.pixelSize: Theme.m12 }
                Label { text: modelData.v; color: modelData.v === "OK" ? Theme.gpTeal : Theme.gpTextSec; face: Theme.osMonoFont; font.pixelSize: Theme.m12 }
            }
        }
    }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s6; y: Theme.bootVersionY; text: "OS 0.9.2 · GUI 0.1.0"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
}
