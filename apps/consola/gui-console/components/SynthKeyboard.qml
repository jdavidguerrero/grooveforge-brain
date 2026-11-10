import QtQuick
import "../theme"
Item {
    id:root
    property bool numeric:false
    property var rows:numeric?['1234567890','-_.!?@#+=','QWERTYUIOP']:['QWERTYUIOP','ASDFGHJKL','ZXCVBNM-.']
    Repeater {model:root.rows
        Item {required property int index;required property string modelData;x:index*20;y:index*56;width:parent.width-2*x;height:52
            Repeater {model:parent.modelData.split('')
                ActionButton {required property int index;required property string modelData;x:index*(parent.width/parent.modelData.length);width:parent.width/parent.modelData.length-6;height:52;text:modelData;onClicked:synth.typeKey(modelData)}
            }
        }
    }
    Row {y:168;spacing:8
        ActionButton {width:104;height:48;text:root.numeric?'ABC':'123';onClicked:root.numeric=!root.numeric}
        ActionButton {width:104;height:48;text:'⌫';onClicked:synth.typeKey('⌫')}
        ActionButton {width:432;height:48;text:'SPACE';onClicked:synth.typeKey('SPACE')}
        ActionButton {width:104;height:48;text:'CANCEL';onClicked:synth.typeKey('CANCEL')}
        ActionButton {width:112;height:48;text:'SAVE';active:true;onClicked:synth.typeKey('SAVE')}
    }
}
