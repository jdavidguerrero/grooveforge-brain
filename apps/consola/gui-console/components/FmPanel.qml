import QtQuick
import "../theme"
Item {
    id: root
    property var operators: synth.fmOperators
    property var focused: operators[Math.max(0,synth.selectedOperator-1)] || ({eg:[],scaling:{}})
    Panel {width:1264;height:160
        FmAlgorithm {x:8;width:488;height:160}
        Item {x:512;y:12;visible:synth.selectedOperator===0
            Repeater {model:synth.operatorCount===6?32:8
                ActionButton {hardwareAction:"choice";required property int index;x:(index%8)*65;y:Math.floor(index/8)*34;width:57;height:28;text:String(index+1);active:synth.algorithm.number===index+1;callback:function(){synth.chooseAlgorithm(index+1)}}
            }
        }
        Label {x:1060;y:20;width:188;visible:synth.selectedOperator===0;text:'ALGORITHM '+synth.algorithm.number+'\nFEEDBACK '+synth.fmFeedback+'\n'+synth.operatorCount+' OPERATORS';lineHeight:1.8;font.pixelSize:13;color:Theme.tealPlus}
        FmEnvelope {x:502;y:4;width:synth.operatorCount===6?438:746;height:152;visible:synth.selectedOperator>0;envelopeData:root.focused.eg;six:synth.operatorCount===6}
        KeyScaling {x:948;y:4;width:304;height:152;visible:synth.selectedOperator>0 && synth.operatorCount===6;scalingData:root.focused.scaling}
    }
    Repeater {model:synth.selectedOperator?[]:root.operators
        Panel {required property var modelData;required property int index
            x:index*(1264/synth.operatorCount);y:168;width:1264/synth.operatorCount-8;height:128
            Label {x:12;y:8;text:'OP '+modelData.op+' · '+((synth.algorithm.carriers || []).indexOf(modelData.op)>=0?'CARRIER':'MOD');font.pixelSize:12;color:Theme.tealPlus}
            Label {x:12;y:32;width:parent.width-24;text:modelData.tuning;face:Theme.displayFont;font.pixelSize:26}
            Label {x:12;y:72;text:'LEVEL '+modelData.level+' · '+(modelData.on?'ON':'OFF');font.pixelSize:12;color:Theme.grayN}
            Rectangle {x:12;y:100;width:parent.width-24;height:5;color:Theme.line2
                Rectangle {width:parent.width*modelData.level/99;height:5;color:Theme.tealPlus}
            }
            TouchTarget { hardwareAction: "navigation";anchors.fill:parent;callback: function() { synth.focusOperator(modelData.op) }}
        }
    }
    Panel {y:168;width:1264;height:128;visible:synth.selectedOperator>0
        Label {x:16;y:8;text:'OP '+synth.selectedOperator+' · '+root.focused.tuning+' · LEVEL '+root.focused.level+' · DETUNE '+root.focused.detune;face:Theme.displayFont;font.pixelSize:30;color:Theme.tealPlus}
        Label {x:16;y:52;width:1220;text:synth.operatorCount===6?'R1 → L1   ·   R2 → L2   ·   R3 → L3 (HOLD)   ·   KEY UP: R4 → L4     /     KEYBOARD SCALING: BREAKPOINT + 17':'ATTACK · DECAY · SUSTAIN · RELEASE';font.pixelSize:12;color:Theme.grayN}
        Row {x:16;y:80;spacing:12
            Repeater {model:synth.operatorCount===6?['TUNING / SENS','R1–R4 / L1–L4','KEY SCALING']:['RATIO / ADSR / WAVE']
                ActionButton {hardwareAction:"choice";required property string modelData;required property int index;width:240;height:36;text:modelData;active:synth.bankIndex===index;callback:function(){synth.chooseBank(index)}}
            }
            ActionButton {hardwareAction:"choice";width:160;height:36;text:'ALL OPERATORS';callback:function(){synth.selectPage('OPS')}}
        }
    }
}
