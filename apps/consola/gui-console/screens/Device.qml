import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s4; y: Theme.s4; text: 'Bass'; color: Theme.liveBass; font.pixelSize: Theme.keyLabel }
    Repeater { model: console.controller.chain
        ActionButton { required property int index; required property var entry; x: Layout.device.chipsX+index*Layout.device.chipPitch; y: Theme.s2; width: Layout.device.chipW; height: Layout.device.chipH; text: '● '+entry.n; tint: entry.fg; color: entry.bg; border.color: entry.bd }
    }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s5; text: 'BANK 1 / 3 · FILTER  ▪ ▪ ▪'; font.pixelSize: Theme.m11; color: Theme.grayN }
    ParameterPage { y: Layout.device.arcsY; width: parent.width; height: Layout.device.arcsH; rows: console.controller.devArcs; arcSize: Layout.device.arcSize }
    Panel { x: Theme.s2; y: Layout.device.graphY; width: Theme.summaryWidth; height: Layout.device.graphH
        Label { x: Theme.s4; y: Theme.s3; text: 'AUTO FILTER · RESPONSE'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s3; text: 'LP 24 · 1.2k · RES 38 %'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        GraphPlot { x: Theme.s4; y: Theme.dL; width: parent.width-2*x; height: parent.height-y-Theme.s5; kind: 'filter'; tint: Theme.tealPlus }
        Label { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s1; width: parent.width-2*x; text: '20                       100                         1k                         10k                         20k'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
    }
}
