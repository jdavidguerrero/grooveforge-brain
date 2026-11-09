import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Theme.layerHeader
        Rectangle { x: Theme.s5; y: Theme.s5-Theme.s1; width: Theme.s4; height: Theme.topHeight; radius: Theme.edge; color: Theme.roleColor("BASS") }
        Label { x: Theme.topHeight; anchors.verticalCenter: parent.verticalCenter; text: "BASS"; color: Theme.roleColor("BASS"); font.family: Theme.displayFont; font.pixelSize: Theme.dXl }
        Column { x: Theme.modeWidth/2; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s1
            Label { text: "● IN ROOM"; color: Theme.tealPlus; font.pixelSize: Theme.m11 }
            Label { text: "Sub Model D"; font.pixelSize: Theme.m20 }
            Label { text: "A1 · round · energy 3 · since bar 17"; color: Theme.mon; font.pixelSize: Theme.m12 }
        }
        Column { x: Theme.nextTitleX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s2
            Label { text: "LEVEL"; color: Theme.mon; font.pixelSize: Theme.m11 }
            Meter { width: Theme.handsLadderWidth; height: Theme.s2; value: .77 }
            Label { text: "−3.2 dB"; font.pixelSize: Theme.m12; color: Theme.grayN }
        }
        Column { x: Theme.transitionSummaryX-Theme.s6; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s1
            Label { text: "ACCEPT  → NEXT PHRASE"; color: Theme.tealPlus; font.pixelSize: Theme.m12 }
            Label { text: "GENERATE  → ANOTHER"; color: Theme.purpleBright; font.pixelSize: Theme.m12 }
            Label { text: "SKIP  → DISCARD CUE"; color: Theme.grayN; font.pixelSize: Theme.m12 }
        }
    }
    Repeater { model: console.options
        OptionCard {
            required property int index
            required letter
            required title
            required subtitle
            required optionState
            required energy
            required notes
            required hint
            required encoder
            objectName: "optionCard"+index
            x: Theme.columnX(index*2); y: Theme.layerCardsY; width: Theme.optionWidth; height: Theme.layerCardsHeight
            MouseArea { anchors.fill: parent; onClicked: console.triggerAction("cue:"+parent.index) }
        }
    }
}
