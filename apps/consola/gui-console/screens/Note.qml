import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.note.summaryH
        Rectangle { x: Theme.s4; y: Theme.s4; width: Theme.s2; height: Theme.dXl; color: Theme.liveDrums; radius: Theme.edge }
        Label { x: Theme.dXl; y: Theme.s4; text: 'TRACK 1 · DRUM RACK'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.dXl; y: Theme.dL; text: 'Drums'; font.pixelSize: Theme.trackSize }
        Label { x: Theme.settingsBodyX; y: Theme.s4; text: 'SELECTED PAD'; font.pixelSize: Theme.m11; color: Theme.mon }
        Label { x: Theme.settingsBodyX; y: Theme.dL; text: 'CLAP 909'; face: Theme.displayFont; font.pixelSize: Theme.s6; weight: Font.ExtraBold }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.s4; text: 'CLIP · Groove A · 1 bar · 1/16'; font.pixelSize: Theme.m14 }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.topHeight; text: 'LAYOUT ▸ DRUM · SCALE A MIN FOR MELODIC TRACKS'; font.pixelSize: Theme.m12; color: Theme.gray }
    }
    PadGrid { y: Layout.note.gridY; width: parent.width; height: Layout.note.gridH; rows: console.controller.noteGrid }
}
