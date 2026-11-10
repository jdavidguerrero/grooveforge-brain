import QtQuick
import "../theme"
Item {
    id: root
    property string mode: 'FILTER'
    property int oscillator: 1
    property int focusedOperator: synth.selectedOperator
    property var values: synth.values
    property color tint: Theme.tealPlus
    property string prefix: synth.pageName.indexOf('A ') === 0 ? 'a.' : synth.pageName.indexOf('B ') === 0 ? 'b.' : ''
    function v(key, fallback) {let n=values[prefix+key];return n === undefined ? fallback : n;}
    function opv(suffix, fallback) {return v('op.'+Math.max(1,root.focusedOperator)+'.'+suffix,fallback);}
    Canvas { id: plot; anchors.fill: parent
        onPaint: {
            let c=getContext('2d');c.reset();let w=width,h=height,p=22;
            c.strokeStyle=Theme.line2;c.lineWidth=1;
            for(let i=0;i<=4;i++){c.beginPath();c.moveTo(p,p+(h-2*p)*i/4);c.lineTo(w-p,p+(h-2*p)*i/4);c.stroke();}
            for(let i=0;i<=8;i++){c.beginPath();c.moveTo(p+(w-2*p)*i/8,p);c.lineTo(p+(w-2*p)*i/8,h-p);c.stroke();}
            function draw(points,col,width=3,fill=false) {c.beginPath();points.forEach((pt,i)=>i?c.lineTo(pt[0],pt[1]):c.moveTo(pt[0],pt[1]));c.strokeStyle=col;c.lineWidth=width;c.stroke();if(fill){c.lineTo(points[points.length-1][0],h-p);c.lineTo(points[0][0],h-p);c.closePath();c.fillStyle=Qt.rgba(root.tint.r,root.tint.g,root.tint.b,.10);c.fill();}}
            if(root.mode === 'FILTER') {
                let fc=Math.log(root.v('filter.cutoff',1100)/20)/Math.log(1000),res=root.v('filter.resonance',40)/100,morph=(root.v('filter.morph',-100)+100)/200,sem=root.values[root.prefix+'filter.morph'] !== undefined,pts=[];
                for(let i=0;i<=240;i++){let x=i/240,low=1/(1+Math.exp((x-fc)*18)),high=1-low;let type=root.v('filter.mode',0);let gain=sem ? low*(1-morph)+high*morph : type===2?high:type===3?Math.exp(-Math.pow((x-fc)*9,2)):low;
                    if(root.v('filter.bp',0))gain=Math.exp(-Math.pow((x-fc)*9,2));
                    gain+=res*.3*Math.exp(-Math.pow((x-fc)*25,2));pts.push([p+x*(w-2*p),h*.7-gain*h*.42]);}
                draw(pts,root.tint,3,true);let marker=p+fc*(w-2*p);c.strokeStyle=Theme.white;c.lineWidth=2;c.beginPath();c.moveTo(marker,p);c.lineTo(marker,h-p);c.stroke();
                c.fillStyle=Theme.white;c.fillRect(marker-5,h*.7-(sem?.5:1)*h*.42-res*h*.12-5,10,10);
                c.fillStyle=Theme.grayN;c.font='12px monospace';c.fillText('20 Hz',p,h-5);c.fillText('20 kHz',w-72,h-5);
            } else if(root.mode === 'ENV' || root.mode === 'DX7 ENV') {
                if(root.mode === 'DX7 ENV') {
                    let total=0,durations=[];for(let i=1;i<=4;i++){let d=.12+Math.pow(1-root.opv('rate.'+i,60)/99,2);durations.push(d);total+=d;}
                    let x=p,pts=[[p,h-p-(h-2*p)*root.opv('level.4',0)/99]];
                    for(let i=1;i<=4;i++){x+=(w-2*p)*durations[i-1]/total;pts.push([x,h-p-(h-2*p)*root.opv('level.'+i,0)/99]);}
                    draw(pts,root.tint,3,true);
                    for(let i=1;i<=4;i++){c.fillStyle=Theme.white;c.fillRect(pts[i][0]-5,pts[i][1]-5,10,10);c.fillStyle=Theme.grayN;c.font='12px monospace';c.fillText('R'+i+' / L'+i,Math.min(w-78,pts[i][0]-25),h-5);}
                } else {
                    for(let env of ['filter','amp']) {
                        let pre=synth.selectedOperator ? 'op.'+synth.selectedOperator : env;
                        let a=root.v(pre+'.attack',4),d=root.v(pre+'.decay',220),r=root.v(pre+'.release',620),s=root.v(pre+'.sustain',70)/100;
                        let t=600+a+d+r,x1=p+(w-2*p)*a/t,x2=x1+(w-2*p)*d/t,x3=x2+(w-2*p)*600/t;
                        let pts=[[p,h-p],[x1,p],[x2,h-p-s*(h-2*p)],[x3,h-p-s*(h-2*p)],[w-p,h-p]];
                        draw(pts,env === 'amp' ? root.tint : '#F2F2F2',env === 'amp'?3:2,env === 'amp');
                    }
                }
            } else {
                let shape=root.mode === 'OSC' ? root.v('osc.'+root.oscillator+'.wave',0) : root.v('lfo.shape',1),pts=[];
                for(let i=0;i<=400;i++){let phase=(i/200)%1,y=shape === 0 ? (root.mode === 'OSC'? 1-2*phase : Math.sin(phase*Math.PI*2)) : shape === 1 ? 1-4*Math.abs(phase-.5) : shape === 2 ? (root.mode === 'OSC' ? Math.sin(phase*Math.PI*2) : 1-2*phase) : (phase<.5?1:-1);pts.push([p+i/400*(w-2*p),h/2-y*(h/2-p)]);}
                draw(pts,root.tint,3);
            }
        }
        Connections {target:root;function onValuesChanged(){plot.requestPaint()} function onModeChanged(){plot.requestPaint()} function onOscillatorChanged(){plot.requestPaint()} function onFocusedOperatorChanged(){plot.requestPaint()} function onPrefixChanged(){plot.requestPaint()} function onTintChanged(){plot.requestPaint()} }
    }
}
