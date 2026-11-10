import QtQuick
import "../theme"
Canvas {
    id: root
    property int kind: 0
    property var values: ({})
    onPaint: {
        let c=getContext('2d');c.reset();c.strokeStyle=Theme.line2;c.lineWidth=1;c.beginPath();c.moveTo(0,height-1);c.lineTo(width,height-1);c.stroke();
        c.strokeStyle=Theme.tealPlus;c.lineWidth=2;c.beginPath();
        let p=root.values;
        if(kind===0){
            let a=p.env_mode?0:p.attack,h=p.env_mode?p.hold:0,d=p.decay,total=Math.max(1,a+h+d);
            c.moveTo(0,height);c.lineTo(width*a/total,2);c.lineTo(width*(a+h)/total,2);c.lineTo(width,height-1);
        } else {
            for(let i=0;i<=64;i++){
                let x=i/64,y=0;
                if(kind===1){
                    let hz=20*Math.pow(1000,x),ratio=hz/Math.max(20,p.cutoff),lp=1/(1+ratio*ratio);
                    y=p.filter_mode?1-lp:lp;
                }else{
                    y=p.velocity_curve===1?Math.sqrt(x):p.velocity_curve===2?x*x:p.velocity_curve===3?1:x;
                    y=1-p.velocity_level+p.velocity_level*y;
                }
                if(i===0)c.moveTo(0,height*(1-y));else c.lineTo(width*x,height*(1-y));
            }
        }
        c.stroke();
    }
    onValuesChanged:requestPaint()
    onKindChanged:requestPaint()
    onWidthChanged:requestPaint()
    onHeightChanged:requestPaint()
}
