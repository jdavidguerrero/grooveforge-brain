import QtQuick
import "../theme"
Panel {
    id: root
    color:Theme.bg
    property bool uppercase: false
    property var rows: ['1234567890-_/.~','QWERTYUIOP','ASDFGHJKL','ZXCVBNM']
    Repeater {model:root.rows
        Row {required property string modelData;required property int index;x:8+(index===2?40:index===3?100:0);y:index*50;spacing:6
            Repeater {model:parent.modelData.split('')
                Panel {required property string modelData;width:72;height:44;color:Theme.surface2
                    Label {anchors.centerIn:parent;text:root.uppercase?parent.modelData:parent.modelData.toLowerCase();font.pixelSize:17}
                    TouchTarget {hardwareAction:'text';callback:function(){drum.typeKey(root.uppercase?parent.modelData:parent.modelData.toLowerCase())}}
                }
            }
        }
    }
    Row {x:8;y:200;spacing:8
        Repeater {model:['SHIFT','SPACE','⌫','DONE']
            Panel {required property string modelData;width:modelData==='SPACE'?600:modelData==='SHIFT'?152:236;height:44;color:modelData==='DONE'||modelData==='SHIFT'&&root.uppercase?Theme.tealDim:Theme.surface2
                Label {anchors.centerIn:parent;text:parent.modelData;font.pixelSize:15}
                TouchTarget {hardwareAction:'text';callback:function(){if(parent.modelData==='SHIFT')root.uppercase=!root.uppercase;else drum.typeKey(parent.modelData)}}
            }
        }
    }
}
