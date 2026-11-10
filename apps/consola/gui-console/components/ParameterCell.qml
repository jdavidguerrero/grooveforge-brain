import QtQuick
import "../theme"
Panel {
    id: root
    property var entry: ({empty:true, name:"—", text:"—", unit:"",kind:"empty",amount:0})
    property color tint: Theme.tealPlus
    property int encoderIndex: 0
    TouchTarget { hardwareAction:"parameter"; enabled:!root.entry.empty && !root.loading; callback:function(){synth.focusParameter(root.encoderIndex)} }
    property bool focused: false
    property bool loading: false
    radius: 6
    border.width: focused && !entry.empty ? 2 : 1
    border.color: focused && !entry.empty ? tint : Theme.line
    color: Theme.surface
    Label { x: 10; y: 7; width: parent.width-20; text: root.entry.name; font.pixelSize: 12; color: root.focused ? Theme.white : Theme.grayN }
    Label { anchors.centerIn: parent; visible: root.entry.empty || root.loading; text: root.loading ? "…" : "—"; font.pixelSize: 28; color: Theme.mon }
    DashedOutline { anchors.fill: parent; visible: root.entry.empty || root.loading; stroke: Theme.line2 }
    Item { x: (parent.width-92)/2; y: 32; width: 92; height: 92; visible: !root.entry.empty && !root.loading && root.entry.kind !== 'bool'
        Canvas { id: ring; anchors.fill: parent
            onPaint: {
                let c=getContext('2d'); c.reset(); c.lineWidth=10;
                let a=Math.PI*.75, sweep=Math.PI*1.5, v=Math.max(0,Math.min(1,root.entry.amount));
                c.strokeStyle=Theme.line2; c.beginPath();c.arc(46,46,40,a,a+sweep);c.stroke();
                if(root.entry.kind === 'enum') {
                    let count=(root.entry.options || []).length;
                    for(let i=0;i<count;i++) { c.strokeStyle=i === Math.round(root.entry.value) ? root.tint : Theme.line2; c.beginPath();c.arc(46,46,40,a+i*sweep/count+.04,a+(i+1)*sweep/count-.04);c.stroke(); }
                } else { c.strokeStyle=root.tint;c.beginPath();let start=root.entry.kind === 'bipolar' ? a+sweep*.5 : a;c.arc(46,46,40,Math.min(start,a+sweep*v),Math.max(start,a+sweep*v));c.stroke(); }
                if(root.entry.kind === 'bipolar') {c.fillStyle=Theme.white;c.fillRect(45,0,2,14);}
            }
            Connections { target: root; function onEntryChanged(){ring.requestPaint()} function onTintChanged(){ring.requestPaint()} }
        }
        Label { x: 8; y: 28; width: 76; height: 30; text: root.entry.text; font.pixelSize: text.length > 7 ? 13 : text.length > 5 ? 16 : 20; horizontalAlignment: Text.AlignHCenter }
        Label { x: 0; y: 57; width: 92; text: root.entry.unit; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; color: Theme.grayN }
    }
    Rectangle { x: (parent.width-76)/2; y: 63; width: 76; height: 36; radius: 18; visible: root.entry.kind === 'bool' && !root.loading
        color: root.entry.value ? root.tint : Theme.line2
        Rectangle { x: root.entry.value ? 44 : 4; y: 4; width: 28; height: 28; radius: 14; color: root.entry.value ? Theme.white : Theme.grayN }
    }
    Label { x: 8; y: 136; width: parent.width-16; text: root.entry.kind === 'bool' ? root.entry.text : root.entry.kind === 'enum' ? 'STEPPED' : root.entry.kind === 'bipolar' ? '−  ·  +' : root.entry.empty ? 'FREE ENCODER' : root.entry.unit; font.pixelSize: 10; color: Theme.mon; horizontalAlignment: Text.AlignHCenter }
}
