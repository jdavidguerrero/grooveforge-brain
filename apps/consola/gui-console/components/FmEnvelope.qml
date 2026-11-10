import QtQuick
import "../theme"
Item {
    id: root
    property var envelopeData: []
    property bool six: true
    Canvas {id:plot;anchors.fill:parent
        onPaint: {
            let c=getContext('2d');c.reset();let p=18,w=width-2*p,h=height-45;
            if(root.envelopeData.length!==4)return;
            c.strokeStyle=Theme.line2;c.lineWidth=1;
            for(let i=0;i<=4;i++){c.beginPath();c.moveTo(p,p+h*i/4);c.lineTo(p+w,p+h*i/4);c.stroke();}
            let durations=root.six?root.envelopeData.map(e=>.12+Math.pow(1-e.rate/99,2)):[root.envelopeData[0].value,root.envelopeData[1].value,600,root.envelopeData[3].value];
            let total=durations.reduce((a,b)=>a+b,0),x=p;
            let levels=root.six?root.envelopeData.map(e=>e.level/99):[1,root.envelopeData[2].value/100,root.envelopeData[2].value/100,0];
            c.beginPath();c.moveTo(p,p+h*(1-(root.six?levels[3]:0)));
            for(let i=0;i<4;i++){x+=w*durations[i]/total;c.lineTo(x,p+h*(1-levels[i]));}
            c.strokeStyle=Theme.tealPlus;c.lineWidth=3;c.stroke();
            c.fillStyle=Theme.grayN;c.font='11px monospace';
            c.fillText(root.six?'R1 / L1      R2 / L2      R3 / L3      R4 / L4':'ATTACK / DECAY / SUSTAIN / RELEASE',p,height-8);
        }
        Connections {target:root;function onEnvelopeDataChanged(){plot.requestPaint()} function onSixChanged(){plot.requestPaint()}}
    }
}
