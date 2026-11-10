import QtQuick
import "../theme"
import "../components"
Item {
    DrumPatternBar {x:0;y:0;width:1280;height:44}
    Repeater {model:drum.pads
        DrumPadCell {required property var model;required property int index;x:Theme.columnX(index%8);y:52+Math.floor(index/8)*92;width:152;height:84;entry:model.entry;compact:true}
    }
    Repeater {model:drum.steps
        DrumStepCell {required property var modelData;required property int index;x:Theme.columnX(index%8);y:236+Math.floor(index/8)*92;width:152;height:84;entry:modelData;toggle:true
            Label {x:10;y:6;text:String(parent.entry.index+1);font.pixelSize:10;color:Theme.grayN}
        }
    }
    Repeater {model:drum.fsr
        Panel {id:force;required property var modelData;required property int index;x:8+index*320;y:420;width:312;height:132;border.color:modelData.tint
            Label {x:12;y:10;text:'FSR '+(force.index+1)+' · '+force.modelData.name;font.pixelSize:11;color:force.modelData.tint}
            Label {x:12;y:31;text:String(force.modelData.velocity);face:Theme.displayFont;font.pixelSize:64}
            Label {x:116;y:50;text:'VELOCITY\nPAD '+(force.modelData.pad+1);font.pixelSize:11;color:Theme.grayN}
            Row {x:116;y:88;spacing:4
                Repeater {model:force.modelData.hits
                    Rectangle {required property int modelData;width:12;height:Math.max(2,24*modelData/127);color:force.modelData.tint;anchors.bottom:parent.bottom}
                }
            }
            Rectangle {x:12;y:108;width:288;height:16;color:Theme.line2
                Rectangle {width:parent.width*force.modelData.velocity/127;height:16;color:force.modelData.tint}
            }
            TouchTarget {hardwareAction:'choice';callback:function(){drum.selectPad(force.modelData.pad)}}
        }
    }
}
