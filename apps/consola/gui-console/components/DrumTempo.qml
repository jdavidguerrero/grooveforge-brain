import QtQuick
import "../theme"
Panel {
    objectName:"drumTempoWindow"
    color:Theme.surface2;border.color:Theme.tealPlus;border.width:2;radius:10
    Label {x:20;y:16;text:'TEMPO · '+(drum.clock.running?'STOP TO EDIT':'ENC 1');font.pixelSize:13;color:Theme.tealPlus}
    Label {x:20;y:54;width:400;horizontalAlignment:Text.AlignHCenter;text:Number(drum.clock.bpm).toFixed(1)+' BPM';face:Theme.displayFont;font.pixelSize:48}
    Row {x:20;y:120;spacing:12
        Repeater {model:[-1,1]
            Panel {required property int modelData;width:194;height:44;border.color:Theme.tealPlus
                Label {anchors.centerIn:parent;text:parent.modelData>0?'+ 1':'− 1';font.pixelSize:20;color:Theme.tealPlus}
                TouchTarget {hardwareAction:'parameter';enabled:!drum.clock.running;callback:function(){drum.setBpm(drum.clock.bpm+parent.modelData)}}
            }
        }
    }
    Panel {x:20;y:178;width:400;height:40;border.color:Theme.line2
        Label {anchors.centerIn:parent;text:'DONE';font.pixelSize:12}
        TouchTarget {hardwareAction:'choice';callback:function(){drum.action('tempo-close')}}
    }
}
