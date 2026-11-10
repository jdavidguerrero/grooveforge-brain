import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s3; y: Theme.s2; text: 'LIVE BROWSER'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    Label { x: Layout.browse.listX; y: Theme.s2; text: 'DRUMS / HOUSE'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    BrowserList { x: Theme.s2; y: Layout.browse.headerH; width: Layout.browse.navW; height: parent.height-y; rows: console.controller.cats; selected: 1 }
    BrowserList { x: Layout.browse.listX; y: Layout.browse.headerH; width: Layout.browse.listW; height: parent.height-y; rows: console.controller.items }
    Panel { x: Layout.browse.previewX; y: Theme.s2; width: Layout.browse.previewW; height: parent.height-Theme.s4
        Label { x: Theme.s4; y: Theme.s4; text: 'PREVIEW · DRUMS'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.topHeight; width: parent.width-2*x; height: Theme.s6; text: 'Organic House Kit'; wrapMode: Text.WordWrap; font.pixelSize: Theme.trackSize }
        Label { x: Theme.s4; y: Theme.layerCardsY; text: 'Drum Rack · 16 pads'; color: Theme.grayN; font.pixelSize: Theme.m12 }
        Waveform { x: Theme.s4; y: Theme.settingsForceY; width: parent.width-2*x; height: Theme.dXl; bars: console.controller.bWave; snapshot: true }
        Label { x: Theme.s4; y: Theme.modalOrderY; width: parent.width-2*x; text: 'house · organic · wood\n124 bpm'; wrapMode: Text.WordWrap; height: Theme.dXl; font.pixelSize: Theme.m12; color: Theme.grayN }
        Label { x: Theme.s4; y: Theme.modalChipY+Theme.dXl; width: parent.width-2*x; text: 'LOAD REPLACES THE DEVICE\nON THE SELECTED TRACK'; wrapMode: Text.WordWrap; height: Theme.dXl; font.pixelSize: Theme.m11; color: Theme.mon }
        ActionButton { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s4; width: parent.width-2*x; height: Theme.topHeight; text: 'LOAD TO DRUMS'; active: true }
    }
}
