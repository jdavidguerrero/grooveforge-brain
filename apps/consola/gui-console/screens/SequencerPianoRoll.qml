import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s3; y: Theme.s2; text: "BASS"; color: Theme.roleColor('BASS'); font.pixelSize: Theme.m12 }
    Label { x: Layout.steps.gridX; y: Theme.s2; text: "BARS 1–4 OF 8 · A1–A3 · A MINOR"; color: Theme.grayN; font.pixelSize: Theme.m12 }
    Label { x: Theme.roomTitleX+Theme.optionWidth; y: Theme.s2; text: "YOU · 14 · locked from engine"; color: Theme.roleColor('BASS'); font.pixelSize: Theme.m11 }
    Label { x: Theme.transitionSummaryX; y: Theme.s2; text: "ENGINE · 6   ·   PINCH / DRAG"; color: Theme.purpleBright; font.pixelSize: Theme.tiny }
    PianoRoll { x: Theme.s2; y: Layout.roll.y; width: Theme.summaryWidth; height: parent.height-y-Theme.s2 }
}
