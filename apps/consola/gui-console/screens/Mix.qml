import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.mix.summaryH
        Label { x: Theme.s4; height: parent.height; text: 'VOLUME  ·  PAN  ·  SEND A  ·  SEND B'; font.pixelSize: Theme.m12 }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; height: parent.height; text: 'MASTER  −1.2 dB  ·  CUE −6.0'; color: Theme.grayN; font.pixelSize: Theme.m12 }
    }
    Repeater { model: console.controller.mixStrips
        Panel { required property int index; required property var entry; objectName: 'column'+index; x: Theme.columnX(index); y: Layout.mix.stripsY; width: Theme.columnWidth; height: Layout.mix.stripsH; border.color: entry.obd
            Rectangle { width: parent.width; height: Theme.s1; color: entry.c }
            Label { x: Theme.s3; y: Theme.s3; text: entry.n; font.pixelSize: Theme.keyLabel }
            Repeater { model: entry.knobs
                Item { required property int index; required property var modelData; x: Theme.s3; y: Layout.mix.panY+index*(Layout.mix.sendAY-Layout.mix.panY); width: parent.width-2*x; height: Layout.mix.arcSize
                    ArcKnob { width: Layout.mix.arcSize; height: width; tint: index === 0 ? Theme.grayN : Theme.teal; value: index === 0 ? .5 : index === 1 ? entry.a/100 : entry.b/100 }
                    Label { x: Layout.mix.arcSize+Theme.s2; y: 0; text: modelData.n; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tagSize }
                    Label { x: Layout.mix.arcSize+Theme.s2; y: Theme.m14; text: index === 0 ? entry.pan : index === 1 ? String(entry.a) : String(entry.b); font.pixelSize: Theme.m12 }
                }
            }
            Rectangle { x: Theme.s6; y: Layout.mix.faderY; width: Theme.rCell; height: Layout.mix.faderH; color: Theme.rollBg; radius: Theme.edge
                Rectangle { x: -Theme.s3; y: (1-entry.vol/100)*(parent.height-Theme.s3); width: Theme.s6-Theme.s4; height: Theme.s3; color: Theme.white; radius: Theme.edge }
            }
            Repeater { model: entry.m
                Rectangle { required property int index; required property var modelData; x: Theme.layerHeader+index*(Theme.s2+Theme.edge); y: Layout.mix.faderY; width: Theme.s2; height: Layout.mix.faderH; color: Theme.rollBg
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*modelData/100; color: Theme.tealPlus }
                }
            }
            Label { y: Layout.mix.buttonsY-Theme.s5; width: parent.width; text: entry.db+' dB'; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.m12 }
            Repeater { model: entry.btn
                ActionButton { required property int index; required property var modelData; x: Theme.s4+index*Layout.mix.buttonPitch; y: Layout.mix.buttonsY; width: Layout.mix.buttonW; height: Layout.mix.buttonW; text: modelData.n; color: modelData.bg; border.color: modelData.bd; tint: modelData.fg }
            }
        }
    }
}
