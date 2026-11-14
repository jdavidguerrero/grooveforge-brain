import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s4; y: Theme.s2; text: "Bass · Groove A"; font.pixelSize: Theme.m14; color: Theme.liveBass }
    Label { x: Theme.settingsBodyX; y: Theme.s2; text: "LOOP 1.1.1 – 3.1.1 · 2 BARS · A MIN"; font.pixelSize: Theme.m12; color: Theme.grayN }
    Meter { x: Theme.nextTitleX+Theme.s6; y: Theme.s4; width: Theme.optionWidth+Theme.infoHeight; height: Theme.m14; value: .5; tint: '#28406A'
        Rectangle { x: parent.width*.125; width: parent.width*.125; y: -Theme.s1; height: parent.height+Theme.s2; color: 'transparent'; border.color: Theme.white; border.width: Theme.edge }
    }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s2; y: Theme.s2; text: "VIEW 1.1–1.3"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    PianoRoll { x: Theme.s2; y: Layout.roll.y; width: Theme.summaryWidth; height: parent.height-y-Theme.s2; clipView: true }
}
