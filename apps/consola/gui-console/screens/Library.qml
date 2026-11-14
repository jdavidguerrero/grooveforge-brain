import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s3; y: Theme.s2; text: 'SETS · 6'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    BrowserList { x: Theme.s2; y: Layout.library.headerH+Theme.s2; width: Layout.library.navW; height: parent.height-y; rows: console.standalone.sets; rowHeight: Layout.library.setH; pitch: Layout.library.setPitch }
    Item { x: Layout.library.listX; y: Theme.s2; width: Layout.library.listW; height: parent.height-y
        Label { x: Theme.s3; text: '#   TRACK'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.library.keyX; text: 'KEY     BPM    ENERGY   LAYERS       SOURCE'; font.pixelSize: Theme.tagSize; color: Theme.mon }
        Repeater { model: console.standalone.tracks
            Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; y: Layout.library.headerH+index*Layout.library.pitch; width: parent.width; height: Layout.library.rowH; color: entry.bg; border.color: entry.bd
                Label { x: Theme.s3; height: parent.height; width: Theme.dL; text: entry.i; font.pixelSize: Theme.tagSize; color: Theme.mon }
                Label { x: Theme.topHeight; height: parent.height; width: Layout.library.keyX-x-Theme.s2; text: entry.n; font.pixelSize: Theme.m14 }
                Label { x: Layout.library.keyX; height: parent.height; text: entry.k; font.pixelSize: Theme.m12; color: entry.kc }
                Label { x: Layout.library.bpmX; height: parent.height; text: entry.b; font.pixelSize: Theme.m12; color: entry.bc }
                Row { x: Layout.library.energyX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.edge; Repeater { model: entry.e; Rectangle { required property var modelData; width: Theme.s2; height: Theme.s2; color: modelData; radius: Theme.hairline } } }
                Row { x: Layout.library.layersX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.edge; Repeater { model: entry.l; Rectangle { required property var modelData; width: Theme.s2; height: Theme.s3; color: modelData; radius: Theme.edge } } }
                Label { x: Layout.library.sourceX; height: parent.height; text: entry.s; font.pixelSize: Theme.tagSize; color: entry.sc }
            }
        }
    }
    Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; x: Layout.library.previewX; y: Theme.s2; width: Layout.library.previewW; height: parent.height-Theme.s4
        Label { x: Theme.s4; y: Theme.s3; text: 'SELECTED'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.dL; text: 'Río Lento'; font.pixelSize: Theme.trackSize }
        Label { x: Theme.s4; y: Theme.mixerStripsY; text: 'A MIN ✓ · 123 → 124 ✓ · STUDIO'; color: Theme.tealPlus; font.pixelSize: Theme.m12 }
        Repeater { model: console.standalone.selLayers
            Item { required property int index; required property var entry; x: Theme.s4; y: Theme.layerCardsY+index*(Theme.m28+Theme.s1); width: parent.width-2*x; height: Theme.m28
                Rectangle { y: Theme.s2; width: Theme.s3; height: Theme.s3; color: entry.c; radius: Theme.edge }
                Label { x: Theme.s5; height: parent.height; text: entry.k; color: entry.tc; font.pixelSize: Theme.m11 }
                Label { x: Theme.layerHeader; height: parent.height; width: parent.width-x; text: entry.s; color: Theme.grayN; font.pixelSize: Theme.m12 }
            }
        }
        ActionButton { x: Theme.s4; y: parent.height-Theme.topHeight*2-Theme.s5; width: parent.width-2*x; height: Theme.topHeight; text: 'LOAD AS NEXT'; active: true }
        ActionButton { x: Theme.s4; y: parent.height-Theme.topHeight-Theme.s4; width: parent.width-2*x; height: Theme.s6; text: 'LOAD ONE LAYER…' }
    }
}
