import QtQuick
import "../theme"
Item {
    id: root
    property var table: synth.algorithm
    property int selected: synth.selectedOperator
    function depth(op, visited=[]) {
        if(visited.indexOf(op)>=0 || visited.length>6) return 0;
        let targets=(table.edges || []).filter(e=>e[0] === op);
        return targets.length ? 1+Math.max.apply(null,targets.map(e=>depth(e[1],visited.concat([op])))) : 0;
    }
    function nx(op) { return 12+(op-1)*80; }
    function ny(op) { return 122-depth(op)*29; }
    Canvas { id: lines; anchors.fill: parent
        onPaint: {
            let c=getContext('2d');c.reset();c.lineWidth=2;c.strokeStyle=Theme.mon;
            for(let e of (root.table.edges || [])) {c.beginPath();c.moveTo(root.nx(e[0])+32,root.ny(e[0])+24);c.lineTo(root.nx(e[1])+32,root.ny(e[1]));c.stroke();}
            c.strokeStyle=Theme.tealPlus;c.beginPath();c.moveTo(12,153);c.lineTo(Math.min(width-10,root.table.operators*80-4),153);c.stroke();
            for(let op of (root.table.carriers || [])){c.beginPath();c.moveTo(root.nx(op)+32,root.ny(op)+24);c.lineTo(root.nx(op)+32,153);c.stroke();}
            let dest=root.table.feedback,src=root.table.feedback_source;
            if(dest && src) {c.strokeStyle=Theme.amber;c.beginPath();c.moveTo(root.nx(src)+64,root.ny(src)+12);c.lineTo(root.nx(src)+72,root.ny(src)+12);c.lineTo(root.nx(src)+72,root.ny(dest)-6);c.lineTo(root.nx(dest)+32,root.ny(dest)-6);c.lineTo(root.nx(dest)+32,root.ny(dest));c.stroke();}
        }
        Connections {target:root; function onTableChanged(){lines.requestPaint()} }
    }
    Repeater { model: root.table.operators || 0
        Rectangle { required property int index; property int op:index+1
            x:root.nx(op);y:root.ny(op);width:64;height:24;radius:3
            color:(root.table.carriers || []).indexOf(op)>=0 ? Theme.teal : Theme.surface2
            border.color:root.selected === op ? Theme.white : Theme.tealPlus;border.width:root.selected === op ? 2 : 1
            Label {anchors.centerIn:parent;text:'OP '+parent.op;font.pixelSize:12}
            TouchTarget { hardwareAction: "choice";anchors.fill:parent;callback: function() { synth.focusOperator(parent.op) }}
        }
    }
}
