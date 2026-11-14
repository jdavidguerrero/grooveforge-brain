import QtQuick
import "../theme"
Item {
    id: root
    property int encoderIndex: console.selectedEncoder
    TouchTarget { hardwareAction:"parameter"; callback:function(){console.selectEncoder(root.encoderIndex);touch.inspect("ENC "+(root.encoderIndex+1)+" · turn to edit")} }
    property real value: .5
    property color tint: Theme.teal
    property string text: ""
    property int textSize: Theme.m20
    Canvas { id: arc; anchors.fill: parent
        onPaint: { let c=getContext("2d");c.reset();let stroke=width/12; c.lineWidth=stroke;let start=Math.PI*.75; c.strokeStyle=Theme.line2; c.beginPath();c.arc(width/2,height/2,width/2-stroke/2,start,start+Math.PI*1.5);c.stroke();c.strokeStyle=root.tint;c.beginPath();c.arc(width/2,height/2,width/2-stroke/2,start,start+Math.PI*1.5*root.value);c.stroke() }
        Connections { target: root; function onValueChanged(){arc.requestPaint()} function onTintChanged(){arc.requestPaint()} }
    }
    Label { anchors.centerIn: parent; text: root.text; font.pixelSize: root.textSize }
}
