import QtQuick
import "../theme"
Item {
    id: root
    property var scalingData: ({})
    Canvas {id:plot;anchors.fill:parent
        onPaint: {
            let c=getContext('2d');c.reset();if(root.scalingData.breakpoint===undefined || !root.scalingData.points || root.scalingData.points.length!==128)return;
            let p=16,w=width-2*p,h=height-48,bp=root.scalingData.breakpoint+17;
            c.strokeStyle=Theme.line2;c.lineWidth=1;c.beginPath();c.moveTo(p,p+h/2);c.lineTo(p+w,p+h/2);c.stroke();
            c.beginPath();
            for(let note=0;note<128;note++){
                let y=p+h/2-Math.max(-127,Math.min(127,root.scalingData.points[note]))*h/254;
                if(note===0)c.moveTo(p,y);else c.lineTo(p+w*note/127,y);
            }
            c.strokeStyle=Theme.tealPlus;c.lineWidth=2;c.stroke();
            c.strokeStyle=Theme.white;c.beginPath();c.moveTo(p+w*bp/127,p);c.lineTo(p+w*bp/127,p+h);c.stroke();
            c.fillStyle=Theme.grayN;c.font='11px monospace';c.fillText('KEY SCALING · BP '+bp,p,height-8);
        }
        Connections {target:root;function onScalingDataChanged(){plot.requestPaint()}}
    }
}
