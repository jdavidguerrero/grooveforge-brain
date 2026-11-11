import QtQuick
import "../theme"
Rectangle {
    id:root
    property var entry: ({index:0,velocity:0,probability:1,micro:0,amount:0,selected:false,active:true,playhead:false,text:''})
    property color tint: drum.tint
    property int padIndex: drum.selectedPad
    property bool toggle: false
    property bool playhead: drum.clock.running && entry.index === Number(drum.clock.pattern_step) % (entry.laneLength || 16)
    color:entry.index%4===0?Theme.surface2:Theme.surface
    radius:3
    border.width:playhead || entry.selected?2:1
    border.color:playhead?Theme.tealPlus:entry.selected?Theme.white:Theme.line2
    opacity:entry.active?1:.3
    Rectangle {x:3;y:drum.layer==='MICRO'?parent.height*.35:parent.height-(parent.height-6)*root.entry.amount-3;width:drum.layer==='MICRO'?parent.width*.4:parent.width-6;height:drum.layer==='MICRO'?parent.height*.3:(parent.height-6)*root.entry.amount;radius:2;color:root.tint;opacity:root.entry.velocity?1:.12
        anchors.horizontalCenter:drum.layer==='MICRO'?undefined:parent.horizontalCenter
        transform:Translate {x:drum.layer==='MICRO'?root.entry.micro*root.width*.6:0}
    }
    Label { anchors.centerIn:parent;text:root.entry.active && root.entry.velocity?root.entry.text:'';font.pixelSize:10;color:Theme.white }
    DashedOutline {anchors.fill:parent;visible:!root.entry.active;stroke:Theme.line2}
    TouchTarget {hardwareAction:'choice';enabled:root.entry.active;callback:function(){if(root.toggle)drum.toggleStep(root.padIndex,root.entry.index);else drum.selectStep(root.padIndex,root.entry.index)}}
}
