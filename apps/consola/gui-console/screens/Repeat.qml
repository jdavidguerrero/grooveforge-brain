import QtQuick
import "../theme"
import "../components"
ReviewWindow {
    repeatWindow: true
    Label { x: Theme.s5; y: Theme.s3; text: 'REPEAT'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
    Label { x: Theme.settingsBodyX; y: Theme.s5; text: '1/16 · swing 56 % · held'; font.pixelSize: Theme.m14 }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.s5; text: 'HOLD REPEAT + PAD · LATCH WITH SHIFT'; font.pixelSize: Theme.m12; color: Theme.mon }
    Repeater { model: console.controller.rates
        ActionButton { required property int index; required property var entry; objectName: 'column'+index; x: index*Theme.columnPitch; y: Layout.modal.repeatTileY; width: Theme.columnWidth; height: Layout.modal.repeatTileH; text: entry.n; display: true; color: entry.bg; border.color: entry.bd; tint: entry.fg; action: 'review:'+index }
    }
    Repeater { model: console.controller.repOpts
        Panel {touchKind:"parameter";touchCallback:function(){console.selectEncoder(index+3);touch.inspect(entry.n+" · encoder selected")};required property int index; required property var entry; x: Theme.s5+index*(parent.width-Theme.s5)/3; y: Layout.modal.repeatOptsY; width: (parent.width-4*Theme.s5)/3; height: Theme.s6
            Label { text: entry.n; color: Theme.grayN; font.pixelSize: Theme.m12 }
            Label { anchors.right: parent.right; text: entry.v; font.pixelSize: Theme.m12 }
            Meter { y: Theme.m28; width: parent.width; height: Theme.s2; value: entry.p/100; tint: Theme.tealPlus }
        }
    }
}
