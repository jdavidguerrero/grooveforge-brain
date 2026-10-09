(function(){
const C={BG:'#0A0A0A',TEAL:'#1D9E75',TEAL_PLUS:'#5DCAA5',TEAL_DIM:'#0E5040',PURPLE:'#534AB7',PURPLE_BRIGHT:'#7A6FE0',GRAY:'#9FE1CB',WHITE:'#F2F2F2',MON:'#5A5A6E',RED:'#E84A4A',AMBER:'#E8B84A',ORANGE:'#FF6A35',SURFACE:'#141414',SURFACE_2:'#1C1C1C',LINE:'#242424',PURPLE_DIM:'#2B2758'};
const MODE={SYNTH:{mc:C.TEAL,hi:C.TEAL_PLUS,dim:C.TEAL_DIM},FX:{mc:C.PURPLE,hi:C.PURPLE_BRIGHT,dim:C.PURPLE_DIM}};
const CAM={8:'#1FB6BB',9:'#1D86C4',10:'#3358C0',11:'#5540B8',12:'#8A2DB5',1:'#B61F32',2:'#DD4D28',3:'#DD902C',4:'#DDB42A',5:'#B0CC30',6:'#74C443',7:'#2EBB87'};
function mix(a,b,t){const p=h=>[1,3,5].map(i=>parseInt(h.slice(i,i+2),16));const A=p(a),B=p(b);return '#'+A.map((v,i)=>Math.round(v*t+B[i]*(1-t)).toString(16).padStart(2,'0')).join('').toUpperCase();}
const fFrac=f=>Math.log(f/20)/Math.log(1000);
const S={cutoff:1240,res:0.62,key:{n:8,m:'A',code:'8A',name:'A MINOR',conf:87,color:CAM[8]}};
S.cutFrac=fFrac(S.cutoff);
function ticks(v,cx,cy,r1,r2,n,on,off,cur){const idx=Math.round(v*(n-1)),o=[];for(let i=0;i<n;i++){const a=(-135+270*i/(n-1))*Math.PI/180,s=Math.sin(a),c=-Math.cos(a),ri=i===idx?r1-10:r1;o.push({x1:+(cx+s*ri).toFixed(1),y1:+(cy+c*ri).toFixed(1),x2:+(cx+s*r2).toFixed(1),y2:+(cy+c*r2).toFixed(1),c:i===idx?(cur||C.WHITE):i<idx?on:off,w:i<=idx?3:2});}return o;}
function filterDb(f,fc,res){const r=f/fc;return -10*Math.log10(1+Math.pow(r,8))+res*14*Math.exp(-Math.pow(Math.log2(r),2)/0.05);}
function spectrum(t,byp,n){n=n||32;const o=[];for(let i=0;i<n;i++){const x=i/(n-1),f=20*Math.pow(1000,(i+.5)/n);let db=-12-x*20+Math.exp(-Math.pow((i-4)/2.2,2))*9*(0.55+0.45*Math.sin(t*13))+Math.sin(t*2.3+i*.9)*4+Math.sin(t*5.7+i*2.7)*3;if(!byp)db+=filterDb(f,S.cutoff,S.res)*0.85;o.push(Math.max(-60,Math.min(0,db)));}return o;}
function curve(W,H,fc,res,top){let d='';for(let x=0;x<=W;x+=4){const f=20*Math.pow(1000,x/W);const db=filterDb(f,fc,res);let y=H*((top-db)/(top+60));y=Math.max(1,Math.min(H-1,y));d+=(x?'L':'M')+x+' '+y.toFixed(1)+' ';}return d;}
function wheel(cx,cy,rO,rM,rI,act){const segs=[];const P=(r,a)=>(cx+r*Math.sin(a)).toFixed(1)+','+(cy-r*Math.cos(a)).toFixed(1);
 for(let n=1;n<=12;n++){for(const ring of ['B','A']){const r2=ring==='B'?rO:rM-2,r1=ring==='B'?rM:rI;const ac=(n%12)*30*Math.PI/180,h=14*Math.PI/180,a0=ac-h,a1=ac+h;
  const d=`M${P(r2,a0)} A${r2} ${r2} 0 0 1 ${P(r2,a1)} L${P(r1,a1)} A${r1} ${r1} 0 0 0 ${P(r1,a0)}Z`;
  const on=act.n===n&&act.m===ring,rm=(r1+r2)/2;
  segs.push({d,fill:on?CAM[n]:mix(CAM[n],C.BG,0.3),stroke:on?C.WHITE:'none',lx:+(cx+rm*Math.sin(ac)).toFixed(1),ly:+(cy-rm*Math.cos(ac)).toFixed(1),label:n+ring,tc:on?C.BG:'#B8B8B8'});}}return segs;}
const ENGINES=[
 {id:'modeld',name:'MODEL D',tag:'MONO · 3 VCO',arch:'MONO · 3 VCO · LADDER FILTER'},
 {id:'juno',name:'JUNO-106',tag:'POLY 6 · CHORUS',arch:'POLY 6 VOICES · DCO · BBD CHORUS'},
 {id:'prophet',name:'PROPHET-5',tag:'POLY 5 · 2 VCO',arch:'POLY 5 VOICES · 2 VCO PER VOICE'},
 {id:'ob6',name:'OB-6',tag:'POLY 6 · DUAL SAW',arch:'POLY 6 VOICES · DUAL SAW'},
 {id:'arp',name:'ARP 2600',tag:'SEMI-MODULAR',arch:'SEMI-MODULAR · PATCH MEMORY'},
 {id:'dx7',name:'DX7',tag:'FM · 4-OP',arch:'FM · 4 OPERATORS · POLY 16'}];
const FX=['CYMATIC RESONATOR','GRANULAR CLOUD','GHOST ECHO','SPECTRAL SMEAR','TAPE SATURATE','BIT SCULPT','MODAL REVERB','PHASE CHORUS','PITCH MOSAIC','SPRING + PLATE','GLITCH STUTTER','SUB GENESIS'].map(n=>({name:n,dis:['SPECTRAL SMEAR','PITCH MOSAIC','GLITCH STUTTER'].includes(n)}));
function illo(R,id,w,h){
 const e=R.createElement,k=[];let n=0,ki=0;
 const A=(t,p)=>k.push(e(t,Object.assign({key:n++},p)));
 const rect=(x,y,W,H,f,s,sw)=>A('rect',{x,y,width:W,height:H,fill:f,stroke:s||'none',strokeWidth:sw||0});
 const circ=(cx,cy,r,f,s,sw)=>A('circle',{cx,cy,r,fill:f,stroke:s||'none',strokeWidth:sw||0});
 const line=(x1,y1,x2,y2,s,sw)=>A('line',{x1,y1,x2,y2,stroke:s,strokeWidth:sw||1});
 const path=(d,s,sw)=>A('path',{d,stroke:s,strokeWidth:sw,fill:'none'});
 const knob=(cx,cy,r)=>{circ(cx,cy,r,C.BG,C.TEAL,1.5);const a=(((ki++)*67)%240-120)*Math.PI/180;line(cx,cy,cx+Math.sin(a)*(r-1.5),cy-Math.cos(a)*(r-1.5),C.TEAL_PLUS,1.5);};
 const keys=(x,y,W,H,cnt)=>{const kw=W/cnt;for(let i=0;i<cnt;i++)rect(x+i*kw,y,kw,H,C.GRAY,C.BG,1);const pat=[1,1,0,1,1,1,0];for(let i=0;i<cnt-1;i++)if(pat[i%7])rect(x+(i+1)*kw-kw*0.3,y,kw*0.6,H*0.6,C.BG);};
 const body=(x,y,W,H)=>{rect(x,y,W,H,'#161616',C.TEAL_DIM,2);line(x+2,y+2,x+W-2,y+2,C.TEAL,2);};
 const fader=(x,y,H,p)=>{line(x,y,x,y+H,C.TEAL_DIM,2);rect(x-3.5,y+(H-6)*(1-p),7,6,C.TEAL_PLUS);};
 const jack=(cx,cy)=>circ(cx,cy,2.5,C.BG,C.GRAY,1);
 const shadow=(x,W)=>rect(x,151,W,5,'#050505');
 const pr=i=>[0.3,0.7,0.5,0.85,0.4,0.6,0.2,0.75][i%8];
 rect(0,0,280,180,C.BG);rect(0,151,280,29,'#0D0F0E');line(0,151,280,151,C.TEAL_DIM,1);
 if(id==='modeld'){shadow(36,208);rect(36,44,12,107,C.TEAL_DIM);rect(232,44,12,107,C.TEAL_DIM);body(48,44,184,58);rect(48,102,184,6,'#0F0F0F');body(48,108,184,43);keys(56,116,168,35,21);for(let r=0;r<3;r++)for(let i=0;i<7;i++)knob(66+i*25,58+r*15,5.5);circ(226,96,2,C.TEAL_PLUS);}
 if(id==='juno'){shadow(16,248);body(16,72,248,79);line(18,110,262,110,C.TEAL_DIM,1);for(let i=0;i<16;i++)fader(30+i*12,80,24,pr(i));for(let i=0;i<6;i++)rect(222+(i%2)*18,80+Math.floor(i/2)*9,14,6,i===1||i===2?C.TEAL_PLUS:C.TEAL_DIM);keys(24,116,232,35,36);}
 if(id==='prophet'){shadow(20,240);rect(20,76,10,75,C.TEAL_DIM);rect(250,76,10,75,C.TEAL_DIM);body(30,76,220,75);for(let r=0;r<2;r++)for(let i=0;i<11;i++)knob(45+i*19,88+r*14,5.5);for(let i=0;i<12;i++)rect(45+i*16.5,111,10,4,i===3?C.TEAL_PLUS:C.TEAL);keys(36,119,208,32,29);}
 if(id==='ob6'){shadow(28,224);rect(40,145,16,6,C.TEAL_DIM);rect(224,145,16,6,C.TEAL_DIM);body(28,60,224,85);path('M40 76 l8 -8 v8 l8 -8 v8',C.TEAL_PLUS,2);path('M64 76 l8 -8 v8 l8 -8 v8',C.TEAL_PLUS,2);for(let r=0;r<2;r++)for(let i=0;i<6;i++)knob(52+i*35,94+r*24,9.5);for(let i=0;i<9;i++)rect(44+i*22,133,14,6,i===0?C.TEAL_PLUS:C.TEAL_DIM);}
 if(id==='arp'){shadow(36,208);body(76,8,128,104);for(let r=0;r<2;r++)for(let i=0;i<8;i++)fader(90+i*14.5,18+r*26,20,pr(i+r*3));for(let i=0;i<9;i++){jack(92+i*12,70);jack(92+i*12,80);}circ(96,98,8,'#0F0F0F',C.TEAL_DIM,1.5);circ(184,98,8,'#0F0F0F',C.TEAL_DIM,1.5);path('M104 70 Q 124 104 152 80',C.PURPLE_BRIGHT,2);path('M128 70 Q 160 98 188 70',C.TEAL_PLUS,2);line(100,112,100,124,C.TEAL_DIM,3);line(180,112,180,124,C.TEAL_DIM,3);body(36,124,208,27);keys(44,129,192,22,24);}
 if(id==='dx7'){shadow(16,248);body(16,92,248,59);rect(28,100,52,14,C.TEAL_DIM);line(32,105,66,105,C.TEAL_PLUS,1.5);line(32,110,56,110,C.TEAL_PLUS,1.5);for(let r=0;r<2;r++)for(let i=0;i<16;i++)rect(88+i*10.4,99+r*9,8,5,r===0?(i%4===3?C.PURPLE_BRIGHT:C.TEAL_DIM):(i%4===3?C.TEAL_PLUS:C.TEAL));keys(24,120,232,31,32);}
 return e('svg',{width:w,height:h,viewBox:'0 0 280 180',preserveAspectRatio:'xMidYMid slice',style:{display:'block'}},k);
}
window.GF={C,MODE,CAM,mix,S,fFrac,ticks,filterDb,spectrum,curve,wheel,illo,ENGINES,FX};
})();
