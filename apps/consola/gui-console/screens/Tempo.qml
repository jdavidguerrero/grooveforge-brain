import QtQuick
import "../theme"
import "../components"
ReviewWindow {
    Label { x: Theme.m28; y: Theme.m28; text: 'TEMPO · ENC 1 COARSE · SHIFT FINE'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
    Label { x: Theme.m28; y: Layout.modal.tempoNumberY; text: '124.00'; face: Theme.displayFont; font.pixelSize: Layout.modal.tempoSize; weight: Font.ExtraBold }
    Row { x: Theme.m28; y: Layout.modal.tempoTapY; spacing: Theme.s3
        Label { text: 'TAP'; color: Theme.grayN; font.pixelSize: Theme.m12 }
        Repeater { model: console.controller.taps; Rectangle { required property int index; required property var entry; width: Theme.m28; height: width; color: entry; radius: width/2 } }
        Label { text: '4 taps · 123.96'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
    }
    Repeater { model: console.controller.tempoBtns
        ActionButton { required property int index; required property var entry; x: Theme.m28+index*(Layout.modal.tempoLeftW-Theme.m28)/3; y: Layout.modal.tempoButtonsY; width: (Layout.modal.tempoLeftW-Theme.m28*2-Theme.s4)/3; height: Theme.topHeight; text: entry.n; color: entry.bg; tint: entry.fg }
    }
    Item { x: Layout.modal.tempoRightX; y: Theme.m28; width: parent.width-x-Theme.s5; height: parent.height-2*y
        Label { text: 'LINK'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
        Label { anchors.right: parent.right; y: Theme.s3; text: '● ON · 3 PEERS'; color: Theme.tealPlus; font.pixelSize: Theme.m11 }
        Repeater { model: console.controller.peers
            Panel { required property int index; required property var entry; y: Theme.topHeight+index*Theme.topHeight; width: parent.width; height: Theme.sectionSize; color: Theme.surface2
                Label { x: Theme.s3; height: parent.height; text: entry.n; font.pixelSize: Theme.m12 }
                Label { anchors.right: parent.right; anchors.rightMargin: Theme.s3; height: parent.height; text: entry.v; color: Theme.grayN; font.pixelSize: Theme.m12 }
            }
        }
        Repeater { model: console.controller.tempoOpts
            Item { required property int index; required property var entry; y: parent.height-Theme.dL*(3-index); width: parent.width; height: Theme.dL
                Rectangle { width: parent.width; height: Theme.hairline; color: Theme.line }
                Label { y: Theme.s2; text: entry.a; color: Theme.grayN; font.pixelSize: Theme.keyLabel }
                Label { anchors.right: parent.right; y: Theme.s2; text: entry.b; font.pixelSize: Theme.keyLabel }
            }
        }
    }
}
