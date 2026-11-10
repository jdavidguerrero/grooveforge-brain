import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.log.summaryH
        Label { x: Theme.s4; y: Theme.s3; text: 'TONIGHT'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.dL; text: 'Saturday · video set'; font.pixelSize: Theme.trackSize }
        Label { x: Theme.roomTitleX; y: Theme.s3; text: 'RUNNING'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.roomTitleX; y: Theme.dL; text: '0:42:18'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
        Label { x: Theme.nextTitleX; y: Theme.s3; text: 'ACCEPTED'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.nextTitleX; y: Theme.dL; text: '23'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
        ActionButton { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s4; width: Theme.optionWidth; height: Theme.topHeight; text: 'EXPORT TO LIVE · .ALS'; active: true }
    }
    Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; x: Theme.s2; y: Layout.log.timelineY; width: Theme.summaryWidth; height: Layout.log.timelineH; color: Theme.rollAlt
        Rectangle { x: Theme.s3; y: parent.height/2; width: parent.width-2*x; height: Theme.edge; color: Theme.line }
        Repeater { model: console.standalone.logDots
            Rectangle { required property int index; required property var entry; x: Theme.s3+entry.x*(parent.width-Theme.s5-Layout.log.dotSize)/100; y: (parent.height-height)/2; width: Layout.log.dotSize; height: width; color: entry.c; radius: Theme.edge }
        }
    }
    Item { x: Theme.s2; y: Layout.log.headerY; width: Theme.summaryWidth; height: Theme.s5
        Label { x: Theme.s3; text: 'TIME'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.log.barX; text: 'BAR'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.log.roleX; text: 'LAYER'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.log.eventX; text: 'EVENT'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.log.sourceX; text: 'SOURCE'; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { x: Layout.log.trackX; text: 'TRACK'; font.pixelSize: Theme.tiny; color: Theme.mon }
    }
    Repeater { model: console.standalone.logRows
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; x: Theme.s2; y: Layout.log.rowsY+index*Layout.log.pitch; width: Theme.summaryWidth; height: Layout.log.rowH
            Label { x: Theme.s3; height: parent.height; text: entry.t; color: Theme.grayN; font.pixelSize: Theme.keyLabel }
            Label { x: Layout.log.barX; height: parent.height; text: entry.b; color: Theme.grayN; font.pixelSize: Theme.keyLabel }
            Label { x: Layout.log.roleX; height: parent.height; text: entry.k; color: entry.c; font.pixelSize: Theme.m11; weight: Font.Bold }
            Label { x: Layout.log.eventX; height: parent.height; width: Layout.log.sourceX-x-Theme.s3; text: entry.e; font.pixelSize: Theme.keyLabel }
            Label { x: Layout.log.sourceX; height: parent.height; text: entry.s; color: entry.sc; font.pixelSize: Theme.tiny }
            Label { x: Layout.log.trackX; height: parent.height; width: parent.width-x-Theme.s3; text: entry.tr; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
        }
    }
}
