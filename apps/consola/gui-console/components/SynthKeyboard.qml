import QtQuick
import "../theme"
Item {
    id:root
    property bool numeric:false
    property var rows:numeric?['1234567890','-_.!?@#+=','QWERTYUIOP']:['QWERTYUIOP','ASDFGHJKL','ZXCVBNM-.']
    Repeater {model:root.rows
        Item {required property int index;required property string modelData;x:index*20;y:index*56;width:parent.width-2*x;height:52
            Repeater {model:parent.modelData.split('')
                ActionButton {hardwareAction:"text";required property int index;required property string modelData;x:index*(parent.width/parent.modelData.length);width:parent.width/parent.modelData.length-6;height:52;text:modelData;callback:function(){synth.typeKey(modelData)}}
            }
        }
    }
    Row {y:168;spacing:8
        ActionButton {hardwareAction:"text";width:104;height:48;text:root.numeric?'ABC':'123';callback:function(){root.numeric=!root.numeric}}
        ActionButton {hardwareAction:"text";width:104;height:48;text:'⌫';callback:function(){synth.typeKey('⌫')}}
        ActionButton {hardwareAction:"text";width:432;height:48;text:'SPACE';callback:function(){synth.typeKey('SPACE')}}
        ActionButton {hardwareAction:"text";width:104;height:48;text:'CANCEL';callback:function(){synth.typeKey('CANCEL')}}
        ActionButton {hardwareAction:"text";width:112;height:48;text:'SAVE';active:true;callback:function(){synth.typeKey('SAVE')}}
    }
}
