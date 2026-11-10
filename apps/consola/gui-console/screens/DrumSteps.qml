import QtQuick
import "../theme"
import "../components"
Item {
    DrumPatternBar {x:0;y:0;width:1280;height:44}
    Panel {x:8;y:52;width:152;height:32;border.color:drum.clock.armed?Theme.red:Theme.line2
        Label {anchors.centerIn:parent;text:'REC '+(drum.clock.armed?'ARMED':'OFF');font.pixelSize:11;color:drum.clock.armed?Theme.red:Theme.grayN}
        TouchTarget {hardwareAction:'choice';callback:function(){drum.record()}}
    }
    Label {x:176;y:62;text:'Q 1/'+drum.quantize+' · SWING '+Math.round(drum.clock.swing*100)+'%';font.pixelSize:11;color:Theme.grayN}
    Row {x:500;y:52;spacing:8
        Repeater {model:['VEL','PROB','MICRO']
            Panel {required property string modelData;width:88;height:32;border.color:drum.layer===modelData?Theme.white:Theme.line2
                Label {anchors.centerIn:parent;text:parent.modelData;font.pixelSize:10}
                TouchTarget {hardwareAction:'choice';callback:function(){drum.setLayer(parent.modelData)}}
            }
        }
    }
    Label {x:814;y:62;text:'LANES '+(drum.laneBank*8+1)+'–'+(drum.laneBank*8+8)+' · PAGE '+(drum.stepPage+1)+'/2';font.pixelSize:11;color:Theme.grayN}
    Label {x:1110;y:58;width:158;horizontalAlignment:Text.AlignRight;text:(drum.clock.running?'▶ ':'■ ')+drum.clock.bar+'.'+(Math.floor(drum.clock.step/4)+1)+'.'+(drum.clock.step%4+1);font.pixelSize:17;color:Theme.tealPlus}
    Repeater {model:drum.lanes
        Item {id:lane;required property var entry;required property int index;x:8;y:92+index*58;width:1264;height:54
            opacity:entry.mute || entry.dimmed?.35:1
            Panel {width:152;height:54;border.color:lane.entry.selected?Theme.white:Theme.line2
                Rectangle {width:5;height:54;color:lane.entry.tint}
                Label {x:10;y:6;width:132;text:('0'+(lane.entry.index+1)).slice(-2)+' '+lane.entry.name;font.pixelSize:11;color:lane.entry.tint}
                Label {x:10;y:30;text:'L '+lane.entry.length+' · '+(drum.stepPage+1)+'/2';font.pixelSize:10;color:Theme.grayN}
                TouchTarget {hardwareAction:'choice';callback:function(){drum.selectPad(lane.entry.index)}}
            }
            Repeater {model:lane.entry.steps
                DrumStepCell {required property var modelData;required property int index;x:160+index*69;width:65;height:54;entry:modelData;tint:lane.entry.tint;padIndex:lane.entry.index}
            }
        }
    }
}
