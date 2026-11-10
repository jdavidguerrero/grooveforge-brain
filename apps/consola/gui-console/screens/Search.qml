import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Layout.search.chipsX-2*x; height: Layout.search.fieldH; border.width: Theme.edge; border.color: Theme.tealPlus
        Label { x: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: console.query+"│"; font.pixelSize: Theme.trackSize }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: "38 RESULTS"; color: Theme.grayN; font.pixelSize: Theme.m12 }
        MouseArea { anchors.fill: parent; onClicked: console.triggerAction('keyboard') }
    }
    Repeater { model: console.standalone.sChips
        Panel { required property int index; required property var entry; x: Layout.search.chipsX+index*(Layout.search.chipW+Theme.s2); y: Theme.s2; width: Layout.search.chipW; height: Layout.search.fieldH; color: entry.bg; border.color: entry.bd
            Label { x: Theme.s3; y: Theme.s3; text: entry.n; color: entry.fg; font.pixelSize: Theme.m12 }
            Label { x: Theme.s3; y: Theme.s3+Theme.m18; text: entry.k; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        }
    }
    Repeater { model: console.standalone.sRes
        Panel { required property int index; required property var entry; x: Theme.s2; y: Layout.search.resultsY+index*Layout.search.rowPitch; width: Layout.search.previewX-2*x; height: Layout.search.rowH; color: entry.bg; border.color: entry.bd
            Label { x: Theme.s4; height: parent.height; text: entry.t; font.pixelSize: Theme.m11; color: Theme.grayN }
            Label { x: Layout.search.nameX; width: Layout.search.metaX-x-Theme.s3; height: parent.height; textFormat: Text.RichText; text: '<font color="'+Theme.tealPlus+'">'+entry.m+'</font>'+entry.b; font.pixelSize: Theme.m18 }
            Label { x: Layout.search.metaX; width: Layout.search.roleX-x-Theme.s2; height: parent.height; text: entry.meta; font.pixelSize: Theme.m12; color: Theme.mon }
            Label { x: Layout.search.roleX; width: parent.width-x-Theme.s3; height: parent.height; text: entry.role; font.pixelSize: Theme.tiny; color: entry.rc }
        }
    }
    Panel { x: Layout.search.previewX; y: Layout.search.resultsY; width: Layout.search.previewW; height: Layout.search.resultsH
        Label { x: Theme.s4; y: Theme.s3; text: "PREVIEW · IN CUE"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.dL; text: "Kalimba Glass"; font.pixelSize: Theme.m20 }
        Label { x: Theme.s4; y: Theme.topHeight+Theme.s3; text: "PRESET · BRAIN CHORUS POLY · LEAD"; font.pixelSize: Theme.m12; color: Theme.gray }
        Waveform { x: Theme.s4; y: Theme.layerHeader; width: parent.width-2*x; height: Theme.topHeight; bars: console.standalone.sWave; snapshot: true }
        ActionButton { x: Theme.s4; y: parent.height-Theme.s6-Theme.s4; width: (parent.width-Theme.s6)/2; height: Theme.s6; text: "LOAD TO LEAD"; active: true }
        ActionButton { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: parent.height-Theme.s6-Theme.s4; width: (parent.width-Theme.s6)/2; height: Theme.s6; text: "STOP CUE" }
    }
    Keyboard { x: Theme.s2; y: Layout.search.keyboardY; width: parent.width-2*x; height: Layout.search.keyboardH; visible: console.keyboardVisible }
}
