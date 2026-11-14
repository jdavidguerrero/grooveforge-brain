import QtQuick
import "../theme"
Item {
    Repeater { model:8
        Panel { required property int index;x:Theme.columnX(index);width:152;height:44
            border.width:drum.clock.pattern===index?2:1;border.color:drum.clock.pattern===index?Theme.tealPlus:drum.clock.queued===index?Theme.white:Theme.line2
            color:drum.clock.pattern===index?Theme.tealDim:Theme.surface
            Label { x:12;anchors.verticalCenter:parent.verticalCenter;text:String.fromCharCode(65+parent.index);face:Theme.displayFont;font.pixelSize:30 }
            Label { x:46;anchors.verticalCenter:parent.verticalCenter;text:drum.clock.queued===parent.index?'NEXT BAR':drum.clock.pattern===parent.index?'ACTIVE':'PATTERN';font.pixelSize:10;color:drum.clock.queued===parent.index?Theme.white:Theme.grayN }
            TouchTarget {hardwareAction:'choice';callback:function(){drum.selectPattern(parent.index)}}
        }
    }
}
