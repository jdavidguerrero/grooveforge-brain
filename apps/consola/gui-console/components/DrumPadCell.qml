import QtQuick
import "../theme"
Panel {
    id: root
    property var entry: ({index:0,name:'EMPTY',tint:'#FF9A2E',empty:true,waveform:[],source:'—',selected:false,mute:false,solo:false,state:'',choke:0})
    property bool compact: false
    property bool editOnTap: false
    color: entry.empty ? Theme.bg : Theme.surface
    border.width: entry.selected ? 2 : 1
    border.color: entry.state ? Theme.amber : entry.selected ? Theme.white : entry.empty ? Theme.line2 : entry.tint
    radius: 6
    opacity: entry.mute || entry.dimmed ? .35 : 1
    Rectangle { x:0;y:0;width:5;height:parent.height;radius:2;color:root.entry.tint;visible:!root.entry.empty }
    Label { x:12;y:8; text:('0'+(root.entry.index+1)).slice(-2);font.pixelSize:11;color:root.entry.tint }
    Label { x:36;y:7;width:parent.width-42;text:root.entry.name;font.pixelSize:12 }
    Label { x:12;y:30;text:root.entry.empty?'PRESS TO LOAD':root.entry.source; font.pixelSize:9;color:Theme.grayN }
    DrumWaveform { x:12;y:root.compact?44:51;width:parent.width-24;height:root.compact?24:58;bars:root.entry.waveform;tint:root.entry.tint;opacity:root.entry.state?.25:1 }
    Label { x:10;y:root.compact?49:72;width:parent.width-20;horizontalAlignment:Text.AlignHCenter;text:root.entry.state;font.pixelSize:12;color:Theme.amber;visible:root.entry.state!=='' }
    Label { x:12;y:parent.height-22;text:root.entry.mute?'MUTED':root.entry.solo?'SOLO':root.entry.choke?'CHOKE '+root.entry.choke:root.entry.empty?'—':'ONE SHOT';font.pixelSize:9;color:Theme.grayN;visible:!root.compact }
    DashedOutline { anchors.fill:parent;visible:root.entry.empty || root.entry.state==='FILE MISSING';stroke:root.entry.state?Theme.amber:Theme.line2 }
    TouchTarget { hardwareAction:'choice';callback:function(){drum.selectPad(root.entry.index);if(root.entry.empty)console.navigate('DrumBrowser');else if(root.editOnTap)drum.openPad(root.entry.index)};onDoubleClicked:if(!root.entry.empty)drum.openPad(root.entry.index) }
}
