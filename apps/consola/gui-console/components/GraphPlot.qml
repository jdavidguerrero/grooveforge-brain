import QtQuick
import "../theme"
Canvas {
    id: root
    property string kind: "energy"
    property color tint: Theme.white
    property var points: [10,14,22,30,28,40,55,70,78,62,50,66,82,88,74,60,70,84,66,40,20]
    onPaint: {let c=getContext("2d");c.reset();c.lineWidth=Theme.edge;c.strokeStyle=tint;c.fillStyle=Qt.rgba(tint.r,tint.g,tint.b,.08);c.beginPath();
        if(kind==="filter"){c.moveTo(0,height*.4);c.lineTo(width*.38,height*.4);c.quadraticCurveTo(width*.45,height*.22,width*.47,height*.4);c.quadraticCurveTo(width*.52,height,width*.6,height)}
        else {for(let i=0;i<points.length;i++){let x=i/(points.length-1)*width,y=(100-points[i])/100*height;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}}
        c.stroke();c.lineTo(width,height);c.lineTo(0,height);c.closePath();c.fill() }
}
