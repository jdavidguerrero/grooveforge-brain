import QtQuick
import "../theme"
import "../components"
Item {
    property bool melodic: console.screenName === 'NoteMelodic'
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.pads.summaryH
        Label { x: Layout.pads.noteX; y: Theme.s3; height: parent.height-Theme.s5; text: "E4"; face: Theme.displayFont; font.pixelSize: Layout.pads.noteSize; weight: Font.ExtraBold; color: melodic ? Theme.liveLead : Theme.roleColor('LEAD') }
        Label { x: Layout.pads.scaleX; y: Theme.s4; text: "LAST NOTE · VEL 104"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Layout.pads.scaleX; y: Theme.dL; text: "A MINOR"; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
        Label { x: Layout.pads.scaleX; y: Theme.infoHeight; text: "IN KEY · 4THS · OCT 3 · LEAD"; color: Theme.gray; font.pixelSize: Theme.m12 }
        Panel { x: Layout.pads.expressionX; width: Layout.pads.expressionW; height: Layout.pads.fsrH
            Label { x: Theme.s2; y: Theme.s3; text: "EXPRESSION"; font.pixelSize: Theme.tiny }
            Repeater { model: console.standalone.expr
                Item { required property int index; required property var entry; x: Theme.s2; y: Theme.dL+index*Theme.topHeight; width: parent.width-2*x; height: Theme.topHeight
                    Label { width: parent.width; height: Theme.m28; wrapMode: Text.WordWrap; text: entry.n; color: Theme.grayN; font.pixelSize: Theme.tagSize }
                    Meter { y: Theme.m28+Theme.s1; width: parent.width; height: Theme.s2; value: entry.v/100; tint: Theme.tealPlus }
                }
            }
        }
        Repeater { model: console.standalone.fsrPads
            Panel { required property int index; required property var entry; x: Layout.pads.fsrX+index*(Layout.pads.fsrW+Theme.s2); y: 0; width: Layout.pads.fsrW; height: Layout.pads.fsrH
                Label { x: Theme.s2; y: Theme.s2; text: entry.n; color: entry.c; font.pixelSize: Theme.m11 }
                PressureBar { x: Theme.s2; y: Theme.dL; width: parent.width-2*x; height: Theme.dXl; value: entry.v/100; tint: entry.c }
                Label { x: Theme.s2; y: parent.height-Theme.s5; text: entry.s; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
            }
        }
    }
    PadGrid { y: Layout.pads.gridY; width: parent.width; height: Layout.pads.gridH; rows: melodic ? console.standalone.melodicPads : console.standalone.pads; rowHeight: Layout.pads.tileH; rowPitch: Layout.pads.tileH+Theme.s2 }
}
