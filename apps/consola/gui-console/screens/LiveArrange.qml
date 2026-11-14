import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.arrange.summaryH
        Label { x: Theme.s3; y: Theme.s3; text: "SET ENERGY"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s3; y: Theme.dL; text: "0:42"; font.pixelSize: Theme.m18 }
        Label { x: Theme.s3; y: Theme.dXl; text: "of 1:30 plan"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        GraphPlot { x: Layout.arrange.riverX; y: Theme.s3; width: Layout.arrange.ringX-x-Theme.s4; height: Layout.arrange.ringSize }
        Rectangle { x: Layout.arrange.riverX+(Layout.arrange.ringX-Layout.arrange.riverX-Theme.s4)*.46; y: Theme.s2; width: Theme.edge; height: parent.height-2*y; color: Theme.tealPlus }
        Repeater { model: console.standalone.eSecs
            Label { required property int index; required property var entry; x: Layout.arrange.riverX+entry.x*(Layout.arrange.ringX-Layout.arrange.riverX-Theme.s4)/100; y: parent.height-Theme.s5; text: entry.n; color: entry.c; font.pixelSize: Theme.tagSize }
        }
        ArcKnob { x: Layout.arrange.ringX; y: Theme.s3; width: Layout.arrange.ringSize; height: width; value: .625; text: "5"; tint: Theme.tealPlus; textSize: Theme.dL }
    }
    Item { x: Theme.s2; y: Layout.arrange.lanesY; width: Theme.summaryWidth; height: parent.height-y-Theme.s2
        Repeater { model: console.standalone.lanes
            ArrangementLane { y: index*Layout.arrange.pitch; width: parent.width; height: Layout.arrange.laneH; entry: model.entry }
        }
        Repeater { model: console.standalone.phraseLines
            Rectangle { required property int index; required property var entry; x: entry; height: parent.height; width: Theme.hairline; color: Theme.off }
        }
        Rectangle { x: Layout.arrange.nowX; y: -Theme.rCell; width: Theme.edge; height: parent.height-y; color: Theme.white }
        Label { x: Layout.arrange.nowX-Theme.m20; y: -Theme.m14; text: "NOW"; font.pixelSize: Theme.tagSize }
    }
}
