import QtQuick
import "../theme"
Item {
    id: root
    property string mode: 'FILTER'
    property var values: synth.values
    property color tint: Theme.tealPlus
    property string prefix: /^[ab]\./.test(synth.pageName)?synth.pageName.slice(0,2):''
    function v(key) {return values[prefix+key];}
    Canvas {id:plot;anchors.fill:parent
        onPaint: {
            let c=getContext('2d');c.reset();let w=width,h=height,p=30;
            c.strokeStyle=Theme.line2;c.lineWidth=1;
            for(let i=0;i<=4;i++){c.beginPath();c.moveTo(p,p+(h-2*p)*i/4);c.lineTo(w-p,p+(h-2*p)*i/4);c.stroke();}
            function draw(points,col){c.beginPath();points.forEach((pt,i)=>i?c.lineTo(pt[0],pt[1]):c.moveTo(pt[0],pt[1]));c.strokeStyle=col;c.lineWidth=3;c.stroke();}
            if(root.mode==='FILTER' && root.v('cutoff')!==undefined){
                let fc=Math.log(root.v('cutoff')/20)/Math.log(1000),res=root.v('resonance'),pts=[];
                for(let i=0;i<=240;i++){let x=i/240,gain=1/(1+Math.exp((x-fc)*18))+res*.3*Math.exp(-Math.pow((x-fc)*25,2));pts.push([p+x*(w-2*p),h*.7-gain*h*.42]);}
                draw(pts,root.tint);
            }else if(root.mode==='ENV'){
                for(let pre of ['filter_','']){
                    let a=root.v(pre+'attack'),d=root.v(pre+'decay'),r=root.v(pre+'release'),s=root.v(pre+'sustain');
                    if(a===undefined||d===undefined||r===undefined||s===undefined)continue;
                    let t=600+a+d+r,x1=p+(w-2*p)*a/t,x2=x1+(w-2*p)*d/t,x3=x2+(w-2*p)*600/t;
                    draw([[p,h-p],[x1,p],[x2,h-p-s*(h-2*p)],[x3,h-p-s*(h-2*p)],[w-p,h-p]],pre===''?root.tint:Theme.white);
                }
            }else if(root.mode==='LFO' && root.v('lfo_shape')!==undefined){
                let shape=root.v('lfo_shape'),pts=[];
                for(let i=0;i<=400;i++){let phase=(i/200)%1,y=shape===0?Math.sin(phase*Math.PI*2):shape===1?1-4*Math.abs(phase-.5):shape===2?1-2*phase:shape===3?(phase<.5?1:-1):Math.sin(Math.floor(i/20)*19.7);pts.push([p+i/400*(w-2*p),h/2-y*(h/2-p)]);}
                draw(pts,root.tint);
            }
        }
        Connections {target:root;function onValuesChanged(){plot.requestPaint()} function onModeChanged(){plot.requestPaint()} function onPrefixChanged(){plot.requestPaint()} function onTintChanged(){plot.requestPaint()}}
    }
}
